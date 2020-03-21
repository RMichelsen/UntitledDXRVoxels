#include "PCH.h"
#include "Renderer.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXUtils.h"
#include "Graphics/DX/DXCommandQueue.h"
#include "Graphics/DX/DXDescriptorHeap.h"
#include "Graphics/Memory/ResourceAllocator.h"
#include "Graphics/Raytracing/RayTracingPipeline.h"
#include "Graphics/ShaderCompiler.h"

using namespace Microsoft::WRL;

constexpr uint32_t MAX_DESCRIPTORS = 1024;

Renderer::Renderer(HWND hwnd_, const InputHandler* inputHandler_) : 
	hwnd(hwnd_),
	inputHandler(inputHandler_)
{
	// Enable debug layer and create factory
	{
#ifdef _DEBUG
		ComPtr<ID3D12Debug> debugController;
		DXCHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();

		DXCHECK(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&context.factory)));
#else
		DXCHECK(CreateDXGIFactory1(IID_PPV_ARGS(&context.factory)));
#endif
	}

	// Ensure support for tearing, which is necessary for VRR (Variable Refresh Rate) displays
	{
		uint32_t allowTearing = 0;
		ComPtr<IDXGIFactory5> factory5;
		DXCHECK(context.factory.As(&factory5));
		DXCHECK(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));
		UNTITLED_ASSERT(allowTearing && "Cannot initialize D3D12, no support for tearing!");
	}

	// Create the DXC shader compiler
	context.shaderCompiler = eastl::make_unique<ShaderCompiler>();

	// Iterate and pick first available D3D12 compatible adapter
	{
		ComPtr<IDXGIAdapter1> adapter;
		DXGI_ADAPTER_DESC1 adapterDesc{};
		for (uint32_t id = 0; context.factory->EnumAdapters1(id, &adapter) != DXGI_ERROR_NOT_FOUND; ++id)
		{
			adapter->GetDesc1(&adapterDesc);

			// Query D3D12 compatibility
			if (((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) &&
				SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)))
			{
				DXCHECK(adapter.As(&context.adapter));
				break;
			}
		}
	}

	// Create the actual device
	DXCHECK(D3D12CreateDevice(context.adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&context.device)));

	// Ensure support for raytracing
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};
		DXCHECK(context.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)));
		UNTITLED_ASSERT(featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED && "Ray tracing not supported on this device!");
	}

	// Create the D3D12 memory allocator
	context.allocator = eastl::make_unique<ResourceAllocator>(context);

	{
#ifdef _DEBUG
		// Enable break on debug messages
		ComPtr<ID3D12InfoQueue> infoQueue;
		DXCHECK(context.device.As(&infoQueue));
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		// Filter unwanted debug messages
		eastl::array<D3D12_MESSAGE_ID, 1> unwantedMessages =
		{
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
		};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = static_cast<uint32_t>(unwantedMessages.size());
		filter.DenyList.pIDList = unwantedMessages.data();
		infoQueue->AddStorageFilterEntries(&filter);
#endif
	}

	// Create the command queues
	context.queues = DXCommandQueueManager(context.device.Get());

	// Create descriptor heap for the render targets
	{
		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
		descriptorHeapDesc.NumDescriptors = MAX_FRAMES_IN_FLIGHT;
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		DXCHECK(context.device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&context.RTVDescriptorHeap)));
		context.RTVDescriptorHeapIncSize = context.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		DXUtils::SetName(context.RTVDescriptorHeap.Get(), L"RTV Descriptor Heap");
	}

	// Create descriptor heap for SRVs/UAVs
	context.descriptorHeap = DXDescriptorHeap(context.device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_DESCRIPTORS);

	// Create copy and graphics command allocators for each back buffer
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		DXCHECK(context.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, 
			IID_PPV_ARGS(&context.graphicsCommandAllocators[i])));
		DXUtils::SetName(context.graphicsCommandAllocators[i].Get(), L"Graphics Command Allocator");

		DXCHECK(context.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
			IID_PPV_ARGS(&context.copyCommandAllocators[i])));
		DXUtils::SetName(context.copyCommandAllocators[i].Get(), L"Copy Command Allocator");
	}

	// Create a command list for copy and graphics commands
	DXCHECK(context.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, 
		context.graphicsCommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&context.graphicsCommands)));
	DXCHECK(context.graphicsCommands->Close());
	DXUtils::SetName(context.graphicsCommands.Get(), L"Graphics Command List");

	DXCHECK(context.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY,
		context.copyCommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&context.copyCommands)));
	DXCHECK(context.copyCommands->Close());
	DXUtils::SetName(context.copyCommands.Get(), L"Copy Command List");

	RECT clientRect {};
	GetClientRect(hwnd, &clientRect);
	outputWidth = clientRect.right - clientRect.left;
	outputHeight = clientRect.bottom - clientRect.top;

	RTPipeline = eastl::make_unique<RaytracingPipeline>(context, outputWidth, outputHeight);
	CreateWindowDependentResources(outputWidth, outputHeight);
}

Renderer::~Renderer()
{
	context.queues.WaitForIdle();

	RTPipeline.reset();
	context.allocator.reset();
}

void Renderer::ReleaseWindowDependentResources()
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		context.backBuffers[i].Reset();
	}
}

void Renderer::CreateWindowDependentResources(uint32_t width, uint32_t height)
{
	UNTITLED_ASSERT(hwnd && "Window handle not present!");

	// Resize the swapchain if its already created, otherwise create it
	if (context.swapChain)
	{
		DXCHECK(context.swapChain->ResizeBuffers(MAX_FRAMES_IN_FLIGHT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING));

		// TODO: It might be possible to recreate all resources in case ResizeBuffers returns DXGI_ERROR_DEVICE_REMOVED or DXGI_ERROR_DEVICE_RESET
		// refer to https://github.com/Microsoft/DirectX-Graphics-Samples
	}
	else
	{
		// Create a descriptor for the swap chain.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = MAX_FRAMES_IN_FLIGHT;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullscreenDesc{};
		swapChainFullscreenDesc.Windowed = true;

		ComPtr<IDXGISwapChain1> swapChain;
		DXCHECK(context.factory->CreateSwapChainForHwnd(context.queues.GetGraphicsQueue(), hwnd, &swapChainDesc,
			&swapChainFullscreenDesc, nullptr, &swapChain));
		DXCHECK(swapChain.As(&context.swapChain));

		// Let the window handle ALT+ENTER transitions
		context.factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	}

	// Retrieve back buffers from the swap chain and create views for them
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		DXCHECK(context.swapChain->GetBuffer(i, IID_PPV_ARGS(&context.backBuffers[i])));

		wchar_t name[25]{};
		swprintf_s(name, L"Render target %i", i);
		context.backBuffers[i]->SetName(name);

		D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
		renderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;


		auto descriptor = DXUtils::GetDescriptorHandle(context.RTVDescriptorHeap.Get(), i, context.RTVDescriptorHeapIncSize);
		context.device->CreateRenderTargetView(context.backBuffers[i].Get(), &renderTargetViewDesc, descriptor);
	}

	// Get the current back buffer index from the 
	context.backBufferIndex = context.swapChain->GetCurrentBackBufferIndex();

	// Set the default viewport and scissor rect to fit the screen size
	context.viewport.TopLeftX = 0;
	context.viewport.TopLeftY = 0;
	context.viewport.Width = static_cast<float>(width);
	context.viewport.Height = static_cast<float>(height);
	context.viewport.MinDepth = D3D12_MIN_DEPTH;
	context.viewport.MaxDepth = D3D12_MAX_DEPTH;

	context.scissorRect.left = 0;
	context.scissorRect.top = 0;
	context.scissorRect.right = width;
	context.scissorRect.bottom = height;
}

void Renderer::PreSimulate()
{
	// Before simulating, reset the command lists to be ready for use
	DXCHECK(context.copyCommandAllocators[context.backBufferIndex]->Reset());
	DXUtils::ResetCopyCommandList(context);
	DXCHECK(context.graphicsCommandAllocators[context.backBufferIndex]->Reset());
	DXUtils::ResetGraphicsCommandList(context);
}

void Renderer::Prepare(float deltaTime)
{
	RTPipeline->RaytracePick();

	// The graphics queue at this point has to wait for all per-frame copies
	// to have finished on the copy queue
	context.queues.ExecuteCopyCommands(context.copyCommands.Get());
	context.queues.CopyToGraphicsQueueBarrier();

	RTPipeline->RaytraceScene(outputWidth, outputHeight, inputHandler, deltaTime);
}

void Renderer::Present()
{
	// Execute the commands
	auto fenceValue = context.queues.ExecuteGraphicsCommands(context.graphicsCommands.Get());

	// Present
	DXCHECK(context.swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING));

	context.queues.WaitForFence(fenceValue);
	context.backBufferIndex = context.swapChain->GetCurrentBackBufferIndex();

	// By this time all per-frame copy commands must have finished
	context.allocator->ReleaseStagingBuffers();
}

void Renderer::WindowResize(const RECT& clientRect)
{
	uint32_t width = clientRect.right - clientRect.left;
	uint32_t height = clientRect.bottom - clientRect.top;

	// Skip the resize if width or height is 0
	// or if the current output width and height match the new width and height
	if ((width == 0 || height == 0) ||
		(width == outputWidth && height == outputHeight))
	{
		return;
	}

	context.queues.WaitForIdle();

	RTPipeline->ReleaseWindowDependentResources();
	ReleaseWindowDependentResources();
	RTPipeline->CreateWindowDependentResources(width, height);
	CreateWindowDependentResources(width, height);
	outputWidth = width;
	outputHeight = height;
}

void Renderer::ToggleFullscreen()
{
	UNTITLED_ASSERT(hwnd && "Window handle not present!");

	if (fullscreenEnabled)
	{
		// Restore window style
		SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

		SetWindowPos(
			hwnd,
			HWND_NOTOPMOST,
			windowRect.left,
			windowRect.top,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			SWP_FRAMECHANGED | SWP_NOACTIVATE
		);

		ShowWindow(hwnd, SW_NORMAL);
	}
	else
	{
		// Save current window rect to be restored when exiting fullscreen
		GetWindowRect(hwnd, &windowRect);

		SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

		// Get display information
		ComPtr<IDXGIOutput> output;
		DXCHECK(context.swapChain->GetContainingOutput(&output));
		DXGI_OUTPUT_DESC outputDesc{};
		DXCHECK(output->GetDesc(&outputDesc));

		SetWindowPos(
			hwnd,
			HWND_TOP,
			outputDesc.DesktopCoordinates.left,
			outputDesc.DesktopCoordinates.top,
			outputDesc.DesktopCoordinates.right,
			outputDesc.DesktopCoordinates.bottom,
			SWP_FRAMECHANGED | SWP_NOACTIVATE
		);

		ShowWindow(hwnd, SW_MAXIMIZE);
	}

	fullscreenEnabled = !fullscreenEnabled;
}

DXDeviceLocalBuffer Renderer::CreateVertexBuffer(const Vertex* vertices, const size_t size)
{
	auto vertexBufferDesc = DXUtils::ResourceDescBuffer(size * sizeof(Vertex));
	auto buffer = context.allocator->CreateDeviceLocalBufferWithData(&vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, vertices);
	// Override default name
	DXUtils::SetName(buffer.GetResource(), L"Vertex Buffer");

	buffer.CreateSRV(sizeof(Vertex), &context.descriptorHeap);
	return buffer;
}

DXDeviceLocalBuffer Renderer::CreateIndexBuffer(const uint32_t* indices, const size_t size)
{
	auto indexBufferDesc = DXUtils::ResourceDescBuffer(size * sizeof(uint32_t));
	auto buffer = context.allocator->CreateDeviceLocalBufferWithData(&indexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, indices);
	// Override default name
	DXUtils::SetName(buffer.GetResource(), L"Index Buffer");

	buffer.CreateSRV(sizeof(uint32_t), &context.descriptorHeap);
	return buffer;
}