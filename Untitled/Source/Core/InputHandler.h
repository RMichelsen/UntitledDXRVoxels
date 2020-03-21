#pragma once

struct KeyboardModifierState
{
	bool del;
	bool backspace;
	bool enter;
	bool alt;
	bool tab;
	bool left;
	bool right;
	bool up;
	bool down;
};

struct MouseButtonState
{
	bool left;
	bool right;
	bool middle;
	bool xbutton1;
	bool xbutton2;
};

struct MouseDrag
{
	bool active;
	uint32_t duration;
	POINT cachedPos;
};

struct MouseDragState
{
	MouseDrag left;
	MouseDrag right;
};

struct MouseDelta
{
	float x;
	float y;
};

struct InputHandler
{
	inline POINT GetMousePosition() { return mousePos; }
	inline bool IsKeyPressed(int key) { return keyPressed[key]; }

	POINT mousePos{};
	MouseDelta mouseDelta{};
	MouseButtonState mouseButtonActive{};
	MouseButtonState mouseButtonPressed{};
	MouseDragState mouseDrag{};

	eastl::array<unsigned char, 0xFF> keyState;
	eastl::array<unsigned char, 0xFF> keyPressed;
	KeyboardModifierState keyModifierState{};

	void NewFrame();
	void ProcessRawInput(RAWINPUT* rawInput, HWND hwnd);
};
