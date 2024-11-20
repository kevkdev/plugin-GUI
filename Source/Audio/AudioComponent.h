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

#ifndef __AUDIOCOMPONENT_H_D97C73CF__
#define __AUDIOCOMPONENT_H_D97C73CF__

#include "../../JuceLibraryCode/JuceHeader.h"
#include "../TestableExport.h"
/**

  Interfaces with system audio hardware.

  Uses the audio card to generate the callbacks to run the ProcessorGraph
  during data acquisition.

  Sends output to the audio card for audio monitoring.

  Determines the initial size of the sample buffer (crucial for
  real-time feedback latency).

  @see MainWindow, ProcessorGraph

*/

class TESTABLE AudioComponent
{
public:
    /** Constructor. Finds the audio component (if there is one), and sets the
    default sample rate and buffer size.*/
    AudioComponent();

    /** Destructor. Ends the audio callbacks if they are active.*/
    ~AudioComponent();

    /** Begins the audio callbacks that drive data acquisition. Returns true if a valid audio device was found*/
    bool beginCallbacks();

    /** Stops the audio callbacks that drive data acquisition.*/
    void endCallbacks();

    /** Connects the AudioComponent to the ProcessorGraph (crucial for any sort of
    data acquisition; done at startup).*/
    void connectToProcessorGraph (AudioProcessorGraph* processorGraph);

    /** Disconnects the AudioComponent to the ProcessorGraph (only done when the application
    is about to close).*/
    void disconnectProcessorGraph();

    /** Returns true if the audio callbacks are active, false otherwise.*/
    bool callbacksAreActive();

    /** Checks whether a device is available*/
    bool checkForDevice();

    /** Restarts communication with the audio device in order to update settings
    or just prior the start of data acquisition callbacks. Returns true if a device was found*/
    bool restartDevice();

    /** Stops communication with the selected audio device (to conserve CPU load
    when callbacks are not active).*/
    void stopDevice();

    /** Returns the buffer size (in samples) currently being used.*/
    int getBufferSize();

    /** Sets the buffer size (in samples) to be used.*/
    void setBufferSize (int bufferSize);

    /** Returns the buffer size (in ms) currently being used.*/
    int getBufferSizeMs();

    /** Returns the sample rate currently being used.*/
    int getSampleRate();

    /** Sets the sample rate to be used.*/
    void setSampleRate (int sampleRate);

    /** Returns the device type */
    String getDeviceType();

    /** Sets the device type */
    void setDeviceType (String deviceType);

    /** Sets the device name */
    void setDeviceName (String deviceName);

    /** Return the device name */
    String getDeviceName();

    /** Gets the available sample rates for the current device */
    Array<double> getAvailableSampleRates();

    /** Gets the available buffer sizes for the current device */
    Array<int> getAvailableBufferSizes();

    /** Saves all audio settings that can be loaded to an XML element */
    void saveStateToXml (XmlElement* parent);

    /** Loads all possible settings from an XML element */
    void loadStateFromXml (XmlElement* parent);

    AudioDeviceManager deviceManager;

private:
    bool isPlaying;

    std::unique_ptr<AudioProcessorPlayer> graphPlayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioComponent);
};

#endif
