float spow(float x, float p)
{
	return sign(x) * pow(abs(x), p);
}

float3 spow3(float3 v, float p)
{
	return float3(spow(v.x, p), spow(v.y, p), spow(v.z, p));
}

static const float PQ_m_1 = 2610.0f / 4096.0f / 4.0f;
static const float PQ_m_1_d = 1.0f / PQ_m_1;
static const float PQ_m_2 = 2523.0f / 4096.0f * 128.0f;
static const float PQ_m_2_d = 1.0f / PQ_m_2;
static const float PQ_c_1 = 3424.0f / 4096.0f;
static const float PQ_c_2 = 2413.0f / 4096.0f * 32.0f;
static const float PQ_c_3 = 2392.0f / 4096.0f * 32.0f;

static const float HDR10_MAX_NITS = 10000.0f;

float3 rec2020_pq_to_rec2020_linear(float3 color, float sdr_max_nits)
{
	// Apply the PQ EOTF (SMPTE ST 2084-2014) in order to linearize it
	// Courtesy of https://github.com/colour-science/colour/blob/38782ac059e8ddd91939f3432bf06811c16667f0/colour/models/rgb/transfer_functions/st_2084.py#L126

	float3 V_p = spow3(color, PQ_m_2_d);

	float3 n = max(0, V_p - PQ_c_1);

	float3 L = spow3(n / (PQ_c_2 - PQ_c_3 * V_p), PQ_m_1_d);
	float3 C = L * HDR10_MAX_NITS / sdr_max_nits;

	return C;
}

float3 rec2020_linear_to_rec2020_pq(float3 color, float sdr_max_nits)
{
	// Apply the inverse of the PQ EOTF (SMPTE ST 2084-2014) in order to encode the signal as PQ
	// Courtesy of https://github.com/colour-science/colour/blob/38782ac059e8ddd91939f3432bf06811c16667f0/colour/models/rgb/transfer_functions/st_2084.py#L56

	float3 Y_p = spow3(max(0.0f, (color / HDR10_MAX_NITS) * sdr_max_nits), PQ_m_1);

	float3 N = spow3((PQ_c_1 + PQ_c_2 * Y_p) / (PQ_c_3 * Y_p + 1.0f), PQ_m_2);

	return N;
}
