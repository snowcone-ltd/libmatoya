// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#if defined(MTY_GL_ES)
	#define GL_SHADER_VERSION "#version 100\n"
#else
	#define GL_SHADER_VERSION "#version 110\n"
#endif

#if defined MTY_GL_EXTERNAL

#if defined(MTY_GL_INCLUDE)
	#include MTY_GL_INCLUDE
#else
	#define GL_GLEXT_PROTOTYPES
	#include "glcorearb.h"
#endif

#define glproc_global_init() true

#else

#if !defined(GLPROC_EXTERN)
	#define GLPROC_EXTERN extern
#endif

#include "glcorearb.h"

GLPROC_EXTERN PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers;
GLPROC_EXTERN PFNGLDELETEFRAMEBUFFERSPROC      glDeleteFramebuffers;
GLPROC_EXTERN PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer;
GLPROC_EXTERN PFNGLBLITFRAMEBUFFERPROC         glBlitFramebuffer;
GLPROC_EXTERN PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;
GLPROC_EXTERN PFNGLENABLEPROC                  glEnable;
GLPROC_EXTERN PFNGLISENABLEDPROC               glIsEnabled;
GLPROC_EXTERN PFNGLDISABLEPROC                 glDisable;
GLPROC_EXTERN PFNGLVIEWPORTPROC                glViewport;
GLPROC_EXTERN PFNGLGETINTEGERVPROC             glGetIntegerv;
GLPROC_EXTERN PFNGLGETFLOATVPROC               glGetFloatv;
GLPROC_EXTERN PFNGLBINDTEXTUREPROC             glBindTexture;
GLPROC_EXTERN PFNGLDELETETEXTURESPROC          glDeleteTextures;
GLPROC_EXTERN PFNGLTEXPARAMETERIPROC           glTexParameteri;
GLPROC_EXTERN PFNGLGENTEXTURESPROC             glGenTextures;
GLPROC_EXTERN PFNGLTEXIMAGE2DPROC              glTexImage2D;
GLPROC_EXTERN PFNGLTEXSUBIMAGE2DPROC           glTexSubImage2D;
GLPROC_EXTERN PFNGLDRAWELEMENTSPROC            glDrawElements;
GLPROC_EXTERN PFNGLGETATTRIBLOCATIONPROC       glGetAttribLocation;
GLPROC_EXTERN PFNGLSHADERSOURCEPROC            glShaderSource;
GLPROC_EXTERN PFNGLBINDBUFFERPROC              glBindBuffer;
GLPROC_EXTERN PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
GLPROC_EXTERN PFNGLCREATEPROGRAMPROC           glCreateProgram;
GLPROC_EXTERN PFNGLUNIFORM1IPROC               glUniform1i;
GLPROC_EXTERN PFNGLUNIFORM1FPROC               glUniform1f;
GLPROC_EXTERN PFNGLUNIFORM4IPROC               glUniform4i;
GLPROC_EXTERN PFNGLUNIFORM4FPROC               glUniform4f;
GLPROC_EXTERN PFNGLACTIVETEXTUREPROC           glActiveTexture;
GLPROC_EXTERN PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
GLPROC_EXTERN PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
GLPROC_EXTERN PFNGLBUFFERDATAPROC              glBufferData;
GLPROC_EXTERN PFNGLDELETESHADERPROC            glDeleteShader;
GLPROC_EXTERN PFNGLGENBUFFERSPROC              glGenBuffers;
GLPROC_EXTERN PFNGLCOMPILESHADERPROC           glCompileShader;
GLPROC_EXTERN PFNGLLINKPROGRAMPROC             glLinkProgram;
GLPROC_EXTERN PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
GLPROC_EXTERN PFNGLCREATESHADERPROC            glCreateShader;
GLPROC_EXTERN PFNGLATTACHSHADERPROC            glAttachShader;
GLPROC_EXTERN PFNGLUSEPROGRAMPROC              glUseProgram;
GLPROC_EXTERN PFNGLGETSHADERIVPROC             glGetShaderiv;
GLPROC_EXTERN PFNGLDETACHSHADERPROC            glDetachShader;
GLPROC_EXTERN PFNGLDELETEPROGRAMPROC           glDeleteProgram;
GLPROC_EXTERN PFNGLCLEARPROC                   glClear;
GLPROC_EXTERN PFNGLCLEARCOLORPROC              glClearColor;
GLPROC_EXTERN PFNGLGETERRORPROC                glGetError;
GLPROC_EXTERN PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
GLPROC_EXTERN PFNGLFINISHPROC                  glFinish;
GLPROC_EXTERN PFNGLSCISSORPROC                 glScissor;
GLPROC_EXTERN PFNGLBLENDFUNCPROC               glBlendFunc;
GLPROC_EXTERN PFNGLBLENDEQUATIONPROC           glBlendEquation;
GLPROC_EXTERN PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
GLPROC_EXTERN PFNGLBLENDEQUATIONSEPARATEPROC   glBlendEquationSeparate;
GLPROC_EXTERN PFNGLBLENDFUNCSEPARATEPROC       glBlendFuncSeparate;
GLPROC_EXTERN PFNGLGETPROGRAMIVPROC            glGetProgramiv;
GLPROC_EXTERN PFNGLPIXELSTOREIPROC             glPixelStorei;

bool glproc_global_init(void);

#endif
