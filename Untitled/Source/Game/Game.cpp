#include "PCH.h"
#include "Game.h"

#include "Graphics/Raytracing/RaytracingPipeline.h"

void Game::Init(HWND hwnd)
{
	input = eastl::make_unique<InputHandler>();
	renderer = eastl::make_unique<Renderer>(hwnd, input.get());

	chunkManager = eastl::make_unique<ChunkManager>(renderer.get());
}

void Game::Shutdown()
{
	// Explicit order of destruction is necessary
	chunkManager.reset();

	input.reset();
	renderer.reset();
}

void Game::Simulate(float deltaTime)
{
	static int i = 0;

	if (i == 0)
	{
		chunkManager->AddChunk({ 0, 0, 0 });
	}
	i++;


	if (input->mouseButtonPressed.left)
	{
		chunkManager->CreateVoxel(renderer->RTPipeline->pickBufferContent);
	}

	if (input->mouseButtonPressed.middle)
	{
		chunkManager->DestroyVoxel(renderer->RTPipeline->pickBufferContent);
	}



	chunkManager->RebuildUpdatedChunks();
}

