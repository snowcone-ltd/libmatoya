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
uniform ivec4 icb; // planes, rotation, conversion

void yuv_to_rgba(int conversion, float y, float u, float v, out vec4 rgba)
{
	// 10-bit -> 16-bit
	if (conversion == 2 || conversion == 8) {
		y = y * 64.0;
		u = u * 64.0;
		v = v * 64.0;
	}

	// Full range
	if (conversion == 4 || conversion == 8) {
		u = u - 0.5;
		v = v - 0.5;

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

	rgba = vec4(r, g, b, 1.0);
}

void sample_rgba(int planes, int conversion, vec2 uv, out vec4 rgba)
{
	if (planes == 2) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex1, uv).r;
		float v = texture2D(tex1, uv).g;

		yuv_to_rgba(conversion, y, u, v, rgba);

	} else if (planes == 3) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex1, uv).r;
		float v = texture2D(tex2, uv).r;

		yuv_to_rgba(conversion, y, u, v, rgba);

	} else if (conversion != 0) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex0, uv).g;
		float v = texture2D(tex0, uv).b;

		yuv_to_rgba(conversion, y, u, v, rgba);

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

	int planes = icb[0];
	int rotation = icb[1];
	int conversion = icb[2];

	// Rotate
	vec2 uv = vs_texcoord;
	rotate(rotation, uv);

	// Sharpen
	for (int x = 0; x < 2; x++)
		if (effects[x] == 2.0)
			sharpen(width, height, levels[x], uv);

	// Sample
	sample_rgba(planes, conversion, uv, gl_FragColor);

	// Effects
	for (int y = 0; y < 2; y++)
		if (effects[y] == 1.0)
			scanline(vs_texcoord.y, vp_height, levels[y], gl_FragColor);
}
