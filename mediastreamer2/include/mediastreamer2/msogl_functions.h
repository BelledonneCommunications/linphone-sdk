/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2017  Belledonne Communications, Grenoble, France

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef msogl_functions_h
#define msogl_functions_h

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#if TARGET_OS_IPHONE
	#include <OpenGLES/ES2/gl.h>
	#include <OpenGLES/ES2/glext.h>
#elif TARGET_OS_MAC
	#include <OpenGL/OpenGL.h>
	#include <OpenGL/gl.h>
#elif __ANDROID__
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>
#elif _WIN32
	#if !defined(QOPENGLFUNCTIONS_H)
		#include <GLES3/gl3.h>
	#endif
#elif !defined(QOPENGLFUNCTIONS_H) // glew is already included by QT.
	#include <GL/glew.h>
#endif

// =============================================================================

typedef void (*resolveGlActiveTexture)(GLenum texture);
typedef void (*resolveGlAttachShader)(GLuint program, GLuint shader);
typedef void (*resolveGlBindAttribLocation)(GLuint program, GLuint index, const char *name);
typedef void (*resolveGlBindBuffer)(GLenum target, GLuint buffer);
typedef void (*resolveGlBindFramebuffer)(GLenum target, GLuint framebuffer);
typedef void (*resolveGlBindRenderbuffer)(GLenum target, GLuint renderbuffer);
typedef void (*resolveGlBindTexture)(GLenum target, GLuint texture);
typedef void (*resolveGlBlendColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (*resolveGlBlendEquation)(GLenum mode);
typedef void (*resolveGlBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
typedef void (*resolveGlBlendFunc)(GLenum sfactor, GLenum dfactor);
typedef void (*resolveGlBlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
typedef void (*resolveGlBufferData)(GLenum target, GLsizei *size, const void *data, GLenum usage);
typedef void (*resolveGlBufferSubData)(GLenum target, GLint *offset, GLsizei *size, const void *data);
typedef GLenum (*resolveGlCheckFramebufferStatus)(GLenum target);
typedef void (*resolveGlClear)(GLbitfield mask);
typedef void (*resolveGlClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (*resolveGlClearDepthf)(GLclampf depth);
typedef void (*resolveGlClearStencil)(GLint s);
typedef void (*resolveGlColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (*resolveGlCompileShader)(GLuint shader);
typedef void (*resolveGlCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
typedef void (*resolveGlCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data);
typedef void (*resolveGlCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (*resolveGlCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef GLuint (*resolveGlCreateProgram)(void);
typedef GLuint (*resolveGlCreateShader)(GLenum type);
typedef void (*resolveGlCullFace)(GLenum mode);
typedef void (*resolveGlDeleteBuffers)(GLsizei n, const GLuint *buffers);
typedef void (*resolveGlDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
typedef void (*resolveGlDeleteProgram)(GLuint program);
typedef void (*resolveGlDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
typedef void (*resolveGlDeleteShader)(GLuint shader);
typedef void (*resolveGlDeleteTextures)(GLsizei n, const GLuint *textures);
typedef void (*resolveGlDepthFunc)(GLenum func);
typedef void (*resolveGlDepthMask)(GLboolean flag);
typedef void (*resolveGlDepthRangef)(GLclampf zNear, GLclampf zFar);
typedef void (*resolveGlDetachShader)(GLuint program, GLuint shader);
typedef void (*resolveGlDisable)(GLenum cap);
typedef void (*resolveGlDisableVertexAttribArray)(GLuint index);
typedef void (*resolveGlDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void (*resolveGlDrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (*resolveGlEnable)(GLenum cap);
typedef void (*resolveGlEnableVertexAttribArray)(GLuint index);
typedef void (*resolveGlFinish)(void);
typedef void (*resolveGlFlush)(void);
typedef void (*resolveGlFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (*resolveGlFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (*resolveGlFrontFace)(GLenum mode);
typedef void (*resolveGlGenBuffers)(GLsizei n, GLuint *buffers);
typedef void (*resolveGlGenFramebuffers)(GLsizei n, GLuint *framebuffers);
typedef void (*resolveGlGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
typedef void (*resolveGlGenTextures)(GLsizei n, GLuint *textures);
typedef void (*resolveGlGenerateMipmap)(GLenum target);
typedef void (*resolveGlGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name);
typedef void (*resolveGlGetActiveUniform)(GLuint program, GLuint index, GLsizei bufsize, GLsizei *length, GLint *size, GLenum *type, char *name);
typedef void (*resolveGlGetAttachedShaders)(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders);
typedef GLint (*resolveGlGetAttribLocation)(GLuint program, const char *name);
typedef void (*resolveGlGetBooleanv)(GLenum pname, GLboolean *params);
typedef void (*resolveGlGetBufferParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef GLenum (*resolveGlGetError)(void);
typedef void (*resolveGlGetFloatv)(GLenum pname, GLfloat *params);
typedef void (*resolveGlGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (*resolveGlGetIntegerv)(GLenum pname, GLint *params);
typedef void (*resolveGlGetProgramInfoLog)(GLuint program, GLsizei bufsize, GLsizei *length, char *infolog);
typedef void (*resolveGlGetProgramiv)(GLuint program, GLenum pname, GLint *params);
typedef void (*resolveGlGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef void (*resolveGlGetShaderInfoLog)(GLuint shader, GLsizei bufsize, GLsizei *length, char *infolog);
typedef void (*resolveGlGetShaderPrecisionFormat)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
typedef void (*resolveGlGetShaderSource)(GLuint shader, GLsizei bufsize, GLsizei *length, char *source);
typedef void (*resolveGlGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
typedef const GLubyte *(*resolveGlGetString)(GLenum name);
typedef void (*resolveGlGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
typedef void (*resolveGlGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef GLint (*resolveGlGetUniformLocation)(GLuint program, const char *name);
typedef void (*resolveGlGetUniformfv)(GLuint program, GLint location, GLfloat *params);
typedef void (*resolveGlGetUniformiv)(GLuint program, GLint location, GLint *params);
typedef void (*resolveGlGetVertexAttribPointerv)(GLuint index, GLenum pname, void **pointer);
typedef void (*resolveGlGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
typedef void (*resolveGlGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
typedef void (*resolveGlHint)(GLenum target, GLenum mode);
typedef GLboolean (*resolveGlIsBuffer)(GLuint buffer);
typedef GLboolean (*resolveGlIsEnabled)(GLenum cap);
typedef GLboolean (*resolveGlIsFramebuffer)(GLuint framebuffer);
typedef GLboolean (*resolveGlIsProgram)(GLuint program);
typedef GLboolean (*resolveGlIsRenderbuffer)(GLuint renderbuffer);
typedef GLboolean (*resolveGlIsShader)(GLuint shader);
typedef GLboolean (*resolveGlIsTexture)(GLuint texture);
typedef void (*resolveGlLineWidth)(GLfloat width);
typedef void (*resolveGlLinkProgram)(GLuint program);
typedef void (*resolveGlPixelStorei)(GLenum pname, GLint param);
typedef void (*resolveGlPolygonOffset)(GLfloat factor, GLfloat units);
typedef void (*resolveGlReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
typedef void (*resolveGlReleaseShaderCompiler)(void);
typedef void (*resolveGlRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*resolveGlSampleCoverage)(GLclampf value, GLboolean invert);
typedef void (*resolveGlScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (*resolveGlShaderBinary)(GLint n, const GLuint *shaders, GLenum binaryformat, const void *binary, GLint length);
typedef void (*resolveGlShaderSource)(GLuint shader, GLsizei count, const char **string, const GLint *length);
typedef void (*resolveGlStencilFunc)(GLenum func, GLint ref, GLuint mask);
typedef void (*resolveGlStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
typedef void (*resolveGlStencilMask)(GLuint mask);
typedef void (*resolveGlStencilMaskSeparate)(GLenum face, GLuint mask);
typedef void (*resolveGlStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void (*resolveGlStencilOpSeparate)(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
typedef void (*resolveGlTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (*resolveGlTexParameterf)(GLenum target, GLenum pname, GLfloat param);
typedef void (*resolveGlTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
typedef void (*resolveGlTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void (*resolveGlTexParameteriv)(GLenum target, GLenum pname, const GLint *params);
typedef void (*resolveGlTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (*resolveGlUniform1f)(GLint location, GLfloat x);
typedef void (*resolveGlUniform1fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (*resolveGlUniform1i)(GLint location, GLint x);
typedef void (*resolveGlUniform1iv)(GLint location, GLsizei count, const GLint *v);
typedef void (*resolveGlUniform2f)(GLint location, GLfloat x, GLfloat y);
typedef void (*resolveGlUniform2fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (*resolveGlUniform2i)(GLint location, GLint x, GLint y);
typedef void (*resolveGlUniform2iv)(GLint location, GLsizei count, const GLint *v);
typedef void (*resolveGlUniform3f)(GLint location, GLfloat x, GLfloat y, GLfloat z);
typedef void (*resolveGlUniform3fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (*resolveGlUniform3i)(GLint location, GLint x, GLint y, GLint z);
typedef void (*resolveGlUniform3iv)(GLint location, GLsizei count, const GLint *v);
typedef void (*resolveGlUniform4f)(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (*resolveGlUniform4fv)(GLint location, GLsizei count, const GLfloat *v);
typedef void (*resolveGlUniform4i)(GLint location, GLint x, GLint y, GLint z, GLint w);
typedef void (*resolveGlUniform4iv)(GLint location, GLsizei count, const GLint *v);
typedef void (*resolveGlUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (*resolveGlUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (*resolveGlUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (*resolveGlUseProgram)(GLuint program);
typedef void (*resolveGlValidateProgram)(GLuint program);
typedef void (*resolveGlVertexAttrib1f)(GLuint indx, GLfloat x);
typedef void (*resolveGlVertexAttrib1fv)(GLuint indx, const GLfloat *values);
typedef void (*resolveGlVertexAttrib2f)(GLuint indx, GLfloat x, GLfloat y);
typedef void (*resolveGlVertexAttrib2fv)(GLuint indx, const GLfloat *values);
typedef void (*resolveGlVertexAttrib3f)(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
typedef void (*resolveGlVertexAttrib3fv)(GLuint indx, const GLfloat *values);
typedef void (*resolveGlVertexAttrib4f)(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (*resolveGlVertexAttrib4fv)(GLuint indx, const GLfloat *values);
typedef void (*resolveGlVertexAttribPointer)(GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *ptr);
typedef void (*resolveGlViewport)(GLint x, GLint y, GLsizei width, GLsizei height);

// -----------------------------------------------------------------------------

struct OpenGlFunctions {
	resolveGlActiveTexture glActiveTexture;
	resolveGlAttachShader glAttachShader;
	resolveGlBindAttribLocation glBindAttribLocation;
	// resolveGlBindBuffer glBindBuffer;
	// resolveGlBindFramebuffer glBindFramebuffer;
	// resolveGlBindRenderbuffer glBindRenderbuffer;
	resolveGlBindTexture glBindTexture;
	// resolveGlBlendColor glBlendColor;
	// resolveGlBlendEquation glBlendEquation;
	// resolveGlBlendEquationSeparate glBlendEquationSeparate;
	// resolveGlBlendFunc glBlendFunc;
	// resolveGlBlendFuncSeparate glBlendFuncSeparate;
	// resolveGlBufferData glBufferData;
	// resolveGlBufferSubData glBufferSubData;
	// resolveGlCheckFramebufferStatus glCheckFramebufferStatus;
	resolveGlClear glClear;
	resolveGlClearColor glClearColor;
	// resolveGlClearDepthf glClearDepthf;
	// resolveGlClearStencil glClearStencil;
	// resolveGlColorMask glColorMask;
	resolveGlCompileShader glCompileShader;
	// resolveGlCompressedTexImage2D glCompressedTexImage2D;
	// resolveGlCompressedTexSubImage2D glCompressedTexSubImage2D;
	// resolveGlCopyTexImage2D glCopyTexImage2D;
	// resolveGlCopyTexSubImage2D glCopyTexSubImage2D;
	resolveGlCreateProgram glCreateProgram;
	resolveGlCreateShader glCreateShader;
	// resolveGlCullFace glCullFace;
	// resolveGlDeleteBuffers glDeleteBuffers;
	// resolveGlDeleteFramebuffers glDeleteFramebuffers;
	resolveGlDeleteProgram glDeleteProgram;
	//resolveGlDeleteRenderbuffers glDeleteRenderbuffers;
	resolveGlDeleteShader glDeleteShader;
	resolveGlDeleteTextures glDeleteTextures;
	// resolveGlDepthFunc glDepthFunc;
	// resolveGlDepthMask glDepthMask;
	// resolveGlDepthRangef glDepthRangef;
	// resolveGlDetachShader glDetachShader;
	resolveGlDisable glDisable;
	// resolveGlDisableVertexAttribArray glDisableVertexAttribArray;
	resolveGlDrawArrays glDrawArrays;
	// resolveGlDrawElements glDrawElements;
	// resolveGlEnable glEnable;
	resolveGlEnableVertexAttribArray glEnableVertexAttribArray;
	// resolveGlFinish glFinish;
	// resolveGlFlush glFlush;
	// resolveGlFramebufferRenderbuffer glFramebufferRenderbuffer;
	// resolveGlFramebufferTexture2D glFramebufferTexture2D;
	// resolveGlFrontFace glFrontFace;
	// resolveGlGenBuffers glGenBuffers;
	// resolveGlGenFramebuffers glGenFramebuffers;
	// resolveGlGenRenderbuffers glGenRenderbuffers;
	resolveGlGenTextures glGenTextures;
	// resolveGlGenerateMipmap glGenerateMipmap;
	// resolveGlGetActiveAttrib glGetActiveAttrib;
	// resolveGlGetActiveUniform glGetActiveUniform;
	// resolveGlGetAttachedShaders glGetAttachedShaders;
	// resolveGlGetAttribLocation glGetAttribLocation;
	// resolveGlGetBooleanv glGetBooleanv;
	// resolveGlGetBufferParameteriv glGetBufferParameteriv;
	resolveGlGetError glGetError;
	// resolveGlGetFloatv glGetFloatv;
	// resolveGlGetFramebufferAttachmentParameteriv glGetFramebufferAttachmentParameteriv;
	// resolveGlGetIntegerv glGetIntegerv;
	resolveGlGetProgramInfoLog glGetProgramInfoLog;
	resolveGlGetProgramiv glGetProgramiv;
	// resolveGlGetRenderbufferParameteriv glGetRenderbufferParameteriv;
	resolveGlGetShaderInfoLog glGetShaderInfoLog;
	// resolveGlGetShaderPrecisionFormat glGetShaderPrecisionFormat;
	// resolveGlGetShaderSource glGetShaderSource;
	resolveGlGetShaderiv glGetShaderiv;
	resolveGlGetString glGetString;
	// resolveGlGetTexParameterfv glGetTexParameterfv;
	// resolveGlGetTexParameteriv glGetTexParameteriv;
	resolveGlGetUniformLocation glGetUniformLocation;
	// resolveGlGetUniformfv glGetUniformfv;
	// resolveGlGetUniformiv glGetUniformiv;
	// resolveGlGetVertexAttribPointerv glGetVertexAttribPointerv;
	// resolveGlGetVertexAttribfv glGetVertexAttribfv;
	// resolveGlGetVertexAttribiv glGetVertexAttribiv;
	// resolveGlHint glHint;
	// resolveGlIsBuffer glIsBuffer;
	// resolveGlIsEnabled glIsEnabled;
	// resolveGlIsFramebuffer glIsFramebuffer;
	// resolveGlIsProgram glIsProgram;
	// resolveGlIsRenderbuffer glIsRenderbuffer;
	// resolveGlIsShader glIsShader;
	// resolveGlIsTexture glIsTexture;
	// resolveGlLineWidth glLineWidth;
	resolveGlLinkProgram glLinkProgram;
	resolveGlPixelStorei glPixelStorei;
	// resolveGlPolygonOffset glPolygonOffset;
	// resolveGlReadPixels glReadPixels;
	// resolveGlReleaseShaderCompiler glReleaseShaderCompiler;
	// resolveGlRenderbufferStorage glRenderbufferStorage;
	// resolveGlSampleCoverage glSampleCoverage;
	// resolveGlScissor glScissor;
	// resolveGlShaderBinary glShaderBinary;
	resolveGlShaderSource glShaderSource;
	// resolveGlStencilFunc glStencilFunc;
	// resolveGlStencilFuncSeparate glStencilFuncSeparate;
	// resolveGlStencilMask glStencilMask;
	// resolveGlStencilMaskSeparate glStencilMaskSeparate;
	// resolveGlStencilOp glStencilOp;
	// resolveGlStencilOpSeparate glStencilOpSeparate;
	resolveGlTexImage2D glTexImage2D;
	// resolveGlTexParameterf glTexParameterf;
	// resolveGlTexParameterfv glTexParameterfv;
	resolveGlTexParameteri glTexParameteri;
	// resolveGlTexParameteriv glTexParameteriv;
	resolveGlTexSubImage2D glTexSubImage2D;
	resolveGlUniform1f glUniform1f;
	// resolveGlUniform1fv glUniform1fv;
	resolveGlUniform1i glUniform1i;
	// resolveGlUniform1iv glUniform1iv;
	// resolveGlUniform2f glUniform2f;
	// resolveGlUniform2fv glUniform2fv;
	// resolveGlUniform2i glUniform2i;
	// resolveGlUniform2iv glUniform2iv;
	// resolveGlUniform3f glUniform3f;
	// resolveGlUniform3fv glUniform3fv;
	// resolveGlUniform3i glUniform3i;
	// resolveGlUniform3iv glUniform3iv;
	// resolveGlUniform4f glUniform4f;
	// resolveGlUniform4fv glUniform4fv;
	// resolveGlUniform4i glUniform4i;
	// resolveGlUniform4iv glUniform4iv;
	// resolveGlUniformMatrix2fv glUniformMatrix2fv;
	// resolveGlUniformMatrix3fv glUniformMatrix3fv;
	resolveGlUniformMatrix4fv glUniformMatrix4fv;
	resolveGlUseProgram glUseProgram;
	resolveGlValidateProgram glValidateProgram;
	// resolveGlVertexAttrib1f glVertexAttrib1f;
	// resolveGlVertexAttrib1fv glVertexAttrib1fv;
	// resolveGlVertexAttrib2f glVertexAttrib2f;
	// resolveGlVertexAttrib2fv glVertexAttrib2fv;
	// resolveGlVertexAttrib3f glVertexAttrib3f;
	// resolveGlVertexAttrib3fv glVertexAttrib3fv;
	// resolveGlVertexAttrib4f glVertexAttrib4f;
	// resolveGlVertexAttrib4fv glVertexAttrib4fv;
	resolveGlVertexAttribPointer glVertexAttribPointer;
	resolveGlViewport glViewport;
};

typedef struct OpenGlFunctions OpenGlFunctions;

#endif
