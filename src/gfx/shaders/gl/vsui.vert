// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#ifdef GL_ES
	precision mediump float;
#endif

uniform mat4 proj;

attribute vec2 pos;
attribute vec2 uv;
attribute vec4 col;

varying vec2 frag_uv;
varying vec4 frag_col;

void main(void)
{
	frag_uv = uv;
	frag_col = col;

	gl_Position = proj * vec4(pos.xy, 0, 1);
}
