#pragma once

class DXCommandQueue
{
public:
	DXCommandQueue() = default;
	DXCommandQueue(ID3D12Device5* device, D3D12_COMMAND_LIST_TYPE commandType);

	bool QueryFence(uint64_t fenceValue);
	void WaitForFence(uint64_t fenceValue);
	void WaitForIdle();
	void StallForQueue(DXCommandQueue& targetQueue);
	uint64_t ExecuteCommands(ID3D12CommandList* commandList);

	inline ID3D12CommandQueue* GetQueue()
	{
		return queue.Get();
	}

private:
	friend class DXCommandQueue;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
	D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	Microsoft::WRL::Wrappers::Event fenceEvent = Microsoft::WRL::Wrappers::Event(NULL);
	uint64_t currentFenceValue = 0;
	uint64_t nextFenceValue = 0;
};