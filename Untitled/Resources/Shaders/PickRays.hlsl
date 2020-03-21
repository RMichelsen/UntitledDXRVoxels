#define HLSL
#include "RaytracingSharedHlsl.h"

#include "Utils.hlsl"

// Global root signature, available to all shaders
GlobalRootSignature GlobalRootSignature =
{
	"SRV(t0, space = 0),"                                           // Scene BVH
	"RootConstants(num32BitConstants = 28, b1, space = 0)"			// Scene constants
};

// Root signatures and associations for the pick hit
LocalRootSignature PickHitLocalRootSignature =
{
	"DescriptorTable(SRV(t0, numDescriptors = 1, space = 1)),"     // Index buffer 
	"DescriptorTable(SRV(t1, numDescriptors = 1, space = 1)),"     // Vertex buffer
	"DescriptorTable(UAV(u0, numDescriptors = 1, space = 1)),"     // Pick buffer
};
TriangleHitGroup PickHitGroup =
{
	"",				// Any Hit
	"PickHit"		// Closest Hit
};
SubobjectToExportsAssociation PickHitAssoc =
{
	"PickHitLocalRootSignature",    // Subobject name
	"PickHit"						// Exports association
};

RaytracingShaderConfig ShaderConfig =
{
	16,  // Max payload size
	8    // Max attribute size
};

RaytracingPipelineConfig PipelineConfig =
{
	1   // Max trace recursion depth
};

StateObjectConfig StateObjectConfig =
{
	STATE_OBJECT_FLAGS_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITONS
};

// Global root signature
RaytracingAccelerationStructure g_SceneBVH : register(t0, RAYTRACING_GLOBAL_SPACE);
ConstantBuffer<RaytracingConstants> g_Constants : register(b1, RAYTRACING_GLOBAL_SPACE);

// Local root signature
StructuredBuffer<uint> l_Indices : register(t0, RAYTRACING_LOCAL_SPACE);
StructuredBuffer<Vertex> l_Vertices : register(t1, RAYTRACING_LOCAL_SPACE);
RWStructuredBuffer<uint> l_PickBuffer : register(u0, RAYTRACING_LOCAL_SPACE);

Vertex GetCurrentVertex()
{
	// Since our voxels don't change attributes across a triangle, 
	// we can simply return the values of the first of the three vertices
	uint startIndex = PrimitiveIndex() * 3;
	return l_Vertices[l_Indices[startIndex]];
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
void GeneratePickRay(out float3 origin, out float3 direction)
{
	float2 screenPos = float2(0.0f, 0.0f); // Center NDC

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_Constants.cameraProjectionToWorld);

	world.xyz /= world.w;
	origin = g_Constants.cameraPosition.xyz;
	direction = normalize(world.xyz - origin);
}

[shader("raygeneration")]
void PickGen()
{
	float3 origin;
	float3 direction;
	GeneratePickRay(origin, direction);

	PickHitInfo payload;
	payload.hitLocation = float4(-100.0f, -100.0f, -100.0f, -100.0f);

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.001;
	ray.TMax = 10000.0;

	TraceRay(g_SceneBVH, RAY_FLAG_NONE, 0xFF, 1, 0, 2, ray, payload);
}

[shader("closesthit")]
void PickHit(inout PickHitInfo payload, Attributes attrib)
{
	Vertex v = GetCurrentVertex();
	l_PickBuffer[0] = asuint(v.position.w);
	l_PickBuffer[1] = asuint(v.normal.w);
}

[shader("miss")]
void PickMiss(inout PickHitInfo payload : SV_RayPayload)
{
}