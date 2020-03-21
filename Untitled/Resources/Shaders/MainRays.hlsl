#define HLSL
#include "RaytracingSharedHlsl.h"

#include "Utils.hlsl"
#include "SkyModel.hlsl"

// Global root signature, available to all shaders
GlobalRootSignature GlobalRootSignature =
{
	"SRV(t0, space = 0),"                                           // Scene BVH
	"RootConstants(num32BitConstants = 28, b1, space = 0)"			// Scene constants
};
// Root signatures and associations for the main rays
LocalRootSignature MainGenLocalRootSignature =
{
	"DescriptorTable(UAV(u0, numDescriptors = 1, space = 1))"      // Output texture
};
LocalRootSignature MainHitLocalRootSignature =
{
	"DescriptorTable(SRV(t0, numDescriptors = 1, space = 1)),"     // Index buffer 
	"DescriptorTable(SRV(t1, numDescriptors = 1, space = 1)),"     // Vertex buffer
	"DescriptorTable(UAV(u1, numDescriptors = 1, space = 1)),"     // Pick buffer
};
TriangleHitGroup MainHitGroup =
{
	"MainAnyHit",   // Any Hit
	"MainHit"       // Closest Hit
};
SubobjectToExportsAssociation MainGenAssoc =
{
	"MainGenLocalRootSignature",    // Subobject name
	"MainGen"                       // Exports association
};
SubobjectToExportsAssociation MainHitAssoc =
{
	"MainHitLocalRootSignature",    // Subobject name
	"MainHitGroup"					// Exports association
};

RaytracingShaderConfig ShaderConfig =
{
	16,  // Max payload size
	8    // Max attribute size
};

RaytracingPipelineConfig PipelineConfig =
{
	2   // Max trace recursion depth
};

StateObjectConfig StateObjectConfig =
{
	STATE_OBJECT_FLAGS_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITONS
};

// Global root signature
RaytracingAccelerationStructure g_SceneBVH : register(t0, RAYTRACING_GLOBAL_SPACE);
ConstantBuffer<RaytracingConstants> g_Constants : register(b1, RAYTRACING_GLOBAL_SPACE);

// Local root signature inputs
RWTexture2D<float4> l_Output : register(u0, RAYTRACING_LOCAL_SPACE);
StructuredBuffer<uint> l_Indices : register(t0, RAYTRACING_LOCAL_SPACE);
StructuredBuffer<Vertex> l_Vertices : register(t1, RAYTRACING_LOCAL_SPACE);
RWStructuredBuffer<uint> l_PickBuffer : register(u1, RAYTRACING_LOCAL_SPACE);

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

	// Invert Y for DirectX-style coordinates.
	screenPos.y = -screenPos.y;

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1), g_Constants.cameraProjectionToWorld);

	world.xyz /= world.w;
	origin = g_Constants.cameraPosition.xyz;
	direction = normalize(world.xyz - origin);
}

float ShootShadowRay(float3 origin, float3 direction)
{
	ShadowHitInfo shadowPayload;
	shadowPayload.visibility = 0.0f;
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0;
	ray.TMax = 10000;
	TraceRay(g_SceneBVH, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0xFF, 1, 0, 1, ray, shadowPayload);
	return shadowPayload.visibility;
}

Vertex GetCurrentVertex()
{
	// Since our voxels don't change attributes across a triangle, 
	// we can simply return the values of the first of the three vertices
	uint startIndex = PrimitiveIndex() * 3;
	return l_Vertices[l_Indices[startIndex]];
}

[shader("raygeneration")]
void MainGen()
{
	// Quick "crosshair"
	float2 xy = DispatchRaysIndex() + 0.5f; // center in the middle of the pixel.
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;
	screenPos.y = -screenPos.y;
	if (distance(screenPos, float2(0, 0)) < 0.0075)
	{
		l_Output[DispatchRaysIndex().xy] = float4(1.0, 1.0, 0.0, 0.0);
		return;
	}


	MainHitInfo payload;

	float3 origin;
	float3 direction;
	GenerateCameraRay(DispatchRaysIndex().xy, origin, direction);

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.001;
	ray.TMax = 10000.0;

	TraceRay(g_SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	l_Output[DispatchRaysIndex().xy] = payload.color;
}

[shader("anyhit")]
void MainAnyHit(inout MainHitInfo payload, Attributes attrib)
{
	//Vertex vertex = GetCurrentVertex();
	//if ((uint)vertex.position.w == 1)
	//{
	//	IgnoreHit();
	//}
	//else
	//{
	//	AcceptHitAndEndSearch();
	//}
}

[shader("closesthit")]
void MainHit(inout MainHitInfo payload, Attributes attrib)
{
	Vertex vertex = GetCurrentVertex();

	float3 N = vertex.normal.xyz;
	float3 sun_dir = normalize(-g_Constants.sunDirection.xyz);
	
	// Compute the Lambertian term N.L
	float NdotL = saturate(dot(N, sun_dir));

	float3 origin = HitWorldPosition() + (0.0001 * N);

	// Shoot a shadow ray, visibility is 1 if it misses, 0 if it hits geometry
	float visibility = ShootShadowRay(origin, sun_dir);

	float3 shade_color = visibility * NdotL * float4(0.995, 0.6, 0.385, 1.0);
	payload.color = float4(shade_color, 1.0);

	if (asuint(vertex.position.w) == l_PickBuffer[0])
	{
		payload.color += (float4(0.995, 0.6, 0.385, 1.0) * 0.5);
	}
}

[shader("miss")]
void MainMiss(inout MainHitInfo payload : SV_RayPayload)
{
	// View and sun vec
	float3 ray_dir = normalize(-WorldRayDirection());
	float3 sun_dir = normalize(g_Constants.sunDirection.xyz);

	// Only render the sky above the horizon
	if (ray_dir.y > 0)
	{
		payload.color = float4(0.13, 0.13, 0.13, 1.0);
		return;
	}

	// Lazy approximation of sun disc, uses the dot product between the ray direction 
	// the direction of the sun
	float4 sun_color = float4(1.0, 0.98, 0.85, 1.0);
	float disc_strength = 0;
	float dot_product_radius = 0.0005;
	disc_strength = clamp(1 - dot(ray_dir, sun_dir), 0.0, 1.0);
	disc_strength = clamp(dot_product_radius - disc_strength, 0, dot_product_radius) / dot_product_radius;

	float turbidity = 2.0f;
	// Sample sky luminance according to the Preetham model for daylight
	float3 luminance = CalculateSkyLuminance(ray_dir, sun_dir, turbidity);
	luminance *= 0.05;
	luminance = float3(1.0f, 1.0f, 1.0f) - exp(-luminance * 2);

	payload.color = lerp(float4(luminance, 1.0), sun_color, disc_strength);
}

[shader("miss")]
void ShadowMiss(inout ShadowHitInfo payload : SV_RayPayload)
{
	payload.visibility = 1.0f;
}