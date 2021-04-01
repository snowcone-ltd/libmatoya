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
	uint filter;
	uint effect;
	uint format;
	uint rotation;
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

static void gaussian(uint type, float w, float h, thread float2 &uv)
{
	float2 res = float2(w, h);
	float2 p = uv * res;
	float2 c = floor(p) + 0.5;
	float2 dist = p - c;

	// Sharp
	if (type == 3) {
		dist = 16.0 * dist * dist * dist * dist * dist;

	// Soft
	} else {
		dist = 4.0 * dist * dist * dist;
	}

	p = c + dist;

	uv = p / res;
}

static void scanline(uint effect, float y, float h, thread float4 &rgba)
{
	float n = (effect == 1) ? 1.0 : 2.0;

	if (fmod(floor(y * h), n * 2.0) < n)
		rgba *= 0.7;
}

fragment float4 fs(
	struct vs_out in [[stage_in]],
	texture2d<float, access::sample> tex0 [[texture(0)]],
	texture2d<float, access::sample> tex1 [[texture(1)]],
	texture2d<float, access::sample> tex2 [[texture(2)]],
	constant struct cb &cb [[buffer(0)]],
	sampler s [[sampler(0)]]
) {
	float4 rgba = 0.0;
	float2 uv = in.texcoord;

	// Rotation
	if (cb.rotation == 1 || cb.rotation == 3) {
		float tmp = uv[0];
		uv[0] = uv[1];
		uv[1] = tmp;
	}

	// Flipped vertically
	if (cb.rotation == 1 || cb.rotation == 2)
		uv[1] = 1.0 - uv[1];

	// Flipped horizontally
	if (cb.rotation == 2 || cb.rotation == 3)
		uv[0] = 1.0 - uv[0];

	// Gaussian
	if (cb.filter == 3 || cb.filter == 4)
		gaussian(cb.filter, cb.width, cb.height, uv);

	// NV12, NV16
	if (cb.format == 2 || cb.format == 5) {
		float y = tex0.sample(s, uv).r;
		float u = tex1.sample(s, uv).r;
		float v = tex1.sample(s, uv).g;

		rgba = yuv_to_rgba(y, u, v);

	// I420, I444
	} else if (cb.format == 3 || cb.format == 4) {
		float y = tex0.sample(s, uv).r;
		float u = tex1.sample(s, uv).r;
		float v = tex2.sample(s, uv).r;

		rgba = yuv_to_rgba(y, u, v);

	// RGBA, BGRA
	} else {
		rgba = tex0.sample(s, uv);
	}

	// Scanlines
	if (cb.effect == 1 || cb.effect == 2)
		scanline(cb.effect, in.texcoord.y, cb.vp_height, rgba);

	return rgba;
}
