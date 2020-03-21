#pragma once

#include "Graphics/Raytracing/RaytracingSharedHlsl.h"

namespace GlobalRootSignature
{
	enum Slot
	{
		SceneBVH = 0,
		ConstantBuffer,
		Count
	};
};

namespace HitGroupLocalRootSignature
{
	enum Slot
	{
		IndexBuffer = 0,
		VertexBuffer,
		Count
	};

	struct RootArguments
	{
		D3D12_GPU_DESCRIPTOR_HANDLE indicesGPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE verticesGPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE pickBufferGPUHandle;
	};
};

namespace RayGenLocalRootSignature
{
	enum Slot
	{
		OutputTexture = 0,
		Count
	};
};