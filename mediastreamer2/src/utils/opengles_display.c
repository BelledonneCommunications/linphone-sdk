/*
 opengles_display.m
 Copyright (C) 2011 Belledonne Communications, Grenoble, France

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

#include "opengles_display.h"
#include "mediastreamer2/mscommon.h"
#include "shaderUtil.h"

#ifdef __cplusplus
extern "C"{
#endif

enum ImageType {
	REMOTE_IMAGE = 0,
	PREVIEW_IMAGE,
	MAX_IMAGE
};

/* helper functions */
static void check_GL_errors(const char* context);
static bool_t load_shaders(GLuint* program, GLint* uniforms);
static void allocate_gl_textures(struct opengles_display* gldisp, int w, int h, enum ImageType type);
static void load_orthographic_matrix(float left, float right, float bottom, float top, float near, float far, float* mat);
static unsigned int align_on_power_of_2(unsigned int value);
static bool_t update_textures_with_yuv(struct opengles_display* gldisp, enum ImageType type);


//#define CHECK_GL_ERROR

#ifdef CHECK_GL_ERROR
	#define GL_OPERATION(x)	\
		(x); \
		check_GL_errors(#x);
#else
	#define GL_OPERATION(x) \
		(x);
#endif

enum {
	UNIFORM_PROJ_MATRIX = 0,
	UNIFORM_ROTATION,
	UNIFORM_TEXTURE_Y,
	UNIFORM_TEXTURE_U,
	UNIFORM_TEXTURE_V,
	NUM_UNIFORMS
};

enum {
	ATTRIB_VERTEX = 0,
	ATTRIB_UV,
	NUM_ATTRIBS
};

enum {
	Y,
	U,
	V
};

#define TEXTURE_BUFFER_SIZE 3

struct opengles_display {
	/* input: yuv image to display */
	ms_mutex_t yuv_mutex;
	mblk_t *yuv[MAX_IMAGE];
	bool_t new_yuv_image[TEXTURE_BUFFER_SIZE][MAX_IMAGE];

	/* GL resources */
	bool_t glResourcesInitialized;
	GLuint program, textures[TEXTURE_BUFFER_SIZE][MAX_IMAGE][3];
	GLint uniforms[NUM_UNIFORMS];
	MSVideoSize allocatedTexturesSize[MAX_IMAGE];

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
};

struct opengles_display* ogl_display_new(void) {
	struct opengles_display* result =
		(struct opengles_display*) malloc(sizeof(struct opengles_display));
	if (result == 0) {
		ms_error("Could not allocate OpenGL display structure\n");
		return 0;
	}
	memset(result, 0, sizeof(struct opengles_display));
	result->zoom_factor = 1;
	result->zoom_cx = result->zoom_cy = 0;
	result->texture_index = 0;

	ms_mutex_init(&result->yuv_mutex, NULL);
	ms_message("%s : %p\n", __FUNCTION__, result);
	return result;
}

void ogl_display_free(struct opengles_display* gldisp) {
	int i;

	if (!gldisp) {
		ms_error("%s called with null struct opengles_display", __FUNCTION__);
		return;
	}

	for(i=0; i<MAX_IMAGE; i++) {
		if (gldisp->yuv[i]) {
			freemsg(gldisp->yuv[i]);
			gldisp->yuv[i] = NULL;
		}
	}
	ms_mutex_destroy(&gldisp->yuv_mutex);

	free(gldisp);
}

void ogl_display_set_size(struct opengles_display* gldisp, int width, int height) {
	gldisp->backingWidth = width;
	gldisp->backingHeight = height;
	ms_message("resize opengles_display (%d x %d, gl initialized:%d)", width, height, gldisp->glResourcesInitialized);

	GL_OPERATION(glViewport(0, 0, gldisp->backingWidth, gldisp->backingHeight));
	check_GL_errors("ogl_display_set_size");
}

void ogl_display_init(struct opengles_display* gldisp, int width, int height) {
	int i,j;
	static bool_t version_displayed = FALSE;
	if (!gldisp) {
		ms_error("%s called with null struct opengles_display", __FUNCTION__);
		return;
	}

	ms_message("init opengles_display (%d x %d, gl initialized:%d)", width, height, gldisp->glResourcesInitialized);

	GL_OPERATION(glDisable(GL_DEPTH_TEST))
	GL_OPERATION(glClearColor(0, 0, 0, 1))

	ogl_display_set_size(gldisp, width, height);

	if (gldisp->glResourcesInitialized)
		return;

	for(j=0; j<TEXTURE_BUFFER_SIZE; j++) {
		// init textures
		for(i=0; i<MAX_IMAGE; i++) {
			GL_OPERATION(glGenTextures(3, gldisp->textures[j][i]))
			gldisp->allocatedTexturesSize[i].width = gldisp->allocatedTexturesSize[i].height = 0;
		}
	}

	if (!version_displayed) {
		version_displayed = TRUE;
		ms_message("OpenGL version string: %s", glGetString(GL_VERSION));
		ms_message("OpenGL extensions: %s",glGetString(GL_EXTENSIONS));
		ms_message("OpenGL vendor: %s", glGetString(GL_VENDOR));
		ms_message("OpenGL renderer: %s", glGetString(GL_RENDERER));
		ms_message("OpenGL version: %s", glGetString(GL_VERSION));
		ms_message("OpenGL GLSL version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
	}
	load_shaders(&gldisp->program, gldisp->uniforms);

	GL_OPERATION(glUseProgram(gldisp->program))

	gldisp->glResourcesInitialized = TRUE;

	check_GL_errors("ogl_display_init");
}

void ogl_display_uninit(struct opengles_display* gldisp, bool_t freeGLresources) {
	int i,j;

	if (!gldisp) {
		ms_error("%s called with null struct opengles_display", __FUNCTION__);
		return;
	}
	ms_message("uninit opengles_display (gl initialized:%d)\n", gldisp->glResourcesInitialized);
	for(i=0; i<MAX_IMAGE; i++) {
		if (gldisp->yuv[i]) {
			freemsg(gldisp->yuv[i]);
			gldisp->yuv[i] = NULL;
		}
	}

	if (gldisp->glResourcesInitialized && freeGLresources) {
		// destroy gl resources
		for(j=0; j<TEXTURE_BUFFER_SIZE; j++) {
			for(i=0; i<MAX_IMAGE; i++) {
				GL_OPERATION(glDeleteTextures(3, gldisp->textures[j][i]));
				gldisp->allocatedTexturesSize[i].width = gldisp->allocatedTexturesSize[i].height = 0;
			}
		}
		GL_OPERATION(glDeleteProgram(gldisp->program));
	}

	gldisp->glResourcesInitialized = FALSE;

	check_GL_errors("ogl_display_uninit");
}

static void ogl_display_set_yuv(struct opengles_display* gldisp, mblk_t *yuv, enum ImageType type) {
	int j;
	if (!gldisp) {
		ms_error("%s called with null struct opengles_display", __FUNCTION__);
		return;
	}
	ms_mutex_lock(&gldisp->yuv_mutex);
	if (gldisp->yuv[type]){
		freemsg(gldisp->yuv[type]);
		gldisp->yuv[type]=NULL;
	}
	if (yuv){
		gldisp->yuv[type] = dupmsg(yuv);
		for(j = 0; j < TEXTURE_BUFFER_SIZE; ++j) {
			gldisp->new_yuv_image[j][type] = TRUE;
		}
	}
	ms_mutex_unlock(&gldisp->yuv_mutex);
}

void ogl_display_set_yuv_to_display(struct opengles_display* gldisp, mblk_t *yuv) {
	ogl_display_set_yuv(gldisp, yuv, REMOTE_IMAGE);
}

void ogl_display_set_preview_yuv_to_display(struct opengles_display* gldisp, mblk_t *yuv) {
	ogl_display_set_yuv(gldisp, yuv, PREVIEW_IMAGE);
}

static void ogl_display_render_type(struct opengles_display* gldisp, enum ImageType type, bool_t clear, float vpx, float vpy, float vpw, float vph, int orientation) {
	float uLeft, uRight, vTop, vBottom;
	GLfloat squareUvs[8];
	GLfloat squareVertices[8];
	int screenW, screenH;
	int x,y,w,h;
	GLfloat mat[16];
	float rad;

	if (!gldisp) {
		ms_error("%s called with null struct opengles_display", __FUNCTION__);
		return;
	}
	if (!gldisp->yuv[type] || !gldisp->glResourcesInitialized) {
		return;
	}

	ms_mutex_lock(&gldisp->yuv_mutex);
	if (gldisp->new_yuv_image[gldisp->texture_index][type]) {
		update_textures_with_yuv(gldisp, type);
		gldisp->new_yuv_image[gldisp->texture_index][type] = FALSE;
	}
	ms_mutex_unlock(&gldisp->yuv_mutex);


	uLeft = vBottom = 0.0f;
	uRight = gldisp->uvx[type];
	vTop = gldisp->uvy[type];

	squareUvs[0] = uLeft;
	squareUvs[1] =  vTop;
	squareUvs[2] = uRight;
	squareUvs[3] =  vTop;
	squareUvs[4] = uLeft;
	squareUvs[5] =  vBottom;
	squareUvs[6] = uRight;
	squareUvs[7] = vBottom;


	if (clear) {
		GL_OPERATION(glClear(GL_COLOR_BUFFER_BIT))
	}


	// drawing surface dimensions
	screenW = gldisp->backingWidth;
	screenH = gldisp->backingHeight;
	if (orientation == 90 || orientation == 270) {
		screenW = screenH;
		screenH = gldisp->backingWidth;
	}

	// Fill the smallest dimension, then compute the other one using the image ratio
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

	x = vpx * screenW;
	y = vpy * screenH;

	squareVertices[0] = (x - w * 0.5) / screenW - 0.;
	squareVertices[1] = (y - h * 0.5) / screenH - 0.;
	squareVertices[2] = (x + w * 0.5) / screenW - 0.;
	squareVertices[3] = (y - h * 0.5) / screenH - 0.;
	squareVertices[4] = (x - w * 0.5) / screenW - 0.;
	squareVertices[5] = (y + h * 0.5) / screenH - 0.;
	squareVertices[6] = (x + w * 0.5) / screenW - 0.;
	squareVertices[7] = (y + h * 0.5) / screenH - 0.;

	#define VP_SIZE 1.0f
	if (type == REMOTE_IMAGE) {
		float scale_factor = 1.0 / gldisp->zoom_factor;
		float vpDim = (VP_SIZE * scale_factor) / 2;

		#define ENSURE_RANGE_A_INSIDE_RANGE_B(a, aSize, bMin, bMax) \
		if (2*aSize >= (bMax - bMin)) \
			a = 0; \
		else if ((a - aSize < bMin) || (a + aSize > bMax)) {  \
			float diff; \
			if (a - aSize < bMin) diff = bMin - (a - aSize); \
			else diff = bMax - (a + aSize); \
			a += diff; \
		}

		ENSURE_RANGE_A_INSIDE_RANGE_B(gldisp->zoom_cx, vpDim, squareVertices[0], squareVertices[2])
		ENSURE_RANGE_A_INSIDE_RANGE_B(gldisp->zoom_cy, vpDim, squareVertices[1], squareVertices[7])

		load_orthographic_matrix(
			gldisp->zoom_cx - vpDim,
			gldisp->zoom_cx + vpDim,
			gldisp->zoom_cy - vpDim,
			gldisp->zoom_cy + vpDim,
			0, 0.5, mat);
	} else {
		load_orthographic_matrix(- VP_SIZE * 0.5, VP_SIZE * 0.5, - VP_SIZE * 0.5, VP_SIZE * 0.5, 0, 0.5, mat);
	}

	GL_OPERATION(glUniformMatrix4fv(gldisp->uniforms[UNIFORM_PROJ_MATRIX], 1, GL_FALSE, mat))

	rad = (2.0 * 3.14157 * orientation / 360.0); // Convert orientation to radian

	GL_OPERATION(glUniform1f(gldisp->uniforms[UNIFORM_ROTATION], rad))

	GL_OPERATION(glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][Y]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_Y], 0))
	GL_OPERATION(glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][U]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_U], 1))
	GL_OPERATION(glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][V]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_V], 2))

	GL_OPERATION(glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices))
	GL_OPERATION(glEnableVertexAttribArray(ATTRIB_VERTEX))
	GL_OPERATION(glVertexAttribPointer(ATTRIB_UV, 2, GL_FLOAT, 1, 0, squareUvs))
	GL_OPERATION(glEnableVertexAttribArray(ATTRIB_UV))

	GL_OPERATION(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4))

	check_GL_errors("ogl_display_render_type");

}

void ogl_display_render(struct opengles_display* gldisp, int orientation) {
	ogl_display_render_type(gldisp, REMOTE_IMAGE, TRUE, 0, 0, 1, 1, orientation);
	// preview image already have the correct orientation
	ogl_display_render_type(gldisp, PREVIEW_IMAGE, FALSE, 0.4f, -0.4f, 0.2f, 0.2f, 0);

	gldisp->texture_index = (gldisp->texture_index + 1) % TEXTURE_BUFFER_SIZE;
}

static void check_GL_errors(const char* context) {
	 int maxIterations=10;
	GLenum error;
	while (((error = glGetError()) != GL_NO_ERROR) && maxIterations > 0)
	{
		switch(error)
		{
			case GL_INVALID_ENUM:  ms_error("[%2d]GL error: '%s' -> GL_INVALID_ENUM\n", maxIterations, context); break;
			case GL_INVALID_VALUE: ms_error("[%2d]GL error: '%s' -> GL_INVALID_VALUE\n", maxIterations, context); break;
			case GL_INVALID_OPERATION: ms_error("[%2d]GL error: '%s' -> GL_INVALID_OPERATION\n", maxIterations, context); break;
			case GL_OUT_OF_MEMORY: ms_error("[%2d]GL error: '%s' -> GL_OUT_OF_MEMORY\n", maxIterations, context); break;
			case GL_INVALID_FRAMEBUFFER_OPERATION: ms_error("[%2d]GL error: '%s' -> GL_INVALID_FRAMEBUFFER_OPERATION\n", maxIterations, context); break;
			default:
				ms_error("[%2d]GL error: '%s' -> %x\n", maxIterations, context, error);
		}
		  maxIterations--;
	}
}

static bool_t load_shaders(GLuint* program, GLint* uniforms) {
	GLuint vertShader, fragShader;
#include "yuv2rgb.vs.h"
#include "yuv2rgb.fs.h"
	(void)yuv2rgb_vs_len;
	(void)yuv2rgb_fs_len;

	*program = glCreateProgram();
	if (!glueCompileShader(GL_VERTEX_SHADER, 1, (const char*)yuv2rgb_vs, &vertShader))
		return FALSE;
	if (!glueCompileShader(GL_FRAGMENT_SHADER, 1, (const char*)yuv2rgb_fs, &fragShader))
		return FALSE;

	GL_OPERATION(glAttachShader(*program, vertShader))
	GL_OPERATION(glAttachShader(*program, fragShader))

	GL_OPERATION(glBindAttribLocation(*program, ATTRIB_VERTEX, "position"))
	GL_OPERATION(glBindAttribLocation(*program, ATTRIB_UV, "uv"))

	if (!glueLinkProgram(*program))
		return FALSE;

	uniforms[UNIFORM_PROJ_MATRIX] = glGetUniformLocation(*program, "proj_matrix");
	uniforms[UNIFORM_ROTATION] = glGetUniformLocation(*program, "rotation");
	uniforms[UNIFORM_TEXTURE_Y] = glGetUniformLocation(*program, "t_texture_y");
	uniforms[UNIFORM_TEXTURE_U] = glGetUniformLocation(*program, "t_texture_u");
	uniforms[UNIFORM_TEXTURE_V] = glGetUniformLocation(*program, "t_texture_v");

	glDeleteShader(vertShader);
	glDeleteShader(fragShader);

	return TRUE;

	check_GL_errors("load_shaders");
}

static void load_orthographic_matrix(float left, float right, float bottom, float top, float near, float far, float* mat)
{
	float r_l = right - left;
	float t_b = top - bottom;
	float f_n = far - near;
	float tx = - (right + left) / (right - left);
	float ty = - (top + bottom) / (top - bottom);
	float tz = - (far + near) / (far - near);

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

static void allocate_gl_textures(struct opengles_display* gldisp, int w, int h, enum ImageType type) {
	int j;
	for(j=0; j<TEXTURE_BUFFER_SIZE; j++) {
	GL_OPERATION(glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[j][type][Y]))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0))

	GL_OPERATION(glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[j][type][U]))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w >> 1, h >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0))

	GL_OPERATION(glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[j][type][V]))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w >> 1, h >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0))

	}
	gldisp->allocatedTexturesSize[type].width =  w;
	gldisp->allocatedTexturesSize[type].height =  h;

	ms_message("%s: allocated new textures[%d] (%d x %d)\n", __FUNCTION__, type, w, h);

	check_GL_errors("allocate_gl_textures");
}

static unsigned int align_on_power_of_2(unsigned int value) {
	int i;
	/* browse all power of 2 value, and find the one just >= value */
	for(i=0; i<32; i++) {
		unsigned int c = 1 << i;
		if (value <= c)
			return c;
	}
	return 0;
}

static int pixel_unpack_alignment(uint8_t *ptr, int datasize) {
	uintptr_t num_ptr = (uintptr_t) ptr;
	int alignment_ptr = !(num_ptr % 4) ? 4 : !(num_ptr % 2) ? 2 : 1;
	int alignment_data = !(datasize % 4) ? 4 : !(datasize % 2) ? 2 : 1;
	return (alignment_ptr <= alignment_data) ? alignment_ptr : alignment_data;
}

static bool_t update_textures_with_yuv(struct opengles_display* gldisp, enum ImageType type) {
	unsigned int aligned_yuv_w, aligned_yuv_h;
	int alignment = 0;
	MSPicture yuvbuf;

	ms_yuv_buf_init_from_mblk(&yuvbuf, gldisp->yuv[type]);

	if (yuvbuf.w == 0 || yuvbuf.h == 0) {
		ms_warning("Incoherent image size: %dx%d\n", yuvbuf.w, yuvbuf.h);
		return FALSE;
	}

	aligned_yuv_w = align_on_power_of_2(yuvbuf.w);
	aligned_yuv_h = align_on_power_of_2(yuvbuf.h);

	/* check if we need to adjust texture sizes */
	if (aligned_yuv_w != (unsigned int)gldisp->allocatedTexturesSize[type].width ||
		aligned_yuv_h != (unsigned int)gldisp->allocatedTexturesSize[type].height) {
		allocate_gl_textures(gldisp, aligned_yuv_w, aligned_yuv_h, type);
	}

	gldisp->uvx[type] = yuvbuf.w / (float)(gldisp->allocatedTexturesSize[type].width);
	gldisp->uvy[type] = yuvbuf.h / (float)(gldisp->allocatedTexturesSize[type].height);

	/* alignment of pointers and datasize */
	{
		int alig_Y = pixel_unpack_alignment(yuvbuf.planes[Y], yuvbuf.w * yuvbuf.h);
		int alig_U = pixel_unpack_alignment(yuvbuf.planes[U], yuvbuf.w >> 1);
		int alig_V = pixel_unpack_alignment(yuvbuf.planes[V], yuvbuf.w >> 1);
		alignment = (alig_U > alig_V)
		  ? ((alig_V > alig_Y) ? alig_Y : alig_V)
			:	((alig_U > alig_Y) ? alig_Y : alig_U);
	}
	/* upload Y plane */
	GL_OPERATION(glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][Y]))
	GL_OPERATION(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment))
	GL_OPERATION(glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, yuvbuf.w, yuvbuf.h,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[Y]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_Y], 0))

	/* upload U plane */
	GL_OPERATION(glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][U]))
	GL_OPERATION(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment))
	GL_OPERATION(glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, yuvbuf.w >> 1, yuvbuf.h >> 1,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[U]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_U], 1))

	/* upload V plane */
	GL_OPERATION(glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[gldisp->texture_index][type][V]))
	GL_OPERATION(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment))
	GL_OPERATION(glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, yuvbuf.w >> 1, yuvbuf.h >> 1,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[V]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_V], 2))
	gldisp->yuv_size[type].width = yuvbuf.w;
	gldisp->yuv_size[type].height = yuvbuf.h;

	check_GL_errors("update_textures_with_yuv");

	return TRUE;
}

void ogl_display_zoom(struct opengles_display* gldisp, float* params) {
	gldisp->zoom_factor = params[0];
	gldisp->zoom_cx = params[1] - 0.5;
	gldisp->zoom_cy = params[2] - 0.5;
}

#ifdef ANDROID
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_init(JNIEnv * env, jobject obj, jlong ptr, jint width, jint height) {
	struct opengles_display* d = (struct opengles_display*) ptr;
	ogl_display_init(d, width, height);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_render(JNIEnv * env, jobject obj, jlong ptr) {
	struct opengles_display* d = (struct opengles_display*) ptr;
	ogl_display_render(d, 0);
}
#endif

#ifdef __cplusplus
}
#endif
