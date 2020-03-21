#pragma once

#include "Graphics/DX/DXCommandQueue.h"

class DXCommandQueueManager
{
public:
	DXCommandQueueManager() = default;
	DXCommandQueueManager(ID3D12Device5* const device) :
		graphics(DXCommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT)),
		copy(DXCommandQueue(device, D3D12_COMMAND_LIST_TYPE_COPY))
	{
	}

	inline uint64_t ExecuteGraphicsCommands(ID3D12CommandList* commandList)
	{
		return graphics.ExecuteCommands(commandList);
	}

	inline uint64_t ExecuteCopyCommands(ID3D12CommandList* commandList)
	{
		return copy.ExecuteCommands(commandList);
	}

	inline bool QueryFence(uint64_t fenceValue)
	{
		GetQueue(fenceValue).QueryFence(fenceValue);
	}

	inline void WaitForFence(uint64_t fenceValue)
	{
		GetQueue(fenceValue).WaitForFence(fenceValue);
	}

	inline void WaitForIdle()
	{
		graphics.WaitForIdle();
		copy.WaitForIdle();
	}

	inline void CopyToGraphicsQueueBarrier()
	{
		graphics.StallForQueue(copy);
	}

	inline ID3D12CommandQueue* GetGraphicsQueue() { return graphics.GetQueue(); }
	inline ID3D12CommandQueue* GetCopyQueue() { return copy.GetQueue(); }

private:
	DXCommandQueue graphics;
	DXCommandQueue copy;

	inline DXCommandQueue& GetQueue(uint64_t fenceValue)
	{
		// Get the queue by shifting back the fence value
		D3D12_COMMAND_LIST_TYPE type = static_cast<D3D12_COMMAND_LIST_TYPE>(fenceValue >> 56);

		switch (type)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: return graphics;
		case D3D12_COMMAND_LIST_TYPE_COPY: return copy;
		default: return graphics;
		}
	}
};