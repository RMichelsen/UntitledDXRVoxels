#pragma once

// Common
#include <assert.h>
#include <cstdint>
#include <cstdio>

// Windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Windowsx.h>
#include <wrl\wrappers\corewrappers.h>
#include <wrl\client.h>

// DirectX
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

// DXC
#pragma comment(lib, "dxcompiler.lib")
#include <DXC/dxcapi.h>

// PIX
#define USE_PIX
#pragma comment(lib, "WinPixEventRuntime.lib")
#include <PIX/pix3.h>

// D3D12MA
#include <D3D12MA/D3D12MemAlloc.h>

// EASTL
#include <EASTL/array.h>
#include <EASTL/chrono.h>
#include <EASTL/deque.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

// FastNoiseSIMD
#include <FastNoiseSIMD/FastNoiseSIMD.h>
