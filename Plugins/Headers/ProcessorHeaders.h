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

/*
This header contains all the headers needed by processor nodes.
Should be included in the source files which declare a processor class.
*/

#include "../../JuceLibraryCode/JuceHeader.h"
#include "../../Source/Processors/Events/Event.h"
#include "../../Source/Processors/Events/Spike.h"
#include "../../Source/Processors/GenericProcessor/GenericProcessor.h"
#include "../../Source/TestableExport.h"
#include "../../Source/Utils/BroadcastParser.h"
#include "DspLib.h"