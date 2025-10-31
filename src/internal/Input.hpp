#pragma once
#include <cstdint>
#include <string>

namespace Input
{
#ifndef INPUT_MACROS
#define BUTTONS(X) 				\
		X(DPAD_LEFT) 			\
		X(DPAD_RIGHT)			\
		X(DPAD_DOWN)			\
		X(DPAD_UP)				\
		X(FACE_BUTTON_LEFT)		\
		X(FACE_BUTTON_RIGHT)	\
		X(FACE_BUTTON_DOWN)		\
		X(FACE_BUTTON_UP)		\
		X(L1)					\
		X(L2)					\
		X(L3)					\
		X(R1)					\
		X(R2)					\
		X(R3)					\
		X(SELECT)				\
		X(START)				\

#define ANALOGS(X)			\
		X(STICK_RIGHT_X)	\
		X(STICK_RIGHT_Y)	\
		X(STICK_LEFT_X)		\
		X(STICK_LEFT_Y)		\

#define ENUM_TO_STRING_ARRAY(X), #X

#define INPUT_MACROS
#endif

enum class ButtonAction : uint8_t
{
#define ENUM_ENTRY(name) name,
	BUTTONS(ENUM_ENTRY)
#undef ENUM_ENTRY
	COUNT
};

enum class AnalogAction : uint8_t
{
#define ENUM_ENTRY(name) name,
	ANALOGS(ENUM_ENTRY)
#undef ENUM_ENTRY
	COUNT
};

struct InputState
{
	uint64_t downMask = 0;
	uint64_t pressedMask = 0;
	uint64_t releasedMask = 0;
	uint64_t clickedMask = 0;

	float lastPressed[(size_t) ButtonAction::COUNT];
	float lastReleased[(size_t) ButtonAction::COUNT];
	float currentAnalog[(size_t) AnalogAction::COUNT];

/*
	std::unordered_map<std::string, bool> dynamicDown;
	std::unordered_map<std::string, bool> dynamicPressed;
	std::unordered_map<std::string, bool> dynamicReleased;
	std::unordered_map<std::string, bool> dynamicClicked;
	
	std::unordered_map<std::string, bool> dynamicLastPressed;
	std::unordered_map<std::string, bool> dynamicLastReleased;
	std::unordered_map<std::string, bool> dynamicAnalog;
*/

	InputState()
	{
		std::fill(std::begin(lastPressed), std::end(lastPressed), 0.f);
		std::fill(std::begin(lastReleased), std::end(lastReleased), 0.f);
		std::fill(std::begin(currentAnalog), std::end(currentAnalog), 0.f);
	};
};

struct InputMapping
{
	int buttonCodes[(size_t) ButtonAction::COUNT] = {-1};
	int axisCodes[(size_t) ButtonAction::COUNT] = {-1};
};


std::string toString(ButtonAction _btn);
std::string toString(AnalogAction _axis);
ButtonAction std::string stringToButton(std::string& _str);
AnalogAction std::string stringToAxis(std::string& _str);

void pollDevices();
void update(float currentTime, 
			const std::vector<std::pair<int, bool>>& buttonEvents, 
			const std::vector<std::pair<int, bool>>& buttonEvents);


bool isPressed(ButtonAction _btn);
bool isReleased(ButtonAction _btn);
bool isHelp(ButtonAction _btn);
bool isClicked(ButtonAction _btn);

float getAxis(AnalogAction _axis);

bool hasGamepad();


static const std::string defaultConfigJSON = 
R"(
{
	"buttons": 
	{
		"0":"DPAD_LEFT",
		"1":"DPAD_RIGHT",
		"2":"DPAD_DOWN",
		"3":"DPAD_UP",
		"4":"FACE_BUTTON_LEFT",
		"5":"FACE_BUTTON_RIGHT",
		"6":"FACE_BUTTON_DOWN",
		"7":"FACE_BUTTON_UP",
		"8":"L1",
		"9":"L2",
		"10":"L3",
		"11":"R1",
		"12":"R2",
		"13":"R3",
		"14":"SELECT",
		"15":"START",
	}
	"axes":
	{
		"0":"STICK_RIGHT_X",
		"1":"STICK_RIGHT_Y",
		"2":"STICK_LEFT_X",
		"3":"STICK_LEFT_Y",
	}

}
)";


}
