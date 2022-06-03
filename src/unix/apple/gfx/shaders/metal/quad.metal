// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
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
	uint planes;
	uint rotation;
	uint conversion;
	uint pad1;
};

vertex struct vs_out vs(struct vtx v [[stage_in]])
{
	struct vs_out out;
	out.position = float4(float2(v.position), 0.0, 1.0);
	out.texcoord = v.texcoord;

	return out;
}

static float4 yuv_to_rgba(uint conversion, float y, float u, float v)
{
	// 10-bit -> 16-bit
	if (conversion & 0x4) {
		y *= 64.0;
		u *= 64.0;
		v *= 64.0;
	}

	// Full range
	if (conversion & 0x1) {
		u -= (128.0 / 255.0);
		v -= (128.0 / 255.0);

	// Limited
	} else {
		y = (y - 16.0 / 255.0) * (255.0 / 219.0);
		u = (u - 128.0 / 255.0) * (255.0 / 224.0);
		v = (v - 128.0 / 255.0) * (255.0 / 224.0);
	}

	float kr = 0.2126;
	float kb = 0.0722;

	float r = y + (2.0 - 2.0 * kr) * v;
	float b = y + (2.0 - 2.0 * kb) * u;
	float g = (y - kr * r - kb * b) / (1.0 - kr - kb);

	return float4(r, g, b, 1.0);
}

static float4 sample_rgba(uint planes, uint conversion,
	texture2d<float, access::sample> tex0, texture2d<float, access::sample> tex1,
	texture2d<float, access::sample> tex2, sampler s, float2 uv)
{
	float4 pixel0 = tex0.sample(s, uv);

	if (planes == 2) {
		float4 pixel1 = tex1.sample(s, uv);
		float y = pixel0.r;
		float u = pixel1.r;
		float v = pixel1.g;

		return yuv_to_rgba(conversion, y, u, v);

	} else if (planes == 3) {
		float y = pixel0.r;
		float u = tex1.sample(s, uv).r;
		float v = tex2.sample(s, uv).r;

		return yuv_to_rgba(conversion, y, u, v);

	} else if (conversion & 0x8) {
		float y = pixel0.r;
		float u = pixel0.g;
		float v = pixel0.b;

		return yuv_to_rgba(conversion, y, u, v);

	} else {
		return pixel0;
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
	float4 rgba = sample_rgba(cb.planes, cb.conversion, tex0, tex1, tex2, s, uv);

	// Effects
	for (uint y = 0; y < 2; y++)
		if (cb.effects[y] == 1)
			scanline(in.texcoord.y, cb.vp_height, cb.levels[y], rgba);

	return rgba;
}
