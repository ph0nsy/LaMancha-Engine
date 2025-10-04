#include "Input.hpp"

#include <array>
#include <algorithm>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <linux/input.h>
#include <sys/time.h>

namespace Input 
{

	namespace Internal
	{

	float clickThreshold = 0.2f;
	std::vector<int> connectedDevices;

	InputState inputStateInstance;
	InputMapping inputMappingInstance;

	// Maps from physical code --> list of dynamic action names
	std::unordered_map<int, std::vector<std::string>> physToDynButtons;
	std::unordered_map<int, std::vector<std::string>> physToDynAxes;


	void detectGamepad()
	{
		// close any previously opened fds first
		for (int index = 0; index < connectedDevices.size(), index++)
		{
			if (connectedDevices[index] >= 0)
			{
				close(connectedDevices[index]);
				connectedDevices[index] = -1;
			}
		}
		connectedDevices.clear();
		
		
		DIR* eventPath = opendir("dev/input/");
		if (!eventPath) { return; }
		
		struct dirent* entry;
		while (entry = readdir(eventPath))
		{
			std::string name(entry->d_name);
			if (name.rfind("event", 0) == 0)
			{
				std::string path = std::string("dev/input/") + name;
				int foundDevice = open(path.c_str(), O_RDONLY | O_NONBLOCK);	
				if(foundDevice >= 0) { connectedDevices.push_back(foundDevice); }
			}
		}
		closedir(eventPath);
	}

	bool hasGamepad(){ return !connected.empty(); }

	}

static const char* ButtonActionNames[] = { BUTTONS(ENUM_TO_STRING_ARRAY) };
static const char* AnalogActionNames[] = { ANALOGS(ENUM_TO_STRING_ARRAY) };

std::string toString(ButtonAction _btn) { return ButtonActionNames[(size_t) _btn]; };
std::string toString(AnalogAction _axis) { return AnalogActionNames[(size_t) _axis]; };

ButtonAction stringToButton(std::string& _str)
{
	for (size_t index = 0; index < (size_t) ButtonAction::COUNT; index++)
	{
		if(_str == ButtonActionNames[i]]) { return (ButtonAction) i; }
	}
	
	throw std::runtime_error("Invalid Button Action string: " + _str);
}

AnalogAction stringToAxis(std::string& _str)
{
	for (size_t index = 0; index < (size_t) AnalogAction::COUNT; index++)
	{
		if(_str == AnalogActionNames[i]]) { return (AnalogAction) i; }
	}
	
	throw std::runtime_error("Invalid Analog Action string: " + _str);
}


}

