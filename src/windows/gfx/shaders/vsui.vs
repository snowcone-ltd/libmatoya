// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

cbuffer vertex_buffer : register(b0) {
	float4x4 proj;
};

struct VS_INPUT {
	float2 pos : POSITION;
	float4 col : COLOR0;
	float2 uv  : TEXCOORD0;
};

struct PS_INPUT {
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv  : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
	PS_INPUT output;
	output.pos = mul(proj, float4(input.pos.xy, 0.0f, 1.0f));
	output.col = input.col;
	output.uv  = input.uv;

	return output;
}
