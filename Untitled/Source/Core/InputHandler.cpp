#include "PCH.h"
#include "InputHandler.h"

void InputHandler::NewFrame()
{
	// Reset per-frame key/button press
	memset(&mouseDelta, 0, sizeof(MouseDelta));
	memset(&mouseButtonPressed, 0, sizeof(MouseButtonState));
	keyPressed.fill(0);
}

void InputHandler::ProcessRawInput(RAWINPUT* rawInput, HWND hwnd)
{
	// Handle keyboard input
	if (rawInput->header.dwType == RIM_TYPEKEYBOARD)
	{
		const auto virtual_key = rawInput->data.keyboard.VKey;

		// If key down.
		if (rawInput->data.keyboard.Message == WM_KEYDOWN ||
			rawInput->data.keyboard.Message == WM_SYSKEYDOWN)
		{
			keyState[rawInput->data.keyboard.VKey] = 0x80;
			keyPressed[rawInput->data.keyboard.VKey] = true;

			if (virtual_key == VK_DELETE) keyModifierState.del = true;
			if (virtual_key == VK_BACK) keyModifierState.backspace = true;
			if (virtual_key == VK_RETURN) keyModifierState.enter = true;
			if (virtual_key == VK_MENU) keyModifierState.alt = true;
			if (virtual_key == VK_TAB) keyModifierState.tab = true;
			if (virtual_key == VK_LEFT) keyModifierState.left = true;
			if (virtual_key == VK_RIGHT) keyModifierState.right = true;
			if (virtual_key == VK_UP) keyModifierState.up = true;
			if (virtual_key == VK_DOWN) keyModifierState.down = true;
		}
		// If key up.
		else if (rawInput->data.keyboard.Message == WM_KEYUP ||
			rawInput->data.keyboard.Message == WM_SYSKEYUP)
		{
			keyState[virtual_key] = 0;

			if (virtual_key == VK_DELETE) keyModifierState.del = false;
			if (virtual_key == VK_BACK) keyModifierState.backspace = false;
			if (virtual_key == VK_RETURN) keyModifierState.enter = false;
			if (virtual_key == VK_MENU) keyModifierState.alt = false;
			if (virtual_key == VK_TAB) keyModifierState.tab = false;
			if (virtual_key == VK_LEFT) keyModifierState.left = false;
			if (virtual_key == VK_RIGHT) keyModifierState.right = false;
			if (virtual_key == VK_UP) keyModifierState.up = false;
			if (virtual_key == VK_DOWN) keyModifierState.down = false;
		}
	}

	// Handle mouse input
	if (rawInput->header.dwType == RIM_TYPEMOUSE)
	{
		// Update mousebutton status
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
		{
			mouseButtonActive.left = true;
			mouseButtonPressed.left = true;
		}
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
		{
			mouseButtonActive.right = true;
			mouseButtonPressed.right = true;
		}
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
		{
			mouseButtonActive.middle = true;
			mouseButtonPressed.middle = true;
		}
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
		{
			mouseButtonActive.xbutton1 = true;
			mouseButtonPressed.xbutton1 = true;
		}
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
		{
			mouseButtonActive.xbutton2 = true;
			mouseButtonPressed.xbutton2 = true;
		}
		// Finish dragging if active on either left or right mousebutton
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
		{
			mouseButtonActive.left = false;
			if (mouseDrag.left.active)
			{
				mouseDrag.left.active = false;
				mouseDrag.left.duration = 0;
			}
		}
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
		{
			mouseButtonActive.right = false;
			if (mouseDrag.right.active)
			{
				mouseDrag.right.active = false;
				mouseDrag.right.duration = 0;
			}
		}
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)		mouseButtonActive.middle = false;
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)			mouseButtonActive.xbutton1 = false;
		if (rawInput->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)			mouseButtonActive.xbutton2 = false;

		if (mouseButtonActive.left && !mouseDrag.left.active) mouseDrag.left.duration++;
		if (mouseButtonActive.right && !mouseDrag.right.active) mouseDrag.right.duration++;

		// If any mousebutton has been held for 5 frames, we're "dragging"
		// Once the drag starts we cache the current mouse position if the camera or 
		// anything else wants to use it later. 
		if (mouseDrag.right.duration > 5 && !mouseDrag.right.active)
		{
			mouseDrag.right.active = true;
			mouseDrag.right.cachedPos = mousePos;
			ClientToScreen(hwnd, &mouseDrag.right.cachedPos);
		}
		if (mouseDrag.left.duration > 5 && !mouseDrag.left.active)
		{
			mouseDrag.left.active = true;
			mouseDrag.left.cachedPos = mousePos;
			ClientToScreen(hwnd, &mouseDrag.left.cachedPos);
		}

		// Update deltamovement if we are dragging
		if (mouseDrag.left.active || mouseDrag.right.active)
		{
			mouseDelta.x = static_cast<float>(rawInput->data.mouse.lLastX);
			mouseDelta.y = static_cast<float>(rawInput->data.mouse.lLastY);
		}
	}
}
