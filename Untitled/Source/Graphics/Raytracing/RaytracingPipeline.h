#pragma once

#include "Core/InputHandler.h"
#include "Graphics/DX/DXBuffer.h"
#include "Graphics/DX/DXTexture.h"
#include "Graphics/DX/DXUtils.h"
#include "Graphics/Raytracing/AccelerationStructureManager.h"
#include "Graphics/Raytracing/RaytracingCamera.h"
#include "Graphics/Raytracing/RaytracingDXILLibrary.h"
#include "Graphics/Raytracing/RaytracingRootSignatures.h"
#include "Graphics/Raytracing/RaytracingShader.h"
#include "Graphics/Raytracing/RaytracingShaderTable.h"

struct GraphicsContext;

class RaytracingPipeline
{
public:
	DirectX::XMUINT2 pickBufferContent;

	RaytracingPipeline(GraphicsContext& context_, uint32_t width, uint32_t height);
	~RaytracingPipeline();

	void RaytracePick();
	void RaytraceScene(uint32_t width, uint32_t height, const InputHandler* inputHandler, float deltaTime);
	void ReleaseWindowDependentResources();
	void CreateWindowDependentResources(uint32_t width, uint32_t height);

	inline BLASHandle AddBLAS(eastl::vector<AccelerationStructureGeometry>&& geometry)
	{
		BLASHandle handle = ASManager->AddBLAS(eastl::forward<eastl::vector<AccelerationStructureGeometry>>(geometry));
		ASManager->BuildBLAS(handle, &context.descriptorHeap);

		AddHitgroupEntry(handle);
		return handle;
	}

	inline void RebuildBLAS(BLASHandle handle, eastl::vector<AccelerationStructureGeometry>&& geometries)
	{
		ASManager->RebuildBLAS(handle, eastl::forward<eastl::vector<AccelerationStructureGeometry>>(geometries), &context.descriptorHeap);
		RepopulateHitgroups();
	}

	inline BLASInstanceHandle AddBLASInstance(BLASHandle handle, DirectX::XMMATRIX transform = MATRIX_IDENTITY)
	{
		return ASManager->AddBLASInstance(handle, transform);
	}

	inline void RemoveBLAS(BLASHandle& handle)
	{
		ASManager->RemoveBLAS(handle);
		RepopulateHitgroups();
	}

	inline void RemoveBLASInstance(BLASInstanceHandle& handle) { ASManager->RemoveBLASInstance(handle); }

	inline void BuildTLAS()
	{
		ASManager->BuildTLAS(&context.descriptorHeap);
	}

private:
	GraphicsContext& context;
	eastl::unique_ptr<AccelerationStructureManager> ASManager;

	DXTexture2D outputTexture;
	DXReadBackBuffer pickBuffer;

	Microsoft::WRL::ComPtr<ID3D12StateObject> pipelineState;
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pipelineStateProperties;
	Microsoft::WRL::ComPtr<ID3D12StateObject> pickPipelineState;
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pickPipelineStateProperties;

	ID3D12RootSignature* globalRootSignature;

	RaytracingDXILLibrary DXILLibrary;
	void* mainRayGenIdentifier;
	void* mainHitGroupIdentifier;
	void* mainMissIdentifier;
	void* shadowMissIdentifier;

	RaytracingDXILLibrary pickDXILLibrary;
	void* pickRayGenIdentifier;
	void* pickHitGroupIdentifier;
	void* pickMissIdentifier;

	eastl::unique_ptr<RaytracingShaderTable> raygenShaderTable;
	eastl::unique_ptr<RaytracingShaderTable> missShaderTable;
	eastl::unique_ptr<RaytracingShaderTable> hitgroupShaderTable;

	// Scene related
	RaytracingCamera camera;
	float sunAltitude;
	float sunAzimuth;
	RaytracingConstants constants;

	void CreateOutputTexture(uint32_t width, uint32_t height);
	void CreatePickBuffer();
	void CreatePipelineState();
	void CreateShaderResources();
	void CreateShaderTables();

	void RepopulateHitgroups();
	void AddHitgroupEntry(BLASHandle handle);
};

