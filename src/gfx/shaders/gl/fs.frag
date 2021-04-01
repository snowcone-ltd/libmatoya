// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#ifdef GL_ES
	precision mediump float;
#endif

varying vec2 vs_texcoord;

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;

// scale, width, height, vp_height
uniform vec4 fcb;

// filter, effect, format
uniform ivec4 icb;

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

void gaussian(int type, float w, float h, inout vec2 uv)
{
	vec2 res = vec2(w, h);
	vec2 p = uv * res;
	vec2 c = floor(p) + 0.5;
	vec2 dist = p - c;

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

void scanline(int effect, float y, float h, inout vec4 rgba)
{
	float n = (effect == 1) ? 1.0 : 2.0;

	if (mod(floor(y * h), n * 2.0) < n)
		rgba *= 0.7;
}

void calc_uv(int rotation, float scale, vec2 texcoord, out vec2 uv)
{
	float x = 0.0;
	float y = 0.0;
	float xscale = scale;
	float yscale = 1.0;

	if (rotation == 2)
		x = xscale - 1.0;

	if (rotation == 3)
		y = xscale - 1.0;

	if (rotation == 1 || rotation == 3) {
		yscale = xscale;
		xscale = 1.0;
	}

	uv = vec2(
		(vs_texcoord[0] - x) * xscale,
		(vs_texcoord[1] - y) * yscale
	);
}

void main(void)
{
	float scale = fcb[0];
	float width = fcb[1];
	float height = fcb[2];
	float vp_height = fcb[3];

	int filter = icb[0];
	int effect = icb[1];
	int format = icb[2];
	int rotation = icb[3];

	vec2 uv;
	calc_uv(rotation, scale, vs_texcoord, uv);

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

	// Gaussian
	if (filter == 3 || filter == 4)
		gaussian(filter, width, height, uv);

	// NV12, NV16
	if (format == 2 || format == 5) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex1, uv).r;
		float v = texture2D(tex1, uv).g;

		yuv_to_rgba(y, u, v, gl_FragColor);

	// I420, I444
	} else if (format == 3 || format == 4) {
		float y = texture2D(tex0, uv).r;
		float u = texture2D(tex1, uv).r;
		float v = texture2D(tex2, uv).r;

		yuv_to_rgba(y, u, v, gl_FragColor);

	// RGBA, BGRA
	} else {
		gl_FragColor = texture2D(tex0, uv);
	}

	// Scanlines
	if (effect == 1 || effect == 2)
		scanline(effect, vs_texcoord[1], vp_height, gl_FragColor);
}
