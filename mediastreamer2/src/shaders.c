/*
 
 File: shaders.c
 
 Abstract: Shader utilities for compiling, linking and validating shaders. 
 It is important to check the result status.
 
 Copyright (C) 2009 Apple Inc. All Rights Reserved.
 Code based on Apple's OpenGLES sample 
*/

#include "shaders.h"
#include "mediastreamer2/mscommon.h"

/* Create and compile a shader from the provided source(s) */
GLint compileShader(GLuint *shader, GLenum type, const char *sources)
{
	GLint status;
	if (!sources)
	{
		ms_error("Failed to load vertex shader");
		return 0;
	}
	
    *shader = glCreateShader(type);				// create shader
    glShaderSource(*shader, 1, &sources, 0);	// set source code in the shader
    glCompileShader(*shader);					// compile shader
	
	GLint logLength;
    glGetShaderiv(*shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char *log = (char *)malloc(logLength);
        glGetShaderInfoLog(*shader, logLength, &logLength, log);
        ms_message("Shader compile log:\n%s", log);
        free(log);
    }
    
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
	{
		ms_error("Failed to compile shader\n");
	}
	
	return status;
}


/* Link a program with all currently attached shaders */
GLint linkProgram(GLuint prog)
{
	GLint status;
	
	glLinkProgram(prog);
	
	GLint logLength;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1)
    {
        char *log = (char *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        ms_message("Program link log:\n%s", log);
        free(log);
    }
    
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
		ms_error("Failed to link program %d", prog);
	
	return status;
}


/* Validate a program (for i.e. inconsistent samplers) */
GLint validateProgram(GLuint prog)
{
	GLint logLength, status;
	
	glValidateProgram(prog);
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        char *log = (char *)malloc(logLength);
        glGetProgramInfoLog(prog, logLength, &logLength, log);
        ms_debug("Program validate log:\n%s", log);
        free(log);
    }
    
    glGetProgramiv(prog, GL_VALIDATE_STATUS, &status);
    if (status == GL_FALSE)
		ms_error("Failed to validate program %d", prog);
	
	return status;
}

/* delete shader resources */
void destroyShaders(GLuint vertShader, GLuint fragShader, GLuint prog)
{	
	if (vertShader) {
		glDeleteShader(vertShader);
		vertShader = 0;
	}
	if (fragShader) {
		glDeleteShader(fragShader);
		fragShader = 0;
	}
	if (prog) {
		glDeleteProgram(prog);
		prog = 0;
	}
}
