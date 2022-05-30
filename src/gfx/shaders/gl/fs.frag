// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#ifdef GL_ES
	#ifdef GL_FRAGMENT_PRECISION_HIGH
		precision highp float;
	#else
		precision mediump float;
	#endif
#endif

varying vec2 vs_texcoord;

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;

uniform vec4 fcb0; // width, height, vp_height
uniform vec4 fcb1; // effects, levels
uniform ivec4 icb; // format, rotation

void yuv_to_rgba(float y, float u, float v, out vec4 rgba)
{
	// Using "RGB to YCbCr color conversion for HDTV" (ITU-R BT.709)

	y = (y - 0.0625) * 1.164;
	u = u - 0.5;
	v = v - 0.5;

	float r = y + 1.793 * v;
	float g = y - 0.213 * u - 0.533 * v;
	float b = y + 2.112 * u;

	rgba = vec4(r, g, b, 1.0);
}

void sample_rgba(int format, vec2 uv, out vec4 rgba)
{
	// NV12, NV16
	if (format == 2 || format == 5) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex1, uv).r;
		float v = texture2D(tex1, uv).g;

		yuv_to_rgba(y, u, v, rgba);

	// I420, I444
	} else if (format == 3 || format == 4) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex1, uv).r;
		float v = texture2D(tex2, uv).r;

		yuv_to_rgba(y, u, v, rgba);

	// AYUV
	} else if (format == 8) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex0, uv).g;
		float v = texture2D(tex0, uv).b;

		yuv_to_rgba(y, u, v, rgba);

	// BGRA
	} else {
		rgba = texture2D(tex0, uv);
	}
}

void sharpen(float w, float h, float level, inout vec2 uv)
{
	vec2 res = vec2(w, h);
	vec2 p = uv * res;
	vec2 c = floor(p) + 0.5;
	vec2 dist = p - c;

	if (level >= 0.5) {
		dist = 16.0 * dist * dist * dist * dist * dist;

	} else {
		dist = 4.0 * dist * dist * dist;
	}

	uv = (c + dist) / res;
}

void scanline(float y, float h, float level, inout vec4 rgba)
{
	float n = floor(h / 240.0);

	if (mod(floor(y * h), n) < n / 2.0)
		rgba *= level;
}

void rotate(int rotation, inout vec2 uv)
{
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
}

void main(void)
{
	// Uniforms
	float width = fcb0[0];
	float height = fcb0[1];
	float vp_height = fcb0[2];

	vec2 effects = vec2(fcb1[0], fcb1[1]);
	vec2 levels = vec2(fcb1[2], fcb1[3]);

	int format = icb[0];
	int rotation = icb[1];

	// Rotate
	vec2 uv = vs_texcoord;
	rotate(rotation, uv);

	// Sharpen
	for (int x = 0; x < 2; x++)
		if (effects[x] == 2.0)
			sharpen(width, height, levels[x], uv);

	// Sample
	sample_rgba(format, uv, gl_FragColor);

	// Effects
	for (int y = 0; y < 2; y++)
		if (effects[y] == 1.0)
			scanline(vs_texcoord.y, vp_height, levels[y], gl_FragColor);
}
