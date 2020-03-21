#pragma once

#include "Core/SparseArray.h"
#include "Graphics/DX/DXBuffer.h"

// Here we define some reasonable default sizes to allow for 
// fixed size stack allocated containers. The containers
// are created with bEnableOverflow = false, which means that
// the container cannot grow beyond the sizes given. Doing so
// is UB.
constexpr size_t MAX_NUM_BLAS = 4096;
constexpr size_t MAX_NUM_TOTAL_BLAS_INSTANCES = 16384;

// Since we need to create geometry dynamically at runtime we
// will simply provide a scratch buffer with a fixed size that should
// be big enough to hold any chunk of voxels (64MB)
constexpr size_t MAX_SCRATCHBUFFER_SIZE = 67'108'864;

constexpr DirectX::XMMATRIX MATRIX_IDENTITY = {
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f }
};

struct GraphicsContext;

struct AccelerationStructureGeometry
{
	DXDeviceLocalBuffer vertices;
	DXDeviceLocalBuffer indices;
};

struct alignas(16) TopLevelAccelerationStructure
{
	DXDeviceLocalBuffer ASBuffer;
};

struct alignas(16) BottomLevelAccelerationStructure
{
	uint32_t instanceContributionToHitGroupIndex;
	DXDeviceLocalBuffer ASBuffer;
	DirectX::XMMATRIX transform;
	eastl::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescriptions;
	eastl::vector<AccelerationStructureGeometry> geometryInstances;
	bool built;
};
using BLASHandle = SparseHandle<BottomLevelAccelerationStructure>;
using BLASInstanceHandle = SparseHandle<D3D12_RAYTRACING_INSTANCE_DESC>;

class AccelerationStructureManager
{
public:
	AccelerationStructureManager(GraphicsContext& context_);
	~AccelerationStructureManager();

	[[nodiscard]] BLASHandle AddBLAS(eastl::vector<AccelerationStructureGeometry>&& geometries);
	void BuildBLAS(BLASHandle handle, DXDescriptorHeap* descriptorHeap);

	void RebuildBLAS(BLASHandle handle, eastl::vector<AccelerationStructureGeometry>&& geometries, DXDescriptorHeap* descriptorHeap);

	[[nodiscard]] BLASInstanceHandle AddBLASInstance(BLASHandle handle, DirectX::XMMATRIX transform = MATRIX_IDENTITY);
	void BuildTLAS(DXDescriptorHeap* descriptorHeap);

	inline void RemoveBLASInstance(BLASInstanceHandle& handle)
	{
		BLInstanceDescriptorsCPU.Remove(handle);
	}

	inline void RemoveBLAS(BLASHandle& handle)
	{
		auto& BLAS = BLAccelerationStructures[handle];
		BLAS.ASBuffer.allocation->Release();

		BLAccelerationStructures.Remove(handle);
	}

	inline SparseArray<BottomLevelAccelerationStructure, MAX_NUM_BLAS>& GetBLAS()
	{
		return BLAccelerationStructures;
	}

	inline const D3D12_GPU_VIRTUAL_ADDRESS GetTLASGPUAddress()
	{
		return TLAccelerationStructure.ASBuffer.GetGPUAddress();
	}

private:
	GraphicsContext& context;

	SparseArray<BottomLevelAccelerationStructure, MAX_NUM_BLAS> BLAccelerationStructures;

	SparseArray<D3D12_RAYTRACING_INSTANCE_DESC, MAX_NUM_TOTAL_BLAS_INSTANCES> BLInstanceDescriptorsCPU;
	DXUploadBuffer BLInstanceDescriptorsGPU;

	TopLevelAccelerationStructure TLAccelerationStructure;
	
	uint64_t requiredScratchSize = 0;
	DXDeviceLocalBuffer scratchBuffer;
};

