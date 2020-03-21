#include "PCH.h"
#include "Core/Application.h"

int __stdcall WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	auto app = Application(hInstance, nCmdShow);
	app.Run();

	return 0;
}

// OPERATOR OVERLOADS FOR EASTL
void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
	UNREFERENCED_PARAMETER(name);
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(debugFlags);
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	 
	return new uint8_t[size];
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
	UNREFERENCED_PARAMETER(alignment);
	UNREFERENCED_PARAMETER(alignmentOffset);
	UNREFERENCED_PARAMETER(pName);
	UNREFERENCED_PARAMETER(flags);
	UNREFERENCED_PARAMETER(debugFlags);
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);

	return new uint8_t[size];
}