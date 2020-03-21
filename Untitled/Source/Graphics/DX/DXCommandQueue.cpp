#include "PCH.h"
#include "DXCommandQueue.h"

#include "Core/Logging.h"
#include "Graphics/DX/DXCommon.h"

DXCommandQueue::DXCommandQueue(ID3D12Device5* const device, D3D12_COMMAND_LIST_TYPE type_) :
	type(type_)
{
	// Here we use a trick to be able to distinguish command queues
	// by their fence value. We bit shift the initial fence value depending on 
	// the queue type.
	currentFenceValue = (static_cast<uint64_t>(type) << 56);
	nextFenceValue = (static_cast<uint64_t>(type) << 56) + 1;

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc {
		.Type = type_,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
	};
	DXCHECK(device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&queue)));
	DXUtils::SetName(queue.Get(), L"Command Queue");

	DXCHECK(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	fenceEvent.Attach(CreateEvent(nullptr, false, false, nullptr));
	UNTITLED_ASSERT(fenceEvent.IsValid() && "CreateEvent Failed!");

	DXCHECK(fence->Signal(currentFenceValue));
}

bool DXCommandQueue::QueryFence(uint64_t fenceValue)
{
	// If the queried fence value is less or equal to the
	// current fence value we know that it has already been
	// processed so we can skip the call to GetCompletedValue
	if (fenceValue > currentFenceValue)
	{
		currentFenceValue = eastl::max(currentFenceValue, fence->GetCompletedValue());
	}

	return fenceValue <= currentFenceValue;
}

void DXCommandQueue::WaitForFence(uint64_t fenceValue)
{
	// Early return if the fence has already completed
	if (QueryFence(fenceValue))
	{
		return;
	}

	DXCHECK(fence->SetEventOnCompletion(fenceValue, fenceEvent.Get()));
	WaitForSingleObjectEx(fenceEvent.Get(), INFINITE, false);
}

void DXCommandQueue::WaitForIdle()
{
	// Signal the next fence value on the GPU side
	queue->Signal(fence.Get(), nextFenceValue);
	WaitForFence(nextFenceValue++);
}

void DXCommandQueue::StallForQueue(DXCommandQueue& targetQueue)
{
	queue->Wait(targetQueue.fence.Get(), targetQueue.nextFenceValue - 1);
}

uint64_t DXCommandQueue::ExecuteCommands(ID3D12CommandList* commandList)
{
	DXCHECK(static_cast<ID3D12GraphicsCommandList1*>(commandList)->Close());
	queue->ExecuteCommandLists(1, &commandList);

	// Signal the next fence value on the GPU side
	queue->Signal(fence.Get(), nextFenceValue);
	return nextFenceValue++;
}
