// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"
#include "glproc.h"

#if defined(MTY_GL_EXTERNAL)

#define glproc_global_init() true

#else

#define GLPROC_LOAD_SYM(name) \
	if (!name) { \
		name = MTY_GLGetProcAddress(#name); \
		if (!name) {r = false; goto except;} \
	}

static PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer;
static PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;
static PFNGLENABLEPROC                  glEnable;
static PFNGLISENABLEDPROC               glIsEnabled;
static PFNGLDISABLEPROC                 glDisable;
static PFNGLVIEWPORTPROC                glViewport;
static PFNGLGETINTEGERVPROC             glGetIntegerv;
static PFNGLGETFLOATVPROC               glGetFloatv;
static PFNGLBINDTEXTUREPROC             glBindTexture;
static PFNGLDELETETEXTURESPROC          glDeleteTextures;
static PFNGLTEXPARAMETERIPROC           glTexParameteri;
static PFNGLGENTEXTURESPROC             glGenTextures;
static PFNGLTEXIMAGE2DPROC              glTexImage2D;
static PFNGLTEXSUBIMAGE2DPROC           glTexSubImage2D;
static PFNGLDRAWELEMENTSPROC            glDrawElements;
static PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation;
static PFNGLSHADERSOURCEPROC            glShaderSource;
static PFNGLBINDBUFFERPROC              glBindBuffer;
static PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
static PFNGLCREATEPROGRAMPROC           glCreateProgram;
static PFNGLUNIFORM1IPROC               glUniform1i;
static PFNGLUNIFORM1FPROC               glUniform1f;
static PFNGLUNIFORM4IPROC               glUniform4i;
static PFNGLUNIFORM4FPROC               glUniform4f;
static PFNGLACTIVETEXTUREPROC           glActiveTexture;
static PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
static PFNGLBUFFERDATAPROC              glBufferData;
static PFNGLDELETESHADERPROC            glDeleteShader;
static PFNGLGENBUFFERSPROC              glGenBuffers;
static PFNGLCOMPILESHADERPROC           glCompileShader;
static PFNGLLINKPROGRAMPROC             glLinkProgram;
static PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
static PFNGLCREATESHADERPROC            glCreateShader;
static PFNGLATTACHSHADERPROC            glAttachShader;
static PFNGLUSEPROGRAMPROC              glUseProgram;
static PFNGLGETSHADERIVPROC             glGetShaderiv;
static PFNGLDETACHSHADERPROC            glDetachShader;
static PFNGLDELETEPROGRAMPROC           glDeleteProgram;
static PFNGLCLEARPROC                   glClear;
static PFNGLCLEARCOLORPROC              glClearColor;
static PFNGLGETERRORPROC                glGetError;
static PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
static PFNGLFINISHPROC                  glFinish;
static PFNGLSCISSORPROC                 glScissor;
static PFNGLBLENDFUNCPROC               glBlendFunc;
static PFNGLBLENDEQUATIONPROC           glBlendEquation;
static PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
static PFNGLBLENDEQUATIONSEPARATEPROC   glBlendEquationSeparate;
static PFNGLBLENDFUNCSEPARATEPROC       glBlendFuncSeparate;
static PFNGLGETPROGRAMIVPROC            glGetProgramiv;
static PFNGLPIXELSTOREIPROC             glPixelStorei;

static MTY_Atomic32 GLPROC_LOCK;
static bool GLPROC_INIT;

static bool glproc_global_init(void)
{
	MTY_GlobalLock(&GLPROC_LOCK);

	if (!GLPROC_INIT) {
		bool r = true;

		GLPROC_LOAD_SYM(glGenFramebuffers);
		GLPROC_LOAD_SYM(glDeleteFramebuffers);
		GLPROC_LOAD_SYM(glBindFramebuffer);
		GLPROC_LOAD_SYM(glBlitFramebuffer);
		GLPROC_LOAD_SYM(glFramebufferTexture2D);
		GLPROC_LOAD_SYM(glEnable);
		GLPROC_LOAD_SYM(glIsEnabled);
		GLPROC_LOAD_SYM(glDisable);
		GLPROC_LOAD_SYM(glViewport);
		GLPROC_LOAD_SYM(glGetIntegerv);
		GLPROC_LOAD_SYM(glGetFloatv);
		GLPROC_LOAD_SYM(glBindTexture);
		GLPROC_LOAD_SYM(glDeleteTextures);
		GLPROC_LOAD_SYM(glTexParameteri);
		GLPROC_LOAD_SYM(glGenTextures);
		GLPROC_LOAD_SYM(glTexImage2D);
		GLPROC_LOAD_SYM(glTexSubImage2D);
		GLPROC_LOAD_SYM(glDrawElements);
		GLPROC_LOAD_SYM(glGetAttribLocation);
		GLPROC_LOAD_SYM(glShaderSource);
		GLPROC_LOAD_SYM(glBindBuffer);
		GLPROC_LOAD_SYM(glVertexAttribPointer);
		GLPROC_LOAD_SYM(glCreateProgram);
		GLPROC_LOAD_SYM(glUniform1i);
		GLPROC_LOAD_SYM(glUniform1f);
		GLPROC_LOAD_SYM(glUniform4i);
		GLPROC_LOAD_SYM(glUniform4f);
		GLPROC_LOAD_SYM(glActiveTexture);
		GLPROC_LOAD_SYM(glDeleteBuffers);
		GLPROC_LOAD_SYM(glEnableVertexAttribArray);
		GLPROC_LOAD_SYM(glBufferData);
		GLPROC_LOAD_SYM(glDeleteShader);
		GLPROC_LOAD_SYM(glGenBuffers);
		GLPROC_LOAD_SYM(glCompileShader);
		GLPROC_LOAD_SYM(glLinkProgram);
		GLPROC_LOAD_SYM(glGetUniformLocation);
		GLPROC_LOAD_SYM(glCreateShader);
		GLPROC_LOAD_SYM(glAttachShader);
		GLPROC_LOAD_SYM(glUseProgram);
		GLPROC_LOAD_SYM(glGetShaderiv);
		GLPROC_LOAD_SYM(glDetachShader);
		GLPROC_LOAD_SYM(glDeleteProgram);
		GLPROC_LOAD_SYM(glClear);
		GLPROC_LOAD_SYM(glClearColor);
		GLPROC_LOAD_SYM(glGetError);
		GLPROC_LOAD_SYM(glGetShaderInfoLog);
		GLPROC_LOAD_SYM(glFinish);
		GLPROC_LOAD_SYM(glScissor);
		GLPROC_LOAD_SYM(glBlendFunc);
		GLPROC_LOAD_SYM(glBlendEquation);
		GLPROC_LOAD_SYM(glUniformMatrix4fv);
		GLPROC_LOAD_SYM(glBlendEquationSeparate);
		GLPROC_LOAD_SYM(glBlendFuncSeparate);
		GLPROC_LOAD_SYM(glGetProgramiv);
		GLPROC_LOAD_SYM(glPixelStorei);

		except:

		GLPROC_INIT = r;
	}

	MTY_GlobalUnlock(&GLPROC_LOCK);

	return GLPROC_INIT;
}

#endif
