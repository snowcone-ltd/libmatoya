// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "gfx/mod.h"
GFX_PROTOTYPES(_gl_)

#include <stdio.h>

#include "glproc.h"
#include "gfx/viewport.h"
#include "gfx/gl/fmt-gl.h"
#include "gfx/fmt.h"

#include "shaders/vs.h"
#include "shaders/fs.h"

#define GL_NUM_STAGING 3

struct gl_rtv {
	GLenum format;
	GLuint texture;
	GLuint fb;
	uint32_t w;
	uint32_t h;
};

struct gl {
	MTY_ColorFormat format;
	struct gl_rtv staging[GL_NUM_STAGING];

	GLuint vs;
	GLuint fs;
	GLuint prog;
	GLuint vb;
	GLuint eb;

	GLuint loc_tex[GL_NUM_STAGING];
	GLuint loc_pos;
	GLuint loc_uv;
	GLuint loc_fcb0;
	GLuint loc_fcb1;
	GLuint loc_icb;
};

static int32_t GL_ORIGIN_Y;

void mty_gl_set_origin_y(int32_t y)
{
	GL_ORIGIN_Y = y;
}

static void gl_log_shader_errors(GLuint shader)
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

struct gfx *mty_gl_create(MTY_Device *device)
{
	if (!glproc_global_init())
		return NULL;

	struct gl *ctx = MTY_Alloc(1, sizeof(struct gl));

	bool r = true;

	GLint status = GL_FALSE;
	const GLchar *vs[2] = {GL_SHADER_VERSION, VERT};
	ctx->vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(ctx->vs, 2, vs, NULL);
	glCompileShader(ctx->vs);
	glGetShaderiv(ctx->vs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gl_log_shader_errors(ctx->vs);
		r = false;
		goto except;
	}

	const GLchar *fs[2] = {GL_SHADER_VERSION, FRAG};
	ctx->fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->fs, 2, fs, NULL);
	glCompileShader(ctx->fs);
	glGetShaderiv(ctx->fs, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gl_log_shader_errors(ctx->fs);
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

	ctx->loc_pos = glGetAttribLocation(ctx->prog, "position");
	ctx->loc_uv = glGetAttribLocation(ctx->prog, "texcoord");
	ctx->loc_fcb0 = glGetUniformLocation(ctx->prog, "fcb0");
	ctx->loc_fcb1 = glGetUniformLocation(ctx->prog, "fcb1");
	ctx->loc_icb = glGetUniformLocation(ctx->prog, "icb");

	for (uint8_t x = 0; x < GL_NUM_STAGING; x++) {
		char name[32];
		snprintf(name, 32, "tex%u", x);
		ctx->loc_tex[x] = glGetUniformLocation(ctx->prog, name);
	}

	glGenBuffers(1, &ctx->vb);
	GLfloat vertices[] = {
		-1.0f,  1.0f,	// Position 0
		 0.0f,  0.0f,	// TexCoord 0
		-1.0f, -1.0f,	// Position 1
		 0.0f,  1.0f,	// TexCoord 1
		 1.0f, -1.0f,	// Position 2
		 1.0f,  1.0f,	// TexCoord 2
		 1.0f,  1.0f,	// Position 3
		 1.0f,  0.0f	// TexCoord 3
	};

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &ctx->eb);
	GLshort elements[] = {
		0, 1, 2,
		2, 3, 0
	};

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	GLenum e = glGetError();
	if (e != GL_NO_ERROR) {
		MTY_Log("'glGetError' returned %d", e);
		r = false;
		goto except;
	}

	except:

	if (!r)
		mty_gl_destroy((struct gfx **) &ctx);

	return (struct gfx *) ctx;
}

static void gl_rtv_destroy(struct gl_rtv *rtv)
{
	if (rtv->texture) {
		glDeleteTextures(1, &rtv->texture);
		rtv->texture = 0;
	}
}

static bool gl_refresh_resource(struct gfx *gfx, MTY_Device *device, MTY_Context *context, MTY_ColorFormat fmt,
	uint8_t plane, const uint8_t *image, uint32_t full_w, uint32_t w, uint32_t h, uint8_t bpp)
{
	struct gl *ctx = (struct gl *) gfx;

	struct gl_rtv *rtv = &ctx->staging[plane];
	GLenum format = FMT_PLANES[fmt][plane][1];
	GLenum type = FMT_PLANES[fmt][plane][2];

	// Resize texture
	if (!rtv->texture || rtv->w != w || rtv->h != h || rtv->format != format) {
		GLenum internal = FMT_PLANES[fmt][plane][0];

		gl_rtv_destroy(rtv);

		glGenTextures(1, &rtv->texture);
		glBindTexture(GL_TEXTURE_2D, rtv->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, NULL);

		rtv->w = w;
		rtv->h = h;
		rtv->format = format;
	}

	// Upload
	glBindTexture(GL_TEXTURE_2D, rtv->texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, full_w);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, format, type, image);

	return true;
}

bool mty_gl_render(struct gfx *gfx, MTY_Device *device, MTY_Context *context,
	const void *image, const MTY_RenderDesc *desc, MTY_Surface *dest)
{
	struct gl *ctx = (struct gl *) gfx;
	GLuint _dest = dest ? *((GLuint *) dest) : 0;

	// Don't do anything until we have real data
	if (desc->format != MTY_COLOR_FORMAT_UNKNOWN)
		ctx->format = desc->format;

	if (ctx->format == MTY_COLOR_FORMAT_UNKNOWN)
		return true;

	// Refresh staging texture dimensions
	if (!fmt_reload_textures(gfx, NULL, NULL, image, desc, gl_refresh_resource))
		return false;

	// Viewport
	float vpx, vpy, vpw, vph;
	mty_viewport(desc, &vpx, &vpy, &vpw, &vph);

	glViewport(lrint(vpx), lrint(vpy) + GL_ORIGIN_Y, lrint(vpw), lrint(vph));

	// Begin render pass (set destination texture if available)
	if (_dest)
		glBindFramebuffer(GL_FRAMEBUFFER, _dest);

	// Context state, set vertex and fragment shaders
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glUseProgram(ctx->prog);

	// Vertex shader
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vb);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->eb);
	glEnableVertexAttribArray(ctx->loc_pos);
	glEnableVertexAttribArray(ctx->loc_uv);
	glVertexAttribPointer(ctx->loc_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
	glVertexAttribPointer(ctx->loc_uv,  2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *) (2 * sizeof(GLfloat)));

	// Clear
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Fragment shader
	for (uint8_t x = 0; x < GL_NUM_STAGING; x++) {
		if (ctx->staging[x].texture) {
			GLint filter = desc->filter == MTY_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;

			glActiveTexture(GL_TEXTURE0 + x);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[x].texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glUniform1i(ctx->loc_tex[x], x);
		}
	}

	// Uniforms
	glUniform4f(ctx->loc_fcb0, (GLfloat) desc->cropWidth, (GLfloat) desc->cropHeight, vph, 0.0f);
	glUniform4f(ctx->loc_fcb1, desc->effects[0], desc->effects[1], desc->levels[0], desc->levels[1]);
	glUniform4i(ctx->loc_icb, FMT_INFO[ctx->format].planes, desc->rotation,
		FMT_CONVERSION(ctx->format, desc->fullRangeYUV, desc->multiplyYUV), 0);

	// Draw
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

	return true;
}

void mty_gl_destroy(struct gfx **gfx)
{
	if (!gfx || !*gfx)
		return;

	struct gl *ctx = (struct gl *) *gfx;

	for (uint8_t x = 0; x < GL_NUM_STAGING; x++)
		gl_rtv_destroy(&ctx->staging[x]);

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
	*gfx = NULL;
}


// State

struct gl_state {
	GLint array_buffer;
	GLenum active_texture;
	GLint unpack_row_length;
	GLint unpack_alignment;
	GLint program;
	GLint texture;
	GLint viewport[4];
	GLint scissor_box[4];
	GLenum blend_src_rgb;
	GLenum blend_dst_rgb;
	GLenum blend_src_alpha;
	GLenum blend_dst_alpha;
	GLenum blend_equation_rgb;
	GLenum blend_equation_alpha;
	GLboolean blend;
	GLboolean cull_face;
	GLboolean depth_test;
	GLboolean scissor_test;
};

void *mty_gl_get_state(MTY_Device *device, MTY_Context *_context)
{
	if (!glproc_global_init())
		return NULL;

	struct gl_state *s = MTY_Alloc(1, sizeof(struct gl_state));

	glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &s->active_texture);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &s->unpack_row_length);
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &s->unpack_alignment);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
	glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->texture);
	glGetIntegerv(GL_VIEWPORT, s->viewport);
	glGetIntegerv(GL_SCISSOR_BOX, s->scissor_box);
	glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *) &s->blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, (GLint *) &s->blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *) &s->blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *) &s->blend_dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *) &s->blend_equation_rgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *) &s->blend_equation_alpha);
	s->blend = glIsEnabled(GL_BLEND);
	s->cull_face = glIsEnabled(GL_CULL_FACE);
	s->depth_test = glIsEnabled(GL_DEPTH_TEST);
	s->scissor_test = glIsEnabled(GL_SCISSOR_TEST);

	return s;
}

static void gl_enable(GLenum cap, bool enable)
{
	if (enable) {
		glEnable(cap);

	} else {
		glDisable(cap);
	}
}

void mty_gl_set_state(MTY_Device *device, MTY_Context *_context, void *state)
{
	if (!glproc_global_init())
		return;

	struct gl_state *s = state;

	glUseProgram(s->program);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, s->unpack_row_length);
	glPixelStorei(GL_UNPACK_ALIGNMENT, s->unpack_alignment);
	glBindTexture(GL_TEXTURE_2D, s->texture);
	glActiveTexture(s->active_texture);
	glBindBuffer(GL_ARRAY_BUFFER, s->array_buffer);
	glBlendEquationSeparate(s->blend_equation_rgb, s->blend_equation_alpha);
	glBlendFuncSeparate(s->blend_src_rgb, s->blend_dst_rgb, s->blend_src_alpha, s->blend_dst_alpha);
	gl_enable(GL_BLEND, s->blend);
	gl_enable(GL_CULL_FACE, s->cull_face);
	gl_enable(GL_DEPTH_TEST, s->depth_test);
	gl_enable(GL_SCISSOR_TEST, s->scissor_test);
	glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
	glScissor(s->scissor_box[0], s->scissor_box[1], s->scissor_box[2], s->scissor_box[3]);
}

void mty_gl_free_state(void **state)
{
	if (!state || !*state)
		return;

	struct gl_state *s = *state;

	MTY_Free(s);
	*state = NULL;
}
