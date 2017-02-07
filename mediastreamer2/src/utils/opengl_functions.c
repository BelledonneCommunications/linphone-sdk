#include "opengl_functions.h"

// =============================================================================

void opengl_functions_default_init (OpenGlFunctions *f) {
	#if !defined(_WIN32)

	f->glActiveTexture = glActiveTexture;
	f->glAttachShader = glAttachShader;
	f->glBindAttribLocation = glBindAttribLocation;
	f->glBindTexture = glBindTexture;
	f->glClear = glClear;
	f->glClearColor = glClearColor;
	f->glCompileShader = glCompileShader;
	f->glCreateProgram = glCreateProgram;
	f->glCreateShader = glCreateShader;
	f->glDeleteProgram = glDeleteProgram;
	f->glDeleteShader = glDeleteShader;
	f->glDeleteTextures = glDeleteTextures;
	f->glDisable = glDisable;
	f->glDrawArrays = glDrawArrays;
	f->glEnableVertexAttribArray = glEnableVertexAttribArray;
	f->glGenTextures = glGenTextures;
	f->glGetError = glGetError;
	f->glGetProgramInfoLog = glGetProgramInfoLog;
	f->glGetProgramiv = glGetProgramiv;
	f->glGetShaderInfoLog = glGetShaderInfoLog;
	f->glGetShaderiv = glGetShaderiv;
	f->glGetString = glGetString;
	f->glGetUniformLocation = glGetUniformLocation;
	f->glLinkProgram = glLinkProgram;
	f->glPixelStorei = glPixelStorei;
	f->glShaderSource = (resolveGlShaderSource)glShaderSource;
	f->glTexImage2D = glTexImage2D;
	f->glTexParameteri = glTexParameteri;
	f->glTexSubImage2D = glTexSubImage2D;
	f->glUniform1f = glUniform1f;
	f->glUniform1i = glUniform1i;
	f->glUniformMatrix4fv = glUniformMatrix4fv;
	f->glUseProgram = glUseProgram;
	f->glValidateProgram = glValidateProgram;
	f->glVertexAttribPointer = glVertexAttribPointer;
	f->glViewport = glViewport;

	#endif
}
