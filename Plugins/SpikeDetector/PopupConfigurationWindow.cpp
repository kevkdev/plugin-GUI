/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2014 Open Ephys

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

#include "PopupConfigurationWindow.h"

#include "SpikeDetector.h"
#include "SpikeDetectorEditor.h"
#include <stdio.h>

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

EditableTextCustomComponent::EditableTextCustomComponent (SpikeDetector* spikeDetector_, StringParameter* name_, bool acquisitionIsActive_)
    : ParameterEditor (name_),
      name (name_),
      spikeDetector (spikeDetector_),
      acquisitionIsActive (acquisitionIsActive_)
{
    label = std::make_unique<Label> (param->getKey(), param->getValueAsString());
    label->setFont (FontOptions ("Inter", "Regular", 14.0f));
    label->setEditable (false, ! acquisitionIsActive, false);
    label->addListener (this);
    addAndMakeVisible (label.get());
}

void EditableTextCustomComponent::setRowAndColumn (const int newRow, const int newColumn)
{
    row = newRow;
    columnId = newColumn;
    name = (StringParameter*) param;
}

void EditableTextCustomComponent::labelTextChanged (Label* labelThatHasChanged)
{
    if (labelThatHasChanged != label.get())
        return;

    String candidateName = label->getText();
    String newName = spikeDetector->ensureUniqueName (candidateName, name->getStreamId());

    label->setText (newName, dontSendNotification);

    name->setNextValue (newName);
}

void EditableTextCustomComponent::updateView()
{
    if (param == nullptr)
        return;

    label->setText (param->getValueAsString(), dontSendNotification);
}

PopupThresholdComponent::PopupThresholdComponent (SpikeDetectorTableModel* table_,
                                                  ThresholdSelectorCustomComponent* owner_,
                                                  int row_,
                                                  int numChannels,
                                                  ThresholderType type,
                                                  Array<FloatParameter*> abs_thresholds_,
                                                  Array<FloatParameter*> std_thresholds_,
                                                  Array<FloatParameter*> dyn_thresholds_,
                                                  bool lockThresholds) : PopupComponent (owner_),
                                                                         table (table_),
                                                                         owner (owner_),
                                                                         row (row_),
                                                                         thresholdType (type),
                                                                         abs_thresholds (abs_thresholds_),
                                                                         dyn_thresholds (dyn_thresholds_),
                                                                         std_thresholds (std_thresholds_)
{
    const int sliderWidth = 20;

    label = std::make_unique<Label> ("Label", "Type:");
    label->setBounds (5, 5, 55, 15);
    label->setEditable (false);
    addAndMakeVisible (label.get());

    absButton = std::make_unique<UtilityButton> ("uV");
    absButton->setBounds (7, 25, 40, 25);
    absButton->setTooltip ("Detection threshold = microvolt value");
    absButton->setToggleState (type == ThresholderType::ABS, dontSendNotification);
    absButton->addListener (this);
    addAndMakeVisible (absButton.get());

    stdButton = std::make_unique<UtilityButton> ("STD");
    stdButton->setBounds (7, 55, 40, 25);
    stdButton->setTooltip ("Detection threshold = multiple of the channel's standard deviation");
    stdButton->setToggleState (type == ThresholderType::STD, dontSendNotification);
    stdButton->addListener (this);
    addAndMakeVisible (stdButton.get());

    dynButton = std::make_unique<UtilityButton> ("MED");
    dynButton->setBounds (7, 85, 40, 25);
    dynButton->setTooltip ("Detection threshold = multiple of the median of the channel's absolute value");
    dynButton->setToggleState (type == ThresholderType::DYN, dontSendNotification);
    dynButton->addListener (this);
    addAndMakeVisible (dynButton.get());

    createSliders();

    lockButton = std::make_unique<UtilityButton> ("LOCK");
    lockButton->setBounds (72 + sliderWidth * numChannels, 50, 42, 20);
    lockButton->setClickingTogglesState (true);

    if (numChannels > 1)
    {
        lockButton->setToggleState (lockThresholds, dontSendNotification);
        addAndMakeVisible (lockButton.get());

        setSize (lockButton->getRight() + 5, 117);
    }
    else
    {
        lockButton->setToggleState (false, dontSendNotification);
        setSize (95, 117);
    }
}

void PopupThresholdComponent::createSliders()
{
    const int sliderWidth = 20;

    sliders.clear();

    for (int i = 0; i < abs_thresholds.size(); i++)
    {
        Slider* slider = new Slider ("SLIDER" + String (i + 1));
        slider->setSliderStyle (Slider::LinearBarVertical);
        slider->setTextBoxStyle (Slider::NoTextBox, false, sliderWidth, 10);
        slider->setChangeNotificationOnlyOnRelease (true);

        switch (thresholdType)
        {
            case ABS:
                slider->setRange (25, 200, 1);
                slider->setValue (std::abs (abs_thresholds[i]->getFloatValue()), dontSendNotification);
                break;

            case STD:
                slider->setRange (2, 10, 0.1);
                slider->setValue (std_thresholds[i]->getFloatValue(), dontSendNotification);
                break;

            case DYN:
                slider->setRange (2, 10, 0.1);
                slider->setValue (dyn_thresholds[i]->getFloatValue(), dontSendNotification);
                break;
        }
        slider->addListener (this);
        slider->setSize (sliderWidth - 2, 100);
        slider->setTopLeftPosition (60 + sliderWidth * i, 10);

        if (thresholdType == ABS)
        {
            slider->setTransform (AffineTransform::rotation (M_PI,
                                                             slider->getX() + slider->getWidth() / 2,
                                                             slider->getY() + slider->getHeight() / 2));
        }

        sliders.add (slider);
        addAndMakeVisible (slider);
    }
}

PopupThresholdComponent::~PopupThresholdComponent()
{
}

void PopupThresholdComponent::sliderValueChanged (Slider* slider)
{
    if (lockButton->getToggleState())
    {
        for (auto sl : sliders)
        {
            sl->setValue (slider->getValue(), dontSendNotification);
        }
    }

    table->broadcastThresholdToSelectedRows (row, // original row
                                             thresholdType, // threshold type
                                             sliders.indexOf (slider), // channel index
                                             lockButton->getToggleState(), // isLocked
                                             slider->getValue()); // value
}

void PopupThresholdComponent::buttonClicked (Button* button)
{
    absButton->setToggleState (button == absButton.get(), dontSendNotification);
    stdButton->setToggleState (button == stdButton.get(), dontSendNotification);
    dynButton->setToggleState (button == dynButton.get(), dontSendNotification);

    if (button == absButton.get())
    {
        thresholdType = ABS;
    }
    else if (button == stdButton.get())
    {
        thresholdType = STD;
    }
    else if (button == dynButton.get())
    {
        thresholdType = DYN;
    }

    table->broadcastThresholdTypeToSelectedRows (row, // original row
                                                 thresholdType); // threshold type

    createSliders();
}

ThresholdSelectorCustomComponent::ThresholdSelectorCustomComponent (SpikeChannel* channel_, bool acquisitionIsActive_)
    : channel (channel_),
      acquisitionIsActive (acquisitionIsActive_)
{
    thresholder_type = (CategoricalParameter*) channel->getParameter ("thrshlder_type");

    for (int ch = 0; ch < channel->getNumChannels(); ch++)
    {
        abs_thresholds.add ((FloatParameter*) channel->getParameter ("abs_threshold" + String (ch + 1)));
        std_thresholds.add ((FloatParameter*) channel->getParameter ("std_threshold" + String (ch + 1)));
        dyn_thresholds.add ((FloatParameter*) channel->getParameter ("dyn_threshold" + String (ch + 1)));
    }
}

ThresholdSelectorCustomComponent::~ThresholdSelectorCustomComponent()
{
}

void ThresholdSelectorCustomComponent::mouseDown (const MouseEvent& event)
{
    if (channel == nullptr)
        return;

    auto* popupComponent = new PopupThresholdComponent (table,
                                                        this,
                                                        row,
                                                        channel->getNumChannels(),
                                                        ThresholderType (thresholder_type->getSelectedIndex()),
                                                        abs_thresholds,
                                                        std_thresholds,
                                                        dyn_thresholds,
                                                        true);

    CoreServices::getPopupManager()->showPopup (std::unique_ptr<Component> (popupComponent), this);
}

void ThresholdSelectorCustomComponent::setSpikeChannel (SpikeChannel* ch)
{
    channel = ch;

    if (channel == nullptr)
        return;

    thresholder_type = (CategoricalParameter*) channel->getParameter ("thrshlder_type");

    abs_thresholds.clear();
    std_thresholds.clear();
    dyn_thresholds.clear();

    for (int ch = 0; ch < channel->getNumChannels(); ch++)
    {
        abs_thresholds.add ((FloatParameter*) channel->getParameter ("abs_threshold" + String (ch + 1)));
        std_thresholds.add ((FloatParameter*) channel->getParameter ("std_threshold" + String (ch + 1)));
        dyn_thresholds.add ((FloatParameter*) channel->getParameter ("dyn_threshold" + String (ch + 1)));
    }
}

void ThresholdSelectorCustomComponent::setRowAndColumn (const int newRow, const int newColumn)
{
    row = newRow;
    columnId = newColumn;
}

void ThresholdSelectorCustomComponent::paint (Graphics& g)
{
    if (channel == nullptr)
        return;

    thresholdString = "";

    switch (thresholder_type->getSelectedIndex())
    {
        case 0:
            thresholdString += "µV: ";
            break;
        case 1:
            thresholdString += "STD: ";
            break;
        case 2:
            thresholdString += "MED: ";
            break;
    }

    for (int i = 0; i < channel->getNumChannels(); i++)
    {
        switch (thresholder_type->getSelectedIndex())
        {
            case 0:
                thresholdString += String (abs_thresholds[i]->getFloatValue(), 0);
                break;
            case 1:
                thresholdString += String (std_thresholds[i]->getFloatValue(), 1);
                break;
            case 2:
                thresholdString += String (dyn_thresholds[i]->getFloatValue(), 1);
                break;
        }

        thresholdString += ",";
    }

    thresholdString = thresholdString.substring (0, thresholdString.length() - 1);

    g.setColour (findColour (ThemeColours::defaultText));
    g.setFont (FontOptions ("Inter", "Regular", 14.0f));
    g.drawFittedText (thresholdString, 4, 0, getWidth() - 8, getHeight(), Justification::centredLeft, 1, 0.75f);
}

void ThresholdSelectorCustomComponent::setThreshold (ThresholderType type, int channelNum, float value)
{
    switch (type)
    {
        case ABS:
            abs_thresholds[channelNum]->setNextValue (value);
            break;
        case STD:
            std_thresholds[channelNum]->setNextValue (value);
            break;
        case DYN:
            dyn_thresholds[channelNum]->setNextValue (value);
            break;
    }

    repaint();
}

ChannelSelectorCustomComponent::ChannelSelectorCustomComponent (int rowNumber, SelectedChannelsParameter* channels_, bool acquisitionIsActive_)
    : ParameterEditor (channels_), channels (channels_), acquisitionIsActive (acquisitionIsActive_)
{
    label = std::make_unique<Label> (param->getKey());
    addAndMakeVisible (label.get());

    label->setFont (FontOptions ("Inter", "Regular", 14.0f));
    label->setEditable (false, false, false);
    label->setInterceptsMouseClicks (false, false);
}

void ChannelSelectorCustomComponent::showAsPopup()
{
    auto* channelSelector = new PopupChannelSelector ((Component*) this, this, channels->getChannelStates());

    channelSelector->setChannelButtonColour (Colour (0, 174, 239));
    channelSelector->setMaximumSelectableChannels (channels->getMaxSelectableChannels());

    CoreServices::getPopupManager()->showPopup (std::unique_ptr<Component> (channelSelector), this);
}

void ChannelSelectorCustomComponent::mouseDown (const juce::MouseEvent& event)
{
    if (acquisitionIsActive)
        return;

    showAsPopup();
}

void ChannelSelectorCustomComponent::setRowAndColumn (const int newRow, const int newColumn)
{
    channels = (SelectedChannelsParameter*) param;
}

void ChannelSelectorCustomComponent::updateView()
{
    if (param == nullptr)
        return;

    String s = "[" + param->getValueAsString() + "]";

    label->setText (s, dontSendNotification);

    if (auto parent = getParentComponent())
        parent->repaint();
}

void WaveformSelectorCustomComponent::mouseDown (const juce::MouseEvent& event)
{
    if (acquisitionIsActive)
        return;

    /*if (waveformtype->getValueAsString().equalsIgnoreCase("FULL"))
    {
        table->broadcastWaveformTypeToSelectedRows(row, 1);
    }
    else
    {
        table->broadcastWaveformTypeToSelectedRows(row, 0);
    }
    
    repaint();*/
}

void WaveformSelectorCustomComponent::setWaveformValue (int value)
{
    waveformtype->setNextValue (value);

    //repaint();
}

void WaveformSelectorCustomComponent::paint (Graphics& g)
{
    if (waveformtype->getValueAsString().equalsIgnoreCase ("FULL"))
        g.setColour (Colours::green);
    else
        g.setColour (Colours::red);

    g.fillRoundedRectangle (6, 6, getWidth() - 12, getHeight() - 12, 4);
    g.setColour (Colours::white);
    g.setFont (FontOptions ("Inter", "Regular", 14.0f));
    g.drawText (waveformtype->getValueAsString(), 4, 4, getWidth() - 8, getHeight() - 8, Justification::centred);
}

void WaveformSelectorCustomComponent::setRowAndColumn (const int newRow, const int newColumn)
{
    row = newRow;
    repaint();
}

void DeleteButtonCustomComponent::mouseDown (const juce::MouseEvent& event)
{
    if (acquisitionIsActive)
        return;

    table->deleteSelectedRows (row);
}

void DeleteButtonCustomComponent::paint (Graphics& g)
{
    int width = getWidth();
    int height = getHeight();

    if (acquisitionIsActive)
    {
        g.setColour (Colours::grey);
    }
    else
    {
        g.setColour (Colours::red);
    }

    g.fillEllipse (7, 7, width - 14, height - 14);
    g.setColour (Colours::white);
    g.fillRect (9, (height / 2) - 2, width - 19, 3);
}

void DeleteButtonCustomComponent::setRowAndColumn (const int newRow, const int newColumn)
{
    row = newRow;
    repaint();
}

SpikeDetectorTableModel::SpikeDetectorTableModel (SpikeDetectorEditor* editor_,
                                                  PopupConfigurationWindow* owner_,
                                                  bool acquisitionIsActive_)
    : editor (editor_), owner (owner_), acquisitionIsActive (acquisitionIsActive_)
{
}

void SpikeDetectorTableModel::cellClicked (int rowNumber, int columnId, const MouseEvent& event)
{
    //std::cout << rowNumber << " " << columnId << " : selected " << std::endl;

    //if (event.mods.isRightButtonDown())
    //    std::cout << "Right click!" << std::endl;
}

void SpikeDetectorTableModel::deleteKeyPressed (int lastRowSelected)
{
    deleteSelectedRows (lastRowSelected);
}

void SpikeDetectorTableModel::deleteSelectedRows (int rowThatWasClicked)
{
    SparseSet<int> selectedRows = table->getSelectedRows();
    Array<int> indeces;

    if (! acquisitionIsActive)
    {
        Array<SpikeChannel*> channelsToDelete;

        for (int i = 0; i < spikeChannels.size(); i++)
        {
            if (selectedRows.contains (i) || i == rowThatWasClicked)
            {
                channelsToDelete.add (spikeChannels[i]);
                indeces.add (i);
            }
        }

        editor->removeSpikeChannels (owner, channelsToDelete, indeces);

        table->deselectAllRows();
    }
}

void SpikeDetectorTableModel::broadcastWaveformTypeToSelectedRows (int rowThatWasClicked, int value)
{
    SparseSet<int> selectedRows = table->getSelectedRows();

    for (int i = 0; i < spikeChannels.size(); i++)
    {
        if (selectedRows.contains (i) || i == rowThatWasClicked)
        {
            Component* c = refreshComponentForCell (i, SpikeDetectorTableModel::WAVEFORM, selectedRows.contains (i), nullptr);

            jassert (c != nullptr);

            WaveformSelectorCustomComponent* waveformButton = (WaveformSelectorCustomComponent*) c;

            jassert (waveformButton != nullptr);

            waveformComponents.add (waveformButton);

            waveformButton->setWaveformValue (value);
        }
    }

    table->updateContent();
}

void SpikeDetectorTableModel::broadcastThresholdTypeToSelectedRows (int rowThatWasClicked, ThresholderType type)
{
    SparseSet<int> selectedRows = table->getSelectedRows();

    for (int i = 0; i < spikeChannels.size(); i++)
    {
        if (selectedRows.contains (i) || i == rowThatWasClicked)
        {
            switch (type)
            {
                case ABS:
                    spikeChannels[i]->getParameter ("thrshlder_type")->setNextValue (0);
                    break;
                case STD:
                    spikeChannels[i]->getParameter ("thrshlder_type")->setNextValue (1);
                    break;
                case DYN:
                    spikeChannels[i]->getParameter ("thrshlder_type")->setNextValue (2);
                    break;
            }

            Component* c = table->getCellComponent (SpikeDetectorTableModel::Columns::THRESHOLD, i);

            if (c != nullptr)
                c->repaint();
        }
    }

    table->updateContent();

    table->repaint();
}

void SpikeDetectorTableModel::broadcastThresholdToSelectedRows (int rowThatWasClicked,
                                                                ThresholderType type,
                                                                int channelIndex,
                                                                bool isLocked,
                                                                float value)
{
    SparseSet<int> selectedRows = table->getSelectedRows();

    //std::cout << "Broadcasting value for " << rowThatWasClicked << ", " << channelIndex << std::endl;

    float actualValue;

    for (int i = 0; i < spikeChannels.size(); i++)
    {
        if (selectedRows.contains (i) || i == rowThatWasClicked)
        {
            //std::cout << "Row = " << i << std::endl;

            String parameterString;

            switch (type)
            {
                case ABS:
                    parameterString = "abs_threshold";
                    actualValue = -value;
                    break;
                case STD:
                    parameterString = "std_threshold";
                    actualValue = value;
                    break;
                case DYN:
                    parameterString = "dyn_threshold";
                    actualValue = value;
                    break;
            }

            //std::cout << "Type = " << parameterString << std::endl;

            if (isLocked)
            {
                //std::cout << "Not locked." << std::endl;

                for (int ch = 0; ch < spikeChannels[i]->getNumChannels(); ch++)
                {
                    //std::cout << "Setting value for channel " << ch << ": " << actualValue << std::endl;
                    spikeChannels[i]->getParameter (parameterString + String (ch + 1))->setNextValue (actualValue);
                }
            }
            else
            {
                //std::cout << "Setting value for channel " << channelIndex << ": " << actualValue << std::endl;

                if (spikeChannels[i]->getNumChannels() > channelIndex)
                    spikeChannels[i]->getParameter (parameterString + String (channelIndex + 1))->setNextValue (actualValue);
            }

            Component* c = table->getCellComponent (SpikeDetectorTableModel::Columns::THRESHOLD, i);

            if (c != nullptr)
            {
                //std::cout << "Repainting" << std::endl;
                c->repaint();
            }
        }
    }

    table->updateContent();

    table->repaint();
}

Component* SpikeDetectorTableModel::refreshComponentForCell (int rowNumber,
                                                             int columnId,
                                                             bool isRowSelected,
                                                             Component* existingComponentToUpdate)
{
    if (columnId == SpikeDetectorTableModel::Columns::NAME)
    {
        auto* textLabel = static_cast<EditableTextCustomComponent*> (existingComponentToUpdate);

        if (textLabel == nullptr)
        {
            SpikeDetector* spikeDetector = (SpikeDetector*) editor->getProcessor();

            textLabel = new EditableTextCustomComponent (spikeDetector, (StringParameter*) spikeChannels[rowNumber]->getParameter ("name"), acquisitionIsActive);
        }

        textLabel->setParameter ((StringParameter*) spikeChannels[rowNumber]->getParameter ("name"));
        textLabel->setRowAndColumn (rowNumber, columnId);

        return textLabel;
    }
    else if (columnId == SpikeDetectorTableModel::Columns::CHANNELS)
    {
        auto* channelsLabel = static_cast<ChannelSelectorCustomComponent*> (existingComponentToUpdate);

        if (channelsLabel == nullptr)
        {
            channelsLabel = new ChannelSelectorCustomComponent (
                rowNumber,
                (SelectedChannelsParameter*) spikeChannels[rowNumber]->getParameter ("local_channels"),
                acquisitionIsActive);
        }

        channelsLabel->setParameter ((SelectedChannelsParameter*) spikeChannels[rowNumber]->getParameter ("local_channels"));
        channelsLabel->setRowAndColumn (rowNumber, columnId);

        return channelsLabel;
    }
    else if (columnId == SpikeDetectorTableModel::Columns::WAVEFORM)
    {
        auto* waveformButton = static_cast<WaveformSelectorCustomComponent*> (existingComponentToUpdate);

        if (waveformButton == nullptr)
        {
            waveformButton = new WaveformSelectorCustomComponent (
                (CategoricalParameter*) spikeChannels[rowNumber]->getParameter ("waveform_type"),
                acquisitionIsActive);
        }

        waveformButton->setParameter ((CategoricalParameter*) spikeChannels[rowNumber]->getParameter ("waveform_type"));
        waveformButton->setRowAndColumn (rowNumber, columnId);
        waveformButton->setTableModel (this);

        return waveformButton;
    }
    else if (columnId == SpikeDetectorTableModel::Columns::THRESHOLD)
    {
        auto* thresholdSelector = static_cast<ThresholdSelectorCustomComponent*> (existingComponentToUpdate);

        if (thresholdSelector == nullptr)
        {
            thresholdSelector = new ThresholdSelectorCustomComponent (
                spikeChannels[rowNumber],
                acquisitionIsActive);
        }

        thresholdSelector->setSpikeChannel (spikeChannels[rowNumber]);
        thresholdSelector->setRowAndColumn (rowNumber, columnId);
        thresholdSelector->setTableModel (this);

        return thresholdSelector;
    }

    else if (columnId == SpikeDetectorTableModel::Columns::DELETE)
    {
        auto* deleteButton = static_cast<DeleteButtonCustomComponent*> (existingComponentToUpdate);

        if (deleteButton == nullptr)
        {
            deleteButton = new DeleteButtonCustomComponent (
                acquisitionIsActive);
        }

        deleteButton->setRowAndColumn (rowNumber, columnId);
        deleteButton->setTableModel (this);

        return deleteButton;
    }

    jassert (existingComponentToUpdate == nullptr);

    return nullptr;
}

int SpikeDetectorTableModel::getNumRows()
{
    return spikeChannels.size();
}

void SpikeDetectorTableModel::update (Array<SpikeChannel*> spikeChannels_)
{
    spikeChannels = spikeChannels_;

    table->updateContent();

    waveformComponents.clear();
    thresholdComponents.clear();

    for (int i = 0; i < getNumRows(); i++)
    {
        Component* c = table->getCellComponent (SpikeDetectorTableModel::Columns::THRESHOLD, i);

        if (c == nullptr)
            continue;

        ThresholdSelectorCustomComponent* th = (ThresholdSelectorCustomComponent*) c;

        th->setSpikeChannel (spikeChannels[i]);

        th->repaint();
    }
}

void SpikeDetectorTableModel::paintRowBackground (Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowNumber >= spikeChannels.size())
        return;

    if (rowIsSelected)
    {
        if (rowNumber % 2 == 0)
            g.fillAll (owner->findColour (ThemeColours::componentBackground));
        else
            g.fillAll (owner->findColour (ThemeColours::componentBackground).darker (0.25f));

        g.setColour (owner->findColour (ThemeColours::highlightedFill));
        g.drawRoundedRectangle (2, 2, width - 4, height - 4, 5, 2);

        return;
    }

    if (spikeChannels[rowNumber]->isValid())
    {
        if (rowNumber % 2 == 0)
            g.fillAll (owner->findColour (ThemeColours::componentBackground));
        else
            g.fillAll (owner->findColour (ThemeColours::componentBackground).darker (0.25f));

        return;
    }

    if (rowNumber % 2 == 0)
        g.fillAll (Colour (90, 50, 50));
    else
        g.fillAll (Colour (60, 30, 30));
}

void SpikeDetectorTableModel::paintCell (Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    g.setFont (FontOptions ("Inter", "Regular", 14.0f));

    if (columnId == SpikeDetectorTableModel::Columns::INDEX)
    {
        g.setColour (owner->findColour (ThemeColours::defaultText));
        g.drawText (String (rowNumber + 1), 4, 0, width, height, Justification::centred);
    }
    else if (columnId == SpikeDetectorTableModel::Columns::TYPE)
    {
        if (rowNumber >= spikeChannels.size())
            return;

        switch (spikeChannels[rowNumber]->getChannelType())
        {
            case SpikeChannel::Type::SINGLE:
                g.setColour (Colours::blue);
                g.fillRoundedRectangle (6, 6, width - 12, height - 12, 4);
                g.setColour (Colours::white);
                g.drawText ("SE", 4, 4, width - 8, height - 8, Justification::centred);
                break;
            case SpikeChannel::Type::STEREOTRODE:
                g.setColour (Colours::purple);
                g.fillRoundedRectangle (6, 6, width - 12, height - 12, 4);
                g.setColour (Colours::white);
                g.drawText ("ST", 4, 4, width - 8, height - 8, Justification::centred);
                break;
            case SpikeChannel::Type::TETRODE:
                g.setColour (Colours::green);
                g.fillRoundedRectangle (6, 6, width - 12, height - 12, 4);
                g.setColour (Colours::white);
                g.drawText ("TT", 4, 4, width - 8, height - 8, Justification::centred);
                break;
            case SpikeChannel::Type::INVALID:
                break;
        }
    }
}

SpikeChannelGenerator::SpikeChannelGenerator (SpikeDetectorEditor* editor_,
                                              PopupConfigurationWindow* window_,
                                              int channelCount_,
                                              bool acquisitionIsActive)
    : editor (editor_), window (window_), channelCount (channelCount_)
{
    lastLabelValue = "1";
    spikeChannelCountLabel = std::make_unique<Label> ("Label", lastLabelValue);
    spikeChannelCountLabel->setEditable (true);
    spikeChannelCountLabel->addListener (this);
    spikeChannelCountLabel->setJustificationType (Justification::right);
    spikeChannelCountLabel->setBounds (120, 5, 35, 20);
    addAndMakeVisible (spikeChannelCountLabel.get());

    spikeChannelTypeSelector = std::make_unique<ComboBox> ("Spike Channel Type");
    spikeChannelTypeSelector->setBounds (157, 5, 125, 20);
    spikeChannelTypeSelector->addItem ("Single electrode", SpikeChannel::SINGLE);
    spikeChannelTypeSelector->addItem ("Stereotrode", SpikeChannel::STEREOTRODE);
    spikeChannelTypeSelector->addItem ("Tetrode", SpikeChannel::TETRODE);
    spikeChannelTypeSelector->setSelectedId (SpikeChannel::SINGLE);
    addAndMakeVisible (spikeChannelTypeSelector.get());

    channelSelectorButton = std::make_unique<UtilityButton> ("Channels");
    channelSelectorButton->addListener (this);
    channelSelectorButton->setBounds (290, 5, 80, 20);
    addAndMakeVisible (channelSelectorButton.get());

    plusButton = std::make_unique<UtilityButton> ("+");
    plusButton->addListener (this);
    plusButton->setBounds (380, 5, 20, 20);
    addAndMakeVisible (plusButton.get());

    if (acquisitionIsActive)
    {
        spikeChannelCountLabel->setEnabled (false);
        spikeChannelTypeSelector->setEnabled (false);
        channelSelectorButton->setEnabled (false);
        plusButton->setEnabled (false);
    }
}

void SpikeChannelGenerator::labelTextChanged (Label* label)
{
    int value = label->getText().getIntValue();

    if (value < 1)
    {
        label->setText (lastLabelValue, dontSendNotification);
        return;
    }

    if (value > 384)
    {
        label->setText ("384", dontSendNotification);
    }
    else
    {
        label->setText (String (value), dontSendNotification);
    }

    lastLabelValue = label->getText();

    if (value == 1)
    {
        int currentId = spikeChannelTypeSelector->getSelectedId();

        spikeChannelTypeSelector->clear();

        spikeChannelTypeSelector->addItem ("Single electrode", SpikeChannel::SINGLE);
        spikeChannelTypeSelector->addItem ("Stereotrode", SpikeChannel::STEREOTRODE);
        spikeChannelTypeSelector->addItem ("Tetrode", SpikeChannel::TETRODE);
        spikeChannelTypeSelector->setSelectedId (currentId, dontSendNotification);
    }
    else
    {
        int currentId = spikeChannelTypeSelector->getSelectedId();

        spikeChannelTypeSelector->clear();

        spikeChannelTypeSelector->addItem ("Single electrodes", SpikeChannel::SINGLE);
        spikeChannelTypeSelector->addItem ("Stereotrodes", SpikeChannel::STEREOTRODE);
        spikeChannelTypeSelector->addItem ("Tetrodes", SpikeChannel::TETRODE);
        spikeChannelTypeSelector->setSelectedId (currentId, dontSendNotification);
    }
}

void SpikeChannelGenerator::buttonClicked (Button* button)
{
    if (button == plusButton.get() && channelCount > 0)
    {
        int numSpikeChannelsToAdd = spikeChannelCountLabel->getText().getIntValue();
        SpikeChannel::Type channelType = (SpikeChannel::Type) spikeChannelTypeSelector->getSelectedId();

        //std::cout << "Button clicked! Sending " << startChannels.size() << " start channels " << std::endl;

        if (startChannels.size() == 0)
            editor->addSpikeChannels (window, channelType, numSpikeChannelsToAdd);
        else
            editor->addSpikeChannels (window, channelType, startChannels.size(), startChannels);
    }
    else if (button == channelSelectorButton.get() && channelCount > 0)
    {
        std::vector<bool> channelStates;

        int numSpikeChannelsToAdd = spikeChannelCountLabel->getText().getIntValue();
        SpikeChannel::Type channelType = (SpikeChannel::Type) spikeChannelTypeSelector->getSelectedId();

        int skip = SpikeChannel::getNumChannels (channelType);

        int channelsAdded = 0;

        for (int i = 0; i < channelCount; i++)
        {
            if (startChannels.size() == 0)
            {
                if (i % skip == 0 && channelsAdded < numSpikeChannelsToAdd)
                {
                    channelStates.push_back (true);
                    channelsAdded++;
                }
                else
                    channelStates.push_back (false);
            }
            else
            {
                if (startChannels.contains (i))
                    channelStates.push_back (true);
                else
                    channelStates.push_back (false);
            }
        }

        auto* channelSelector = new PopupChannelSelector (button, this, channelStates);

        channelSelector->setChannelButtonColour (Colour (0, 174, 239));

        channelSelector->setMaximumSelectableChannels (numSpikeChannelsToAdd);

        CoreServices::getPopupManager()->showPopup (std::unique_ptr<PopupComponent> (channelSelector), channelSelectorButton.get());

        /*
        CallOutBox& myBox
            = CallOutBox::launchAsynchronously(std::unique_ptr<Component>(channelSelector),
                button->getScreenBounds(),
                nullptr);
        */
    }
}

void SpikeChannelGenerator::channelStateChanged (Array<int> selectedChannels)
{
    startChannels = selectedChannels;

    //std::cout << "Size of start channels: " << startChannels.size() << std::endl;
}

void SpikeChannelGenerator::paint (Graphics& g)
{
    g.setColour (findColour (ThemeColours::widgetBackground));
    g.fillRoundedRectangle (0, 0, getWidth(), getHeight(), 4.0f);
    g.setColour (findColour (ThemeColours::defaultText));
    g.setFont (FontOptions ("Inter", "Regular", 14.0f));
    g.drawText ("ADD ELECTRODES: ", 17, 6, 120, 19, Justification::left, false);
}

PopupConfigurationWindow::PopupConfigurationWindow (SpikeDetectorEditor* editor_,
                                                    UtilityButton* anchor,
                                                    Array<SpikeChannel*> spikeChannels,
                                                    bool acquisitionIsActive)
    : PopupComponent (anchor), editor (editor_)
{
    //tableHeader.reset(new TableHeaderComponent());

    setSize (310, 40);

    spikeChannelGenerator = std::make_unique<SpikeChannelGenerator> (editor, this, editor->getNumChannelsForCurrentStream(), acquisitionIsActive);
    addAndMakeVisible (spikeChannelGenerator.get());

    tableModel.reset (new SpikeDetectorTableModel (editor, this, acquisitionIsActive));

    electrodeTable = std::make_unique<TableListBox> ("Electrode Table", tableModel.get());
    tableModel->table = electrodeTable.get();
    electrodeTable->setHeader (std::make_unique<TableHeaderComponent>());

    electrodeTable->getHeader().addColumn ("#", SpikeDetectorTableModel::Columns::INDEX, 30, 30, 30, TableHeaderComponent::notResizableOrSortable);
    electrodeTable->getHeader().addColumn ("Name", SpikeDetectorTableModel::Columns::NAME, 140, 140, 140, TableHeaderComponent::notResizableOrSortable);
    electrodeTable->getHeader().addColumn ("Type", SpikeDetectorTableModel::Columns::TYPE, 40, 40, 40, TableHeaderComponent::notResizableOrSortable);
    electrodeTable->getHeader().addColumn ("Channels", SpikeDetectorTableModel::Columns::CHANNELS, 100, 100, 100, TableHeaderComponent::notResizableOrSortable);
    electrodeTable->getHeader().addColumn ("Thresholds", SpikeDetectorTableModel::Columns::THRESHOLD, 120, 120, 120, TableHeaderComponent::notResizableOrSortable);
    electrodeTable->getHeader().addColumn ("Waveform", SpikeDetectorTableModel::Columns::WAVEFORM, 70, 70, 70, TableHeaderComponent::notResizableOrSortable);
    electrodeTable->getHeader().addColumn (" ", SpikeDetectorTableModel::Columns::DELETE, 30, 30, 30, TableHeaderComponent::notResizableOrSortable);

    electrodeTable->setHeaderHeight (30);
    electrodeTable->setRowHeight (30);
    electrodeTable->setMultipleSelectionEnabled (true);

    viewport = std::make_unique<Viewport>();

    viewport->setViewedComponent (electrodeTable.get(), false);
    viewport->setScrollBarsShown (true, false);
    viewport->getVerticalScrollBar().addListener (this);

    addAndMakeVisible (viewport.get());
    update (spikeChannels);

    auto stream = editor->getProcessor()->getDataStream (editor->getCurrentStream());
    popupTitle = String(stream->getSourceNodeId()) + " " + stream->getSourceNodeName() + " - " + stream->getName();
}

void PopupConfigurationWindow::scrollBarMoved (ScrollBar* scrollBar, double newRangeStart)
{
    if (! updating)
    {
        scrollDistance = viewport->getViewPositionY();
    }
}

void PopupConfigurationWindow::update (Array<SpikeChannel*> spikeChannels)
{
    if (spikeChannels.size() > 0)
    {
        updating = true;

        tableModel->update (spikeChannels);

        int maxRows = 16;

        int numRowsVisible = spikeChannels.size() <= maxRows ? spikeChannels.size() : maxRows;

        int scrollBarWidth = 0;

        if (spikeChannels.size() > maxRows)
        {
            viewport->getVerticalScrollBar().setVisible (true);
            scrollBarWidth += 20;
        }
        else
        {
            viewport->getVerticalScrollBar().setVisible (false);
        }

        setSize (540 + scrollBarWidth, (numRowsVisible + 1) * 30 + 70);
        viewport->setBounds (5, 25, 530 + scrollBarWidth, (numRowsVisible + 1) * 30);
        electrodeTable->setBounds (0, 0, 530 + scrollBarWidth, (spikeChannels.size() + 1) * 30);

        viewport->setViewPosition (0, scrollDistance);

        electrodeTable->setVisible (true);

        spikeChannelGenerator->setBounds (60, viewport->getBottom() + 8, 420, 30);

        updating = false;
    }
    else
    {
        tableModel->update (spikeChannels);
        electrodeTable->setVisible (false);
        setSize (440, 65);
        spikeChannelGenerator->setBounds (10, 28, 420, 30);
    }
}

void PopupConfigurationWindow::updatePopup()
{
    SpikeDetector* spikeDetector = (SpikeDetector*) editor->getProcessor();
    update (spikeDetector->getSpikeChannelsForStream (editor->getCurrentStream()));
}

void PopupConfigurationWindow::paint (Graphics& g)
{
    g.setColour (findColour (ThemeColours::controlPanelText));
    g.setFont (FontOptions ("Inter", "Regular", 15.0f));
    g.drawFittedText (popupTitle, 10, 0, getWidth() - 20, 20, Justification::centredLeft, 1);
}

bool PopupConfigurationWindow::keyPressed (const KeyPress& key)
{
    // Popup component handles globally reserved undo/redo keys
    PopupComponent::keyPressed (key);

    // Pressing 'a' key adds a new spike channel
    if (key.getTextCharacter() == 'a')
    {
        editor->addSpikeChannels (this, spikeChannelGenerator->getSelectedType(), 1);
    }

    return true;
}
