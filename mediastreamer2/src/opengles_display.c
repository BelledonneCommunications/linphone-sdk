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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "opengles_display.h"
#include "mediastreamer2/mscommon.h"
#include "shaders.h"


/* helper functions */
static void check_GL_errors(const char* context);
static bool_t load_shaders(GLuint* program, GLint* uniforms);
static void allocate_gl_textures(struct opengles_display* gldisp, int w, int h);
static void load_orthographic_matrix(float left, float right, float bottom, float top, float near, float far, float* mat);
static unsigned int align_on_power_of_2(unsigned int value);
static bool_t update_textures_with_yuv(struct opengles_display* gldisp);

static const GLfloat squareVertices[] = {
	0, 0,
	1, 0,
	0, 1,
	1, 1
};

#undef CHECK_GL_ERROR

#ifdef CHECK_GL_ERROR
	#define GL_OPERATION(x)	\
		(x); \
		check_GL_errors(#x);
#else
	#define GL_OPERATION(x) \
		(x);
#endif

enum {
    UNIFORM_MATRIX = 0,
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

struct opengles_display {
	/* input: yuv image to display */
	ms_mutex_t yuv_mutex;
	mblk_t *yuv;
	bool_t new_yuv_image;

	/* GL resources */
	bool_t glResourcesInitialized;
	GLuint program, textures[3];
	GLint uniforms[NUM_UNIFORMS];
	int allocatedTexturesW, allocatedTexturesH;

	/* GL view size */
	GLint backingWidth;
	GLint backingHeight;

	/* runtime data */
	float uvx, uvy;
	int yuv_w, yuv_h;
};

struct opengles_display* ogl_display_new() {
	struct opengles_display* result =
		(struct opengles_display*) malloc(sizeof(struct opengles_display));
	if (result == 0) {
		ms_error("Could not allocate OpenGL display structure\n");
		return 0;
	}
	memset(result, 0, sizeof(struct opengles_display));

	ms_mutex_init(&result->yuv_mutex, NULL);
	ms_message("%s : %p\n", __FUNCTION__, result);
	return result;
}

void ogl_display_free(struct opengles_display* gldisp) {
	if (gldisp->yuv) {
		ms_free(gldisp->yuv);
		gldisp->yuv = NULL;
	}
	ms_mutex_destroy(&gldisp->yuv_mutex);

	free(gldisp);
}

void ogl_display_init(struct opengles_display* gldisp, int width, int height) {
	ms_message("init opengles_display (%d x %d, gl initialized:%d)\n", width, height, gldisp->glResourcesInitialized);

	GL_OPERATION(glDisable(GL_DEPTH_TEST))

	gldisp->backingWidth = width;
	gldisp->backingHeight = height;

	if (gldisp->glResourcesInitialized)
		return;

	// init textures
	GL_OPERATION(glGenTextures(3, gldisp->textures))
	gldisp->allocatedTexturesW = gldisp->allocatedTexturesH = 0;

	load_shaders(&gldisp->program, gldisp->uniforms);
	check_GL_errors("load_shaders");

	gldisp->glResourcesInitialized = TRUE;
}

void ogl_display_uninit(struct opengles_display* gldisp, bool_t freeGLresources) {
	ms_message("uninit opengles_display (gl initialized:%d)\n", gldisp->glResourcesInitialized);

	if (gldisp->yuv) {
		ms_free(gldisp->yuv);
		gldisp->yuv = NULL;
	}

	if (gldisp->glResourcesInitialized && freeGLresources) {
		// destroy gl resources
		GL_OPERATION(glDeleteTextures(3, gldisp->textures));
		GL_OPERATION(glDeleteProgram(gldisp->program));
	}

	gldisp->allocatedTexturesW = 0;
	gldisp->allocatedTexturesH = 0;

	gldisp->glResourcesInitialized = FALSE;
}

void ogl_display_set_yuv_to_display(struct opengles_display* gldisp, mblk_t *yuv) {
	ms_mutex_lock(&gldisp->yuv_mutex);
	if (gldisp->yuv)
		freeb(gldisp->yuv);
	gldisp->yuv = dupb(yuv);
	gldisp->new_yuv_image = TRUE;
	ms_mutex_unlock(&gldisp->yuv_mutex);
}

void ogl_display_render(struct opengles_display* gldisp) {
	if (!gldisp->yuv || !gldisp->glResourcesInitialized) {
		return;
	}

    GL_OPERATION(glUseProgram(gldisp->program))

	ms_mutex_lock(&gldisp->yuv_mutex);
	if (gldisp->new_yuv_image) {
    	update_textures_with_yuv(gldisp);
		gldisp->new_yuv_image = FALSE;
	}
	ms_mutex_unlock(&gldisp->yuv_mutex);

	GLfloat squareUvs[] = {
		0.0f, gldisp->uvy,
		gldisp->uvx, gldisp->uvy,
		0.0f, 0.0f,
		gldisp->uvx, 0.0f
    };

    GL_OPERATION(glViewport(0, 0, gldisp->backingWidth, gldisp->backingHeight))
    GL_OPERATION(glClearColor(0, 0, 0, 1))
    GL_OPERATION(glClear(GL_COLOR_BUFFER_BIT))

	int x,y,w,h;
	if (gldisp->backingHeight > gldisp->backingWidth) {
		float ratio = gldisp->yuv_h / (float)gldisp->yuv_w;
		w = gldisp->backingWidth;
		h = w * ratio;
		x = 0;
		y = (gldisp->backingHeight - h) * 0.5f;
	} else {
		float ratio = gldisp->yuv_w / (float)gldisp->yuv_h;
		h = gldisp->backingHeight;
		w = gldisp->backingHeight * ratio;
		x = (gldisp->backingWidth - w) * 0.5f;
		y = 0;
	}
	GL_OPERATION(glViewport(x, y, w, h))

	GLfloat mat[16];
	load_orthographic_matrix(0, 1.0f, 0, 1, 0, 1, mat);
	GL_OPERATION(glUniformMatrix4fv(gldisp->uniforms[UNIFORM_MATRIX], 1, GL_FALSE, mat))

    GL_OPERATION(glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[Y]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_Y], 0))
    GL_OPERATION(glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[U]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_U], 1))
    GL_OPERATION(glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[V]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_V], 2))

	GL_OPERATION(glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, squareVertices))
	GL_OPERATION(glEnableVertexAttribArray(ATTRIB_VERTEX))
	GL_OPERATION(glVertexAttribPointer(ATTRIB_UV, 2, GL_FLOAT, 1, 0, squareUvs))
	GL_OPERATION(glEnableVertexAttribArray(ATTRIB_UV))

	GL_OPERATION(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4))
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
#include "yuv2rgb.vs.h"
#include "yuv2rgb.fs.h"
    GLuint vertShader, fragShader;
    *program = glCreateProgram();

    if (!compileShader(&vertShader, GL_VERTEX_SHADER, YUV2RGB_VERTEX_SHADER))
        return FALSE;
    if (!compileShader(&fragShader, GL_FRAGMENT_SHADER, YUV2RGB_FRAGMENT_SHADER))
        return FALSE;

    glAttachShader(*program, vertShader);
    glAttachShader(*program, fragShader);

    glBindAttribLocation(*program, ATTRIB_VERTEX, "position");
    glBindAttribLocation(*program, ATTRIB_UV, "uv");

    if (!linkProgram(*program))
        return FALSE;

    uniforms[UNIFORM_MATRIX] = glGetUniformLocation(*program, "matrix");
    uniforms[UNIFORM_TEXTURE_Y] = glGetUniformLocation(*program, "t_texture_y");
    uniforms[UNIFORM_TEXTURE_U] = glGetUniformLocation(*program, "t_texture_u");
    uniforms[UNIFORM_TEXTURE_V] = glGetUniformLocation(*program, "t_texture_v");

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return TRUE;
}

static void load_orthographic_matrix(float left, float right, float bottom, float top, float near, float far, float* mat)
{
    float r_l = right - left;
    float t_b = top - bottom;
    float f_n = far - near;
    float tx = - (right + left) / (right - left);
    float ty = - (top + bottom) / (top - bottom);
    float tz = - (far + near) / (far - near);

    mat[0] = 2.0f / r_l;
    mat[1] = mat[2] = mat[3] = 0.0f;

    mat[4] = 0.0f;
    mat[5] = 2.0f / t_b;
    mat[6] = mat[7] = 0.0f;

    mat[8] = mat[9] = 0.0f;
    mat[10] = -2.0f / f_n;
    mat[11] = 0.0f;

    mat[12] = tx;
    mat[13] = ty;
    mat[14] = tz;
    mat[15] = 1.0f;
}

static void allocate_gl_textures(struct opengles_display* gldisp, int w, int h) {
	GL_OPERATION(glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[Y]))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0))

	GL_OPERATION(glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[U]))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w >> 1, h >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0))

	GL_OPERATION(glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[V]))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE))
	GL_OPERATION(glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w >> 1, h >> 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, 0))

	gldisp->allocatedTexturesW =  w;
	gldisp->allocatedTexturesH =  h;

	ms_message("%s: allocated new textures (%d x %d)\n", __FUNCTION__, gldisp->allocatedTexturesW, gldisp->allocatedTexturesH);
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

static bool_t update_textures_with_yuv(struct opengles_display* gldisp) {
	unsigned int aligned_yuv_w, aligned_yuv_h;
	MSPicture yuvbuf;

	ms_yuv_buf_init_from_mblk(&yuvbuf, gldisp->yuv);

	if (yuvbuf.w == 0 || yuvbuf.h == 0) {
		ms_warning("Incoherent image size: %dx%d\n", yuvbuf.w, yuvbuf.h);
		return FALSE;
	}
	aligned_yuv_w = align_on_power_of_2(yuvbuf.w);
	aligned_yuv_h = align_on_power_of_2(yuvbuf.h);

	/* check if we need to adjust texture sizes */
	if (aligned_yuv_w != gldisp->allocatedTexturesW ||
		aligned_yuv_h != gldisp->allocatedTexturesH) {
		allocate_gl_textures(gldisp, aligned_yuv_w, aligned_yuv_h);
	}
	gldisp->uvx = yuvbuf.w / (float)(gldisp->allocatedTexturesW+1);
	gldisp->uvy = yuvbuf.h / (float)(gldisp->allocatedTexturesH+1);

	/* upload Y plane */
	GL_OPERATION(glActiveTexture(GL_TEXTURE0))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[Y]))
	GL_OPERATION(glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, yuvbuf.w, yuvbuf.h,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[Y]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_Y], 0))

	/* upload U plane */
	GL_OPERATION(glActiveTexture(GL_TEXTURE1))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[U]))
	GL_OPERATION(glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, yuvbuf.w >> 1, yuvbuf.h >> 1,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[U]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_U], 1))

	/* upload V plane */
	GL_OPERATION(glActiveTexture(GL_TEXTURE2))
	GL_OPERATION(glBindTexture(GL_TEXTURE_2D, gldisp->textures[V]))
	GL_OPERATION(glTexSubImage2D(GL_TEXTURE_2D, 0,
			0, 0, yuvbuf.w >> 1, yuvbuf.h >> 1,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvbuf.planes[V]))
	GL_OPERATION(glUniform1i(gldisp->uniforms[UNIFORM_TEXTURE_V], 2))

	gldisp->yuv_w = yuvbuf.w;
	gldisp->yuv_h = yuvbuf.h;

	return TRUE;
}

#ifdef ANDROID
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_init(JNIEnv * env, jobject obj, jint ptr, jint width, jint height) {
	struct opengles_display* d = (struct opengles_display*) ptr;
	ogl_display_init(d, width, height);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_render(JNIEnv * env, jobject obj, jint ptr) {
	struct opengles_display* d = (struct opengles_display*) ptr;
	ogl_display_render(d);
}
#endif
