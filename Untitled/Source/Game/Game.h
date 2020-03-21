#pragma once

#include "Core/InputHandler.h"
#include "Game/ChunkManager.h"
#include "Graphics/Renderer.h"

class Game
{
public:
	eastl::unique_ptr<InputHandler> input;
	eastl::unique_ptr<Renderer> renderer;

	void Init(HWND hwnd);
	void Shutdown();
	void Simulate(float deltaTime);

private:
	eastl::unique_ptr<ChunkManager> chunkManager;
};

