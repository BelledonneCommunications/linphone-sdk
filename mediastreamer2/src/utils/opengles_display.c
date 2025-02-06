/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <bctoolbox/defs.h>

#include "mediastreamer2/mscommon.h"
#include "opengles_display.h"
#include "shader_util.h"
#include <EGL/eglext.h>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h" // ENABLE_OPENGL_PROFILING
#endif

#ifdef HAVE_GLX
#include <GL/glx.h>
#include <X11/Xlib.h>
#ifdef HAVE_XV
#include <X11/extensions/Xvlib.h>
#endif
#endif
enum ImageType { REMOTE_IMAGE = 0, PREVIEW_IMAGE, MAX_IMAGE };

// #define CHECK_GL_ERROR

#ifdef CHECK_GL_ERROR
#define GL_OPERATION(f, x)                                                                                             \
	do {                                                                                                               \
		(f->x);                                                                                                        \
		check_GL_errors(f, #x);                                                                                        \
	} while (0);

#define GL_OPERATION_RET(f, x, ret)                                                                                    \
	do {                                                                                                               \
		ret = (f->x);                                                                                                  \
		check_GL_errors(f, #x);                                                                                        \
	} while (0);
#else
#define GL_OPERATION(f, x) (f->x);

#define GL_OPERATION_RET(f, x, ret) ret = (f->x);
#endif

#ifdef ENABLE_OPENGL_PROFILING
#include <locale.h>
#include <time.h>

static GLuint gl_time_query;
#endif

enum {
	UNIFORM_PROJ_MATRIX = 0,
	UNIFORM_ROTATION,
	UNIFORM_TEXTURE_Y,
	UNIFORM_TEXTURE_U,
	UNIFORM_TEXTURE_V,
	NUM_UNIFORMS
};

enum { ATTRIB_VERTEX = 0, ATTRIB_UV, NUM_ATTRIBS };

enum { Y, U, V };

#define TEXTURE_BUFFER_SIZE 3

// -----------------------------------------------------------------------------

struct opengles_display {
	/* input: yuv image to display */
	ms_mutex_t yuv_mutex;
	mblk_t *yuv[MAX_IMAGE];
	bool_t new_yuv_image[TEXTURE_BUFFER_SIZE][MAX_IMAGE];

	/* GL resources */
	bool_t glResourcesInitialized;
	bool_t shadersLoaded;
	GLuint arrayBufferHandle;
	GLuint vertexArrayObjectHandle;
	GLuint program, textures[TEXTURE_BUFFER_SIZE][MAX_IMAGE][3];
	GLint uniforms[NUM_UNIFORMS];
	MSVideoSize allocatedTexturesSize[MAX_IMAGE];
	GLint texInternalFormat;
	GLenum texFormat;

	int texture_index;

	/* GL view size */
	GLint backingWidth;
	GLint backingHeight;

	/* runtime data */
	float uvx[MAX_IMAGE], uvy[MAX_IMAGE];
	MSVideoSize yuv_size[MAX_IMAGE];

	/* coordinates of for zoom-in */
	float zoom_factor;
	float zoom_cx;
	float zoom_cy;

	/* whether mirroring (vertical flip) is requested*/
	bool_t do_mirroring[MAX_IMAGE];

	OpenGlFunctions *default_functions;
	const OpenGlFunctions *functions;

	EGLDisplay mEglDisplay;
	// Describes the EGLContext to be requested
	const MSEGLContextDescriptor *target_context;
	EGLContext mEglContext;
	EGLConfig mEglConfig; // No need to be cleaned
	EGLSurface mRenderSurface;

	// Callbacks
	GLenum mLastError;              // 0 if none else best error coming from eglGetError()
	bool_t mSendUnrecoverableError; // When a Window ID becomes unavailable and need to be restarted, a restart callback
	                                // has been called. This boolean avoid to call the callback multiple times.
};

// -----------------------------------------------------------------------------

static void error_mngmt_set(struct opengles_display *display, GLenum error) {
	unsigned int lastError = display->mLastError; // avoid mLastError to be reset while checking priorities
	if (lastError == 0) {
		display->mLastError = error;
	} else {
		// Low priority
		if (error == EGL_BAD_MATCH || error == EGL_BAD_ATTRIBUTE || error == EGL_BAD_PARAMETER) return;
		// Simple priority
		if (lastError == EGL_CONTEXT_LOST || lastError == EGL_BAD_NATIVE_WINDOW || lastError == EGL_BAD_SURFACE ||
		    lastError == EGL_BAD_NATIVE_PIXMAP)
			return;
		// Level 1
		if (error == EGL_BAD_ALLOC && lastError == EGL_BAD_ACCESS) return;

		display->mLastError = error;
	}
}

static void clean_GL_errors(const OpenGlFunctions *f) {
	if (f->glInitialized)
		while (f->glGetError() != GL_NO_ERROR)
			;
}

static void check_GL_errors(const OpenGlFunctions *f, const char *context) {
	if (f->glInitialized) {
		GLenum error;
		while ((error = f->glGetError()) != GL_NO_ERROR) {
			switch (error) {
				case GL_INVALID_ENUM:
					ms_error("[ogl_display] GL error: '%s' -> GL_INVALID_ENUM\n", context);
					break;
				case GL_INVALID_VALUE:
					ms_error("[ogl_display] GL error: '%s' -> GL_INVALID_VALUE\n", context);
					break;
				case GL_INVALID_OPERATION:
					ms_error("[ogl_display] GL error: '%s' -> GL_INVALID_OPERATION\n", context);
					break;
				case GL_OUT_OF_MEMORY:
					ms_error("[ogl_display] GL error: '%s' -> GL_OUT_OF_MEMORY\n", context);
					break;
				case GL_INVALID_FRAMEBUFFER_OPERATION:
					ms_error("[ogl_display] GL error: '%s' -> GL_INVALID_FRAMEBUFFER_OPERATION\n", context);
					break;
				default:
					ms_error("[ogl_display] GL error: '%s' -> %x\n", context, error);
			}
		}
	}
}

static void check_EGL_errors(struct opengles_display *display, const char *context) {
	if (display->functions->eglInitialized) {
		GLenum error;
		if ((error = display->functions->eglGetError()) != EGL_SUCCESS) {
			ms_error("[ogl_display] EGL error: '%s' -> %x%s\n", context, error,
			         error != EGL_CONTEXT_LOST ? "(notify not implemented)" : "");
			// TODO: We know exactly what do to with EGL_CONTEXT_LOST so we can stop sending notifications. For other
			// errors, we don't want to spam notifications on each call of ogl_display_notify_errors().
			error_mngmt_set(display, error);
		}
	}
}
static unsigned int align_on_power_of_2(unsigned int value) {
	int i;
	/* browse all power of 2 value, and find the one just >= value */
	for (i = 0; i < 32; i++) {
		unsigned int c = 1 << i;
		if (value <= c) return c;
	}
	return 0;
}

static void apply_mirroring(float xCenter, float *mat) {
	// If mirroring is enabled, then we must apply the reflection matrix
	// -1 0 0 0
	//  0 1 0 0
	//  0 0 1 0
	//  0 0 0 1
	// Based on the above and exploiting the properties of the orthographic matrix, only mat[0] must be changed
	mat[0] = -mat[0];

	// Translation on the X axis to compensate OpenGl attribute ATTRIB_VERTEX
	// Applying mirroring to the orthographic matrix means that the image is mirrored but also its position in the
	// window Multiply by 4 because:
	// - xCenter assumes that dimension screens goes from -0.5 to 0.5
	// - Windows must be moved on the other side of the window therefore it is twice the distance between the center of
	// the window and the center of the image
	mat[12] += (4.0f * xCenter);
}

// Remap left and bottom coordinate to -1
// Remap right and top coordinate to 1
static void
load_orthographic_matrix(float left, float right, float bottom, float top, float _near, float _far, float *mat) {
	float r_l = right - left;
	float t_b = top - bottom;
	float f_n = _far - _near;
	float tx = -(right + left) / (right - left);
	float ty = -(top + bottom) / (top - bottom);
	float tz = -(_far + _near) / (_far - _near);

	mat[0] = (2.0f / r_l);
	mat[1] = mat[2] = mat[3] = 0.0f;

	mat[4] = 0.0f;
	mat[5] = (2.0f / t_b);
	mat[6] = mat[7] = 0.0f;

	mat[8] = mat[9] = 0.0f;
	mat[10] = -2.0f / f_n;
	mat[11] = 0.0f;

	mat[12] = tx;
	mat[13] = ty;
	mat[14] = tz;
	mat[15] = 1.0f;
}

static void load_projection_matrix(float left,
                                   float right,
                                   float bottom,
                                   float top,
                                   float _near,
                                   float _far,
                                   float xCenter,
                                   bool_t mirror,
                                   float *mat) {
	load_orthographic_matrix(left, right, bottom, top, _near, _far, mat);
	if (mirror) apply_mirroring(xCenter, mat);
}

// -----------------------------------------------------------------------------

static void print_program_info(const OpenGlFunctions *f, GLuint program) {
	GLint logLength;
	char *msg;

	GL_OPERATION(f, glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength));
	if (logLength > 0) {
		msg = ms_new(char, logLength);

		GL_OPERATION(f, glGetProgramInfoLog(program, logLength, &logLength, msg));
		ms_message("[ogl_display] OpenGL program info: %s", msg);

		ms_free(msg);
	} else ms_message("[ogl_display] OpenGL program info: [NO INFORMATION]");
}

#define ARRAY_BUFFER_SIZE (16 * sizeof(GL_FLOAT))

static void bind_vertex_shader_input(const OpenGlFunctions *f) {
	// The first 8 slots are UV (4 * vec2)
	GL_OPERATION(f, glVertexAttribPointer(ATTRIB_UV, 2, GL_FLOAT, 1, 0, (const void *)0))
	GL_OPERATION(f, glEnableVertexAttribArray(ATTRIB_UV))
	// The last 8 are vertex positions (4 * vec2)
	GL_OPERATION(f, glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, (const void *)(8 * sizeof(GL_FLOAT))))
	GL_OPERATION(f, glEnableVertexAttribArray(ATTRIB_VERTEX))
}

static bool_t compile_gl_program(const OpenGlFunctions *f,
                                 GLuint program,
                                 GLuint vertShader,
                                 const char *vertSource,
                                 GLuint fragShader,
                                 const char *fragSource) {
	if (!glueCompileShader(f, vertSource, vertShader)) return FALSE;
	if (!glueCompileShader(f, fragSource, fragShader)) return FALSE;
	return glueLinkProgram(f, program);
}

static bool_t load_shaders(struct opengles_display *gldisp) {
#include "YCbCr_to_RGB.frag.h"
#include "screen_transform.vert.h"
// GLSL 1.00 legacy shaders
#include "yuv2rgb.fs.h"
#include "yuv2rgb.vs.h"

	const OpenGlFunctions *f = gldisp->functions;

	/* To support the widest range of OpenGL implementations, we have two
	   different concerns:
	   - Using supported API calls (OpenGL version)
	   - Using supported shaders (GLSL version)
	*/

	/* When running inside an OpenGL Core profile (introduced in OpenGL 3.2),
	   we need to generate and bind a Vertex Array Object (VAO) to store the
	   state needed to supply vertex data. This step is not required for
	   OpenGL 3.0 and 3.1 (and plain impossible in OpenGL 2.1), but for the sake
	   of simplicity we do it whenever the version permits.
	*/

	// GL_MAJOR_VERSION was introduced in OpenGL 3. If glGetIntegerv fails, that
	// means we're in version 2.
	GLint opengl_version_major = 2;
	f->glGetIntegerv(GL_MAJOR_VERSION, &opengl_version_major);
	f->glGetError(); // Silence the possible error
	// Luckily the following functions were also added in OpenGL ES 3.0, so no need
	// to check for ES
	if (3 <= opengl_version_major) {
		GL_OPERATION(f, glGenVertexArrays(1, &gldisp->vertexArrayObjectHandle))
		GL_OPERATION(f, glBindVertexArray(gldisp->vertexArrayObjectHandle))
	} else {
		// These functions can be found on systems (e.g. macOS) that *also* support
		// higher OpenGL versions but calling them will result in an error, so
		// let's make sure that won't happen.
		OpenGlFunctions *errata = (OpenGlFunctions *)f;
		errata->glGenVertexArrays = NULL;
		errata->glBindVertexArray = NULL;

		gldisp->texInternalFormat = GL_LUMINANCE;
		gldisp->texFormat = GL_LUMINANCE;
	}

	// The Array Buffer will hold input data for the vertex shader
	GL_OPERATION(f, glGenBuffers(1, &gldisp->arrayBufferHandle))
	GL_OPERATION(f, glBindBuffer(GL_ARRAY_BUFFER, gldisp->arrayBufferHandle))
	// Allocate buffer (uninitialized)
	GL_OPERATION(f, glBufferData(GL_ARRAY_BUFFER, (GLsizei *)ARRAY_BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW))
	bind_vertex_shader_input(f);

	GLuint program = f->glCreateProgram();
	GLuint vertShader = f->glCreateShader(GL_VERTEX_SHADER);
	GLuint fragShader = f->glCreateShader(GL_FRAGMENT_SHADER);

	GL_OPERATION(f, glAttachShader(program, vertShader))
	GL_OPERATION(f, glAttachShader(program, fragShader))

	GL_OPERATION(f, glBindAttribLocation(program, ATTRIB_VERTEX, "position"))
	GL_OPERATION(f, glBindAttribLocation(program, ATTRIB_UV, "uv"))

	// Try to compile newest shaders first
	if (!compile_gl_program(f, program, vertShader, screen_transform_vert, fragShader, YCbCr_to_RGB_frag)) {
		// If that fails...
		ms_message("[ogl_display] Falling back to legacy shaders.");
		if (!compile_gl_program(f, program, vertShader, yuv2rgb_vs, fragShader, yuv2rgb_fs)) {
			check_GL_errors(f, "load_shaders");
			return FALSE;
		}
	}

	GLint *uniforms = gldisp->uniforms;
	GL_OPERATION_RET(f, glGetUniformLocation(program, "proj_matrix"), uniforms[UNIFORM_PROJ_MATRIX])
	GL_OPERATION_RET(f, glGetUniformLocation(program, "rotation"), uniforms[UNIFORM_ROTATION])
	GL_OPERATION_RET(f, glGetUniformLocation(program, "t_texture_y"), uniforms[UNIFORM_TEXTURE_Y])
	GL_OPERATION_RET(f, glGetUniformLocation(program, "t_texture_u"), uniforms[UNIFORM_TEXTURE_U])
	GL_OPERATION_RET(f, glGetUniformLocation(program, "t_texture_v"), uniforms[UNIFORM_TEXTURE_V])

	GL_OPERATION(f, glDeleteShader(vertShader))
	GL_OPERATION(f, glDeleteShader(fragShader))

	print_program_info(f, program);

	gldisp->program = program;

	check_GL_errors(f, "load_shaders");
	return TRUE;
}

// -----------------------------------------------------------------------------

static void allocate_gl_textures(struct opengles_display *gldisp, int w, int h, enum ImageType type) {
	const OpenGlFunctions *f = gldisp->functions;
	const GLenum texture_internal_format = gldisp->texInternalFormat;
	const GLenum texture_format = gldisp->texFormat;
	int j;

	for (j = 0; j < TEXTURE_BUFFER_SIZE; j++) {
		GL_OPERATION(f, glActiveTexture(GL_TEXTURE0))
		GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[j][type][Y]))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
		GL_OPERATION(
		    f, glTexImage2D(GL_TEXTURE_2D, 0, texture_internal_format, w, h, 0, texture_format, GL_UNSIGNED_BYTE, 0))

		GL_OPERATION(f, glActiveTexture(GL_TEXTURE1))
		GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[j][type][U]))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
		GL_OPERATION(f, glTexImage2D(GL_TEXTURE_2D, 0, texture_internal_format, w >> 1, h >> 1, 0, texture_format,
		                             GL_UNSIGNED_BYTE, 0))

		GL_OPERATION(f, glActiveTexture(GL_TEXTURE2))
		GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[j][type][V]))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
		GL_OPERATION(f, glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
		GL_OPERATION(f, glTexImage2D(GL_TEXTURE_2D, 0, texture_internal_format, w >> 1, h >> 1, 0, texture_format,
		                             GL_UNSIGNED_BYTE, 0))
	}

	gldisp->allocatedTexturesSize[type].width = w;
	gldisp->allocatedTexturesSize[type].height = h;

	ms_message("[ogl_display] %s: allocated new textures[%d] (%d x %d)\n", __FUNCTION__, type, w, h);

	check_GL_errors(f, "allocate_gl_textures");
}

static int pixel_unpack_alignment(uint8_t *ptr, int datasize) {
	uintptr_t num_ptr = (uintptr_t)ptr;
	int alignment_ptr = !(num_ptr % 4) ? 4 : !(num_ptr % 2) ? 2 : 1;
	int alignment_data = !(datasize % 4) ? 4 : !(datasize % 2) ? 2 : 1;
	return (alignment_ptr <= alignment_data) ? alignment_ptr : alignment_data;
}

static void ogl_display_set_yuv(struct opengles_display *gldisp, mblk_t *yuv, enum ImageType type) {
	int j;

	if (!gldisp) {
		ms_error("[ogl_display] %s called with null struct opengles_display", __FUNCTION__);
		return;
	}

	ms_mutex_lock(&gldisp->yuv_mutex);

	if (gldisp->yuv[type]) {
		freemsg(gldisp->yuv[type]);
		gldisp->yuv[type] = NULL;
	}

	if (yuv) {
		gldisp->yuv[type] = dupmsg(yuv);
		for (j = 0; j < TEXTURE_BUFFER_SIZE; ++j) {
			gldisp->new_yuv_image[j][type] = TRUE;
		}
	}

	ms_mutex_unlock(&gldisp->yuv_mutex);
}

static bool_t update_textures_with_yuv(struct opengles_display *gldisp, enum ImageType type) {
	const OpenGlFunctions *f = gldisp->functions;

	unsigned int aligned_yuv_w, aligned_yuv_h;
	int alignment = 0;
	MSPicture yuvbuf;

	ms_yuv_buf_init_from_mblk(&yuvbuf, gldisp->yuv[type]);

	if (yuvbuf.w == 0 || yuvbuf.h == 0) {
		ms_warning("[ogl_display] Incoherent image size: %dx%d\n", yuvbuf.w, yuvbuf.h);
		return FALSE;
	}

	aligned_yuv_w = align_on_power_of_2(yuvbuf.w);
	aligned_yuv_h = align_on_power_of_2(yuvbuf.h);

	/* check if we need to adjust texture sizes */
	if (aligned_yuv_w != (unsigned int)gldisp->allocatedTexturesSize[type].width ||
	    aligned_yuv_h != (unsigned int)gldisp->allocatedTexturesSize[type].height)
		allocate_gl_textures(gldisp, aligned_yuv_w, aligned_yuv_h, type);

	/* We must add 2 to width and height due to a precision issue with the division */
	gldisp->uvx[type] = yuvbuf.w / (float)(gldisp->allocatedTexturesSize[type].width + 2);
	gldisp->uvy[type] = yuvbuf.h / (float)(gldisp->allocatedTexturesSize[type].height + 2);

	/* alignment of pointers and datasize */
	{
		int alig_Y = pixel_unpack_alignment(yuvbuf.planes[Y], yuvbuf.w * yuvbuf.h);
		int alig_U = pixel_unpack_alignment(yuvbuf.planes[U], yuvbuf.w >> 1);
		int alig_V = pixel_unpack_alignment(yuvbuf.planes[V], yuvbuf.w >> 1);
		alignment = (alig_U > alig_V) ? ((alig_V > alig_Y) ? alig_Y : alig_V) : ((alig_U > alig_Y) ? alig_Y : alig_U);
	}

	const GLenum texture_format = gldisp->texFormat;
	/* upload Y plane */
	GL_OPERATION(f, glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][Y]))
	GL_OPERATION(f, glPixelStorei(GL_UNPACK_ALIGNMENT, alignment))
	GL_OPERATION(f, glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, yuvbuf.w, yuvbuf.h, texture_format, GL_UNSIGNED_BYTE,
	                                yuvbuf.planes[Y]))

	GL_OPERATION(f, glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_Y], 0))

	/* upload U plane */
	GL_OPERATION(f, glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][U]))
	GL_OPERATION(f, glPixelStorei(GL_UNPACK_ALIGNMENT, alignment))
	GL_OPERATION(f, glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, yuvbuf.w >> 1, yuvbuf.h >> 1, texture_format,
	                                GL_UNSIGNED_BYTE, yuvbuf.planes[U]))

	GL_OPERATION(f, glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_U], 1))

	/* upload V plane */
	GL_OPERATION(f, glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][V]))
	GL_OPERATION(f, glPixelStorei(GL_UNPACK_ALIGNMENT, alignment))
	GL_OPERATION(f, glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, yuvbuf.w >> 1, yuvbuf.h >> 1, texture_format,
	                                GL_UNSIGNED_BYTE, yuvbuf.planes[V]))

	GL_OPERATION(f, glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_V], 2))
	gldisp->yuv_size[type].width = yuvbuf.w;
	gldisp->yuv_size[type].height = yuvbuf.h;

	check_GL_errors(f, "update_textures_with_yuv");

	return TRUE;
}

static void ogl_display_render_type(struct opengles_display *gldisp,
                                    enum ImageType type,
                                    bool_t clear,
                                    float vpx,
                                    float vpy,
                                    float vpw,
                                    float vph,
                                    int orientation,
                                    MSVideoDisplayMode mode) {
	if (!gldisp) {
		ms_error("[ogl_display] %s called with null struct opengles_display", __FUNCTION__);
		return;
	}
	if (!gldisp->yuv[type] || !gldisp->glResourcesInitialized) {
		return;
	}

	const OpenGlFunctions *f = gldisp->functions;

	GLint bound_buffer;
	GL_OPERATION(f, glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &bound_buffer))
	if ((GLuint)bound_buffer != gldisp->arrayBufferHandle) {
		GL_OPERATION(f, glBindBuffer(GL_ARRAY_BUFFER, gldisp->arrayBufferHandle))
		if (f->glBindVertexArray) { // OpenGL 3.0+
			GL_OPERATION(f, glBindVertexArray(gldisp->vertexArrayObjectHandle))
		} else { // OpenGL 2.0-2.1
			bind_vertex_shader_input(f);
		}
	}

	if (clear) GL_OPERATION(f, glClear(GL_COLOR_BUFFER_BIT))

	ms_mutex_lock(&gldisp->yuv_mutex);
	if (gldisp->new_yuv_image[gldisp->texture_index][type]) {
		update_textures_with_yuv(gldisp, type);
		gldisp->new_yuv_image[gldisp->texture_index][type] = FALSE;
	}
	ms_mutex_unlock(&gldisp->yuv_mutex);

	// drawing surface dimensions
	GLfloat screenW = (GLfloat)gldisp->backingWidth;
	GLfloat screenH = (GLfloat)gldisp->backingHeight;
	if (orientation == 90 || orientation == 270) {
		screenW = screenH;
		screenH = (GLfloat)gldisp->backingWidth;
	}

	MSVideoDisplayMode usedMode = mode;
	if (mode == MSVideoDisplayHybrid) {
		// If the image has the same orientation as the screen then use OccupyAllSpace mode
		if ((screenW > screenH && gldisp->yuv_size[type].width > gldisp->yuv_size[type].height) ||
		    (screenH > screenW && gldisp->yuv_size[type].height > gldisp->yuv_size[type].width)) {
			usedMode = MSVideoDisplayOccupyAllSpace;
		} else {
			usedMode = MSVideoDisplayBlackBars;
		}
	}

	// Fill the smallest dimension, then compute the other one using the image ratio
	GLfloat w, h;
	if (usedMode == MSVideoDisplayBlackBars) {
		if (screenW <= screenH) {
			float ratio = gldisp->yuv_size[type].height / (float)(gldisp->yuv_size[type].width);
			w = screenW * vpw;
			h = w * ratio;
			if (h > screenH) {
				w *= screenH / (float)h;
				h = screenH;
			}
		} else {
			float ratio = gldisp->yuv_size[type].width / (float)(gldisp->yuv_size[type].height);
			h = screenH * vph;
			w = h * ratio;
			if (w > screenW) {
				h *= screenW / (float)w;
				w = screenW;
			}
		}
	} else {
		if (screenW > screenH) {
			if (gldisp->yuv_size[type].width > gldisp->yuv_size[type].height) {
				float ratio = gldisp->yuv_size[type].width / (float)(gldisp->yuv_size[type].height);
				if ((screenW / screenH) > ratio) {
					w = screenW * vpw;
					h = w * (gldisp->yuv_size[type].height / (float)(gldisp->yuv_size[type].width));
				} else {
					h = screenH * vph;
					w = h * ratio;
				}
			} else {
				w = screenW * vpw;
				h = w * (gldisp->yuv_size[type].height / (float)(gldisp->yuv_size[type].width));
			}
		} else {
			if (gldisp->yuv_size[type].height > gldisp->yuv_size[type].width) {
				float ratio = gldisp->yuv_size[type].height / (float)(gldisp->yuv_size[type].width);
				if ((screenH / screenW) > ratio) {
					h = screenH * vph;
					w = h * (gldisp->yuv_size[type].width / (float)(gldisp->yuv_size[type].height));
				} else {
					w = screenW * vpw;
					h = w * ratio;
				}
			} else {
				h = screenH * vph;
				w = h * (gldisp->yuv_size[type].width / (float)(gldisp->yuv_size[type].height));
			}
		}
	}

	GLfloat x = vpx * screenW;
	GLfloat y = vpy * screenH;

	GLfloat xLeft = (x - w * 0.5f) / screenW;
	GLfloat yTop = (y - h * 0.5f) / screenH;
	GLfloat xRight = (x + w * 0.5f) / screenW;
	GLfloat yBottom = (y + h * 0.5f) / screenH;

	GLfloat uLeft = 0.0f;
	GLfloat vBottom = 0.0f;
	GLfloat uRight = gldisp->uvx[type];
	GLfloat vTop = gldisp->uvy[type];

	GLfloat arrayBuffer[] = {
	    // Relative coordinates (UV) of the decoded image
	    uLeft,
	    vTop,
	    uRight,
	    vTop,
	    uLeft,
	    vBottom,
	    uRight,
	    vBottom,

	    // X and Y coordinates of the rectangle where the image has to be displayed
	    xLeft,
	    yTop,
	    xRight,
	    yTop,
	    xLeft,
	    yBottom,
	    xRight,
	    yBottom,
	};

	GLfloat pLeft, pRight, pTop, pBottom, pNear, pFar;
#define VP_SIZE 1.0f
	if (type == REMOTE_IMAGE) {
		float scale_factor = 1.0f / gldisp->zoom_factor;
		float vpDim = (VP_SIZE * scale_factor) / 2;

#define ENSURE_RANGE_A_INSIDE_RANGE_B(a, aSize, bMin, bMax)                                                            \
	if (2 * aSize >= (bMax - bMin)) a = 0;                                                                             \
	else if ((a - aSize < bMin) || (a + aSize > bMax)) {                                                               \
		float diff;                                                                                                    \
		if (a - aSize < bMin) diff = bMin - (a - aSize);                                                               \
		else diff = bMax - (a + aSize);                                                                                \
		a += diff;                                                                                                     \
	}

		ENSURE_RANGE_A_INSIDE_RANGE_B(gldisp->zoom_cx, vpDim, xLeft, xRight)
		ENSURE_RANGE_A_INSIDE_RANGE_B(gldisp->zoom_cy, vpDim, yTop, yBottom)

		pLeft = gldisp->zoom_cx - vpDim;
		pRight = gldisp->zoom_cx + vpDim;
		pBottom = gldisp->zoom_cy - vpDim;
		pTop = gldisp->zoom_cy + vpDim;
		pNear = 0;
		pFar = 0.5;
	} else {
		pLeft = -VP_SIZE * 0.5;
		pRight = VP_SIZE * 0.5;
		pBottom = -VP_SIZE * 0.5;
		pTop = VP_SIZE * 0.5;
		pNear = 0;
		pFar = 0.5;
	}
	const bool_t mirror = gldisp->do_mirroring[type];

	GLfloat mat[16];
	load_projection_matrix(pLeft, pRight, pBottom, pTop, pNear, pFar, vpx, mirror, mat);

	GL_OPERATION(f, glUniformMatrix4fv(gldisp->uniforms[UNIFORM_PROJ_MATRIX], 1, GL_FALSE, mat))

	GLfloat rad = (2.0f * 3.14157f * orientation / 360.0f); // Convert orientation to radian
	GL_OPERATION(f, glUniform1f(gldisp->uniforms[UNIFORM_ROTATION], rad))

	GL_OPERATION(f, glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][Y]))
	GL_OPERATION(f, glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_Y], 0))
	GL_OPERATION(f, glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][U]))
	GL_OPERATION(f, glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_U], 1))
	GL_OPERATION(f, glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(f, glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][V]))
	GL_OPERATION(f, glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_V], 2))

	GL_OPERATION(f, glBufferSubData(GL_ARRAY_BUFFER, (GLint *)0, (GLsizei *)ARRAY_BUFFER_SIZE, arrayBuffer))

	GL_OPERATION(f, glDrawArrays(GL_TRIANGLE_STRIP, 0, 4))

	check_GL_errors(f, "ogl_display_render_type");
}

// -----------------------------------------------------------------------------
struct opengles_display *ogl_display_new() {
	struct opengles_display *result = (struct opengles_display *)malloc(sizeof(struct opengles_display));
	if (result == 0) {
		ms_error("[ogl_display] Could not allocate OpenGL display structure");
		return NULL;
	}

	memset(result, 0, sizeof(struct opengles_display));

	result->zoom_factor = 1;
	result->zoom_cx = result->zoom_cy = 0;
	result->texture_index = 0;
	result->texInternalFormat = GL_R8;
	result->texFormat = GL_RED;

	static const EGLint request_opengl_4_1_core[] = {
	    EGL_CONTEXT_MAJOR_VERSION,           4,       EGL_CONTEXT_MINOR_VERSION, 1, EGL_CONTEXT_OPENGL_PROFILE_MASK,
	    EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
	static const MSEGLContextDescriptor default_context = {EGL_OPENGL_API, request_opengl_4_1_core};

	result->mEglDisplay = EGL_NO_DISPLAY;
	result->target_context = &default_context;
	result->mEglContext = EGL_NO_CONTEXT;
	result->mRenderSurface = EGL_NO_SURFACE;
	result->shadersLoaded = FALSE;
	result->arrayBufferHandle = 0;
	result->vertexArrayObjectHandle = 0;
	result->mSendUnrecoverableError = FALSE;

	ms_mutex_init(&result->yuv_mutex, NULL);
	ms_message("[ogl_display] %s : %p\n", __FUNCTION__, result);

	return result;
}

void ogl_display_clean(struct opengles_display *gldisp) {
	if (gldisp->mEglDisplay != EGL_NO_DISPLAY) {
		if (gldisp->functions->eglInitialized) {
			gldisp->functions->eglMakeCurrent(
			    gldisp->mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
			    EGL_NO_CONTEXT); // Allow OpenGL to Release memory without delay as the current is no more binded
			check_EGL_errors(gldisp, "ogl_display_clean: eglMakeCurrent");
		}
		if (gldisp->mRenderSurface != EGL_NO_SURFACE) {
			if (gldisp->functions->eglInitialized) {
				gldisp->functions->eglDestroySurface(gldisp->mEglDisplay, gldisp->mRenderSurface);
				check_EGL_errors(gldisp, "ogl_display_clean: eglDestroySurface");
			}
			gldisp->mRenderSurface = EGL_NO_SURFACE;
		}
		if (gldisp->mEglContext != EGL_NO_CONTEXT) {
			// if( gldisp->functions->eglInitialized){
			//	gldisp->functions->eglDestroyContext(gldisp->mEglDisplay, gldisp->mEglContext);// This lead to crash on
			// future eglMakeCurrent. Bug? Let eglReleaseThread to do it. 	check_EGL_errors(gldisp,
			//"ogl_display_clean: eglDestroyContext");
			// }
			gldisp->mEglContext = EGL_NO_CONTEXT;
		}
		// if( gldisp->functions->eglInitialized){
		//	gldisp->functions->eglTerminate(gldisp->mEglDisplay);// Do not call terminate. It will delete all other
		// context/rendering surface that can be still in used. Let eglReleaseThread to do it. Reminder : mEglDisplay is
		// the same for all. 	check_EGL_errors(gldisp, "ogl_display_clean: eglTerminate");
		// }
		if (gldisp->functions->eglInitialized) {
			gldisp->functions->eglReleaseThread(); // Release all OpenGL resources
			check_EGL_errors(gldisp, "ogl_display_clean: eglReleaseThread");
			gldisp->functions->glFinish(); // Synchronize the clean
			check_EGL_errors(gldisp, "ogl_display_clean: glFinish");
		}
		gldisp->mEglDisplay = EGL_NO_DISPLAY;
	}
}

void ogl_display_free(struct opengles_display *gldisp) {
	int i;

	if (!gldisp) {
		ms_error("[ogl_display] %s called with null struct opengles_display", __FUNCTION__);
		return;
	}

	ogl_display_clean(gldisp);

	for (i = 0; i < MAX_IMAGE; i++) {
		if (gldisp->yuv[i]) {
			freemsg(gldisp->yuv[i]);
			gldisp->yuv[i] = NULL;
		}
	}
	if (gldisp->default_functions) {
		ms_free(gldisp->default_functions);
		gldisp->default_functions = NULL;
	}
	ms_mutex_destroy(&gldisp->yuv_mutex);

	free(gldisp);
}

void ogl_display_set_size(struct opengles_display *gldisp, int width, int height) {
	const OpenGlFunctions *f = gldisp->functions;
	if (f->glInitialized) {
		gldisp->backingWidth = width;
		gldisp->backingHeight = height;
		ms_message("[ogl_display] resize opengles_display (%d x %d, gl initialized:%d)", width, height,
		           gldisp->glResourcesInitialized);

		GL_OPERATION(f, glViewport(0, 0, width, height))
		check_GL_errors(f, "ogl_display_set_size");
	}
}

void ogl_display_set_target_context(struct opengles_display *gldisp,
                                    const MSEGLContextDescriptor *const target_context) {
	gldisp->target_context = target_context;
}

#ifdef HAVE_GLX

bool_t ogl_create_window(EGLNativeWindowType *window, void **window_id) {
	Display *dpy;
	Window root;
	static GLint visual_attribs[] = {GLX_X_RENDERABLE,
	                                 True,
	                                 GLX_DRAWABLE_TYPE,
	                                 GLX_WINDOW_BIT,
	                                 GLX_RENDER_TYPE,
	                                 GLX_RGBA_BIT,
	                                 GLX_RED_SIZE,
	                                 8,
	                                 GLX_GREEN_SIZE,
	                                 8,
	                                 GLX_BLUE_SIZE,
	                                 8,
	                                 GLX_DOUBLEBUFFER,
	                                 True,
	                                 None};
	XVisualInfo *vi;
	Colormap cmap;
	XSetWindowAttributes swa;
	const char *display = getenv("DISPLAY"); // For debug feedbacks

	dpy = XOpenDisplay(NULL); // NULL will look at DISPLAY variable
	if (dpy == NULL) {
		dpy = XOpenDisplay(":0"); // Try to 0
		if (dpy == NULL) {
			if (display != NULL) ms_error("[ogl_display] Could not open DISPLAY: %s", display);
			else ms_error("[ogl_display] Could not open DISPLAY.");
			*window = (EGLNativeWindowType)0;
			*window_id = NULL;
			return FALSE;
		}
	}
	XSync(dpy, False);
#ifdef HAVE_XV
	unsigned int adaptorsCount = 0;
	XvAdaptorInfo *xai = NULL;
	if (XvQueryAdaptors(dpy, DefaultRootWindow(dpy), &adaptorsCount, &xai) != 0) {
		ms_error("[ogl_display] XvQueryAdaptors failed.");
		return FALSE;
	} else if (adaptorsCount == 0) {
		if (display != NULL) ms_error("[ogl_display] Xvfb: No adaptors available on DISPLAY:%s", display);
		else ms_error("[ogl_display] Xvfb: No adaptors available on DISPLAY");
		return FALSE;
	}
#endif

	int glx_major, glx_minor;
	int fbcount;
	GLXFBConfig *fbc;
	int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;
	GLXFBConfig bestFbc;
	// FBConfigs were added in GLX version 1.3.
	if (!glXQueryVersion(dpy, &glx_major, &glx_minor) || ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1)) {
		ms_error("[ogl_display] Invalid GLX version");
		return FALSE;
	}

	ms_message("[ogl_display] Getting matching framebuffer configs");
	fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), visual_attribs, &fbcount);
	if (!fbc) {
		ms_error("[ogl_display] Failed to retrieve a framebuffer config");
		return FALSE;
	}
	ms_message("[ogl_display] Found %d matching FB configs.", fbcount);
	// Pick the FB config/visual with the most samples per pixel
	ms_message("[ogl_display] Getting XVisualInfos");

	for (int i = 0; i < fbcount; ++i) {
		XVisualInfo *vi = glXGetVisualFromFBConfig(dpy, fbc[i]);
		if (vi) {
			int samp_buf, samples;
			glXGetFBConfigAttrib(dpy, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
			glXGetFBConfigAttrib(dpy, fbc[i], GLX_SAMPLES, &samples);
			ms_message("[ogl_display] Matching fbconfig %d, visual ID 0x%lu: SAMPLE_BUFFERS = %d, SAMPLES = %d", i,
			           vi->visualid, samp_buf, samples);
			if (best_fbc < 0 || (samp_buf && samples > best_num_samp)) best_fbc = i, best_num_samp = samples;
			if (worst_fbc < 0 || (!samp_buf || samples < worst_num_samp)) worst_fbc = i, worst_num_samp = samples;
		}
		XFree(vi);
	}
	bestFbc = fbc[best_fbc];

	// Be sure to free the FBConfig list allocated by glXChooseFBConfig()
	XFree(fbc);

	// Get a visual
	vi = glXGetVisualFromFBConfig(dpy, bestFbc);
	ms_message("[ogl_display] Chosen visual ID = 0x%lu", vi->visualid);

	root = RootWindow(dpy, vi->screen);
	cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	swa.colormap = cmap;
	swa.background_pixmap = None;
	swa.border_pixel = 0;
	swa.event_mask = StructureNotifyMask;
	ms_message("[ogl_display] Creating XWindow");
	*window = XCreateWindow(dpy, root, 200, 200, MS_VIDEO_SIZE_CIF_W, MS_VIDEO_SIZE_CIF_H, 0, vi->depth, InputOutput,
	                        vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);
	*window_id = dpy;
	XStoreName(dpy, *window, "Video");
	XMapWindow(dpy, *window);
	XFree(vi);
	XSync(dpy, False);
	return (*window) != (EGLNativeWindowType)0;
}

void ogl_destroy_window(EGLNativeWindowType *window, void **window_id) {
	if (*window) {
		Display *dpy = (Display *)*window_id;
		if (dpy != NULL) {
			XSync(dpy, FALSE);
			XDestroyWindow(dpy, *window);
			*window = (EGLNativeWindowType)0;
			XCloseDisplay(dpy);
			*window_id = NULL;
		}
	}
}
#elif defined(MS2_WINDOWS_UWP)

#include <Memorybuffer.h>
#include <agile.h>
#include <collection.h>
#include <mfidl.h>
#include <ppltasks.h>
#include <windows.h>

using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Core;
using namespace Windows::System::Threading;
using namespace Concurrency;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml;
using namespace Windows::Foundation;

using namespace Platform;

bool_t ogl_create_window(EGLNativeWindowType *window, Platform::Agile<CoreApplicationView> *windowId) {
	auto worker = ref new WorkItemHandler([window, windowId](IAsyncAction ^ workItem) {
		CoreApplicationView ^ coreView = CoreApplication::CreateNewView();
		*windowId = Platform::Agile<CoreApplicationView>(coreView);

		Concurrency::create_task(coreView->Dispatcher->RunAsync(
		                             CoreDispatcherPriority::Normal, ref new DispatchedHandler([window, windowId]() {
			                             auto layoutRoot = ref new Grid();
			                             SwapChainPanel ^ panel = ref new SwapChainPanel();
			                             panel->HorizontalAlignment = HorizontalAlignment::Stretch;
			                             panel->VerticalAlignment = VerticalAlignment::Stretch;

			                             layoutRoot->Children->Append(panel);
			                             Window::Current->Content = layoutRoot;
			                             Window::Current->Activate();
			                             *window = (EGLNativeWindowType)panel;
			                             ApplicationViewSwitcher::TryShowAsStandaloneAsync(
			                                 Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->Id);
		                             })))
		    .wait();
	});
	Concurrency::create_task(ThreadPool::RunAsync(worker)).then([] {}).wait();
	return (*window) != (EGLNativeWindowType)0;
}

void ogl_destroy_window(EGLNativeWindowType *window, Platform::Agile<CoreApplicationView> *windowId) {
	if (windowId->Get()) {
		Concurrency::create_task(windowId->Get()->Dispatcher->RunAsync(
		                             CoreDispatcherPriority::Normal,
		                             ref new DispatchedHandler([]() { CoreWindow::GetForCurrentThread()->Close(); })))
		    .wait();
		*window = (EGLNativeWindowType)0;
		*windowId = NULL;
	}
}
#elif defined(_WIN32)

struct WindowsThreadData {
	HWND window;       // Window to monitor
	bool_t stop;       // Flag to stop event loop thread
	ms_cond_t running; // Signal to indicate that the thread is running
	ms_mutex_t locker;
	ms_thread_t thread;
};

LRESULT CALLBACK WindowProc(__in HWND hWindow, __in UINT uMsg, __in WPARAM wParam, __in LPARAM lParam) {
	switch (uMsg) {
		case WM_CLOSE:
			ShowWindow(hWindow,
			           SW_HIDE); // Destroying windows is reserved to mediatreamer. We just hide the Window on Close.
			break;
		default:
			return DefWindowProc(hWindow, uMsg, wParam, lParam);
	}
	return 0;
}

// CreateWindow need to be in the same thread as Message loop. If not, PeekMessage/GetMessage will not receive all
// events. We need the event loop to avoid unresponding window.
static void *window_thread(void *p) {
	WindowsThreadData *data = (WindowsThreadData *)p;
#ifdef UNICODE
	const wchar_t className[] = L"Video";
	const wchar_t title[] = L"Video";
#else
	const char className[] = "Video";
	const char title[] = "Video";
#endif
	HINSTANCE hInstance = ::GetModuleHandle(NULL);
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = className;
	RegisterClass(&wc);
	data->window = CreateWindow(className, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
	                            CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
	if (data->window != NULL) {
		ShowWindow(data->window, SW_SHOWNORMAL);
	}
	ms_cond_signal(&data->running);
	while (!data->stop &&
	       data->window != NULL) { // No need to protect stop for threadsafe as it is a boolean and used like that
		MSG msg;
		while (PeekMessage(&msg, data->window, NULL, NULL, PM_REMOVE) != 0) {
			if (msg.message == WM_DESTROY || msg.message == WM_CLOSE) {
				data->stop = TRUE;
				break;
			} else {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	ms_thread_exit(NULL);
	return NULL;
}

bool_t ogl_create_window(EGLNativeWindowType *window, void **window_id) {
	WindowsThreadData *data = (WindowsThreadData *)ms_malloc(sizeof(WindowsThreadData));
	ms_cond_init(&data->running, NULL);
	ms_mutex_init(&data->locker, NULL);
	data->stop = FALSE;
	ms_thread_create(&data->thread, NULL, window_thread,
	                 data); // Create the event loop and wait when Window can be used
	ms_cond_wait(&data->running, &data->locker);
	*window_id = data;
	*window = data->window; // It is safe to use this data as the event loop doesn't make any changes on it after
	                        // signaling running
	return *window != NULL;
}

void ogl_destroy_window(EGLNativeWindowType *window, void **window_id) {
	if (window_id != NULL) {
		WindowsThreadData *data = (WindowsThreadData *)(*window_id);
		data->stop = TRUE; // Stop event loop and wait till its end
		ms_thread_join(data->thread, NULL);
		DestroyWindow(*window);
		ms_free(data);
		*window_id = NULL;
		*window = NULL;
	}
}
#else
bool_t ogl_create_window(BCTBX_UNUSED(EGLNativeWindowType *window), BCTBX_UNUSED(void **window_id)) {
	ms_error("[ogl_display] Ceating a Window is not supported for the current platform");
	return FALSE;
}
void ogl_destroy_window(BCTBX_UNUSED(EGLNativeWindowType *window), BCTBX_UNUSED(void **window_id)) {
}
#endif

/* Returns a context compatible with the given attributes.
   E.g. requesting a 2.0 context may return a 4.6 context in Compatibility profile
*/
static EGLContext ogl_create_context(const struct opengles_display *const gldisp,
                                     const MSEGLContextDescriptor *const desc) {
	const OpenGlFunctions *const f = gldisp->functions;
	if (!f->eglBindAPI(desc->api)) {
		return EGL_NO_CONTEXT;
	}

	return f->eglCreateContext(gldisp->mEglDisplay, gldisp->mEglConfig, EGL_NO_CONTEXT, desc->attrib_list);
}

static void
ogl_create_surface_default(struct opengles_display *gldisp, const OpenGlFunctions *f, EGLNativeWindowType window) {
	if (!f->eglInitialized) {
		return;
	}

#ifdef _WIN32
	// To initialize the display, we make three sets of calls to eglGetPlatformDisplayEXT and eglInitialize, with
	// varying parameters passed to eglGetPlatformDisplayEXT: 1) The first calls uses "defaultDisplayAttributes" as a
	// parameter. This corresponds to D3D11 Feature Level 10_0+. 2) If eglInitialize fails for step 1 (e.g. because
	// 10_0+ isn't supported by the default GPU), then we try again
	//    using "fl9_3DisplayAttributes". This corresponds to D3D11 Feature Level 9_3.
	// 3) If eglInitialize fails for step 2 (e.g. because 9_3+ isn't supported by the default GPU), then we try again
	//    using "warpDisplayAttributes".  This corresponds to D3D11 Feature Level 11_0 on WARP, a D3D11 software
	//    rasterizer.

	// ogl_display_clean(gldisp);// Clean the display before creating surface

	int defaultDisplayAttributes[] = {
	    // These are the default display attributes, used to request ANGLE's D3D11 renderer.
	    // eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
	    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
	    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
	    // EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call
	    // the IDXGIDevice3.Trim method on behalf of the application when it gets suspended.
	    // Calling IDXGIDevice3.Trim when an application is suspended is a Windows Store application certification
	    // requirement.
	    EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
	    EGL_TRUE,
	    EGL_NONE,
	};
	// This tries to initialize EGL to D3D11 Feature Level 10_0+. See above comment for details.
	gldisp->mEglDisplay =
	    f->eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
	check_EGL_errors(gldisp, "ogl_create_surface");
	if (gldisp->mEglDisplay == EGL_NO_DISPLAY) {
		ms_error("[ogl_display] Failed to get EGL display (D3D11 10.0+).");
	}

	int major = 0, minor = 0;
	if (f->eglInitialize(gldisp->mEglDisplay, &major, &minor) == EGL_FALSE) {
		// This tries to initialize EGL to D3D11 Feature Level 9_3, if 10_0+ is unavailable (e.g. on some mobile
		// devices).
		int fl9_3DisplayAttributes[] = {
		    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
		    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
		    EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
		    9,
		    EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
		    3,
		    EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
		    EGL_TRUE,
		    EGL_NONE,
		};
		gldisp->mEglDisplay =
		    f->eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
		if (gldisp->mEglDisplay == EGL_NO_DISPLAY) {
			ms_error("[ogl_display] Failed to get EGL display (D3D11 9.3).");
		}
		if (f->eglInitialize(gldisp->mEglDisplay, &major, &minor) == EGL_FALSE) {
			// This initializes EGL to D3D11 Feature Level 11_0 on WARP, if 9_3+ is unavailable on the default GPU.
			int warpDisplayAttributes[] = {
			    EGL_PLATFORM_ANGLE_TYPE_ANGLE,
			    EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
			    EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
			    EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
			    EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE,
			    EGL_TRUE,
			    EGL_NONE,
			};
			gldisp->mEglDisplay =
			    f->eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
			if (gldisp->mEglDisplay == EGL_NO_DISPLAY) {
				ms_error("[ogl_display] Failed to get EGL display (D3D11 11.0 WARP)");
			}
			if (f->eglInitialize(gldisp->mEglDisplay, &major, &minor) == EGL_FALSE) {
				// If all of the calls to eglInitialize returned EGL_FALSE then an error has occurred.
				ms_error("[ogl_display] Failed to initialize EGLDisplay");
			}
		}
	}
#else
	ogl_display_clean(gldisp); // Clean the display before creating surface

	// gldisp->mEglDisplay = f->eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_KHR, EGL_DEFAULT_DISPLAY,
	// defaultDisplayAttributes);
	gldisp->mEglDisplay = f->eglGetDisplay(EGL_DEFAULT_DISPLAY);
	check_EGL_errors(gldisp, "ogl_create_surface");
	if (gldisp->mEglDisplay == EGL_NO_DISPLAY) {
		ms_error("[ogl_display] Failed to get EGL display.");
	}

	int major = 0, minor = 0;
	if (f->eglInitialize(gldisp->mEglDisplay, &major, &minor) == EGL_FALSE) {
		ms_error("[ogl_display] Failed to initialize EGLDisplay");
	}
#endif

	check_EGL_errors(gldisp, "ogl_create_surface");
	ms_message("OpenEGL client API: %s", f->eglQueryString(gldisp->mEglDisplay, EGL_CLIENT_APIS));
	check_EGL_errors(gldisp, "ogl_create_surface");
	ms_message("OpenEGL vendor: %s", f->eglQueryString(gldisp->mEglDisplay, EGL_VENDOR));
	check_EGL_errors(gldisp, "ogl_create_surface");
	ms_message("OpenEGL version: %s", f->eglQueryString(gldisp->mEglDisplay, EGL_VERSION));
	check_EGL_errors(gldisp, "ogl_create_surface");
	ms_message("OpenEGL extensions: %s", f->eglQueryString(gldisp->mEglDisplay, EGL_EXTENSIONS));
	check_EGL_errors(gldisp, "ogl_create_surface");
	if (gldisp->mEglDisplay == EGL_NO_DISPLAY) {
		return;
	}

	int numConfigs = 0;
	EGLint configAttributes[] = {EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
	                             EGL_DEPTH_SIZE, 8, EGL_STENCIL_SIZE, 8, EGL_NONE};
	if (f->eglChooseConfig(gldisp->mEglDisplay, configAttributes, &gldisp->mEglConfig, 1, &numConfigs) == EGL_FALSE ||
	    numConfigs == 0) {
		ms_error("[ogl_display] Failed to choose first EGLConfig");
		check_EGL_errors(gldisp, "ogl_create_surface");
		return;
	}

	gldisp->mEglContext = ogl_create_context(gldisp, gldisp->target_context);
	if (gldisp->mEglContext == EGL_NO_CONTEXT) {
		ms_warning("[ogl_display] target EGL context creation failed. Falling back to OpenGL ES 2.0+ ...");
		static const EGLint request_opengl_2[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
		static const MSEGLContextDescriptor fallback = {EGL_OPENGL_ES_API, request_opengl_2};
		gldisp->mEglContext = ogl_create_context(gldisp, &fallback);
	}
	if (gldisp->mEglContext == EGL_NO_CONTEXT) {
		ms_error("[ogl_display] Failed to create EGL context");
		check_EGL_errors(gldisp, "ogl_create_surface");
	}
	gldisp->mRenderSurface = f->eglCreateWindowSurface(gldisp->mEglDisplay, gldisp->mEglConfig, window, NULL);
	if (gldisp->mRenderSurface == EGL_NO_SURFACE) {
		ms_error("[ogl_display] Failed to create EGL Render Surface");
		check_EGL_errors(gldisp, "ogl_create_surface");
	}
}

void ogl_create_surface(struct opengles_display *gldisp, const OpenGlFunctions *f, EGLNativeWindowType window) {
	if (window) {
		ogl_create_surface_default(gldisp, f, window);
	}
	if (gldisp->mRenderSurface == EGL_NO_SURFACE) { // Use pointers to set surface (we don't create)
		if (window) ms_error("[ogl_display] Couldn't create a eglCreateWindowSurface. Try to get one from EGL");
		if (f->eglInitialized) {
			if (gldisp->mEglDisplay == EGL_NO_DISPLAY) gldisp->mEglDisplay = f->eglGetCurrentDisplay();
			if (gldisp->mEglContext == EGL_NO_CONTEXT) gldisp->mEglContext = f->eglGetCurrentContext();
			if (gldisp->mRenderSurface == EGL_NO_SURFACE) gldisp->mRenderSurface = f->eglGetCurrentSurface(EGL_DRAW);
		}
		if (gldisp->mEglDisplay == EGL_NO_DISPLAY || gldisp->mEglContext == EGL_NO_CONTEXT ||
		    gldisp->mRenderSurface == EGL_NO_SURFACE) {
			ms_error("[ogl_display] Display/Context/Surface couldn't be set");
			check_EGL_errors(gldisp, "ogl_create_surface");
		}
	}
}

void ogl_display_auto_init(
    struct opengles_display *gldisp, const OpenGlFunctions *f, EGLNativeWindowType window, int width, int height) {
	if (!gldisp) {
		ms_error("[ogl_display] %s called with null struct opengles_display", __FUNCTION__);
		return;
	}
	// Create default functions if necessary. (No opengl functions given.)
	if (!gldisp->default_functions) {
		gldisp->default_functions = ms_new0(OpenGlFunctions, 1);
		if (f && f->getProcAddress) {
			gldisp->default_functions->getProcAddress = f->getProcAddress;
		}
		opengl_functions_default_init(gldisp->default_functions);
	}
	if (f && f->glInitialized) gldisp->functions = f;
	else gldisp->functions = gldisp->default_functions;

	if (!gldisp->functions) {
		ms_error("[ogl_display] functions is still NULL!");
		return;
	}

	ogl_create_surface(gldisp, gldisp->functions, window);
	if (gldisp->functions->eglInitialized) {
		gldisp->functions->eglMakeCurrent(gldisp->mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (gldisp->mRenderSurface == EGL_NO_SURFACE || gldisp->mEglContext == EGL_NO_CONTEXT ||
		    gldisp->functions->eglMakeCurrent(gldisp->mEglDisplay, gldisp->mRenderSurface, gldisp->mRenderSurface,
		                                      gldisp->mEglContext) == EGL_FALSE) {
			ms_error("[ogl_display] Failed to make EGLSurface current");
		} else {
			if (gldisp->mRenderSurface != EGL_NO_SURFACE) {
				gldisp->functions->eglQuerySurface(gldisp->mEglDisplay, gldisp->mRenderSurface, EGL_WIDTH, &width);
				gldisp->functions->eglQuerySurface(gldisp->mEglDisplay, gldisp->mRenderSurface, EGL_HEIGHT, &height);
			} // On failure, eglQuerySurface doesn't change output. It is safe to use it on all cases.
		}
	}
	if (width != 0 && height != 0) ogl_display_init(gldisp, gldisp->functions, width, height);
}

void ogl_display_init(struct opengles_display *gldisp, const OpenGlFunctions *f, int width, int height) {
	static bool_t version_displayed = FALSE;
	int i, j;

	if (!gldisp) {
		ms_error("[ogl_display] %s called with null struct opengles_display", __FUNCTION__);
		return;
	}
	gldisp->mSendUnrecoverableError = FALSE;
	gldisp->mLastError = 0;
	// Create default functions if necessary. (No opengl functions given.)
	if (!gldisp->default_functions) {
		gldisp->default_functions = ms_new0(OpenGlFunctions, 1);
		if (f && f->getProcAddress) {
			gldisp->default_functions->getProcAddress = f->getProcAddress;
		}
		opengl_functions_default_init(gldisp->default_functions);
	}
	if (f && f->glInitialized) gldisp->functions = f;
	else gldisp->functions = gldisp->default_functions;

	ms_message("[ogl_display] init opengles_display (%d x %d, gl initialized:%d)", width, height,
	           gldisp->glResourcesInitialized);
	if (gldisp->functions == NULL || !gldisp->functions->glInitialized)
		ms_error("[ogl_display] OpenGL functions have not been initialized");
	else {
		if (!version_displayed) {
			version_displayed = TRUE;
			ms_message("OpenGL version string: %s", gldisp->functions->glGetString(GL_VERSION));
			ms_message("OpenGL extensions: %s", gldisp->functions->glGetString(GL_EXTENSIONS));
			ms_message("OpenGL vendor: %s", gldisp->functions->glGetString(GL_VENDOR));
			ms_message("OpenGL renderer: %s", gldisp->functions->glGetString(GL_RENDERER));
			ms_message("OpenGL version: %s", gldisp->functions->glGetString(GL_VERSION));
			ms_message("OpenGL GLSL version: %s", gldisp->functions->glGetString(GL_SHADING_LANGUAGE_VERSION));
			check_GL_errors(gldisp->functions, "glGetString");
		}
		clean_GL_errors(gldisp->functions);

		GL_OPERATION(gldisp->functions, glDisable(GL_DEPTH_TEST))
		GL_OPERATION(gldisp->functions, glDisable(GL_SCISSOR_TEST))
		GL_OPERATION(gldisp->functions, glClearColor(0, 0, 0, 0))

		ogl_display_set_size(gldisp, width, height);

		if (gldisp->glResourcesInitialized) return;

		for (j = 0; j < TEXTURE_BUFFER_SIZE; j++) {
			// init textures
			for (i = 0; i < MAX_IMAGE; i++) {
				GL_OPERATION(gldisp->functions, glGenTextures(3, gldisp->textures[j][i]))
				gldisp->allocatedTexturesSize[i].width = gldisp->allocatedTexturesSize[i].height = 0;
			}
		}

		check_GL_errors(gldisp->functions, "ogl_display_init");

		gldisp->glResourcesInitialized = TRUE;
		gldisp->shadersLoaded = load_shaders(gldisp);

		if (!gldisp->shadersLoaded) {
			ms_error("[ogl_display] Failed to load shaders. Cleaning up...");
			ogl_display_uninit(gldisp, TRUE);
		}
	}

#ifdef ENABLE_OPENGL_PROFILING
	// With the French locale, floats are printed with ',' which does not play
	// nice with csv files.
	setlocale(LC_NUMERIC, "POSIX");

	gldisp->functions->glGenQueries(1, &gl_time_query);
#endif
}

void ogl_display_uninit(struct opengles_display *gldisp, bool_t freeGLresources) {
	int i, j;
	const OpenGlFunctions *f;

	if (!gldisp) {
		ms_error("[ogl_display] %s called with null struct opengles_display", __FUNCTION__);
		return;
	}

	ms_message("[ogl_display] uninit opengles_display (gl initialized:%d)\n", gldisp->glResourcesInitialized);

	for (i = 0; i < MAX_IMAGE; i++) {
		if (gldisp->yuv[i]) {
			freemsg(gldisp->yuv[i]);
			gldisp->yuv[i] = NULL;
		}
	}

	f = gldisp->functions;

	if (gldisp->glResourcesInitialized && freeGLresources) {
		// destroy gl resources
		for (j = 0; j < TEXTURE_BUFFER_SIZE; j++) {
			for (i = 0; i < MAX_IMAGE; i++) {
				GL_OPERATION(f, glDeleteTextures(3, gldisp->textures[j][i]));
				gldisp->allocatedTexturesSize[i].width = gldisp->allocatedTexturesSize[i].height = 0;
			}
		}
		if (f->glInitialized) GL_OPERATION(f, glDeleteProgram(gldisp->program));
		ogl_display_clean(gldisp);
	}

	if (f) check_GL_errors(f, "ogl_display_uninit");

	gldisp->glResourcesInitialized = FALSE;
}

void ogl_display_set_yuv_to_display(struct opengles_display *gldisp, mblk_t *yuv) {
	ogl_display_set_yuv(gldisp, yuv, REMOTE_IMAGE);
}

mblk_t *ogl_display_get_yuv_to_display(struct opengles_display *gldisp) {
	return gldisp->yuv[REMOTE_IMAGE];
}

void ogl_display_set_preview_yuv_to_display(struct opengles_display *gldisp, mblk_t *yuv) {
	ogl_display_set_yuv(gldisp, yuv, PREVIEW_IMAGE);
}

void ogl_display_render(struct opengles_display *gldisp, int orientation, MSVideoDisplayMode mode) {
	const OpenGlFunctions *f = gldisp->functions;
	bool_t render = TRUE;
	if (!f) // Do no try to render if functions are not defined.
		return;
	check_GL_errors(f, "ogl_display_render");
	clean_GL_errors(f);
#ifdef ENABLE_OPENGL_PROFILING
	f->glFinish();
	f->glBeginQuery(GL_TIME_ELAPSED, gl_time_query);
	clock_t start_time = clock();
	struct timespec wall_start_time;
	clock_gettime(CLOCK_MONOTONIC, &wall_start_time);
#endif

	if (gldisp->functions->eglInitialized) {
		if (gldisp->mRenderSurface != EGL_NO_SURFACE &&
		    gldisp->functions->eglMakeCurrent(gldisp->mEglDisplay, gldisp->mRenderSurface, gldisp->mRenderSurface,
		                                      gldisp->mEglContext) ==
		        EGL_FALSE) { // No need to test other variable as if mRenderSurface is set, then others are too.
			ms_error("[ogl_display] Failed to make EGLSurface current");
			render = FALSE;
		} else {
			int width = 0, height = 0; // Get current surface size from EGL if we can
			if (gldisp->mRenderSurface != EGL_NO_SURFACE &&
			    EGL_TRUE == gldisp->functions->eglQuerySurface(gldisp->mEglDisplay, gldisp->mRenderSurface, EGL_WIDTH,
			                                                   &width) &&
			    EGL_TRUE == gldisp->functions->eglQuerySurface(gldisp->mEglDisplay, gldisp->mRenderSurface, EGL_HEIGHT,
			                                                   &height)) {
				if (width == 0 || height == 0) {
					ms_warning("Is eglQuerySurface() working ? it returned %ix%i.", width, height);
				} else if (width != gldisp->backingWidth ||
				           height !=
				               gldisp->backingHeight) { // Size has changed : update display (buffers, viewport, etc.)
					ogl_display_init(gldisp, f, width, height);
				}
			}
		}
	}
	if (render && gldisp->functions->glInitialized && gldisp->shadersLoaded) {
		GL_OPERATION(f, glClearColor(0.f, 0.f, 0.f, 0.f));
		GL_OPERATION(f, glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		GL_OPERATION(f, glUseProgram(gldisp->program))
		check_GL_errors(f, "ogl_display_render");
		ogl_display_render_type(gldisp, REMOTE_IMAGE, TRUE, 0, 0, 1, 1, orientation, mode);
		// preview image already have the correct orientation
		ogl_display_render_type(gldisp, PREVIEW_IMAGE, FALSE, 0.4f, -0.4f, 0.2f, 0.2f, 0, MSVideoDisplayBlackBars);
		gldisp->texture_index = (gldisp->texture_index + 1) % TEXTURE_BUFFER_SIZE;
		if (f->eglInitialized && gldisp->mRenderSurface != EGL_NO_SURFACE)
			f->eglSwapBuffers(gldisp->mEglDisplay, gldisp->mRenderSurface);
	}

#ifdef ENABLE_OPENGL_PROFILING
	f->glEndQuery(GL_TIME_ELAPSED);
	f->glFinish();

	double elapsed_cpu_time = (double)(clock() - start_time) / CLOCKS_PER_SEC * 1000.f;

	GLuint64 elapsed_gpu_time;
	f->glGetQueryObjectui64v(gl_time_query, GL_QUERY_RESULT, &elapsed_gpu_time);

	struct timespec wall_end_time;
	clock_gettime(CLOCK_MONOTONIC, &wall_end_time);

	double elapsed_wall_clock_time = (double)(wall_end_time.tv_nsec - wall_start_time.tv_nsec) / 1000000.f +
	                                 (double)(wall_end_time.tv_sec - wall_start_time.tv_sec) * 1000.f;

	ms_message("<Magic for grep>, CPU (ms), GPU (ms), Wall clock (ms):\n RUgx7N %f %f %f", elapsed_cpu_time,
	           (double)elapsed_gpu_time / 1000000.f, elapsed_wall_clock_time);
#endif
}

void ogl_display_zoom(struct opengles_display *gldisp, float *params) {
	gldisp->zoom_factor = params[0];
	gldisp->zoom_cx = params[1] - 0.5f;
	gldisp->zoom_cy = params[2] - 0.5f;
}

static void ogl_display_enable_mirroring(struct opengles_display *gldisp, bool_t enabled, enum ImageType type) {
	gldisp->do_mirroring[type] = enabled;
}

void ogl_display_enable_mirroring_to_display(struct opengles_display *gldisp, bool_t enabled) {
	ogl_display_enable_mirroring(gldisp, enabled, REMOTE_IMAGE);
}

void ogl_display_enable_mirroring_to_preview(struct opengles_display *gldisp, bool_t enabled) {
	ogl_display_enable_mirroring(gldisp, enabled, PREVIEW_IMAGE);
}

void ogl_display_notify_errors(struct opengles_display *display, MSFilter *filter) {
	if (filter) {
		GLenum lastError =
		    display->mLastError; // We ensure to get the last error one time in order to avoid changes while checking.
		if (!display->mSendUnrecoverableError &&
		    lastError == EGL_CONTEXT_LOST) { // TODO: This notification is only send one time for the life of the
			                                 // display. As this is the only one to take account (for now), we can
			                                 // ensure to not send other notifications till the restart.
			// Some work has to be done to avoid sending multiple notifications for errors that are recoverable for the
			// current display.
			display->mSendUnrecoverableError = TRUE;
			ms_filter_notify(filter, MS_VIDEO_DISPLAY_ERROR_OCCURRED, &lastError);
		}
		display->mLastError = 0; // Resetting lastError to 0 will let the error manager to fill new errors.
	}
}
// -----------------------------------------------------------------------------

#ifdef __ANDROID__
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_init(
    BCTBX_UNUSED(JNIEnv *env), BCTBX_UNUSED(jobject obj), jlong ptr, jint width, jint height) {
	struct opengles_display *d = (struct opengles_display *)ptr;
	ogl_display_init(d, NULL, width, height);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_render(BCTBX_UNUSED(JNIEnv *env),
                                                                                          BCTBX_UNUSED(jobject obj),
                                                                                          jlong ptr) {
	struct opengles_display *d = (struct opengles_display *)ptr;
	ogl_display_render(d, 0, MSVideoDisplayBlackBars);
}
#endif

// #ifdef __cplusplus
// }
// #endif
