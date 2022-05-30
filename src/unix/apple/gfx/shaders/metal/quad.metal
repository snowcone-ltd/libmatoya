// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include <metal_stdlib>

using namespace metal;

struct vtx {
	float2 position [[attribute(0)]];
	float2 texcoord [[attribute(1)]];
};

struct vs_out {
	float4 position [[position]];
	float2 texcoord;
};

struct cb {
	float width;
	float height;
	float vp_height;
	float pad0;
	uint4 effects;
	float4 levels;
	uint format;
	uint rotation;
	uint2 pad1;
};

vertex struct vs_out vs(struct vtx v [[stage_in]])
{
	struct vs_out out;
	out.position = float4(float2(v.position), 0.0, 1.0);
	out.texcoord = v.texcoord;

	return out;
}

static float4 yuv_to_rgba(float y, float u, float v)
{
	// Using "RGB to YCbCr color conversion for HDTV" (ITU-R BT.709)

	y = (y - 0.0625) * 1.164;
	u = u - 0.5;
	v = v - 0.5;

	float r = y + 1.793 * v;
	float g = y - 0.213 * u - 0.533 * v;
	float b = y + 2.112 * u;

	return float4(r, g, b, 1.0);
}

static float4 sample_rgba(uint format, texture2d<float, access::sample> tex0,
	texture2d<float, access::sample> tex1, texture2d<float, access::sample> tex2,
	sampler s, float2 uv)
{
	// NV12, NV16
	if (format == 2 || format == 5) {
		float y = tex0.sample(s, uv).r;
		float u = tex1.sample(s, uv).r;
		float v = tex1.sample(s, uv).g;

		return yuv_to_rgba(y, u, v);

	// I420, I444
	} else if (format == 3 || format == 4) {
		float y = tex0.sample(s, uv).r;
		float u = tex1.sample(s, uv).r;
		float v = tex2.sample(s, uv).r;

		return yuv_to_rgba(y, u, v);

	// AYUV
	} else if (format == 8) {
		float y = tex0.sample(s, uv).r;
		float u = tex0.sample(s, uv).g;
		float v = tex0.sample(s, uv).b;

		return yuv_to_rgba(y, u, v);

	// BGRA
	} else {
		return tex0.sample(s, uv);
	}
}

static void sharpen(float w, float h, float level, thread float2 &uv)
{
	float2 res = float2(w, h);
	float2 p = uv * res;
	float2 c = floor(p) + 0.5;
	float2 dist = p - c;

	if (level >= 0.5) {
		dist = 16.0 * dist * dist * dist * dist * dist;

	} else {
		dist = 4.0 * dist * dist * dist;
	}

	uv = (c + dist) / res;
}

static void scanline(float y, float h, float level, thread float4 &rgba)
{
	float n = floor(h / 240.0);

	if (fmod(floor(y * h), n) < n / 2.0)
		rgba *= level;
}

static float2 rotate(uint rotation, float2 texcoord)
{
	float2 uv = texcoord;

	// Rotation
	if (rotation == 1 || rotation == 3) {
		float tmp = uv[0];
		uv[0] = uv[1];
		uv[1] = tmp;
	}

	// Flipped vertically
	if (rotation == 1 || rotation == 2)
		uv[1] = 1.0 - uv[1];

	// Flipped horizontally
	if (rotation == 2 || rotation == 3)
		uv[0] = 1.0 - uv[0];

	return uv;
}

fragment float4 fs(
	struct vs_out in [[stage_in]],
	texture2d<float, access::sample> tex0 [[texture(0)]],
	texture2d<float, access::sample> tex1 [[texture(1)]],
	texture2d<float, access::sample> tex2 [[texture(2)]],
	constant struct cb &cb [[buffer(0)]],
	sampler s [[sampler(0)]]
) {
	// Rotate
	float2 uv = rotate(cb.rotation, in.texcoord);

	// Sharpen
	for (uint x = 0; x < 2; x++)
		if (cb.effects[x] == 2)
			sharpen(cb.width, cb.height, cb.levels[x], uv);

	// Sample
	float4 rgba = sample_rgba(cb.format, tex0, tex1, tex2, s, uv);

	// Effects
	for (uint y = 0; y < 2; y++)
		if (cb.effects[y] == 1)
			scanline(in.texcoord.y, cb.vp_height, cb.levels[y], rgba);

	return rgba;
}
