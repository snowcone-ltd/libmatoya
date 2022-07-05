// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod-ui.h"
GFX_UI_PROTOTYPES(_gl_)

#if !defined(MTY_GLUI_CLEAR_ALPHA)
	#define MTY_GLUI_CLEAR_ALPHA 1.0f
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "glproc.h"

#include "shaders/vsui.h"
#include "shaders/fsui.h"

struct gl_ui {
	GLuint prog;
	GLuint vs;
	GLuint fs;
	GLint loc_tex;
	GLint loc_proj;
	GLint loc_pos;
	GLint loc_uv;
	GLint loc_col;
	GLuint vb;
	GLuint eb;
};

static int32_t GL_UI_ORIGIN_Y;

void mty_gl_ui_set_origin_y(int32_t y)
{
	GL_UI_ORIGIN_Y = y;
}

static void gl_ui_log_shader_errors(GLuint shader)
{
	GLint n = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);

	if (n > 0) {
		char *log = MTY_Alloc(n, 1);

		glGetShaderInfoLog(shader, n, NULL, log);
		MTY_Log("%s", log);
		MTY_Free(log);
	}
}

struct gfx_ui *mty_gl_ui_create(MTY_Device *device)
{
	if (!glproc_global_init())
		return NULL;

	struct gl_ui *ctx = MTY_Alloc(1, sizeof(struct gl_ui));

	bool r = true;

	// Create vertex, fragment shaders
	GLint status = 0;
	const GLchar *vertex_shader[2] = {GL_SHADER_VERSION, VERT};
	ctx->vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(ctx->vs, 2, vertex_shader, NULL);
	glCompileShader(ctx->vs);
	glGetShaderiv(ctx->vs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gl_ui_log_shader_errors(ctx->vs);
		r = false;
		goto except;
	}

	const GLchar *fragment_shader[2] = {GL_SHADER_VERSION, FRAG};
	ctx->fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->fs, 2, fragment_shader, NULL);
	glCompileShader(ctx->fs);
	glGetShaderiv(ctx->fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gl_ui_log_shader_errors(ctx->fs);
		r = false;
		goto except;
	}

	ctx->prog = glCreateProgram();
	glAttachShader(ctx->prog, ctx->vs);
	glAttachShader(ctx->prog, ctx->fs);
	glLinkProgram(ctx->prog);

	glGetProgramiv(ctx->prog, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		MTY_Log("Program failed to link");
		r = false;
		goto except;
	}

	// Store uniform locations
	ctx->loc_proj = glGetUniformLocation(ctx->prog, "proj");
	ctx->loc_pos = glGetAttribLocation(ctx->prog, "pos");
	ctx->loc_uv = glGetAttribLocation(ctx->prog, "uv");
	ctx->loc_col = glGetAttribLocation(ctx->prog, "col");
	ctx->loc_tex = glGetUniformLocation(ctx->prog, "tex");

	// Pre create dynamically resizing vertex, index (element) buffers
	glGenBuffers(1, &ctx->vb);
	glGenBuffers(1, &ctx->eb);

	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		MTY_Log("'glGetError' returned %d", e);
		r = false;
		goto except;
	}

	except:

	if (!r)
		mty_gl_ui_destroy((struct gfx_ui **) &ctx, device);

	return (struct gfx_ui *) ctx;
}

bool mty_gl_ui_render(struct gfx_ui *gfx_ui, MTY_Device *device, MTY_Context *context,
	const MTY_DrawData *dd, MTY_Hash *cache, MTY_Surface *dest)
{
	struct gl_ui *ctx = (struct gl_ui *) gfx_ui;
	GLuint _dest = dest ? *((GLuint *) dest) : 0;

	int32_t fb_width = lrint(dd->displaySize.x);
	int32_t fb_height = lrint(dd->displaySize.y);

	// Prevent rendering under invalid scenarios
	if (fb_width <= 0 || fb_height <= 0 || dd->cmdListLength == 0)
		return false;

	// Update the vertex shader's proj data based on the current display size
	float L = 0;
	float R = dd->displaySize.x;
	float T = 0;
	float B = dd->displaySize.y;
	float proj[4][4] = {
		{2.0f,  0.0f,  0.0f,  0.0f},
		{0.0f,  2.0f,  0.0f,  0.0f},
		{0.0f,  0.0f, -1.0f,  0.0f},
		{0.0f,  0.0f,  0.0f,  1.0f},
	};

	proj[0][0] /= R - L;
	proj[1][1] /= T - B;
	proj[3][0] = (R + L) / (L - R);
	proj[3][1] = (T + B) / (B - T);

	// Bind texture to draw framebuffer
	if (_dest)
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _dest);

	// Set viewport based on display size
	glViewport(0, GL_UI_ORIGIN_Y, fb_width, fb_height);

	// Clear render target to black
	if (dd->clear) {
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0.0f, 0.0f, 0.0f, MTY_GLUI_CLEAR_ALPHA);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	// Set up rendering pipeline
	glUseProgram(ctx->prog);
	glUniform1i(ctx->loc_tex, 0);
	glUniformMatrix4fv(ctx->loc_proj, 1, GL_FALSE, &proj[0][0]);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glActiveTexture(GL_TEXTURE0);
	glEnableVertexAttribArray(ctx->loc_pos);
	glEnableVertexAttribArray(ctx->loc_uv);
	glEnableVertexAttribArray(ctx->loc_col);

	glVertexAttribPointer(ctx->loc_pos, 2, GL_FLOAT, GL_FALSE,
		sizeof(MTY_Vtx), (GLvoid *) offsetof(MTY_Vtx, pos));

	glVertexAttribPointer(ctx->loc_uv, 2, GL_FLOAT, GL_FALSE,
		sizeof(MTY_Vtx), (GLvoid *) offsetof(MTY_Vtx, uv));

	glVertexAttribPointer(ctx->loc_col, 4, GL_UNSIGNED_BYTE, GL_TRUE,
		sizeof(MTY_Vtx), (GLvoid *) offsetof(MTY_Vtx, col));

	// Draw
	for (uint32_t n = 0; n < dd->cmdListLength; n++) {
		MTY_CmdList *cmdList = &dd->cmdList[n];

		// Copy vertex, index buffer data
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) cmdList->vtxLength * sizeof(MTY_Vtx),
			cmdList->vtx, GL_STREAM_DRAW);

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmdList->idxLength * sizeof(uint16_t),
			cmdList->idx, GL_STREAM_DRAW);

		for (uint32_t i = 0; i < cmdList->cmdLength; i++) {
			MTY_Cmd *pcmd = &cmdList->cmd[i];

			// Use the clip to apply scissor
			MTY_Rect r = pcmd->clip;

			// Make sure the rect is actually in the viewport
			if (r.left < fb_width && r.top < fb_height && r.right >= 0.0f && r.bottom >= 0.0f) {

				// Adjust for origin (from lower left corner)
				r.top -= GL_UI_ORIGIN_Y;
				r.bottom -= GL_UI_ORIGIN_Y;

				glScissor(
					lrint(r.left),
					lrint(fb_height - r.bottom),
					lrint(r.right - r.left),
					lrint(r.bottom - r.top)
				);

				// Optionally sample from a texture (fonts, images)
				glBindTexture(GL_TEXTURE_2D, !pcmd->texture ? 0 :
					(GLuint) (uintptr_t) MTY_HashGetInt(cache, pcmd->texture));

				// Draw indexed
				glDrawElements(GL_TRIANGLES, pcmd->elemCount, GL_UNSIGNED_SHORT,
					(void *) (uintptr_t) (pcmd->idxOffset * sizeof(uint16_t)));
			}
		}
	}

	return true;
}

void *mty_gl_ui_create_texture(struct gfx_ui *gfx_ui, MTY_Device *device, const void *rgba,
	uint32_t width, uint32_t height)
{
	GLuint texture = 0;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	return (void *) (uintptr_t) texture;
}

void mty_gl_ui_destroy_texture(struct gfx_ui *gfx_ui, void **texture, MTY_Device *device)
{
	if (!texture || !*texture)
		return;

	GLuint utexture = (GLuint) (uintptr_t) *texture;
	glDeleteTextures(1, &utexture);

	*texture = NULL;
}

void mty_gl_ui_destroy(struct gfx_ui **gfx_ui, MTY_Device *device)
{
	if (!gfx_ui || !*gfx_ui)
		return;

	struct gl_ui *ctx = (struct gl_ui *) *gfx_ui;

	if (ctx->vb)
		glDeleteBuffers(1, &ctx->vb);

	if (ctx->eb)
		glDeleteBuffers(1, &ctx->eb);

	if (ctx->prog) {
		if (ctx->vs)
			glDetachShader(ctx->prog, ctx->vs);

		if (ctx->fs)
			glDetachShader(ctx->prog, ctx->fs);
	}

	if (ctx->vs)
		glDeleteShader(ctx->vs);

	if (ctx->fs)
		glDeleteShader(ctx->fs);

	if (ctx->prog)
		glDeleteProgram(ctx->prog);

	MTY_Free(ctx);
	*gfx_ui = NULL;
}
