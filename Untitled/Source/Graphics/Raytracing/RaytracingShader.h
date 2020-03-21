#pragma once

struct RaytracingShader
{
	eastl::wstring identifier;
	eastl::wstring entry;

	D3D12_SHADER_BYTECODE bytecode;

	void* GetShaderIdentifier(ID3D12StateObjectProperties* const pipelineProperties);

	D3D12_STATE_SUBOBJECT CreateSubobject(D3D12_EXPORT_DESC& exportDesc,
		D3D12_DXIL_LIBRARY_DESC& libraryDesc);
};
