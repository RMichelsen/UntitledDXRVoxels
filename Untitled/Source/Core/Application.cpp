#include "PCH.h"
#include "Application.h"

#include "Core/Logging.h"

inline constexpr RAWINPUTDEVICE RAW_INPUT_DEVICES[2] = {
	RAWINPUTDEVICE{0x01, 0x02, 0, nullptr},  // Mouse
	RAWINPUTDEVICE{0x01, 0x06, 0, nullptr}   // Keyboard
};

Application::Application(HINSTANCE instance_, int nCmdShow) : instance(instance_)
{
	// No Windows bitmap-stretching
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

	// Create a console for debugging purposes
	FILE* dummy;
	AllocConsole();
	freopen_s(&dummy, "CONIN$", "r", stdin);
	freopen_s(&dummy, "CONOUT$", "w", stderr);
	freopen_s(&dummy, "CONOUT$", "w", stdout);

	game = eastl::make_unique<Game>();

	if (!SetupWindow(*game, nCmdShow))
	{
		UNTITLED_LOG_ERROR("Failed to setup win32 Window\n");
		return;
	}
	game->Init(hwnd);

	ShowWindow(hwnd, nCmdShow);


	if (!RegisterRawInputDevices(&RAW_INPUT_DEVICES[0], 2, sizeof(RAWINPUTDEVICE)))
	{
		UNTITLED_LOG_ERROR("Failed to register input devices\n");
		return;
	}
}

Application::~Application()
{
	DestroyWindow(hwnd);
	UnregisterClassW(L"Nimble_Class", instance);
}

void Application::Run()
{
	while (ProcessMessages())
	{
		if (IsIconic(hwnd))
		{
			continue;
		}
		UpdateTime();

		game->renderer->PreSimulate();
		game->Simulate(deltaTime);
		game->renderer->Prepare(deltaTime);
		game->renderer->Present();
	}

	game->Shutdown();
}

bool Application::ProcessMessages()
{
	game->input->NewFrame();

	MSG msg;
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		if (msg.message == WM_QUIT) return false;
	}
	return true;
}

bool Application::SetupWindow(Game& game, int nCmdShow)
{
	// Could make these parameters but fine as const for now
	auto windowClassName = L"Untitled_Class";
	auto windowTitle = L"Untitled Engine";

	WNDCLASSEX windowClass{};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = instance;
	windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
	windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	windowClass.lpszClassName = windowClassName;

	if (!RegisterClassExW(&windowClass))
	{
		return false;
	}

	hwnd = CreateWindow(
		windowClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		2560, // Resolution width
		1440, // Resolution height
		nullptr,
		nullptr,
		instance,
		&game
	);

	if (hwnd == NULL) return false;
	return true;
}

void Application::UpdateTime()
{
	using clock = eastl::chrono::steady_clock;
	static auto start = eastl::chrono::time_point_cast<eastl::chrono::nanoseconds>(clock::now());

	auto now = clock::now();
	deltaTime = static_cast<eastl::chrono::duration<float, eastl::ratio<1>>>(now - start).count();
	start = now;
}

LRESULT CALLBACK Application::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Game* game = reinterpret_cast<Game*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (uMsg == WM_CREATE)
	{
		LPCREATESTRUCT createStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, 
			reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));

		return 0;
	}

	switch (uMsg)
	{
	case WM_SIZE:
	{
		// Handle resize event
		RECT clientRect{};
		GetClientRect(hwnd, &clientRect);
		game->renderer->WindowResize(clientRect);
		return 0;
	}
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	case WM_INPUT:
	{
		uint32_t inputSize = 0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &inputSize, sizeof(RAWINPUTHEADER));
		LPBYTE rawInput = new BYTE[inputSize];
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, rawInput, &inputSize, sizeof(RAWINPUTHEADER)) != inputSize)
		{
			UNTITLED_LOG_ERROR("GetRawInputData didn't return correct size!\n");
		}
		else
		{
			game->input->ProcessRawInput(reinterpret_cast<RAWINPUT*>(rawInput), hwnd);

			// Handle ALT+ENTER fullscreen toggle
			if (game->input->keyModifierState.alt && game->input->IsKeyPressed(VK_RETURN))
			{
				game->renderer->ToggleFullscreen();
			}

			delete[] rawInput;
		}
		return 0;
	}
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
	{
		game->input->mousePos = POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		return 0;
	}
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
