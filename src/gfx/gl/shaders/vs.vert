// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

attribute vec2 position;
attribute vec2 texcoord;

varying vec2 frag_texcoord;

void main(void)
{
	gl_Position = vec4(position.xy, 0, 1);;

	frag_texcoord = texcoord;
}
