#pragma once

#include "Graphics/DX/DXBuffer.h"
#include "Graphics/Raytracing/RaytracingSharedHlsl.h"
#include "Graphics/Raytracing/AccelerationStructureManager.h"

enum VisibleFaces
{
	North = 1,			// 000001
	South = 1 << 1,		// 000010
	East = 1 << 2,		// 000100
	West = 1 << 3,		// 001000
	Top = 1 << 4,		// 010000
	Bottom = 1 << 5		// 100000
};

enum class FillType
{
	Empty,
	Solid,
	Transparent
};

struct Voxel
{
	FillType fillType;
	uint32_t visibleFaces;
	DirectX::XMVECTOR color;
};

struct ChunkGPUResources
{
	DXDeviceLocalBuffer vBuffer;
	DXDeviceLocalBuffer iBuffer;

	BLASHandle BLAS;
	BLASInstanceHandle BLASInstance;
};

constexpr uint32_t VOXEL_CHUNK_WIDTH = 64;


inline int GetIndex(int x, int y, int z)
{
	return x + VOXEL_CHUNK_WIDTH * (y + VOXEL_CHUNK_WIDTH * z);
}

struct Chunk
{
	DirectX::XMINT3 position;
	size_t index;
	bool needsRebuild;

	eastl::vector<Voxel> voxels;
	ChunkGPUResources GPUResources;

	Chunk(DirectX::XMINT3 position_, size_t index_) : position(position_), index(index_), needsRebuild(false) {}

	inline void SetFacesForVoxel(int x, int y, int z)
	{
		auto& voxel = voxels[GetIndex(x, y, z)];

		voxel.visibleFaces = (VisibleFaces::East | VisibleFaces::West | VisibleFaces::Top | VisibleFaces::Bottom | VisibleFaces::North | VisibleFaces::South);
		if (x + 1 < VOXEL_CHUNK_WIDTH	&& voxels[GetIndex(x + 1, y, z)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::East;
		if (x - 1 >= 0					&& voxels[GetIndex(x - 1, y, z)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::West;
		if (y + 1 < VOXEL_CHUNK_WIDTH	&& voxels[GetIndex(x, y + 1, z)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::Top;
		if (y - 1 >= 0					&& voxels[GetIndex(x, y - 1, z)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::Bottom;
		if (z + 1 < VOXEL_CHUNK_WIDTH	&& voxels[GetIndex(x, y, z + 1)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::North;
		if (z - 1 >= 0					&& voxels[GetIndex(x, y, z - 1)].fillType == FillType::Solid) voxel.visibleFaces &= ~VisibleFaces::South;
	}
};

constexpr eastl::array<uint32_t, 36> CUBE_INDICES =
{
	3,1,0,
	2,1,3,

	6,4,5,
	7,4,6,

	11,9,8,
	10,9,11,

	14,12,13,
	15,12,14,

	19,17,16,
	18,17,19,

	22,20,21,
	23,20,22
};

constexpr eastl::array<DirectX::XMVECTOR, 24> CUBE_POSITIONS =
{
	DirectX::XMVECTOR {-1.0f,  1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  1.0f,  1.0f, 1.0f },
	
	DirectX::XMVECTOR {-1.0f, -1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f, -1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f, -1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f, -1.0f,  1.0f, 1.0f },
	
	DirectX::XMVECTOR {-1.0f, -1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f, -1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  1.0f,  1.0f, 1.0f },
	
	DirectX::XMVECTOR { 1.0f, -1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f, -1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  1.0f,  1.0f, 1.0f },
	
	DirectX::XMVECTOR {-1.0f, -1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f, -1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  1.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  1.0f, -1.0f, 1.0f },
	
	DirectX::XMVECTOR {-1.0f, -1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f, -1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  1.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  1.0f,  1.0f, 1.0f }
};

constexpr eastl::array<DirectX::XMVECTOR, 24> CUBE_NORMALS =
{
	DirectX::XMVECTOR { 0.0f,  1.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  1.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  1.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  1.0f,  0.0f, 1.0f },

	DirectX::XMVECTOR { 0.0f, -1.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f, -1.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f, -1.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f, -1.0f,  0.0f, 1.0f },

	DirectX::XMVECTOR {-1.0f,  0.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  0.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  0.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR {-1.0f,  0.0f,  0.0f, 1.0f },

	DirectX::XMVECTOR { 1.0f,  0.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  0.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  0.0f,  0.0f, 1.0f },
	DirectX::XMVECTOR { 1.0f,  0.0f,  0.0f, 1.0f },

	DirectX::XMVECTOR { 0.0f,  0.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  0.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  0.0f, -1.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  0.0f, -1.0f, 1.0f },

	DirectX::XMVECTOR { 0.0f,  0.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  0.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  0.0f,  1.0f, 1.0f },
	DirectX::XMVECTOR { 0.0f,  0.0f,  1.0f, 1.0f }
};