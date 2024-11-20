/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2024 Open Ephys

    ------------------------------------------------------------------

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "EditorViewport.h"

#include "../AccessClass.h"
#include "../Processors/MessageCenter/MessageCenterEditor.h"
#include "../Processors/PluginManager/OpenEphysPlugin.h"
#include "../Processors/ProcessorGraph/ProcessorGraph.h"
#include "../Processors/ProcessorGraph/ProcessorGraphActions.h"
#include "GraphViewer.h"
#include "ProcessorList.h"

const int BORDER_SIZE = 6;
const int TAB_SIZE = 30;

EditorViewport::EditorViewport (SignalChainTabComponent* s_)
    : message ("Drag-and-drop some rows from the top-left box onto this component!"),
      somethingIsBeingDraggedOver (false),
      shiftDown (false),
      lastEditorClicked (0),
      selectionIndex (-1),
      insertionPoint (0),
      componentWantsToMove (false),
      indexOfMovingComponent (-1),
      loadingConfig (false),
      signalChainTabComponent (s_),
      dragProcType (Plugin::Processor::INVALID)
{
    addMouseListener (this, true);

    sourceDropImage = ImageCache::getFromMemory (BinaryData::SourceDrop_png,
                                                 BinaryData::SourceDrop_pngSize);

    sourceDropImage = sourceDropImage.rescaled (25, 135, Graphics::highResamplingQuality);

    signalChainTabComponent->setEditorViewport (this);

    editorNamingLabel.setEditable (true);
    editorNamingLabel.setBounds (0, 0, 100, 20);
    editorNamingLabel.setFont (FontOptions ("Inter", "Regular", 16.0f));
    editorNamingLabel.addListener (this);
}

EditorViewport::~EditorViewport()
{
    copyBuffer.clear();
}

void EditorViewport::paint (Graphics& g)
{
    g.setColour (findColour (ThemeColours::componentParentBackground));
    g.fillRoundedRectangle (1, 1, getWidth() - 2, getHeight() - 14, 5.0f);

    // Draw drop shadow for each editor
    for (int i = 0; i < editorArray.size(); i++)
    {
        if (editorArray[i]->getProcessor()->isEmpty())
            continue;

        DropShadow (findColour (ThemeColours::dropShadowColour), 10, Point<int> (4, 2))
            .drawForRectangle (g, editorArray[i]->getBounds().reduced (1, 1));
    }

    if (somethingIsBeingDraggedOver)
    {
        if (insertionPoint == 1
            && editorArray[0]->getProcessor()->isEmpty()
            && dragProcType == Plugin::Processor::SOURCE)
            return;

        float insertionX = 0;

        if (insertionPoint == 0)
        {
            insertionX = (float) (BORDER_SIZE) * 2.5f;
        }
        else if (insertionPoint > 0)
        {
            insertionX += editorArray[insertionPoint - 1]->getRight() + (BORDER_SIZE) * 1.5f;
        }

        g.setColour (Colours::yellow);
        g.fillRect (insertionX, (float) BORDER_SIZE + 5, 3.0f, (float) (getHeight() - 3 * (BORDER_SIZE + 5)));
    }
}

bool EditorViewport::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    if (! CoreServices::getAcquisitionStatus() && dragSourceDetails.description.toString().startsWith ("Processors"))
    {
        return false;
    }
    else if (dragSourceDetails.description.toString().startsWith ("EditorDrag"))
    {
        return false;
    }
    else
    {
        return true;
    }
}

void EditorViewport::itemDragEnter (const SourceDetails& dragSourceDetails)
{
    if (! CoreServices::getAcquisitionStatus())
    {
        somethingIsBeingDraggedOver = true;
        beginDragAutoRepeat (20);
        repaint();
    }
}

void EditorViewport::itemDragMove (const SourceDetails& dragSourceDetails)
{
    const int x = dragSourceDetails.localPosition.getX();

    if (! CoreServices::getAcquisitionStatus())
    {
        const auto mousePos = signalChainTabComponent->getViewport()->getMouseXYRelative();
        signalChainTabComponent->getViewport()->autoScroll (mousePos.getX(), mousePos.getY(), 40, 10);

        Array<var>* descr = dragSourceDetails.description.getArray();
        dragProcType = (Plugin::Processor::Type) int (descr->getUnchecked (4));

        bool foundInsertionPoint = false;

        int lastCenterPoint = -1;
        int leftEdge;
        int centerPoint;

        for (int n = 0; n < editorArray.size(); n++)
        {
            leftEdge = editorArray[n]->getX();
            centerPoint = leftEdge + (editorArray[n]->getWidth()) / 2;

            if (x < centerPoint && x > lastCenterPoint)
            {
                if ((n == 0 || n == 1) && editorArray[0]->getProcessor()->isEmpty())
                {
                    insertionPoint = 1;

                    if (dragProcType == Plugin::Processor::SOURCE)
                        editorArray[0]->highlight();
                }
                else
                {
                    insertionPoint = n;

                    if (editorArray[0]->getProcessor()->isEmpty()
                        && editorArray[0]->getSelectionState())
                        editorArray[0]->deselect();
                }
                foundInsertionPoint = true;
            }

            lastCenterPoint = centerPoint;
        }

        if (! foundInsertionPoint)
        {
            insertionPoint = editorArray.size();

            if (editorArray.size() > 0
                && editorArray[0]->getProcessor()->isEmpty()
                && editorArray[0]->getSelectionState())
                editorArray[0]->deselect();
        }

        repaint();

        refreshEditors();
    }
}

void EditorViewport::itemDragExit (const SourceDetails& dragSourceDetails)
{
    somethingIsBeingDraggedOver = false;
    dragProcType = Plugin::Processor::INVALID;

    beginDragAutoRepeat (0);

    if (editorArray.size() > 0
        && editorArray[0]->getProcessor()->isEmpty()
        && editorArray[0]->getSelectionState())
        editorArray[0]->deselect();

    repaint();

    refreshEditors();
}

void EditorViewport::itemDropped (const SourceDetails& dragSourceDetails)
{
    if (! CoreServices::getAcquisitionStatus())
    {
        Array<var>* descr = dragSourceDetails.description.getArray();

        Plugin::Description description;

        description.fromProcessorList = descr->getUnchecked (0);
        description.name = descr->getUnchecked (1);
        description.index = descr->getUnchecked (2);
        description.type = (Plugin::Type) int (descr->getUnchecked (3));
        description.processorType = (Plugin::Processor::Type) int (descr->getUnchecked (4));
        description.nodeId = 0;

        LOGD ("Item dropped at insertion point ", insertionPoint);

        addProcessor (description, insertionPoint);

        insertionPoint = -1; // make sure all editors are left-justified
        indexOfMovingComponent = -1;
        somethingIsBeingDraggedOver = false;
        dragProcType = Plugin::Processor::INVALID;

        if (editorArray.size() > 0
            && editorArray[0]->getProcessor()->isEmpty()
            && editorArray[0]->getSelectionState())
            editorArray[0]->deselect();

        refreshEditors();
    }
}

GenericProcessor* EditorViewport::addProcessor (Plugin::Description description, int insertionPt)
{
    GenericProcessor* source = nullptr;
    GenericProcessor* dest = nullptr;

    if (insertionPoint > 0)
    {
        source = editorArray[insertionPoint - 1]->getProcessor();
    }

    if (editorArray.size() > insertionPoint)
    {
        dest = editorArray[insertionPoint]->getProcessor();
    }

    AddProcessor* action = new AddProcessor (description, source, dest, loadingConfig);

    if (! loadingConfig)
    {
        AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");
        AccessClass::getProcessorGraph()->getUndoManager()->perform (action);
        return action->processor;
    }
    else
    {
        action->perform();
        orphanedActions.add (action);
        return action->processor;
    }
}

void EditorViewport::clearSignalChain()
{
    if (! CoreServices::getAcquisitionStatus() && ! signalChainIsLocked)
    {
        LOGD ("Clearing signal chain.");

        AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");
        ClearSignalChain* action = new ClearSignalChain();
        AccessClass::getProcessorGraph()->getUndoManager()->perform (action);
    }
    else
    {
        CoreServices::sendStatusMessage ("Cannot clear signal chain while acquisition is active.");
    }
}

void EditorViewport::lockSignalChain (bool shouldLock)
{
    signalChainIsLocked = shouldLock;
}

void EditorViewport::makeEditorVisible (GenericEditor* editor, bool updateSettings)
{
    if (updateSettings)
        AccessClass::getProcessorGraph()->updateSettings (editor->getProcessor());
    else
        AccessClass::getProcessorGraph()->updateViews (editor->getProcessor());

    for (auto ed : editorArray)
    {
        if (ed == editor)
        {
            ed->select();
        }
        else
        {
            ed->deselect();
        }
    }
}

void EditorViewport::highlightEditor (GenericEditor* editor)
{
    // Do not highlight if the editor is already selected
    if (editor->getSelectionState())
        return;

    AccessClass::getProcessorGraph()->updateViews (editor->getProcessor());

    Array<GenericProcessor*> processors = AccessClass::getProcessorGraph()->getListOfProcessors();

    for (auto proc : processors)
    {
        auto ed = proc->getEditor();
        if (ed == editor)
        {
            ed->highlight();
        }
        else
        {
            ed->deselect();
        }
    }
}

void EditorViewport::removeEditor (GenericEditor* editor)
{
    int matchingIndex = -1;

    for (int i = 0; i < editorArray.size(); i++)
    {
        if (editorArray[i] == editor)
            matchingIndex = i;
    }

    if (matchingIndex > -1)
        editorArray.remove (matchingIndex);
}

void EditorViewport::updateVisibleEditors (Array<GenericEditor*> visibleEditors,
                                           int numberOfTabs,
                                           int selectedTab)
{
    if (visibleEditors.size() > 0)
    {
        for (auto editor : editorArray)
        {
            LOGD ("Updating ", editor->getNameAndId());
            editor->setVisible (false);
        }
    }

    editorArray.clear();

    for (auto editor : visibleEditors)
    {
        editorArray.add (editor);
        addChildComponent (editor);
        editor->setVisible (true);
        editor->refreshColours();
    }

    refreshEditors();
    signalChainTabComponent->refreshTabs (numberOfTabs, selectedTab);
    repaint();
}

int EditorViewport::getDesiredWidth()
{
    int desiredWidth = 0;

    for (auto editor : editorArray)
    {
        desiredWidth += editor->getTotalWidth() + BORDER_SIZE;
    }

    if (somethingIsBeingDraggedOver && insertionPoint == editorArray.size())
        desiredWidth += 2 * BORDER_SIZE;

    return desiredWidth + BORDER_SIZE;
}

void EditorViewport::refreshEditors()
{
    int lastBound = BORDER_SIZE;

    bool pastRightEdge = false;

    int rightEdge = getWidth();
    int numEditors = editorArray.size();

    for (int n = 0; n < editorArray.size(); n++)
    {
        GenericEditor* editor = editorArray[n];

        int componentWidth = editor->getTotalWidth();

        // if (n == 0 && !editor->getProcessor()->isSource())
        // {
        //     // leave room to drop a source node
        //     lastBound += (BORDER_SIZE * 25) + BORDER_SIZE ;
        // }

        if (somethingIsBeingDraggedOver && n == insertionPoint)
        {
            if (indexOfMovingComponent == -1
                && n == 1
                && editorArray[0]->getProcessor()->isEmpty()
                && dragProcType == Plugin::Processor::SOURCE)
            {
                // Do not move any processor
            }
            else if (indexOfMovingComponent == -1 // adding new processor
                     || (n != indexOfMovingComponent && n != indexOfMovingComponent + 1))
            {
                if (n == 0)
                    lastBound += BORDER_SIZE * 3;
                else
                    lastBound += BORDER_SIZE * 2;
            }
        }

        editor->setVisible (true);
        editor->setBounds (lastBound, BORDER_SIZE, componentWidth, getHeight() - BORDER_SIZE * 4);
        lastBound += (componentWidth + BORDER_SIZE);
    }

    signalChainTabComponent->resized();

    repaint();
}

void EditorViewport::moveSelection (const KeyPress& key)
{
    ModifierKeys mk = key.getModifiers();

    if (key.getKeyCode() == key.leftKey)
    {
        if (mk.isShiftDown()
            && lastEditorClicked != nullptr
            && editorArray.contains (lastEditorClicked))
        {
            int primaryIndex = editorArray.indexOf (lastEditorClicked);

            // set new selection index
            if (selectionIndex == -1)
            {
                // if no selection index has been set yet, set it to the primary index
                selectionIndex = primaryIndex == 0 ? 0 : primaryIndex - 1;
            }
            else if (selectionIndex == 0)
            {
                // if the selection index is already at the left edge, return
                return;
            }
            else if (selectionIndex <= primaryIndex)
            {
                // if previous selection index is to the left of the primary index, decrement it
                selectionIndex--;
            }

            // if the editor at the new selection index is empty, skip it
            if (editorArray[selectionIndex]->getProcessor()->isEmpty())
            {
                selectionIndex++;
                return;
            }

            // switch selection state of the editor at the new selection index
            if (selectionIndex != primaryIndex)
                editorArray[selectionIndex]->switchSelectedState();

            // if the selection index is to the right of the primary index,
            // decrement it after switching the selection state
            if (selectionIndex > primaryIndex)
                selectionIndex--;
        }
        else
        {
            selectionIndex = -1;

            for (int i = 0; i < editorArray.size(); i++)
            {
                if (editorArray[i]->getSelectionState() && i > 0)
                {
                    editorArray[i - 1]->select();
                    lastEditorClicked = editorArray[i - 1];
                    editorArray[i]->deselect();
                }
            }
        }
    }
    else if (key.getKeyCode() == key.rightKey)
    {
        if (mk.isShiftDown()
            && lastEditorClicked != nullptr
            && editorArray.contains (lastEditorClicked))
        {
            int primaryIndex = editorArray.indexOf (lastEditorClicked);

            if (selectionIndex == -1)
            {
                // if no selection index has been set yet, set it to the primary index
                selectionIndex = primaryIndex == (editorArray.size() - 1) ? primaryIndex : primaryIndex + 1;
            }
            else if (selectionIndex == editorArray.size() - 1)
            {
                // if the selection index is already at the right edge, return
                return;
            }
            else if (selectionIndex >= primaryIndex)
            {
                // if previous selection index is to the right of the primary index, increment it
                selectionIndex++;
            }

            // switch selection state of the editor at the new selection index
            if (selectionIndex != primaryIndex)
                editorArray[selectionIndex]->switchSelectedState();

            // if the selection index is to the left of the primary index,
            // increment it after switching the selection state
            if (selectionIndex < primaryIndex)
                selectionIndex++;
        }
        else
        {
            selectionIndex = -1;

            // bool stopSelection = false;
            int i = 0;

            while (i < editorArray.size() - 1)
            {
                if (editorArray[i]->getSelectionState())
                {
                    lastEditorClicked = editorArray[i + 1];
                    editorArray[i + 1]->select();
                    editorArray[i]->deselect();
                    i += 2;
                }
                else
                {
                    editorArray[i]->deselect();
                    i++;
                }
            }
        }
    }
}

bool EditorViewport::keyPressed (const KeyPress& key)
{
    LOGDD ("Editor viewport received ", key.getKeyCode());

    if (! CoreServices::getAcquisitionStatus() && editorArray.size() > 0)
    {
        ModifierKeys mk = key.getModifiers();

        if (key.getKeyCode() == key.deleteKey || key.getKeyCode() == key.backspaceKey)
        {
            if (! mk.isAnyModifierKeyDown())
            {
                deleteSelectedProcessors();

                return true;
            }
        }
        else if (key.getKeyCode() == key.leftKey || key.getKeyCode() == key.rightKey)
        {
            moveSelection (key);

            return true;
        }
        else if (key.getKeyCode() == key.upKey)
        {
            if (lastEditorClicked)
            {
                if (lastEditorClicked->isMerger() || lastEditorClicked->isSplitter())
                {
                    lastEditorClicked->switchIO (0);
                    AccessClass::getProcessorGraph()->updateViews (lastEditorClicked->getProcessor());
                    this->grabKeyboardFocus();
                }
            }
            else
            {
                lastEditorClicked = editorArray.getFirst();
                lastEditorClicked->select();
            }

            return true;
        }
        else if (key.getKeyCode() == key.downKey)
        {
            if (lastEditorClicked)
            {
                if (lastEditorClicked->isMerger() || lastEditorClicked->isSplitter())
                {
                    lastEditorClicked->switchIO (1);
                    AccessClass::getProcessorGraph()->updateViews (lastEditorClicked->getProcessor());
                    this->grabKeyboardFocus();
                }
            }
            else
            {
                lastEditorClicked = editorArray.getFirst();
                lastEditorClicked->select();
            }
            return true;
        }
    }

    return false;
}

void EditorViewport::switchIO (GenericProcessor* processor, int path)
{
    AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");

    SwitchIO* switchIO = new SwitchIO (processor, path);

    AccessClass::getProcessorGraph()->getUndoManager()->perform (switchIO);
}

void EditorViewport::copySelectedEditors()
{
    LOGDD ("Editor viewport received copy signal");

    if (! CoreServices::getAcquisitionStatus())
    {
        Array<XmlElement*> copyInfo;

        for (auto editor : editorArray)
        {
            if (! editor->getProcessor()->isEmpty() && editor->getSelectionState())
                copyInfo.add (AccessClass::getProcessorGraph()->createNodeXml (editor->getProcessor(), false));
        }

        if (copyInfo.size() > 0)
        {
            copy (copyInfo);
        }
        else
        {
            CoreServices::sendStatusMessage ("No processors selected.");
        }
    }
    else
    {
        CoreServices::sendStatusMessage ("Cannot copy while acquisition is active.");
    }
}

bool EditorViewport::editorIsSelected()
{
    for (auto editor : editorArray)
    {
        if (editor->getSelectionState())
            return true;
    }

    return false;
}

bool EditorViewport::canPaste()
{
    if (copyBuffer.size() > 0 && editorIsSelected())
        return true;
    else
        return false;
}

void EditorViewport::copy (Array<XmlElement*> copyInfo)
{
    copyBuffer.clear();
    copyBuffer.addArray (copyInfo);
}

void EditorViewport::paste()
{
    LOGDD ("Editor viewport received paste signal");

    if (! CoreServices::getAcquisitionStatus())
    {
        int insertionPoint;
        bool foundSelected = false;

        for (int i = 0; i < editorArray.size(); i++)
        {
            if (editorArray[i]->getSelectionState())
            {
                insertionPoint = i + 1;
                foundSelected = true;
            }
        }

        LOGDD ("Insertion point: ", insertionPoint);

        if (foundSelected)
        {
            Array<XmlElement*> processorInfo;

            for (auto xml : copyBuffer)
            {
                for (auto* element : xml->getChildWithTagNameIterator ("EDITOR"))
                {
                    for (auto* subelement : element->getChildWithTagNameIterator ("WINDOW"))
                    {
                        subelement->setAttribute ("Active", 0);
                        subelement->setAttribute ("Index", -1);
                    }

                    for (auto* subelement : element->getChildWithTagNameIterator ("TAB"))
                    {
                        subelement->setAttribute ("Active", 0);
                    }
                }
                processorInfo.add (xml);
            }

            AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");

            GenericProcessor* source = nullptr;
            GenericProcessor* dest = nullptr;
            if (insertionPoint > 0)
            {
                source = editorArray[insertionPoint - 1]->getProcessor();
            }

            if (editorArray.size() > insertionPoint)
            {
                dest = editorArray[insertionPoint]->getProcessor();
            }

            PasteProcessors* action = new PasteProcessors (processorInfo, insertionPoint, source, dest);

            AccessClass::getProcessorGraph()->getUndoManager()->perform (action);
        }
        else
        {
            CoreServices::sendStatusMessage ("Select an insertion point to paste.");
        }
    }
    else
    {
        CoreServices::sendStatusMessage ("Cannot paste while acquisition is active.");
    }
}

void EditorViewport::labelTextChanged (Label* label)
{
    if (label == &editorNamingLabel && label->getText().isNotEmpty())
    {
        editorToUpdate->setDisplayName (label->getText());

        if (auto parentComp = editorNamingLabel.getParentComponent())
            parentComp->exitModalState (0);
    }
    else
    {
        editorNamingLabel.setText (editorToUpdate->getDisplayName(), dontSendNotification);
    }
}

void EditorViewport::mouseDown (const MouseEvent& e)
{
    bool clickInEditor = false;

    for (int i = 0; i < editorArray.size(); i++)
    {
        if (e.eventComponent == editorArray[i])
        {
            if (editorArray[i]->getProcessor()->isEmpty())
                return;

            if (e.getNumberOfClicks() == 2) // double-clicks toggle collapse state
            {
                if (editorArray[i]->getCollapsedState())
                {
                    editorArray[i]->switchCollapsedState();
                }
                else
                {
                    if (e.y < 22)
                    {
                        editorArray[i]->switchCollapsedState();
                    }
                }
                return;
            }

            if (e.mods.isRightButtonDown())
            {
                if (! editorArray[i]->getCollapsedState() && e.y > 22)
                    return;

                if (editorArray[i]->isMerger() || editorArray[i]->isSplitter())
                    return;

                editorArray[i]->highlight();

                PopupMenu m;
                m.setLookAndFeel (&getLookAndFeel());

                if (editorArray[i]->getCollapsedState())
                    m.addItem (3, "Uncollapse", true);
                else
                    m.addItem (3, "Collapse", true);

                if (! CoreServices::getAcquisitionStatus() && ! signalChainIsLocked)
                    m.addItem (2, "Delete", true);
                else
                    m.addItem (2, "Delete", false);

                m.addItem (1, "Rename", ! signalChainIsLocked);

                m.addSeparator();

                m.addItem (4, "Save settings...", true);

                if (! CoreServices::getAcquisitionStatus() && ! signalChainIsLocked)
                    m.addItem (5, "Load settings...", true);
                else
                    m.addItem (5, "Load settings...", false);

                m.addSeparator();

                m.addItem (6, "Save image...", true);

                Plugin::Type type = editorArray[i]->getProcessor()->getPluginType();
                if (type != Plugin::Type::BUILT_IN && type != Plugin::Type::INVALID)
                {
                    m.addSeparator();
                    String pluginVer = editorArray[i]->getProcessor()->getLibVersion();
                    m.addItem (7, "Plugin v" + pluginVer, false);
                }

                const int result = m.showMenu (PopupMenu::Options {}.withStandardItemHeight (20));

                if (result == 1)
                {
                    editorToUpdate = editorArray[i];
                    editorNamingLabel.setText (editorToUpdate->getDisplayName(), dontSendNotification);

                    int nameWidth = GlyphArrangement::getStringWidthInt (editorNamingLabel.getFont(), editorNamingLabel.getText()) + 10;
                    editorNamingLabel.setSize (nameWidth > 100 ? nameWidth : 100, 20);
                    editorNamingLabel.setColour (Label::backgroundColourId, findColour (ThemeColours::widgetBackground));
                    editorNamingLabel.showEditor();

                    juce::Rectangle<int> rect1 = juce::Rectangle<int> (editorToUpdate->getScreenX() + 40, editorToUpdate->getScreenY() + 18, 1, 1);

                    CallOutBox callOut (editorNamingLabel, rect1, nullptr);
                    callOut.runModalLoop();
                    callOut.setDismissalMouseClicksAreAlwaysConsumed (true);

                    return;
                }
                else if (result == 2)
                {
                    deleteSelectedProcessors();

                    return;
                }
                else if (result == 3)
                {
                    editorArray[i]->switchCollapsedState();
                    refreshEditors();
                    return;
                }
                else if (result == 4)
                {
                    FileChooser fc ("Choose the file name...",
                                    CoreServices::getDefaultUserSaveDirectory(),
                                    "*",
                                    true);

                    if (fc.browseForFileToSave (true))
                    {
                        savePluginState (fc.getResult(), editorArray[i]);
                    }
                    else
                    {
                        CoreServices::sendStatusMessage ("No file chosen.");
                    }
                }
                else if (result == 5)
                {
                    FileChooser fc ("Choose a settings file to load...",
                                    CoreServices::getDefaultUserSaveDirectory(),
                                    "*",
                                    true);

                    if (fc.browseForFileToOpen())
                    {
                        currentFile = fc.getResult();
                        loadPluginState (currentFile, editorArray[i]);
                    }
                    else
                    {
                        CoreServices::sendStatusMessage ("No file selected.");
                    }
                }
                else if (result == 6)
                {
                    File picturesDirectory = File::getSpecialLocation (File::SpecialLocationType::userPicturesDirectory);

                    String editorName = editorArray[i]->getName() + "_" + String (editorArray[i]->getProcessor()->getNodeId());
                    File outputFile = picturesDirectory.getNonexistentChildFile (editorName, ".png");

                    juce::Rectangle<int> bounds = juce::Rectangle<int> (3, 3, editorArray[i]->getWidth() - 6, editorArray[i]->getHeight() - 6);

                    Image componentImage = editorArray[i]->createComponentSnapshot (
                        bounds,
                        true,
                        1.5f);

                    FileOutputStream stream (outputFile);
                    PNGImageFormat pngWriter;
                    pngWriter.writeImageToStream (componentImage, stream);

                    CoreServices::sendStatusMessage ("Saved image to " + outputFile.getFullPathName());
                }
            }

            // make sure uncollapsed editors don't accept clicks outside their title bar
            if (! editorArray[i]->getCollapsedState() && e.y > 22)
                return;

            clickInEditor = true;
            editorArray[i]->select();

            if (e.mods.isShiftDown())
            {
                if (editorArray.contains (lastEditorClicked))
                {
                    int index = editorArray.indexOf (lastEditorClicked);

                    if (index > i)
                    {
                        for (int j = i + 1; j <= index; j++)
                        {
                            editorArray[j]->select();
                        }
                    }
                    else
                    {
                        for (int j = i - 1; j >= index; j--)
                        {
                            editorArray[j]->select();
                        }
                    }
                }

                selectionIndex = i;
                break;
            }

            beginDragAutoRepeat (20);

            lastEditorClicked = editorArray[i];
            selectionIndex = -1;
        }
        else
        {
            if (! e.mods.isCtrlDown() && ! e.mods.isShiftDown())
                editorArray[i]->deselect();
        }
    }

    if (! clickInEditor)
        lastEditorClicked = 0;
}

void EditorViewport::mouseDrag (const MouseEvent& e)
{
    if (! signalChainIsLocked)
    {
        if (editorArray.contains ((GenericEditor*) e.originalComponent)
            && e.y < 15
            && ! CoreServices::getAcquisitionStatus()
            && editorArray.size() > 1
            && e.getDistanceFromDragStart() > 10)
        {
            indexOfMovingComponent = editorArray.indexOf ((GenericEditor*) e.originalComponent);
            dragProcType = editorArray[indexOfMovingComponent]->getProcessor()->getProcessorType();
            if (editorArray[indexOfMovingComponent]->getProcessor()->isEmpty())
            {
                editorArray[indexOfMovingComponent]->deselect();
                indexOfMovingComponent = -1;
                return;
            }
            else
            {
                componentWantsToMove = true;
            }
        }

        if (componentWantsToMove)
        {
            somethingIsBeingDraggedOver = true;

            bool foundInsertionPoint = false;

            int lastCenterPoint = 0;
            int leftEdge;
            int centerPoint;

            const MouseEvent event = e.getEventRelativeTo (this);

            const auto mousePos = signalChainTabComponent->getViewport()->getMouseXYRelative();
            signalChainTabComponent->getViewport()->autoScroll (mousePos.getX(), mousePos.getY(), 40, 10);

            for (int n = 0; n < editorArray.size(); n++)
            {
                leftEdge = editorArray[n]->getX();
                centerPoint = leftEdge + (editorArray[n]->getWidth()) / 2;

                if (event.x < centerPoint && event.x > lastCenterPoint)
                {
                    if (editorArray[n]->getProcessor()->isSource() && ! editorArray[indexOfMovingComponent]->getProcessor()->isSource())
                        return;

                    if (n == 0 && editorArray[0]->getProcessor()->isEmpty())
                    {
                        insertionPoint = n + 1;
                    }
                    else
                    {
                        insertionPoint = n;
                    }
                    foundInsertionPoint = true;
                }

                lastCenterPoint = centerPoint;
            }

            if (! foundInsertionPoint && indexOfMovingComponent != editorArray.size() - 1)
            {
                insertionPoint = editorArray.size();
            }

            refreshEditors();
            repaint();
        }
    }
}

void EditorViewport::mouseUp (const MouseEvent& e)
{
    if (componentWantsToMove)
    {
        somethingIsBeingDraggedOver = false;
        componentWantsToMove = false;
        dragProcType = Plugin::Processor::INVALID;

        if (! getScreenBounds().contains (e.getScreenPosition()))
        {
            //AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction("Disabled during acquisition");

            //DeleteProcessor* action =
            //       new DeleteProcessor(
            //           editorArray[indexOfMovingComponent]->getProcessor(),
            //          this);

            //AccessClass::getProcessorGraph()->getUndoManager()->perform(action);

            repaint();

            refreshEditors();
        }
        else
        {
            if (indexOfMovingComponent != insertionPoint
                && indexOfMovingComponent != insertionPoint - 1)
            {
                GenericProcessor* newSource;
                GenericProcessor* newDest;

                if (insertionPoint == editorArray.size())
                {
                    newDest = nullptr;
                    newSource = editorArray.getLast()->getProcessor();
                }
                else if (insertionPoint == 0)
                {
                    newDest = editorArray.getFirst()->getProcessor();
                    newSource = nullptr;
                }
                else
                {
                    newSource = editorArray[insertionPoint - 1]->getProcessor();
                    newDest = editorArray[insertionPoint]->getProcessor();
                }

                AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");

                MoveProcessor* action = new MoveProcessor (
                    editorArray[indexOfMovingComponent]->getProcessor(),
                    newSource,
                    newDest,
                    insertionPoint > indexOfMovingComponent);

                AccessClass::getProcessorGraph()->getUndoManager()->perform (action);
            }
            else
            {
                repaint();
            }
        }
    }
}

void EditorViewport::mouseExit (const MouseEvent& e)
{
    if (componentWantsToMove)
    {
        somethingIsBeingDraggedOver = false;
        componentWantsToMove = false;
        dragProcType = Plugin::Processor::INVALID;

        repaint();
    }
}

bool EditorViewport::isSignalChainEmpty()
{
    if (editorArray.size() == 0)
        return true;
    else
        return false;
}

///////////////////////////////////////////////////////////////////
////////////////SIGNAL CHAIN TAB BUTTON///////////
///////////////////////////////////////////////////////////////////

SignalChainTabButton::SignalChainTabButton (int index) : Button ("Signal Chain Tab Button " + String (index)),
                                                         num (index)
{
    setRadioGroupId (99);
    setClickingTogglesState (true);

    buttonFont = FontOptions ("Silkscreen", 10, Font::plain).withHeight (14);

    offset = 0;
}

void SignalChainTabButton::clicked()
{
    if (getToggleState())
    {
        LOGDD ("Tab button clicked: ", num);

        AccessClass::getProcessorGraph()->viewSignalChain (num);
    }
}

void SignalChainTabButton::paintButton (Graphics& g, bool isMouseOver, bool isButtonDown)
{
    ColourGradient grad1, grad2;

    if (getToggleState() == true)
    {
        grad1 = ColourGradient (Colour (255, 136, 34), 0.0f, 0.0f, Colour (230, 193, 32), 0.0f, 20.0f, false);

        grad2 = ColourGradient (Colour (255, 136, 34), 0.0f, 20.0f, Colour (230, 193, 32), 0.0f, 0.0f, false);
    }
    else
    {
        grad2 = ColourGradient (Colour (80, 80, 80), 0.0f, 20.0f, Colour (120, 120, 120), 0.0f, 0.0f, false);

        grad1 = ColourGradient (Colour (80, 80, 80), 0.0f, 0.0f, Colour (120, 120, 120), 0.0f, 20.0f, false);
    }

    if (isMouseOver)
    {
        grad1.multiplyOpacity (0.7f);
        grad2.multiplyOpacity (0.7f);
    }

    g.setGradientFill (grad2);
    g.fillEllipse (0, 0, getWidth(), getHeight());

    g.setGradientFill (grad1);
    g.fillEllipse (2, 2, getWidth() - 4, getHeight() - 4);

    g.setFont (buttonFont);
    g.setColour (Colours::black);

    String n;

    if (num == 0)
        n = "A";
    else if (num == 1)
        n = "B";
    else if (num == 2)
        n = "C";
    else if (num == 3)
        n = "D";
    else if (num == 4)
        n = "E";
    else if (num == 5)
        n = "F";
    else if (num == 6)
        n = "G";
    else if (num == 7)
        n = "H";
    else if (num == 8)
        n = "I";
    else
        n = "-";

    g.drawText (n, 0, 0, getWidth(), getHeight() - 2, Justification::centred, true);
}

SignalChainScrollButton::SignalChainScrollButton (int direction)
    : TextButton ("Signal Chain Scroll Button " + String (direction))
{
    if (direction == DOWN)
    {
        path.addTriangle (0.0f, 0.0f, 9.0f, 20.0f, 18.0f, 0.0f);
    }
    else
    {
        path.addTriangle (0.0f, 20.0f, 9.0f, 0.0f, 18.0f, 20.0f);
    }

    setClickingTogglesState (false);
}

void SignalChainScrollButton::setActive (bool state)
{
    isActive = state;
}

void SignalChainScrollButton::paintButton (Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    g.setColour (findColour (ThemeColours::defaultFill));
    path.scaleToFit (0, 0, getWidth(), getHeight(), true);

    g.strokePath (path, PathStrokeType (1.0f, PathStrokeType::curved, PathStrokeType::rounded));
}

SignalChainTabComponent::SignalChainTabComponent()
{
    topTab = 0;

    upButton = std::make_unique<SignalChainScrollButton> (UP);
    downButton = std::make_unique<SignalChainScrollButton> (DOWN);

    upButton->addListener (this);
    downButton->addListener (this);

    addAndMakeVisible (upButton.get());
    addAndMakeVisible (downButton.get());

    viewport = std::make_unique<Viewport>();
    viewport->setScrollBarsShown (false, true, false, true);
    viewport->setScrollBarThickness (12);
    addAndMakeVisible (viewport.get());

    for (int i = 0; i < 8; i++)
    {
        SignalChainTabButton* button = new SignalChainTabButton (i);
        signalChainTabButtonArray.add (button);
        addChildComponent (signalChainTabButtonArray.getLast());
    }
}

SignalChainTabComponent::~SignalChainTabComponent()
{
}

void SignalChainTabComponent::setEditorViewport (EditorViewport* ev)
{
    editorViewport = ev;
    viewport->setViewedComponent (ev, true);
}

int SignalChainTabComponent::getScrollOffset()
{
    return viewport->getViewPositionX();
}

void SignalChainTabComponent::setScrollOffset (int offset)
{
    viewport->setViewPosition (offset, 0);
}

void SignalChainTabComponent::paint (Graphics& g)
{
    g.setColour (findColour (ThemeColours::defaultFill));

    for (int n = 0; n < 4; n++)
    {
        g.drawEllipse (7,
                       (TAB_SIZE - 2) * n + 24,
                       TAB_SIZE - 12,
                       TAB_SIZE - 12,
                       1.0);
    }
}

void SignalChainTabComponent::paintOverChildren (Graphics& g)
{
    Path leftCornerPath;
    leftCornerPath.startNewSubPath (0, 0);
    leftCornerPath.lineTo (0, 20);
    leftCornerPath.quadraticTo (-3, -3, 20, 0);
    leftCornerPath.closeSubPath();
    leftCornerPath.applyTransform (AffineTransform::translation (TAB_SIZE, 0));

    g.setColour (findColour (ThemeColours::windowBackground));
    g.fillPath (leftCornerPath);

    leftCornerPath.applyTransform (AffineTransform::verticalFlip (getHeight() - 12));
    g.fillPath (leftCornerPath);

    Path rightCornerPath;
    rightCornerPath.startNewSubPath (0, 0);
    rightCornerPath.lineTo (0, 20);
    rightCornerPath.quadraticTo (3, 3, -18, 0);
    rightCornerPath.closeSubPath();
    rightCornerPath.applyTransform (AffineTransform::translation (getWidth(), 0));

    g.fillPath (rightCornerPath);

    rightCornerPath.applyTransform (AffineTransform::verticalFlip (getHeight() - 12));
    g.fillPath (rightCornerPath);

    if (editorViewport->somethingIsBeingDraggedOver)
    {
        g.setColour (Colours::yellow);
    }
    else
    {
        g.setColour (findColour (ThemeColours::defaultFill));
    }

    g.drawRoundedRectangle (TAB_SIZE + 1, 1, getWidth() - TAB_SIZE - 2, getHeight() - 14, 10.0f, 2.0f);
}

void SignalChainTabComponent::resized()
{
    int scrollOffset = getScrollOffset();

    downButton->setBounds (10, getHeight() - 25, 12, 12);
    upButton->setBounds (10, 4, 12, 12);

    viewport->setBounds (TAB_SIZE, 0, getWidth() - TAB_SIZE, getHeight());

    int width = editorViewport->getDesiredWidth() < getWidth() - TAB_SIZE ? getWidth() - TAB_SIZE : editorViewport->getDesiredWidth();
    editorViewport->setBounds (0, 0, width, getHeight());

    setScrollOffset (scrollOffset);
}

void SignalChainTabComponent::refreshTabs (int numberOfTabs_, int selectedTab_, bool internal)
{
    numberOfTabs = numberOfTabs_;
    selectedTab = selectedTab_;

    if (! internal)
    {
        if (topTab < (selectedTab - 3))
            topTab = selectedTab - 3;
        else if (topTab > selectedTab && selectedTab != -1)
            topTab = selectedTab;
    }

    for (int i = 0; i < signalChainTabButtonArray.size(); i++)
    {
        signalChainTabButtonArray[i]->setBounds (6,
                                                 (TAB_SIZE - 2) * (i - topTab) + 23,
                                                 TAB_SIZE - 10,
                                                 TAB_SIZE - 10);

        if (i < numberOfTabs && i >= topTab && i < topTab + 4)
        {
            signalChainTabButtonArray[i]->setVisible (true);
        }
        else
        {
            signalChainTabButtonArray[i]->setVisible (false);
        }

        if (i == selectedTab)
        {
            signalChainTabButtonArray[i]->setToggleState (true, NotificationType::dontSendNotification);
        }
        else
        {
            signalChainTabButtonArray[i]->setToggleState (false, NotificationType::dontSendNotification);
        }
    }
}

void SignalChainTabComponent::buttonClicked (Button* button)
{
    if (button == upButton.get())
    {
        LOGDD ("Up button pressed.");

        if (topTab > 0)
            topTab -= 1;
    }
    else if (button == downButton.get())
    {
        LOGDD ("Down button pressed.");

        if (numberOfTabs > 4)
        {
            if (topTab < (numberOfTabs - 4))
                topTab += 1;
        }
    }

    refreshTabs (numberOfTabs, selectedTab, true);
}

// LOADING AND SAVING

const String EditorViewport::saveState (File fileToUse, String& xmlText)
{
    return saveState (fileToUse, &xmlText);
}

const String EditorViewport::saveState (File fileToUse, String* xmlText)
{
    String error;

    currentFile = fileToUse;

    std::unique_ptr<XmlElement> xml = std::make_unique<XmlElement> ("SETTINGS");

    AccessClass::getProcessorGraph()->saveToXml (xml.get());

    if (! xml->writeTo (currentFile))
        error = "Couldn't write to file ";
    else
        error = "Saved configuration as ";

    error += currentFile.getFileName();

    LOGD ("Editor viewport saved state.");

    return error;
}

void EditorViewport::saveEditorViewportSettingsToXml (XmlElement* xml)
{
    XmlElement* editorViewportSettings = new XmlElement ("EDITORVIEWPORT");
    editorViewportSettings->setAttribute ("selectedTab", signalChainTabComponent->getSelectedTab());
    editorViewportSettings->setAttribute ("scroll", signalChainTabComponent->getScrollOffset());

    xml->addChildElement (editorViewportSettings);
}

void EditorViewport::loadEditorViewportSettingsFromXml (XmlElement* element)
{
    auto* pg = AccessClass::getProcessorGraph();

    int numRootNodes = pg->getRootNodes().size();
    int selectedTab = element->getIntAttribute ("selectedTab", 0);

    if (numRootNodes > 0 && selectedTab <= numRootNodes)
        pg->viewSignalChain (selectedTab);

    int scrollOffset = element->getIntAttribute ("scroll", 0);

    signalChainTabComponent->setScrollOffset (scrollOffset);
}

const String EditorViewport::loadPluginState (File fileToLoad, GenericEditor* selectedEditor)
{
    int numSelected = 0;

    if (selectedEditor == nullptr)
    {
        for (auto editor : editorArray)
        {
            if (editor->getSelectionState())
            {
                selectedEditor = editor;
                numSelected++;
            }
        }
    }
    else
    {
        numSelected = 1;
    }

    if (numSelected == 0)
    {
        return ("No editors selected.");
    }
    else if (numSelected > 1)
    {
        return ("Multiple editors selected.");
    }
    else
    {
        XmlDocument doc (fileToLoad);
        std::unique_ptr<XmlElement> xml = doc.getDocumentElement();

        if (xml == 0 || ! xml->hasTagName ("PROCESSOR"))
        {
            LOGC ("Not a valid file.");
            return "Not a valid file.";
        }
        else
        {
            AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");

            LoadPluginSettings* action = new LoadPluginSettings (selectedEditor->getProcessor(), xml.get());
            AccessClass::getProcessorGraph()->getUndoManager()->perform (action);
        }
    }

    return "Success";
}

const String EditorViewport::savePluginState (File fileToSave, GenericEditor* selectedEditor)
{
    int numSelected = 0;

    if (selectedEditor == nullptr)
    {
        for (auto editor : editorArray)
        {
            if (editor->getSelectionState())
            {
                selectedEditor = editor;
                numSelected++;
            }
        }
    }
    else
    {
        numSelected = 1;
    }

    if (numSelected == 0)
    {
        return ("No editors selected.");
    }
    else if (numSelected > 1)
    {
        return ("Multiple editors selected.");
    }
    else
    {
        String error;

        XmlElement* settings = AccessClass::getProcessorGraph()->createNodeXml (selectedEditor->getProcessor(), false);

        if (! settings->writeTo (fileToSave))
            error = "Couldn't write to file ";
        else
            error = "Saved plugin settings to ";

        error += fileToSave.getFileName();

        delete settings;

        return error;
    }
}

std::unique_ptr<XmlElement> EditorViewport::createSettingsXml()
{
    std::unique_ptr<XmlElement> xml = std::make_unique<XmlElement> ("SETTINGS");

    AccessClass::getProcessorGraph()->saveToXml (xml.get());

    return xml;
}

XmlElement* EditorViewport::createNodeXml (GenericProcessor* processor, bool isStartOfSignalChain)
{
    XmlElement* node = AccessClass::getProcessorGraph()->createNodeXml (processor, isStartOfSignalChain);
    return node;
}

const String EditorViewport::loadState (File fileToLoad)
{
    currentFile = fileToLoad;

    LOGC ("Loading configuration from ", fileToLoad.getFullPathName());

    XmlDocument doc (currentFile);
    std::unique_ptr<XmlElement> xml = doc.getDocumentElement();

    if (xml == 0 || ! xml->hasTagName ("SETTINGS"))
    {
        LOGC ("Not a valid configuration file.");
        return "Not a valid file.";
    }

    AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");

    LoadSignalChain* action = new LoadSignalChain (xml);
    AccessClass::getProcessorGraph()->getUndoManager()->perform (action);

    CoreServices::sendStatusMessage ("Loaded " + fileToLoad.getFileName());

    AccessClass::getControlPanel()->createNewRecordingDirectory();

    return "Loaded signal chain.";
}

const String EditorViewport::loadStateFromXml (XmlElement* xml)
{
    AccessClass::getProcessorGraph()->loadFromXml (xml);
    return String();
}

void EditorViewport::deleteSelectedProcessors()
{
    if (signalChainIsLocked)
        return;

    AccessClass::getProcessorGraph()->getUndoManager()->beginNewTransaction ("Disabled during acquisition");

    Array<GenericEditor*> editors = Array (editorArray);

    for (auto editor : editors)
    {
        if (! editor->getProcessor()->isEmpty() && editor->getSelectionState())
        {
            editorArray.remove (editorArray.indexOf (editor));
            DeleteProcessor* action = new DeleteProcessor (editor->getProcessor());
            AccessClass::getProcessorGraph()->getUndoManager()->perform (action);
        }
    }
}

Plugin::Description EditorViewport::getDescriptionFromXml (XmlElement* settings, bool ignoreNodeId)
{
    return AccessClass::getProcessorGraph()->getDescriptionFromXml (settings, ignoreNodeId);
}

GenericProcessor* EditorViewport::createProcessorAtInsertionPoint (XmlElement* parametersAsXml,
                                                                   int insertionPt,
                                                                   bool ignoreNodeId)
{
    return AccessClass::getProcessorGraph()->createProcessorAtInsertionPoint (parametersAsXml, insertionPt, ignoreNodeId);
}
