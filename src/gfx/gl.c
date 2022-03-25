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

#include "shaders/gl/vs.h"
#include "shaders/gl/fs.h"

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
	GLuint loc_fcb;
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
	ctx->loc_fcb = glGetUniformLocation(ctx->prog, "fcb");
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

static void gl_rtv_refresh(struct gl_rtv *rtv, GLint internal, GLenum format, GLenum type, uint32_t w, uint32_t h)
{
	if (!rtv->texture || rtv->w != w || rtv->h != h || rtv->format != format) {
		gl_rtv_destroy(rtv);

		glGenTextures(1, &rtv->texture);
		glBindTexture(GL_TEXTURE_2D, rtv->texture);
		glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, NULL);

		rtv->w = w;
		rtv->h = h;
		rtv->format = format;
	}
}

static void gl_reload_textures(struct gl *ctx, const void *image, const MTY_RenderDesc *desc)
{
	switch (desc->format) {
		case MTY_COLOR_FORMAT_BGRA:
		case MTY_COLOR_FORMAT_RGBA: 
		case MTY_COLOR_FORMAT_AYUV:
		case MTY_COLOR_FORMAT_BGR565:
		case MTY_COLOR_FORMAT_BGRA5551: {
			GLenum internal = GL_BGRA;
			GLenum format = GL_BGRA;
			GLenum type = GL_UNSIGNED_BYTE;
			GLint bpp = 4;

			if (desc->format == MTY_COLOR_FORMAT_RGBA) {
				internal = GL_RGBA;
				format = GL_RGBA;

			} else if (desc->format == MTY_COLOR_FORMAT_BGR565) {
				internal = GL_RGB;
				format = GL_RGB;
				type = GL_UNSIGNED_SHORT_5_6_5;
				bpp = 2;

			} else if (desc->format == MTY_COLOR_FORMAT_BGRA5551) {
				type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
				bpp = 2;
			}

			// BGRA
			gl_rtv_refresh(&ctx->staging[0], internal, format, type, desc->cropWidth, desc->cropHeight);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[0].texture);
			glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, desc->imageWidth);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->cropWidth, desc->cropHeight, format, type, image);
			break;
		}
		case MTY_COLOR_FORMAT_NV12:
		case MTY_COLOR_FORMAT_NV16: {
			uint32_t div = desc->format == MTY_COLOR_FORMAT_NV12 ? 2 : 1;

			// Y
			gl_rtv_refresh(&ctx->staging[0], GL_R8, GL_RED, GL_UNSIGNED_BYTE, desc->cropWidth, desc->cropHeight);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[0].texture);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, desc->imageWidth);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->cropWidth, desc->cropHeight, GL_RED, GL_UNSIGNED_BYTE, image);

			// UV
			gl_rtv_refresh(&ctx->staging[1], GL_RG8, GL_RG, GL_UNSIGNED_BYTE, desc->cropWidth / 2, desc->cropHeight / div);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[1].texture);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, desc->imageWidth / 2);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->cropWidth / 2, desc->cropHeight / div, GL_RG, GL_UNSIGNED_BYTE, (uint8_t *) image + desc->imageWidth * desc->imageHeight);
			break;
		}
		case MTY_COLOR_FORMAT_I420:
		case MTY_COLOR_FORMAT_I444: {
			uint32_t div = desc->format == MTY_COLOR_FORMAT_I420 ? 2 : 1;

			// Y
			gl_rtv_refresh(&ctx->staging[0], GL_R8, GL_RED, GL_UNSIGNED_BYTE, desc->cropWidth, desc->cropHeight);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[0].texture);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, desc->imageWidth);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->cropWidth, desc->cropHeight, GL_RED, GL_UNSIGNED_BYTE, image);

			// U
			uint8_t *p = (uint8_t *) image + desc->imageWidth * desc->imageHeight;
			gl_rtv_refresh(&ctx->staging[1], GL_R8, GL_RED, GL_UNSIGNED_BYTE, desc->cropWidth / div, desc->cropHeight / div);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[1].texture);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, desc->imageWidth / div);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->cropWidth / div, desc->cropHeight / div, GL_RED, GL_UNSIGNED_BYTE, p);

			// V
			p += (desc->imageWidth / div) * (desc->imageHeight / div);
			gl_rtv_refresh(&ctx->staging[2], GL_R8, GL_RED, GL_UNSIGNED_BYTE, desc->cropWidth / div, desc->cropHeight / div);
			glBindTexture(GL_TEXTURE_2D, ctx->staging[2].texture);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, desc->imageWidth / div);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc->cropWidth / div, desc->cropHeight / div, GL_RED, GL_UNSIGNED_BYTE, p);
			break;
		}
	}
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
	gl_reload_textures(ctx, image, desc);

	// Viewport
	float vpx, vpy, vpw, vph;
	mty_viewport(desc, &vpx, &vpy, &vpw, &vph, true);

	glViewport(lrint(vpx), lrint(vpy) + GL_ORIGIN_Y, lrint(vpw), lrint(vph));

	// Begin render pass (set destination texture if available)
	if (_dest)
		glBindFramebuffer(GL_FRAMEBUFFER, _dest);

	// Blending capability
	if (desc->blend) {
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	} else {
		glDisable(GL_BLEND);
	}
	
	// Context state, set vertex and fragment shaders
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
	if (desc->clear) {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

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
	glUniform4f(ctx->loc_fcb, (GLfloat) desc->cropWidth, (GLfloat) desc->cropHeight, vph, 0.0f);
	glUniform4i(ctx->loc_icb, desc->filter, desc->effect, ctx->format, desc->rotation);

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
