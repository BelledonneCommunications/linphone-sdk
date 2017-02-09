#include "opengl_functions.h"

// =============================================================================

#ifdef ANDROID
#define CAST(type, fn) (type)fn
#else
#define CAST(type, fn) fn
#endif

void opengl_functions_default_init (OpenGlFunctions *f) {
	#if !defined(_WIN32)

	f->glActiveTexture = CAST(resolveGlActiveTexture, glActiveTexture);
	f->glAttachShader = CAST(resolveGlAttachShader, glAttachShader);
	f->glBindAttribLocation = CAST(resolveGlBindAttribLocation, glBindAttribLocation);
	f->glBindTexture = CAST(resolveGlBindTexture, glBindTexture);
	f->glClear = CAST(resolveGlClear, glClear);
	f->glClearColor = CAST(resolveGlClearColor, glClearColor);
	f->glCompileShader = CAST(resolveGlCompileShader, glCompileShader);
	f->glCreateProgram = CAST(resolveGlCreateProgram, glCreateProgram);
	f->glCreateShader = CAST(resolveGlCreateShader, glCreateShader);
	f->glDeleteProgram = CAST(resolveGlDeleteProgram, glDeleteProgram);
	f->glDeleteShader = CAST(resolveGlDeleteShader, glDeleteShader);
	f->glDeleteTextures = CAST(resolveGlDeleteTextures, glDeleteTextures);
	f->glDisable = CAST(resolveGlDisable,glDisable);
	f->glDrawArrays = CAST(resolveGlDrawArrays, glDrawArrays);
	f->glEnableVertexAttribArray = CAST(resolveGlEnableVertexAttribArray, glEnableVertexAttribArray);
	f->glGenTextures = CAST(resolveGlGenTextures,glGenTextures);
	f->glGetError = CAST(resolveGlGetError, glGetError);
	f->glGetProgramInfoLog = CAST(resolveGlGetProgramInfoLog, glGetProgramInfoLog);
	f->glGetProgramiv = CAST(resolveGlGetProgramiv, glGetProgramiv);
	f->glGetShaderInfoLog = CAST(resolveGlGetShaderInfoLog, glGetShaderInfoLog);
	f->glGetShaderiv = CAST(resolveGlGetShaderiv, glGetShaderiv);
	f->glGetString = CAST(resolveGlGetString, glGetString);
	f->glGetUniformLocation = CAST(resolveGlGetUniformLocation, glGetUniformLocation);
	f->glLinkProgram = CAST(resolveGlLinkProgram, glLinkProgram);
	f->glPixelStorei = CAST(resolveGlPixelStorei, glPixelStorei);
	f->glShaderSource = (resolveGlShaderSource)glShaderSource;
	f->glTexImage2D = CAST(resolveGlTexImage2D, glTexImage2D);
	f->glTexParameteri = CAST(resolveGlTexParameteri, glTexParameteri);
	f->glTexSubImage2D = CAST(resolveGlTexSubImage2D, glTexSubImage2D);
	f->glUniform1f = CAST(resolveGlUniform1f, glUniform1f);
	f->glUniform1i = CAST(resolveGlUniform1i, glUniform1i);
	f->glUniformMatrix4fv = CAST(resolveGlUniformMatrix4fv, glUniformMatrix4fv);
	f->glUseProgram = CAST(resolveGlUseProgram, glUseProgram);
	f->glValidateProgram = CAST(resolveGlValidateProgram, glValidateProgram);
	f->glVertexAttribPointer = CAST(resolveGlVertexAttribPointer, glVertexAttribPointer);
	f->glViewport = CAST(resolveGlViewport, glViewport);

	#endif
}
