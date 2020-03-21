#include "PCH.h"
#include "RaytracingShader.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXCommon.h"
#include "Graphics/DX/DXUtils.h"

using namespace Microsoft::WRL;

void* RaytracingShader::GetShaderIdentifier(ID3D12StateObjectProperties* const pipelineProperties)
{
	return pipelineProperties->GetShaderIdentifier(identifier.c_str());
}

D3D12_STATE_SUBOBJECT RaytracingShader::CreateSubobject(D3D12_EXPORT_DESC& exportDesc,
	D3D12_DXIL_LIBRARY_DESC& libraryDesc)
{
	exportDesc = {
	.Name = identifier.c_str(),
	.ExportToRename = entry.c_str(),
	.Flags = D3D12_EXPORT_FLAG_NONE
	};

	libraryDesc = {
		.DXILLibrary = bytecode,
		.NumExports = 1,
		.pExports = &exportDesc
	};

	return {
		.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
		.pDesc = &libraryDesc
	};
}

