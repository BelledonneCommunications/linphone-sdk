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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mediastreamer2/mscommon.h"
#include "opengl_debug.h"
#include "shader_util.h"

#define LogInfo ms_message
#define LogError ms_error

/* Compile a shader from the provided source(s) */
GLint glueCompileShader(const OpenGlFunctions *f, const GLchar *sources, GLuint shader) {
	GLint logLength = 0, status;

	f->glShaderSource(shader, 1, &sources, NULL);
	f->glCompileShader(shader);

	f->glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == 0) {
		LogError("Failed to compile shader:\n");
		LogInfo("%s", sources);
	}
	glError(f);

	f->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
	// logLength can be set even on error.
	GLenum err = f->glGetError();
	if (err == GL_NO_ERROR) {
		if (logLength > 0) {
			GLchar *log = (GLchar *)malloc(logLength);
			f->glGetShaderInfoLog(shader, logLength, &logLength, log);
			LogInfo("Shader compile log:\n%s", log);
			free(log);
		}
	} else LogError("glError: %04x caught at %s:%u\n", err, __FILE__, __LINE__);

	return status;
}

/* Link a program with all currently attached shaders */
GLint glueLinkProgram(const OpenGlFunctions *f, GLuint program) {
	GLint logLength, status;

	f->glLinkProgram(program);
	f->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0) {
		GLchar *log = (GLchar *)malloc(logLength);
		f->glGetProgramInfoLog(program, logLength, &logLength, log);
		bool_t haveText = FALSE;
		for (int i = 0; !haveText && i < logLength; ++i)
			haveText = (log[i] >= 'a' && log[i] <= 'z') || (log[i] >= 'A' && log[i] <= 'Z') ||
			           (log[i] >= '0' && log[i] <= '9');
		if (haveText) LogInfo("Program link log:\n%s", log);

		free(log);
	}

	f->glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == 0) LogError("Failed to link program %d", program);

	glError(f);

	return status;
}

/* Validate a program (for i.e. inconsistent samplers) */
GLint glueValidateProgram(const OpenGlFunctions *f, GLuint program) {
	GLint logLength, status;

	f->glValidateProgram(program);
	f->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
	if (logLength > 0) {
		GLchar *log = (GLchar *)malloc(logLength);
		f->glGetProgramInfoLog(program, logLength, &logLength, log);
		LogInfo("Program validate log:\n%s", log);
		free(log);
	}

	f->glGetProgramiv(program, GL_VALIDATE_STATUS, &status);
	if (status == 0) LogError("Failed to validate program %d", program);

	glError(f);

	return status;
}

/* Return named uniform location after linking */
GLint glueGetUniformLocation(const OpenGlFunctions *f, GLuint program, const GLchar *uniformName) {
	return f->glGetUniformLocation(program, uniformName);
}
