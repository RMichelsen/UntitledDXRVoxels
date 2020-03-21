#define PI 3.14159265359

float3 YxyToXYZ(float3 v)
{
	float X = v.x * (v.y / v.z);
	float Z = (X / v.y) - X - v.x;
	return float3(X, v.x, Z);
}

float3 XYZToSRGB(float3 v)
{
	float3x3 M = float3x3(3.2404542, -1.5371385, -0.4985314,
		-0.9692660, 1.8760108, 0.0415560,
		0.0556434, -0.2040259, 1.0572252);

	return mul(M, v);
}

void CalculateLuminanceDistribution(float T, out float3 A, out float3 B, out float3 C, out float3 D, out float3 E)
{
	A = float3(0.1787 * T - 1.4630, -0.0193 * T - 0.2592, -0.0167 * T - 0.2608);
	B = float3(-0.3554 * T + 0.4275, -0.0665 * T + 0.0008, -0.0950 * T + 0.0092);
	C = float3(-0.0227 * T + 5.3251, -0.0004 * T + 0.2125, -0.0079 * T + 0.2102);
	D = float3(0.1206 * T - 2.5771, -0.0641 * T - 0.8989, -0.0441 * T - 1.6537);
	E = float3(-0.0670 * T + 0.3703, -0.0033 * T + 0.0452, -0.0109 * T + 0.0529);
}

float3 CalculatePerezLuminance(float3 A, float3 B, float3 C, float3 D, float3 E, float theta, float gamma)
{
	return (1.0 + A * exp(B / (cos(theta) + 0.01f))) * (1.0 + C * exp(D * gamma) + E * (cos(gamma) * cos(gamma)));
}

float3 CalculateZenithLuminance(float T, float theta_s)
{
	float chi = (0.4444444 - T / 120) * (PI - 2 * theta_s);
	float Y_z = (4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192;

	float T2 = T * T;
	float theta_s2 = theta_s * theta_s;
	float theta_s3 = theta_s2 * theta_s;

	float3x4 x_constants = float3x4(0.00166, -0.00375, 0.00209, 0,
		-0.02903, 0.06377, -0.03202, 0.00394,
		0.11693, -0.21196, 0.06052, 0.25886);

	float3x4 y_constants = float3x4(0.00275, -0.00610, 0.00317, 0,
		-0.04214, 0.08970, -0.04153, 0.00516,
		0.15346, -0.26756, 0.06670, 0.26688);

	float3 turbidities = float3(T2, T, 1);
	float4 thetas = float4(theta_s3, theta_s2, theta_s, 1);

	float x_z = mul(turbidities, mul(x_constants, thetas));
	float y_z = mul(turbidities, mul(y_constants, thetas));

	return float3(Y_z, x_z, y_z);
}

float3 CalculateSkyLuminance(float3 V, float3 S, float T)
{
	float3 A, B, C, D, E;
	CalculateLuminanceDistribution(T, A, B, C, D, E);

	float3 up = float3(0, -1, 0);
	float mag_up = length(up);
	float mag_S = length(S);
	float mag_V = length(V);

	// Calculate angles using a.b = |a|*|b|*cos(x)
	float theta = acos(clamp(dot(V, up), 0.0, 1.0) / (mag_V * mag_up));
	float theta_s = acos(clamp(dot(S, up), 0.0, 1.0) / (mag_S * mag_up));
	float gamma = acos(clamp(dot(S, V), 0.0, 1.0) / (mag_S * mag_V));

	float3 Y = CalculateZenithLuminance(T, theta_s);
	float3 F_0 = CalculatePerezLuminance(A, B, C, D, E, theta, gamma);
	float3 F_1 = CalculatePerezLuminance(A, B, C, D, E, 0, theta_s);

	float3 Y_p = Y * (F_0 / F_1);

	return float3(XYZToSRGB(YxyToXYZ(Y_p)));
}