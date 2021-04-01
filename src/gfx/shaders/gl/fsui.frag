// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#ifdef GL_ES
	precision mediump float;
#endif

uniform sampler2D tex;

varying vec2 frag_uv;
varying vec4 frag_col;

void main(void)
{
	gl_FragColor = frag_col * texture2D(tex, frag_uv.st);
}
