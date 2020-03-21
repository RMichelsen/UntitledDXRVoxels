#include "PCH.h"
#include "ShaderCompiler.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXUtils.h"

using namespace Microsoft::WRL;

ShaderCompiler::ShaderCompiler()
{
	// Initialize the compiler, library and include handler
	DXCHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
	DXCHECK(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library)));
	DXCHECK(library->CreateIncludeHandler(&includeHandler));
}

RaytracingShader ShaderCompiler::CompileShader(const wchar_t* path, const wchar_t* entry,
	const wchar_t* identifier)
{
	IDxcBlob* blob = Compile(path);

	RaytracingShader shader {
		.identifier = identifier,
		.entry = entry,
		.bytecode = {
			.pShaderBytecode = blob->GetBufferPointer(),
			.BytecodeLength = blob->GetBufferSize()
		}
	};

	return shader;
}

RaytracingDXILLibrary ShaderCompiler::CompileDXILLibrary(const wchar_t* path)
{
	IDxcBlob* blob = Compile(path);

	RaytracingDXILLibrary library {
		.bytecode = {
			.pShaderBytecode = blob->GetBufferPointer(),
			.BytecodeLength = blob->GetBufferSize()
		}
	};

	return library;
}

IDxcBlob* ShaderCompiler::Compile(const wchar_t* path)
{
	// Encode the shader file
	uint32_t code = 0;
	IDxcBlobEncoding* encodedShader = nullptr;
	DXCHECK(library->CreateBlobFromFile(path, &code, &encodedShader));

	// Compile the shader
	IDxcOperationResult* result;
	DXCHECK(compiler->Compile(
		encodedShader,
		path,
		L"",
		L"lib_6_3",
		nullptr,
		0,
		nullptr,
		0,
		includeHandler.Get(),
		&result
	));

	HRESULT hr;
	result->GetStatus(&hr);
	if (FAILED(hr))
	{
		IDxcBlobEncoding* error;
		DXCHECK(result->GetErrorBuffer(&error));

		// Convert error blob to a string
		eastl::vector<char> message;
		message.resize(error->GetBufferSize() + 1);
		memcpy(message.data(), error->GetBufferPointer(), error->GetBufferSize());
		message[error->GetBufferSize()] = '\0';

		UNTITLED_LOG_ERROR("%s\n", message.data());

		return nullptr;
	}

	IDxcBlob* blob = nullptr;
	DXCHECK(result->GetResult(&blob));
	return blob;
}
