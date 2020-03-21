#pragma once

#include "Game/Game.h"

class Application
{
public:
	Application(HINSTANCE hInstance_, int nCmdShow);
	~Application();

	void Run();

private:
	HINSTANCE instance = NULL;
	HWND hwnd = NULL;
	float deltaTime;

	eastl::unique_ptr<Game> game;

	bool ProcessMessages();
	bool SetupWindow(Game& game, int nCmdShow);
	void UpdateTime();

	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};