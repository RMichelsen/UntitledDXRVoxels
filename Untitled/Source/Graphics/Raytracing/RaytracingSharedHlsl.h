#if defined(HLSL)
#include "HlslCompat.hlsl"
#ifndef RAYTRACING_GLOBAL_SPACE
#define RAYTRACING_GLOBAL_SPACE space0
#endif

#ifndef RAYTRACING_LOCAL_SPACE
#define RAYTRACING_LOCAL_SPACE space1
#endif
#else 
#pragma once
using namespace DirectX;
#ifndef RAYTRACING_GLOBAL_SPACE
#define RAYTRACING_GLOBAL_SPACE 0
#endif

#ifndef RAYTRACING_LOCAL_SPACE
#define RAYTRACING_LOCAL_SPACE 1
#endif
#endif

struct RaytracingConstants
{
	XMMATRIX cameraProjectionToWorld;
	XMVECTOR cameraPosition;
	XMVECTOR sunDirection;

	int width;
	int height;
	int framecount;
};

struct Vertex
{
	XMVECTOR position;
	XMVECTOR normal;
};


// Hit information for the main rays
struct MainHitInfo
{
	XMVECTOR color;
};

// Hit information for shadow rays
struct ShadowHitInfo
{
	float visibility;
};

// Hit information for the pick ray
struct PickHitInfo
{
	XMVECTOR hitLocation;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes
{
	XMFLOAT2 bary;
};