#include "PCH.h"
#include "RaytracingCamera.h"

using namespace DirectX;

RaytracingCamera::RaytracingCamera(XMFLOAT3A position_, float aspectRatio_, float viewDistance_, float yaw_, float pitch_) :
	position(position_),
	up({ 0.0f, 1.0f, 0.0 }),
	aspectRatio(aspectRatio_),
	viewDistance(viewDistance_),
	yaw(yaw_),
	pitch(pitch_),
	projection(),
	dragActive(false)
{
	projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspectRatio, 1.0f, viewDistance);

	DirectX::XMVECTOR tempForward;

	forward.x = cosf(XMConvertToRadians(yaw)) * cosf(XMConvertToRadians(pitch));
	forward.y = sinf(XMConvertToRadians(pitch));
	forward.z = sinf(XMConvertToRadians(yaw)) * cosf(XMConvertToRadians(pitch));
	tempForward = XMLoadFloat3A(&forward);
	tempForward = XMVector3Normalize(tempForward);
	XMStoreFloat3A(&forward, tempForward);

	view = XMMatrixLookAtLH(XMLoadFloat3A(&position), 
		XMVectorAdd(XMLoadFloat3A(&position), tempForward), XMLoadFloat3A(&up));
}

void RaytracingCamera::Update(const InputHandler* inputHandler, float deltaTime)
{
	if (inputHandler->mouseDrag.right.active && !dragActive)
	{
		dragActive = true;
		ShowCursor(false);
	}
	else if (!inputHandler->mouseDrag.right.active && dragActive)
	{
		dragActive = false;
		// Set the cursor back to the cached position
		SetCursorPos(inputHandler->mouseDrag.right.cachedPos.x, inputHandler->mouseDrag.right.cachedPos.y);
		ShowCursor(true);
	}

	if (inputHandler->mouseDrag.right.active || inputHandler->keyState[0x41] || inputHandler->keyState[0x44] ||
		inputHandler->keyState[0x53] || inputHandler->keyState[0x57])
	{

		const float cameraSpeed = 30.0f * deltaTime;

		// 'W'
		if (inputHandler->keyState[0x57])
		{
			XMVECTOR v = XMLoadFloat3A(&position);
			v = XMVectorAdd(v, XMVectorScale(XMLoadFloat3A(&forward), cameraSpeed));
			XMStoreFloat3A(&position, v);
		}
		// 'S'
		if (inputHandler->keyState[0x53])
		{
			XMVECTOR v = XMLoadFloat3A(&position);
			v = XMVectorSubtract(v, XMVectorScale(XMLoadFloat3A(&forward), cameraSpeed));
			XMStoreFloat3A(&position, v);
		}
		// 'D'
		if (inputHandler->keyState[0x44])
		{
			XMVECTOR v = XMLoadFloat3A(&position);
			v = XMVectorSubtract(v, XMVectorScale(XMVector3Normalize(XMVector3Cross(
				XMLoadFloat3A(&forward), { 0.0f, 1.0f, 0.0f })), cameraSpeed));

			XMStoreFloat3A(&position, v);
		}
		// 'A'
		if (inputHandler->keyState[0x41])
		{
			XMVECTOR v = XMLoadFloat3A(&position);
			v = XMVectorAdd(v, XMVectorScale(XMVector3Normalize(
				XMVector3Cross(XMLoadFloat3A(&forward), { 0.0f, 1.0f, 0.0f })), cameraSpeed));

			XMStoreFloat3A(&position, v);
		}

		if (inputHandler->mouseDrag.right.active)
		{
			const float dragSpeed = 50.0f * deltaTime;

			if (inputHandler->mouseDelta.x != 0)
			{
				yaw -= inputHandler->mouseDelta.x * dragSpeed;
			}
			if (inputHandler->mouseDelta.y != 0)
			{
				pitch -= inputHandler->mouseDelta.y * dragSpeed;
				// Put constraints on pitch.
				// 1.5533rad is approx 89deg
				if (pitch > 89.0f)
				{
					pitch = 89.0f;
				}
				if (pitch < -89.0f)
				{
					pitch = -89.0f;
				}
			}

			// Update camera front (direction)
			XMVECTOR tempForward;
			forward.x = cosf(XMConvertToRadians(yaw)) * cosf(XMConvertToRadians(pitch));
			forward.y = sinf(XMConvertToRadians(pitch));
			forward.z = sinf(XMConvertToRadians(yaw)) * cosf(XMConvertToRadians(pitch));
			tempForward = XMLoadFloat3A(&forward);
			tempForward = XMVector3Normalize(tempForward);
			XMStoreFloat3A(&forward, tempForward);
		}
		// Update view matrix
		view = XMMatrixLookAtLH(XMLoadFloat3A(&position),
			XMVectorAdd(XMLoadFloat3A(&position), XMLoadFloat3A(&forward)), XMLoadFloat3A(&up));
	}
}

