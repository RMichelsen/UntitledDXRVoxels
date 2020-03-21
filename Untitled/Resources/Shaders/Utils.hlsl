#define PI 3.14159265359

// Retrieve hit world position.
float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Get a cosine-weighted random vector centered around a 
// specified normal, tangent and bitangent coordinate system
float3 getCosHemisphereSample(inout uint randSeed, float3 N, float3 NT, float3 NB)
{
	float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return NT * (r * cos(phi).x) + NB * (r * sin(phi)) + N * sqrt(1 - randVal.x);
}