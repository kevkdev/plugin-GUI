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

#include "AudioEditor.h"
#include "../../AccessClass.h"
#include "../../Audio/AudioComponent.h"
#include "../../UI/EditorViewport.h"
#include "../../Utils/Utils.h"

static const Colour COLOUR_SLIDER_TRACK (Colour::fromRGB (92, 92, 92));
static const Colour COLOUR_SLIDER_TRACK_FILL (Colour::fromRGB (255, 255, 255));

MuteButton::MuteButton()
    : ImageButton ("MuteButton")
{
    offimage = ImageCache::getFromMemory (BinaryData::muteoff_png, BinaryData::muteoff_pngSize);
    onimage = ImageCache::getFromMemory (BinaryData::muteon_png, BinaryData::muteon_pngSize);

    setImages (false, true, true, offimage, 1.0f, findColour (ThemeColours::controlPanelText), offimage, 0.5f, findColour (ThemeColours::controlPanelText).withAlpha (0.5f), onimage, 0.7f, findColour (ThemeColours::controlPanelText).withAlpha (0.7f));

    setClickingTogglesState (true);

    setTooltip ("Mute audio");
}

void MuteButton::updateImages()
{
    setImages (false, true, true, offimage, 1.0f, findColour (ThemeColours::controlPanelText), offimage, 0.5f, findColour (ThemeColours::controlPanelText).withAlpha (0.5f), onimage, 0.7f, findColour (ThemeColours::controlPanelText).withAlpha (0.7f));
}

AudioWindowButton::AudioWindowButton()
    : Button ("AudioWindowButton")
{
    setClickingTogglesState (true);

    textString = ":AUDIO";
    setTooltip ("Change the buffer size");

    String svgPathString = "M3 12a9 9 0 1 0 18 0a9 9 0 0 0 -18 0 M12 7v5l3 3";

    latencySvgPath = Drawable::parseSVGPath (svgPathString);
}

void AudioWindowButton::paintButton (Graphics& g, bool isMouseOver, bool isButtonDown)
{
    float alpha = (isMouseOver && getClickingTogglesState()) ? 0.6f : 1.0f;

    if (getToggleState())
        g.setColour (Colours::yellow.withAlpha (alpha));
    else
        g.setColour (findColour (ThemeColours::controlPanelText).withAlpha (alpha));

    g.strokePath (latencySvgPath, PathStrokeType (1.5f), latencySvgPath.getTransformToScaleToFit (5, 7, 14, 14, true));

    g.setFont (FontOptions ("Silkscreen", "Regular", 14));
    g.drawFittedText (textString, 25, 0, getWidth() - 30, getHeight(), Justification::centredLeft, 1);
}

void AudioWindowButton::setText (const String& newText)
{
    textString = newText;
    repaint();
}

AudioEditor::AudioEditor (AudioNode* owner)
    : AudioProcessorEditor (owner), lastValue (1.0f), isEnabled (true), audioConfigurationWindow (nullptr)
{
    muteButton = new MuteButton();
    muteButton->addListener (this);
    muteButton->setToggleState (false, dontSendNotification);
    addAndMakeVisible (muteButton);

    audioWindowButton = new AudioWindowButton();
    audioWindowButton->addListener (this);
    audioWindowButton->setToggleState (false, dontSendNotification);
    addAndMakeVisible (audioWindowButton);

    volumeSlider = new Slider ("Volume Slider");
    volumeSlider->setSliderStyle (Slider::LinearHorizontal);
    volumeSlider->setTextBoxStyle (Slider::NoTextBox,
                                   false,
                                   0,
                                   0);
    volumeSlider->setRange (0, 100, 1);
    volumeSlider->addListener (this);
    volumeSlider->setValue (50);
    addAndMakeVisible (volumeSlider);

    noiseGateSlider = new Slider ("Noise Gate Slider");
    volumeSlider->setSliderStyle (Slider::LinearHorizontal);
    noiseGateSlider->setTextBoxStyle (Slider::NoTextBox,
                                      false,
                                      0,
                                      0);
    noiseGateSlider->setRange (0, 100, 1);
    noiseGateSlider->addListener (this);
    addAndMakeVisible (noiseGateSlider);
}

AudioEditor::~AudioEditor()
{
}

void AudioEditor::resized()
{
    const int width = getWidth();
    const int height = getHeight();

    // Since width of the label on button is always the same, we should reserve it.
    const bool isLatencyLabelVisible = width >= 450;
    const int audioWindowButtonWidth = 68;
    const int gateLabelWidth = 45;

    const int availableWidth = width - audioWindowButtonWidth - gateLabelWidth;
    const int sliderWidth = availableWidth * 0.4;
    const int sliderHeight = height - 6;
    const int sliderY = (height - sliderHeight) / 2;
    const int margin = availableWidth * 0.03;

    muteButton->setBounds (margin, 5, 20, 20);
    volumeSlider->setBounds (margin + 30, sliderY, sliderWidth, sliderHeight);
    noiseGateSlider->setBounds (volumeSlider->getRight() + margin + gateLabelWidth, sliderY, sliderWidth, sliderHeight);
    audioWindowButton->setBounds (width - audioWindowButtonWidth + 2, 2, audioWindowButtonWidth - 4, height - 4);
}

void AudioEditor::updateBufferSizeText()
{
    String t = String (AccessClass::getAudioComponent()->getBufferSizeMs());
    t = t + " ms";

    audioWindowButton->setText (t);
}

void AudioEditor::enable()
{
    isEnabled = true;
    audioWindowButton->setClickingTogglesState (true);
}

void AudioEditor::disable()
{
    isEnabled = false;

    if (audioConfigurationWindow)
    {
        audioConfigurationWindow->setVisible (false);
        audioWindowButton->setToggleState (false, dontSendNotification);
    }

    audioWindowButton->setClickingTogglesState (false);
}

void AudioEditor::buttonClicked (Button* button)
{
    if (button == muteButton)
    {
        AudioNode* audioNode = (AudioNode*) getAudioProcessor();

        if (muteButton->getToggleState())
        {
            lastValue = volumeSlider->getValue();

            audioNode->setParameter (1, 0.0f);
            LOGD ("Mute on.");
        }
        else
        {
            audioNode->setParameter (1, lastValue);
            LOGD ("Mute off.");
        }
    }
    else if (button == audioWindowButton && isEnabled)
    {
        if (audioWindowButton->getToggleState())
        {
            if (! audioConfigurationWindow)
            {
                audioConfigurationWindow = new AudioConfigurationWindow (AccessClass::getAudioComponent()->deviceManager,
                                                                         audioWindowButton);
                audioConfigurationWindow->addComponentListener (this);
            }

            AccessClass::getAudioComponent()->restartDevice();
            audioConfigurationWindow->setLookAndFeel (&getLookAndFeel());
            audioConfigurationWindow->setVisible (true);
            audioConfigurationWindow->toFront (true);
        }
        else
        {
            audioConfigurationWindow->setVisible (false);
        }
    }
}

void AudioEditor::sliderValueChanged (Slider* slider)
{
    AudioNode* audioNode = (AudioNode*) getAudioProcessor();

    if (slider == volumeSlider)
        audioNode->setParameter (1, slider->getValue());
    else if (slider == noiseGateSlider)
        audioNode->setParameter (2, slider->getValue());
}

void AudioEditor::componentVisibilityChanged (Component& component)
{
    if (component.getName() == audioConfigurationWindow->getName() && ! component.isVisible())
    {
        updateBufferSizeText();
        AccessClass::getAudioComponent()->stopDevice();
    }
}

void AudioEditor::paint (Graphics& g)
{
    const int margin = getWidth() * 0.03;
    g.setColour (findColour (ThemeColours::controlPanelText));
    g.setFont (FontOptions ("Silkscreen", "Regular", 14));
    g.drawSingleLineText ("GATE:", volumeSlider->getBounds().getRight() + margin, 20);

    muteButton->updateImages();
}

void AudioEditor::saveStateToXml (XmlElement* xml)
{
    XmlElement* audioEditorState = xml->createNewChildElement ("AUDIOEDITOR");
    audioEditorState->setAttribute ("isMuted", muteButton->getToggleState());
    audioEditorState->setAttribute ("volume", volumeSlider->getValue());
    audioEditorState->setAttribute ("noiseGate", noiseGateSlider->getValue());
}

void AudioEditor::loadStateFromXml (XmlElement* xml)
{
    for (auto* xmlNode : xml->getChildIterator())
    {
        if (xmlNode->hasTagName ("AUDIOEDITOR"))
        {
            muteButton->setToggleState (xmlNode->getBoolAttribute ("isMuted", false), dontSendNotification);
            volumeSlider->setValue (xmlNode->getDoubleAttribute ("volume", 0.0f), NotificationType::sendNotification);
            noiseGateSlider->setValue (xmlNode->getDoubleAttribute ("noiseGate", 0.0f), NotificationType::sendNotification);
        }
    }

    updateBufferSizeText();
}

AudioConfigurationWindow::AudioConfigurationWindow (AudioDeviceManager& adm, AudioWindowButton* cButton)
    : DocumentWindow ("Audio Settings",
                      Colours::red,
                      DocumentWindow::closeButton),
      controlButton (cButton)

{
    centreWithSize (360, 500);

    setUsingNativeTitleBar (true);

    setResizable (false, false);

    LOGDD ("Audio CPU usage:", adm.getCpuUsage());

    AudioDeviceSelectorComponent* adsc = new AudioDeviceSelectorComponent (adm,
                                                                           0, // minAudioInputChannels
                                                                           0, // maxAudioInputChannels
                                                                           0, // minAudioOutputChannels
                                                                           2, // maxAudioOutputChannels
                                                                           false, // showMidiInputOptions
                                                                           false, // showMidiOutputSelector
                                                                           false, // showChannelsAsStereoPairs
                                                                           false); // hideAdvancedOptionsWithButton

    adsc->setBounds (10, 0, 500, 440);
    adsc->setItemHeight (20);

    setContentOwned (adsc, true);
    setVisible (false);

    int fixedWidth = adsc->getWidth() + 10;
    int fixedHeight = adsc->getHeight() + getTitleBarHeight() + 20;

    setResizeLimits (fixedWidth, fixedHeight, fixedWidth, fixedHeight);
}

void AudioConfigurationWindow::closeButtonPressed()
{
    CoreServices::saveRecoveryConfig();

    controlButton->setToggleState (false, dontSendNotification);
    setVisible (false);
}

void AudioConfigurationWindow::resized()
{
}

void AudioConfigurationWindow::paint (Graphics& g)
{
    g.fillAll (findColour (ThemeColours::componentBackground));
}
