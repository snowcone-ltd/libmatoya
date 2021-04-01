// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include <metal_stdlib>

using namespace metal;

struct Uniforms {
	float4x4 proj;
};

struct VertexIn {
	float2 pos   [[attribute(0)]];
	float2 uv	 [[attribute(1)]];
	uchar4 color [[attribute(2)]];
};

struct VertexOut {
	float4 pos [[position]];
	float2 uv;
	float4 color;
};

vertex VertexOut vs(VertexIn in [[stage_in]],
	constant Uniforms &uniforms [[buffer(1)]])
{
	VertexOut out;
	out.pos = uniforms.proj * float4(in.pos, 0, 1);
	out.uv = in.uv;
	out.color = float4(in.color) / float4(255.0);

	return out;
}

fragment half4 fs(VertexOut in [[stage_in]],
	texture2d<half, access::sample> texture [[texture(0)]])
{
	constexpr sampler linearSampler(coord::normalized,
		min_filter::linear, mag_filter::linear, mip_filter::linear);

	return half4(in.color) * texture.sample(linearSampler, in.uv);
}
