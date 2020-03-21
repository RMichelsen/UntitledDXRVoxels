#pragma once

#include "Core/InputHandler.h"

struct alignas(16) RaytracingCamera
{
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX projection;

	DirectX::XMFLOAT3A position;
	DirectX::XMFLOAT3A forward;
	DirectX::XMFLOAT3A up;

	float aspectRatio;
	float viewDistance;
	float yaw;
	float pitch;

	bool dragActive;

	RaytracingCamera(DirectX::XMFLOAT3A position_, float aspectRatio_,
		float viewDistance_, float yaw_, float pitch_);

	void Update(const InputHandler* inputHandler, float deltaTime);
};