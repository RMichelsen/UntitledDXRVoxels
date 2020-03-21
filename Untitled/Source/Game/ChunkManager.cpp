#include "PCH.h"
#include "ChunkManager.h"

#include "Graphics/Raytracing/RaytracingPipeline.h"

using namespace DirectX;

ChunkManager::ChunkManager(Renderer* const renderer_) :
	renderer(renderer_)
{
	// Initialize the noise generator 
	noise = FastNoiseSIMD::NewFastNoiseSIMD(42);
	noise->SetNoiseType(FastNoiseSIMD::SimplexFractal);
	noise->SetFrequency(0.025f);
	noise->SetFractalType(FastNoiseSIMD::FractalType::FBM);
	noise->SetFractalOctaves(2);
	noise->SetFractalLacunarity(2.33f);
	noise->SetFractalGain(0.366f);

	vertices.resize(4'100'000);
	indices.resize(16'400'000);
}

ChunkManager::~ChunkManager()
{
	for (auto& chunk : chunks)
	{
		FreeChunk(chunk);
	}

	delete noise;
}

void ChunkManager::AddChunk(XMINT3 position)
{
	auto& chunk = chunks.emplace_back(Chunk(position, chunks.size()));
	GenerateVoxels(chunk);
	GenerateMesh(chunk);
}

void ChunkManager::CreateVoxel(DirectX::XMUINT2 pickBuffer)
{
	BlockIdentifier identifier;
	identifier.value = pickBuffer.x;

	int x = identifier.bits.voxelIndex % VOXEL_CHUNK_WIDTH;
	int y = (identifier.bits.voxelIndex / VOXEL_CHUNK_WIDTH) % VOXEL_CHUNK_WIDTH;
	int z = identifier.bits.voxelIndex / (VOXEL_CHUNK_WIDTH * VOXEL_CHUNK_WIDTH);

	if (pickBuffer.y == VisibleFaces::Bottom)
	{
		chunks[0].voxels[GetIndex(x, y - 1, z)].fillType = FillType::Solid;
		chunks[0].SetFacesForVoxel(x, y - 1, z);
	}
	else if (pickBuffer.y == VisibleFaces::Top)
	{
		chunks[0].voxels[GetIndex(x, y + 1, z)].fillType = FillType::Solid;
		chunks[0].SetFacesForVoxel(x, y + 1, z);
	}
	else if (pickBuffer.y == VisibleFaces::East)
	{
		chunks[0].voxels[GetIndex(x + 1, y, z)].fillType = FillType::Solid;
		chunks[0].SetFacesForVoxel(x + 1, y, z);
	}
	else if (pickBuffer.y == VisibleFaces::West)
	{
		chunks[0].voxels[GetIndex(x - 1, y, z)].fillType = FillType::Solid;
		chunks[0].SetFacesForVoxel(x - 1, y, z);
	}
	else if (pickBuffer.y == VisibleFaces::North)
	{
		chunks[0].voxels[GetIndex(x, y, z + 1)].fillType = FillType::Solid;
		chunks[0].SetFacesForVoxel(x, y, z + 1);
	}
	else if (pickBuffer.y == VisibleFaces::South)
	{
		chunks[0].voxels[GetIndex(x, y, z - 1)].fillType = FillType::Solid;
		chunks[0].SetFacesForVoxel(x, y, z - 1);
	}
	
	RegenerateMesh(chunks[0]);
}

void ChunkManager::DestroyVoxel(DirectX::XMUINT2 pickBuffer)
{
	BlockIdentifier identifier;
	identifier.value = pickBuffer.x;

	int x = identifier.bits.voxelIndex % VOXEL_CHUNK_WIDTH;
	int y = (identifier.bits.voxelIndex / VOXEL_CHUNK_WIDTH) % VOXEL_CHUNK_WIDTH;
	int z = identifier.bits.voxelIndex / (VOXEL_CHUNK_WIDTH * VOXEL_CHUNK_WIDTH);

	chunks[0].voxels[identifier.bits.voxelIndex].fillType = FillType::Empty;

	auto idx = GetIndex(x, y, z);
	UNTITLED_ASSERT(idx == identifier.bits.voxelIndex);

	// Since the voxel is now visible, flags need to be updated for surrounding voxels
	if (x + 1 < VOXEL_CHUNK_WIDTH && chunks[0].voxels[GetIndex(x + 1, y, z)].fillType == FillType::Solid) chunks[0].voxels[GetIndex(x + 1, y, z)].visibleFaces |= VisibleFaces::West;
	if (x - 1 >= 0				  && chunks[0].voxels[GetIndex(x - 1, y, z)].fillType == FillType::Solid) chunks[0].voxels[GetIndex(x - 1, y, z)].visibleFaces |= VisibleFaces::East;
	if (y + 1 < VOXEL_CHUNK_WIDTH && chunks[0].voxels[GetIndex(x, y + 1, z)].fillType == FillType::Solid) chunks[0].voxels[GetIndex(x, y + 1, z)].visibleFaces |= VisibleFaces::Bottom;
	if (y - 1 >= 0				  && chunks[0].voxels[GetIndex(x, y - 1, z)].fillType == FillType::Solid) chunks[0].voxels[GetIndex(x, y - 1, z)].visibleFaces |= VisibleFaces::Top;
	if (z + 1 < VOXEL_CHUNK_WIDTH && chunks[0].voxels[GetIndex(x, y, z + 1)].fillType == FillType::Solid) chunks[0].voxels[GetIndex(x, y, z + 1)].visibleFaces |= VisibleFaces::South;
	if (z - 1 >= 0				  && chunks[0].voxels[GetIndex(x, y, z - 1)].fillType == FillType::Solid) chunks[0].voxels[GetIndex(x, y, z - 1)].visibleFaces |= VisibleFaces::North;

	RegenerateMesh(chunks[0]);
}

void ChunkManager::RebuildUpdatedChunks()
{
	for (auto& chunk : chunks)
	{
		if (chunk.needsRebuild)
		{
			renderer->RTPipeline->RebuildBLAS(chunk.GPUResources.BLAS, {
				AccelerationStructureGeometry {
					.vertices = chunk.GPUResources.vBuffer,
					.indices = chunk.GPUResources.iBuffer
				} 
			});
			chunk.needsRebuild = false;
		}
	}
}

void ChunkManager::FreeChunk(Chunk& chunk)
{
	renderer->RTPipeline->RemoveBLAS(chunk.GPUResources.BLAS);
	renderer->RTPipeline->RemoveBLASInstance(chunk.GPUResources.BLASInstance);
	chunk.GPUResources.iBuffer.Release();
	chunk.GPUResources.vBuffer.Release();
}

void ChunkManager::GenerateVoxels(Chunk& chunk)
{
	chunk.voxels.resize(VOXEL_CHUNK_WIDTH * VOXEL_CHUNK_WIDTH * VOXEL_CHUNK_WIDTH);

	XMVECTOR positionVec = XMLoadSInt3(&chunk.position);

	float* noiseSet = noise->GetNoiseSet(chunk.position.x, chunk.position.y, chunk.position.z, VOXEL_CHUNK_WIDTH, 1, VOXEL_CHUNK_WIDTH, 0.25f);
	for (int z = 0; z < VOXEL_CHUNK_WIDTH; ++z)
	{
		for (int y = 0; y < VOXEL_CHUNK_WIDTH; ++y)
		{
			for (int x = 0; x < VOXEL_CHUNK_WIDTH; ++x)
			{
				auto& voxel = chunk.voxels[GetIndex(x, y, z)];

				const bool belowCutoff = (y + chunk.position.y) < (VOXEL_CHUNK_WIDTH * ((noiseSet[(z * VOXEL_CHUNK_WIDTH) + x] + 0.8f) / 1.6f));
				belowCutoff ? voxel.fillType = FillType::Solid : voxel.fillType = FillType::Empty;
			}
		}
	}
	noise->FreeNoiseSet(noiseSet);

	for (auto z = 0; z < VOXEL_CHUNK_WIDTH; z++)
	{
		for (auto y = 0; y < VOXEL_CHUNK_WIDTH; y++)
		{
			for (auto x = 0; x < VOXEL_CHUNK_WIDTH; x++)
			{
				auto& voxel = chunk.voxels[GetIndex(x, y, z)];

				// Skip empty voxels
				if (voxel.fillType == FillType::Empty) continue;


				// Set visible faces for the current voxel, unsets the faces that have a solid block next to them
				chunk.SetFacesForVoxel(x, y, z);

				// Faces that aren't part of the top are not visible
				// However if the top face is visible on this voxel, the sides are too to avoid gaps!
				if (voxel.visibleFaces & VisibleFaces::Top)
				{
					if (x == VOXEL_CHUNK_WIDTH - 1) voxel.visibleFaces |= VisibleFaces::East;
					if (x == 0)						voxel.visibleFaces |= VisibleFaces::West;
					if (y == VOXEL_CHUNK_WIDTH - 1) voxel.visibleFaces |= VisibleFaces::Bottom;
					if (y == 0)						voxel.visibleFaces |= VisibleFaces::North;
					if (z == VOXEL_CHUNK_WIDTH - 1) voxel.visibleFaces |= VisibleFaces::South;
				}
			}
		}
	}
}

void ChunkManager::GenerateMesh(Chunk& chunk)
{
	const auto& GetPointsFromFace = [](VisibleFaces face)
	{
		static const eastl::array<XMINT3, 8> cubePoints { {
			{0, 0, 0},
			{0, 1, 0},
			{0, 0, 1},
			{0, 1, 1},
			{1, 0, 0},
			{1, 1, 0},
			{1, 0, 1},
			{1, 1, 1}
		} };

		switch (face)
		{
		case VisibleFaces::North:
			return eastl::array<XMINT3, 4> { cubePoints[6], cubePoints[7], cubePoints[3], cubePoints[2] };
		case VisibleFaces::South:
			return eastl::array<XMINT3, 4> { cubePoints[4], cubePoints[0], cubePoints[1], cubePoints[5] };
		case VisibleFaces::East:
			return eastl::array<XMINT3, 4> { cubePoints[4], cubePoints[5], cubePoints[7], cubePoints[6] };
		case VisibleFaces::West:
			return eastl::array<XMINT3, 4> { cubePoints[0], cubePoints[2], cubePoints[3], cubePoints[1] };
		case VisibleFaces::Top:
			return eastl::array<XMINT3, 4> { cubePoints[5], cubePoints[1], cubePoints[3], cubePoints[7] };
		case VisibleFaces::Bottom:
			return eastl::array<XMINT3, 4> { cubePoints[4], cubePoints[6], cubePoints[2], cubePoints[0] };
		default:
			return eastl::array<XMINT3, 4> { cubePoints[0], cubePoints[0], cubePoints[0], cubePoints[0] };
		}
	};

	const auto& GetNormalFromFace = [](VisibleFaces face)
	{
		switch (face)
		{
		case VisibleFaces::North:
			return XMVECTOR { 0, 0, 1 };
		case VisibleFaces::South:
			return XMVECTOR { 0, 0, -1 };
		case VisibleFaces::East:
			return XMVECTOR { 1, 0, 0 };
		case VisibleFaces::West:
			return XMVECTOR { -1, 0, 0 };
		case VisibleFaces::Top:
			return XMVECTOR { 0, 1, 0 };
		case VisibleFaces::Bottom:
			return XMVECTOR { 0, -1, 0 };
		default:
			return XMVECTOR { 0, 0, 0 };
		}
	};

	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	for (uint32_t face = VisibleFaces::North; face <= VisibleFaces::Bottom; face *= 2)
	{
		for (int z = 0; z < VOXEL_CHUNK_WIDTH; ++z)
		{
			for (int y = 0; y < VOXEL_CHUNK_WIDTH; ++y)
			{
				for (int x = 0; x < VOXEL_CHUNK_WIDTH; ++x)
				{
					const auto index = x + VOXEL_CHUNK_WIDTH * (y + VOXEL_CHUNK_WIDTH * z);
					const auto& voxel = chunk.voxels[index];

					if (!(voxel.visibleFaces & face)) continue;
					if (voxel.fillType == FillType::Empty) continue;

					auto points = GetPointsFromFace(static_cast<VisibleFaces>(face));
					for (int i = 0; i < 4; i++)
					{
						XMVECTOR finalPos = XMVECTOR { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), 0 } + XMLoadSInt3(&points[i]);
						XMVECTOR normal = GetNormalFromFace(static_cast<VisibleFaces>(face));

						BlockIdentifier identifier;
						identifier.bits.chunkIndex = chunk.index;
						identifier.bits.voxelIndex = index;

						finalPos = DirectX::XMVectorSetIntW(finalPos, identifier.value);
						normal = DirectX::XMVectorSetIntW(normal, face);
						vertices[vertexCount++] = (Vertex { finalPos, normal });
					}

					indices[indexCount++] = (vertexCount - 4 + 0);
					indices[indexCount++] = (vertexCount - 4 + 1);
					indices[indexCount++] = (vertexCount - 4 + 2);

					indices[indexCount++] = (vertexCount - 4 + 0);
					indices[indexCount++] = (vertexCount - 4 + 2);
					indices[indexCount++] = (vertexCount - 4 + 3);
				}
			}
		}
	}

	chunk.GPUResources.vBuffer = renderer->CreateVertexBuffer(vertices.data(), vertexCount);
	chunk.GPUResources.iBuffer = renderer->CreateIndexBuffer(indices.data(), indexCount);

	chunk.GPUResources.BLAS = renderer->RTPipeline->AddBLAS({
		AccelerationStructureGeometry {
			.vertices = chunk.GPUResources.vBuffer,
			.indices = chunk.GPUResources.iBuffer
		}
		});

	chunk.GPUResources.BLASInstance = renderer->RTPipeline->AddBLASInstance(chunk.GPUResources.BLAS);
}

void ChunkManager::RegenerateMesh(Chunk& chunk)
{
	FreeChunk(chunk);
	GenerateMesh(chunk);
	chunk.needsRebuild = true;
}
