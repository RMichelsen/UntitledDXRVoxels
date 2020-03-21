#pragma once

#include "Game/Chunk.h"
#include "Graphics/Renderer.h"

union BlockIdentifier
{
	struct
	{
		uint32_t chunkIndex : 8;
		uint32_t voxelIndex : 24;
	} bits;
	uint32_t value;
};

class ChunkManager
{
public:
	ChunkManager(Renderer* const renderer_);
	~ChunkManager();

	void AddChunk(DirectX::XMINT3 position);

	void CreateVoxel(DirectX::XMUINT2 pickBuffer);
	void DestroyVoxel(DirectX::XMUINT2 pickBuffer);

	void RebuildUpdatedChunks();

private:
	FastNoiseSIMD* noise;
	Renderer* const renderer;

	eastl::vector<uint32_t> indices;
	eastl::vector<Vertex> vertices;

	eastl::vector<Chunk> chunks;
	void FreeChunk(Chunk& chunk);

	void GenerateVoxels(Chunk& chunk);
	void GenerateMesh(Chunk& chunk);
	void RegenerateMesh(Chunk& chunk);
};

