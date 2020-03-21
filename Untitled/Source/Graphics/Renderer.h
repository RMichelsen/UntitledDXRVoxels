#pragma once

#include "Core/InputHandler.h"
#include "Graphics/DX/DXCommon.h"
#include "Graphics/Raytracing/AccelerationStructureManager.h"
#include "Graphics/Raytracing/RaytracingSharedHlsl.h"

class RaytracingPipeline;
class ShaderCompiler;

class Renderer
{
public:
	eastl::unique_ptr<RaytracingPipeline> RTPipeline;

	Renderer(HWND hwnd_, const InputHandler* inputHandler_);
	~Renderer();

	void ReleaseWindowDependentResources();
	void CreateWindowDependentResources(uint32_t width, uint32_t height);
	void PreSimulate();
	void Prepare(float deltaTime);
	void Present();

	void WindowResize(const RECT& clientRect);
	void ToggleFullscreen();

	[[nodiscard]] DXDeviceLocalBuffer CreateVertexBuffer(const Vertex* vertices, size_t size);
	[[nodiscard]] DXDeviceLocalBuffer CreateIndexBuffer(const uint32_t* indices, size_t size);

private:
	HWND hwnd;
	RECT windowRect;
	uint32_t outputWidth;
	uint32_t outputHeight;
	bool fullscreenEnabled;

	GraphicsContext context {};
	const InputHandler* inputHandler;
};