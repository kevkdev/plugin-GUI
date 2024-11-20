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

#ifndef ___POPUPCHANNELSELECTOR_H_E47DE5C__
#define ___POPUPCHANNELSELECTOR_H_E47DE5C__

#include "../../../JuceLibraryCode/JuceHeader.h"
#include "../../UI/PopupComponent.h"
#include "../../Utils/Utils.h"
#include "../PluginManager/OpenEphysPlugin.h"

enum Select
{
    ALL,
    NONE,
    RANGE
};

class PopupChannelSelector;

/**
*
	Button representing a single channel

*/
class PLUGIN_API ChannelButton : public Button
{
public:
    /** Constructor */
    ChannelButton (int id, PopupChannelSelector* parent);

    /** Destructor */
    ~ChannelButton() {}

    /** Returns the channel id */
    int getId() { return id; };

private:
    /** Mouse-related callbacks*/
    void mouseDown (const MouseEvent& event) override;
    void mouseDrag (const MouseEvent& event) override;
    void mouseUp (const MouseEvent& event) override;

    int id;
    PopupChannelSelector* parent;
    int width;
    int height;
    void paintButton (Graphics& g, bool isMouseOver, bool isButtonDown) override;
};

/**
*
	Button that selects the channels specified by the range editor

*/
class PLUGIN_API SelectButton : public Button
{
public:
    /** Constructor */
    SelectButton (const String& name);

    /** Destructor */
    ~SelectButton() {}

private:
    /** Draws the button*/
    void paintButton (Graphics& g, bool isMouseOver, bool isButtonDown) override;
};

/**
*
	Text field that allows the user to select a custom 
	range of channels, using Matlab-like range syntax:

	e.g. 1:10   (selects channels 1-10)
	     2:2:20 (selects even channels between 2 and 20)

*/
class PLUGIN_API RangeEditor : public TextEditor
{
public:
    /** Constructor */
    RangeEditor (const String& name, const Font& font);

    /** Destructor*/
    ~RangeEditor() {}
};

/**
* 
	Automatically creates an interactive pop-up editor for selecting channels.
	@see GenericEditor

	A plugin can specify:
	- The maximum number of selectable channels (setMaximumSelectableChannels)
	- The colour of the buttons (setChannelButtonColour)

*/
class PLUGIN_API PopupChannelSelector : public PopupComponent,
                                        public Button::Listener,
                                        public TextEditor::Listener
{
public:
    class Listener
    {
    public:
        virtual ~Listener() {}
        virtual Array<int> getSelectedChannels() = 0;
        virtual void channelStateChanged (Array<int> selectedChannels) = 0;
    };

    /** Constructor */
    PopupChannelSelector (Component* parent, Listener* listener, std::vector<bool> channelStates, Array<String> channelNames = Array<String>(), const String& title = String());

    /** Destructor */
    ~PopupChannelSelector() {}

    /** Sets the maximum number of channels that can be selected at once*/
    void setMaximumSelectableChannels (int num);

    /** Sets the colour of the channel buttons*/
    void setChannelButtonColour (Colour c);

    /** Mouse-related callbacks*/
    void mouseMove (const MouseEvent& event) override;
    void mouseDown (const MouseEvent& event) override;
    void mouseDrag (const MouseEvent& event) override;
    void mouseUp (const MouseEvent& event) override;

    /** Respond to button clicks*/
    void buttonClicked (Button*) override;

    /** Checks whether shift key is down*/
    void modifierKeysChanged (const ModifierKeys& modifiers) override;

    /** Returns a pointer to the button with a given id*/
    ChannelButton* getButtonForId (int btnId);

    bool firstButtonSelectedState;

    juce::Point<int> startDragCoords;

    Colour buttonColour;

    OwnedArray<ChannelButton> channelButtons;

    void updatePopup() override;

    void resized() override;

    void paint (Graphics& g) override;

private:
    std::unique_ptr<Viewport> viewport;
    std::unique_ptr<Component> contentComponent;
    Listener* listener;

    /** Methods for parsing range strings*/
    int convertStringToInteger (String s);
    Array<int> parseStringIntoRange (int rangeValue);

    void textEditorReturnKeyPressed (TextEditor&) override;
    void updateRangeString();
    void parseRangeString();

    String title;

    OwnedArray<SelectButton> selectButtons;
    std::unique_ptr<RangeEditor> rangeEditor;

    bool editable;
    bool isDragging;
    bool mouseDragged;
    bool shiftKeyDown;

    juce::Rectangle<int> dragBox;

    int nChannels;
    int maxSelectable;

    String rangeString;

    Array<int> channelStates;
    Array<int> selectedButtons;
    Array<int> activeChannels;
};

#endif // ___POPUPCHANNELSELECTOR_H_E47DE5C__
