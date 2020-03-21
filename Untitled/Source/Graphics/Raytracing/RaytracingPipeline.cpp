#include "PCH.h"
#include "RaytracingPipeline.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXUtils.h"
#include "Graphics/Memory/ResourceAllocator.h"
#include "Graphics/Raytracing/RaytracingRootSignatures.h"
#include "Graphics/Renderer.h"
#include "Graphics/ShaderCompiler.h"

using namespace Microsoft::WRL;
using namespace DirectX;

RaytracingPipeline::RaytracingPipeline(GraphicsContext& context_, uint32_t width, uint32_t height) :
	context(context_),
	ASManager(eastl::make_unique<AccelerationStructureManager>(context)),
	camera(RaytracingCamera({ 34.0f, 70.0f, -37.0f }, 16.0f / 9.0f, 1000.0f, -270.5f, -33.0f)),
	sunAzimuth(0),
	sunAltitude(DirectX::XM_PI / 1.25f)
{
	constants.cameraPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
	constants.cameraProjectionToWorld = XMMatrixIdentity();
	constants.framecount = 0;

	CreatePickBuffer();
	CreateOutputTexture(width, height);

	CreateShaderResources();
	CreatePipelineState();
	CreateShaderTables();
}

RaytracingPipeline::~RaytracingPipeline()
{
	outputTexture.allocation->Release();
	pickBuffer.Release();
}

void RaytracingPipeline::RaytracePick()
{
	pickBuffer.IssueBufferReadBack(context.copyCommands.Get());
}

void RaytracingPipeline::RaytraceScene(uint32_t width, uint32_t height, const InputHandler* inputHandler, float deltaTime)
{
	XMUINT2* data = pickBuffer.StartBufferRead<XMUINT2>();
	pickBufferContent = *data;
	pickBuffer.EndBufferRead();

	// Update the camera
	camera.Update(inputHandler, deltaTime);
	constants.cameraPosition = XMLoadFloat3A(&camera.position);
	XMMATRIX viewProj = camera.view * camera.projection;
	constants.cameraProjectionToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, viewProj));
	constants.width = width;
	constants.height = height;
	constants.framecount++;

	PIXBeginEvent(context.graphicsCommands.Get(), PIX_COLOR_DEFAULT, L"Raytrace Scene");
	ASManager->BuildTLAS(&context.descriptorHeap);

	// Update the sun position
	sunAltitude = (sunAltitude/* + 0.05f  * deltaTime*/);
	float sinAltitude = sinf(sunAltitude);
	float sinAzimuth = sinf(sunAzimuth);
	float cosAltitude = cosf(sunAltitude);
	float cosAzimuth = cosf(sunAzimuth);

	constants.sunDirection = XMVECTOR { sinAltitude * cosAzimuth, cosAltitude, sinAltitude * sinAzimuth, 0.0f };

	// Transition the back buffer to a copy destination
	// Transition the DXR output buffer to a copy source
	eastl::array<D3D12_RESOURCE_BARRIER, 2> initialBarriers {
		DXUtils::ResourceBarrierTransition(context.backBuffers[context.backBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST),
		DXUtils::ResourceBarrierTransition(outputTexture.GetResource(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
	};
	context.graphicsCommands->ResourceBarrier(2, initialBarriers.data());

	eastl::array<ID3D12DescriptorHeap*, 1> heaps {
		context.descriptorHeap.Get()
	};
	context.graphicsCommands->SetDescriptorHeaps(static_cast<uint32_t>(heaps.size()), heaps.data());

	D3D12_DISPATCH_RAYS_DESC dispatchDesc {
		.RayGenerationShaderRecord {
			.StartAddress = raygenShaderTable->GetGPUAddress(),
			.SizeInBytes = raygenShaderTable->GetAlignedRecordSize()
		},
		.MissShaderTable {
			.StartAddress = missShaderTable->GetGPUAddress(),
			.SizeInBytes = missShaderTable->GetTotalSizeInBytes(),
			.StrideInBytes = missShaderTable->GetAlignedRecordSize()
		},
		.HitGroupTable {
			.StartAddress = hitgroupShaderTable->GetGPUAddress(),
			.SizeInBytes = hitgroupShaderTable->GetTotalSizeInBytes(),
			.StrideInBytes = hitgroupShaderTable->GetAlignedRecordSize()
		},
		.Width = width,
		.Height = height,
		.Depth = 1
	};

	D3D12_DISPATCH_RAYS_DESC pickDispatchDesc {
		.RayGenerationShaderRecord {
			.StartAddress = raygenShaderTable->GetGPUAddress() + raygenShaderTable->GetAlignedRecordSize(),
			.SizeInBytes = raygenShaderTable->GetAlignedRecordSize()
		},
		.MissShaderTable {
			.StartAddress = missShaderTable->GetGPUAddress(),
			.SizeInBytes = missShaderTable->GetTotalSizeInBytes(),
			.StrideInBytes = missShaderTable->GetAlignedRecordSize()
		},
		.HitGroupTable {
			.StartAddress = hitgroupShaderTable->GetGPUAddress(),
			.SizeInBytes = hitgroupShaderTable->GetTotalSizeInBytes(),
			.StrideInBytes = hitgroupShaderTable->GetAlignedRecordSize()
		},
		.Width = 1,
		.Height = 1,
		.Depth = 1
	};

	context.graphicsCommands->SetComputeRootSignature(globalRootSignature);
	context.graphicsCommands->SetComputeRootShaderResourceView(0, ASManager->GetTLASGPUAddress());
	context.graphicsCommands->SetComputeRoot32BitConstants(1, sizeof(RaytracingConstants) / sizeof(uint32_t),
		&constants, 0);

	context.graphicsCommands->SetPipelineState1(pickPipelineState.Get());
	context.graphicsCommands->DispatchRays(&pickDispatchDesc);

	context.graphicsCommands->SetPipelineState1(pipelineState.Get());
	context.graphicsCommands->DispatchRays(&dispatchDesc);

	// Transition output texture from UAV to a copy source
	auto barrier = DXUtils::ResourceBarrierTransition(outputTexture.GetResource(), 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	context.graphicsCommands->ResourceBarrier(1, &barrier);

	// Copy the ray traced output into the back buffer
	context.graphicsCommands->CopyResource(context.backBuffers[context.backBufferIndex].Get(),
		outputTexture.GetResource());

	// Transition the back buffer to present
	barrier = DXUtils::ResourceBarrierTransition(context.backBuffers[context.backBufferIndex].Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	context.graphicsCommands->ResourceBarrier(1, &barrier);

	PIXEndEvent(context.graphicsCommands.Get());
}

void RaytracingPipeline::ReleaseWindowDependentResources()
{
	outputTexture.allocation->Release();
}

void RaytracingPipeline::CreateWindowDependentResources(uint32_t width, uint32_t height)
{
	CreateOutputTexture(width, height);
}

void RaytracingPipeline::CreateOutputTexture(uint32_t width, uint32_t height)
{
	auto outputTextureDesc = DXUtils::ResourceDescTexture2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	outputTextureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Preserve heap index by overriding the UAV index
	uint32_t index = outputTexture.handles.heapIndex;
	outputTexture = context.allocator->CreateTexture2D(&outputTextureDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr);
	outputTexture.CreateUAV(&context.descriptorHeap, index);
}

void RaytracingPipeline::CreatePickBuffer()
{
	auto pickBufferDesc = DXUtils::ResourceDescBuffer(sizeof(uint32_t) * 2);
	pickBuffer = context.allocator->CreateReadBackBuffer(&pickBufferDesc);
	pickBuffer.CreateUAV(sizeof(uint32_t), &context.descriptorHeap);
}

void RaytracingPipeline::CreatePipelineState()
{
	D3D12_DXIL_LIBRARY_DESC DXILLibraryDesc {
		.DXILLibrary = DXILLibrary.bytecode
	};
	D3D12_STATE_SUBOBJECT DXILLibrarySubobject {
		.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
		.pDesc = &DXILLibraryDesc
	};
	D3D12_STATE_OBJECT_DESC pipelineDesc {
		.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
		.NumSubobjects = 1,
		.pSubobjects = &DXILLibrarySubobject
	};

	DXCHECK(context.device->CreateStateObject(
		&pipelineDesc, IID_PPV_ARGS(&pipelineState)));
	DXUtils::SetName(pipelineState.Get(), L"DXR Pipeline State Object");

	// Get the pipeline state properties
	DXCHECK(pipelineState->QueryInterface(IID_PPV_ARGS(&pipelineStateProperties)));

	mainRayGenIdentifier = pipelineStateProperties->GetShaderIdentifier(L"MainGen");
	mainHitGroupIdentifier = pipelineStateProperties->GetShaderIdentifier(L"MainHitGroup");
	mainMissIdentifier = pipelineStateProperties->GetShaderIdentifier(L"MainMiss");
	shadowMissIdentifier = pipelineStateProperties->GetShaderIdentifier(L"ShadowMiss");
	

	D3D12_DXIL_LIBRARY_DESC pickDXILLibraryDesc {
		.DXILLibrary = pickDXILLibrary.bytecode
	};
	D3D12_STATE_SUBOBJECT pickDXILLibrarySubobject {
		.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
		.pDesc = &pickDXILLibraryDesc
	};
	D3D12_STATE_OBJECT_DESC pickPipelineDesc {
		.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
		.NumSubobjects = 1,
		.pSubobjects = &pickDXILLibrarySubobject
	};

	DXCHECK(context.device->CreateStateObject(
		&pickPipelineDesc, IID_PPV_ARGS(&pickPipelineState)));
	DXUtils::SetName(pickPipelineState.Get(), L"DXR Pipeline State Object");

	// Get the pipeline state properties
	DXCHECK(pickPipelineState->QueryInterface(IID_PPV_ARGS(&pickPipelineStateProperties)));

	pickRayGenIdentifier = pickPipelineStateProperties->GetShaderIdentifier(L"PickGen");
	pickHitGroupIdentifier = pickPipelineStateProperties->GetShaderIdentifier(L"PickHitGroup");
	pickMissIdentifier = pickPipelineStateProperties->GetShaderIdentifier(L"PickMiss");
}

void RaytracingPipeline::CreateShaderResources()
{
	// Create the DXIL library
	DXILLibrary = context.shaderCompiler->CompileDXILLibrary(L"MainRays2.hlsl");
	pickDXILLibrary = context.shaderCompiler->CompileDXILLibrary(L"PickRays.hlsl");

	// Create the global root signature from the DXIL library bytecode
	DXCHECK(context.device->CreateRootSignature(0, DXILLibrary.bytecode.pShaderBytecode, 
		DXILLibrary.bytecode.BytecodeLength, IID_PPV_ARGS(&globalRootSignature)));
}

void RaytracingPipeline::CreateShaderTables()
{
	const auto& CreateShaderTable = [this](uint32_t shaderRecordSize, uint32_t shaderRecordCount, 
		const wchar_t* identifier)
	{
		return eastl::make_unique<RaytracingShaderTable>(context.allocator.get(), pipelineStateProperties.Get(),
			shaderRecordSize, shaderRecordCount, identifier);
	};

	raygenShaderTable = CreateShaderTable(64, 2, L"Ray Generation Shader Table");
	missShaderTable = CreateShaderTable(64, 3, L"Miss Shader Table");
	hitgroupShaderTable = CreateShaderTable(64, MAX_NUM_BLAS, L"Hit Group Shader Table");

	// Insert shader record for the ray generation shader
	raygenShaderTable->InsertShaderRecord(mainRayGenIdentifier, &outputTexture.handles.gpuHandle, sizeof(uint64_t));
	raygenShaderTable->InsertEmptyShaderRecord(pickRayGenIdentifier);

	// Insert shader record for the two miss shaders
	missShaderTable->InsertEmptyShaderRecord(mainMissIdentifier);
	missShaderTable->InsertEmptyShaderRecord(shadowMissIdentifier);
	missShaderTable->InsertEmptyShaderRecord(pickMissIdentifier);

	RepopulateHitgroups();
}

void RaytracingPipeline::RepopulateHitgroups()
{
	hitgroupShaderTable->Reset();

	for (auto& BLAS : ASManager->GetBLAS())
	{
		BLAS.instanceContributionToHitGroupIndex = hitgroupShaderTable->GetCurrentTableOffset();
		for (const auto& instance : BLAS.geometryInstances)
		{
			HitGroupLocalRootSignature::RootArguments args {
				.indicesGPUHandle = instance.indices.handles.gpuHandle,
				.verticesGPUHandle = instance.vertices.handles.gpuHandle,
				.pickBufferGPUHandle = pickBuffer.handles.gpuHandle
			};
			hitgroupShaderTable->InsertShaderRecord(mainHitGroupIdentifier, &args, sizeof(HitGroupLocalRootSignature::RootArguments));
			hitgroupShaderTable->InsertShaderRecord(pickHitGroupIdentifier, &args, sizeof(HitGroupLocalRootSignature::RootArguments));
		}
	}
}

void RaytracingPipeline::AddHitgroupEntry(BLASHandle handle)
{
	auto& BLAS = ASManager->GetBLAS()[handle];

	BLAS.instanceContributionToHitGroupIndex = hitgroupShaderTable->GetCurrentTableOffset();
	for (const auto& instance : BLAS.geometryInstances)
	{
		HitGroupLocalRootSignature::RootArguments args {
			.indicesGPUHandle = instance.indices.handles.gpuHandle,
			.verticesGPUHandle = instance.vertices.handles.gpuHandle,
			.pickBufferGPUHandle = pickBuffer.handles.gpuHandle
		};
		hitgroupShaderTable->InsertShaderRecord(mainHitGroupIdentifier, &args, sizeof(HitGroupLocalRootSignature::RootArguments));
		hitgroupShaderTable->InsertShaderRecord(pickHitGroupIdentifier, &args, sizeof(HitGroupLocalRootSignature::RootArguments));
	}
}
