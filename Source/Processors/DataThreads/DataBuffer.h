/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI
    Copyright (C) 2016 Open Ephys

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

#ifndef __DATABUFFER_H_11C6C591__
#define __DATABUFFER_H_11C6C591__

#include "../../../JuceLibraryCode/JuceHeader.h"
#include "../PluginManager/OpenEphysPlugin.h"


/**
    Manages reading and writing data to a circular buffer.

    See @DataThread
*/
class PLUGIN_API DataBuffer
{
public:

    /** Constructor */
    DataBuffer (int chans, int size);

    /** Destructor */
    ~DataBuffer() { }

    /** Clears the buffer.*/
    void clear();

    /** Add an array of floats to the buffer.
        The data is laid out as a set of channels with a series of samples per channel.
        The number of samples (chunkSize) in each sequence can be from 1..numItems.

        Examples:
        In the following examples, the numbers shown in the data indicate the channel #.

        numItems = 4
        chunkSize = 1
        # channels = 3

        Data:
        1 2 3 1 2 3 1 2 3 1 2 3

        numItems = 4
        chunkSize = 2
        # channels = 3

        Data:
        1 1 2 2 3 3 1 1 2 2 3 3

        numItmes = 4
        chunkSize = 4
        # channels = 3

        Data:
        1 1 1 1 2 2 2 2 3 3 3 3

        @param data The data.
        @param sampleNumbers  Array of sample numbers (integers). Same length as numItems.
        @param timestamps  Array of timestamps (in seconds) (double). Same length as numItems.
        @param eventCodes Array of event codes. Same length as numItems.
        @param numItems Total number of samples per channel.
        @param chunkSize Number of consecutive samples per channel per chunk.
        1 by default. Typically, 1 or numItems.
        Must be a multiple of numItems.

        @return The number of items actually written. May be less than numItems if
        the buffer doesn't have space.
    */
    int addToBuffer (float* data,
                     int64* sampleNumbers,
                     double* timestamps,
                     uint64* eventCodes,
                     int numItems,
                     int chunkSize=1);

    /** Returns the number of samples currently available in the buffer.*/
    int getNumSamples() const;

    /** Copies as many samples as possible from the DataBuffer to an AudioBuffer.*/
    int readAllFromBuffer (AudioBuffer<float>& data,
                           int64* sampleNumbers,
                           double* timestamps,
                           uint64* eventCodes,
                           int maxSize,
                           int dstStartChannel = 0,
                           int numChannels = -1);

    /** Resizes the data buffer */
    void resize (int chans, int size);


private:
    AbstractFifo abstractFifo;
    AudioBuffer<float> buffer;

    HeapBlock<int64> sampleNumberBuffer;
    HeapBlock<double> timestampBuffer;
    HeapBlock<uint64> eventCodeBuffer;

	int64 lastSampleNumber;
    double lastTimestamp;

    int numChans;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DataBuffer);
};


#endif  // __DATABUFFER_H_11C6C591__
