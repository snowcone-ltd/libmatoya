// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#ifdef GL_ES
	precision mediump float;
#endif

attribute vec4 position;
attribute vec2 texcoord;

varying vec2 vs_texcoord;

void main(void)
{
	vs_texcoord = texcoord;
	gl_Position = position;
}
