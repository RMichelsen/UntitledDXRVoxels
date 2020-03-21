#pragma once

#include "Graphics/DX/DXCommon.h"
#include "Graphics/Raytracing/RaytracingShader.h"
#include "Graphics/Raytracing/RaytracingDXILLibrary.h"

class ShaderCompiler
{
public:
	ShaderCompiler();

	RaytracingShader CompileShader(const wchar_t* path, const wchar_t* entry,
		const wchar_t* identifier);

	RaytracingDXILLibrary CompileDXILLibrary(const wchar_t* path);

private:
	IDxcBlob* Compile(const wchar_t* path);

	Microsoft::WRL::ComPtr<IDxcCompiler> compiler;
	Microsoft::WRL::ComPtr<IDxcLibrary> library;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
};
