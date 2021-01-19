/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "opengl_functions.h"
#include "mediastreamer2/mscommon.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

// =============================================================================

#if defined( __ANDROID__ )
#  define CAST(type, fn) (f->getProcAddress && f->getProcAddress(#fn) ? (type)f->getProcAddress(#fn) : (type)fn)
#elif defined(WIN32)

#  ifndef MS2_WINDOWS_UWP
//[Desktop app only]
#    include <wingdi.h>

#    pragma comment(lib,"Opengl32.lib")	//Opengl32 symbols for wglGetProcAddress

void *GetAnyGLFuncAddress(HMODULE library, HMODULE firstFallback, const char *name)
{
	void *p = (void *)wglGetProcAddress(name);
	if(p == 0 ||
		(p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) ||
		(p == (void*)-1) ) {
		if(library)
			p = (void *)GetProcAddress(library, name);
		if(!p && firstFallback)
			p = (void *)GetProcAddress(firstFallback, name);	
	}
	
	return p;
}
#  endif
#  include <windows.h>
#  ifdef MS2_WINDOWS_UWP
#    define CAST(type, fn) (f->getProcAddress && f->getProcAddress(#fn) ? (type)f->getProcAddress(#fn) : (type)GetProcAddress( openglLibrary, #fn))
#  else
#    define CAST(type, fn) (f->getProcAddress && f->getProcAddress(#fn) ? (type)f->getProcAddress(#fn) : (type)GetAnyGLFuncAddress(openglLibrary,firstFallbackLibrary, #fn))
#  endif
#else

void *getAnyGLFuncAddress(void* library, void *firstFallback, const char *name)
{
	void * p = NULL;
	if(library)
		p = (void *)dlsym(library, name);
	if(!p && firstFallback)
		p = (void *)dlsym(firstFallback, name);	
	return p;
}

#  define CAST(type, fn) (f->getProcAddress && f->getProcAddress(#fn) ? (type)f->getProcAddress(#fn) : (type)getAnyGLFuncAddress(openglLibrary,firstFallbackLibrary, #fn))
#endif

// Remove EGL from Android
#if defined( __ANDROID__ )
#define CAST_EGL(type, fn) NULL
#else
#define CAST_EGL(type, fn) (f->eglGetProcAddress && f->eglGetProcAddress(#fn) ? (type)f->eglGetProcAddress(#fn):CAST(type,fn))
#endif
#ifdef __cplusplus
extern "C"{
#endif

void opengl_functions_default_init (OpenGlFunctions *f) {
#if defined(_WIN32)
	HMODULE openglLibrary, firstFallbackLibrary = NULL;
#else
	void * openglLibrary = NULL, *firstFallbackLibrary = NULL;
#endif
	
	//----------------------    GL   
#if defined(_WIN32)// On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to embedd "libGLESv2.dll" with the app
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	
#	ifdef UNICODE
		firstFallbackLibrary = LoadLibraryW(L"opengl32.dll");
		openglLibrary = LoadLibraryW(L"libGLESv2.dll");
#	else
		firstFallbackLibrary = LoadLibraryA("opengl32.dll");
		openglLibrary = LoadLibraryA("libGLESv2.dll");
#	endif
#else
		openglLibrary = LoadPackagedLibrary(L"libGLESv2.dll", 0);// UWP compatibility
#endif
		if( openglLibrary == NULL ){
			ms_warning("[ogl_functions] Function : Fail to load plugin libGLESv2.dll: error %i", (int)GetLastError());
		}
	
#else
		openglLibrary = dlopen("libGLESv2.so", RTLD_LAZY);
		if( openglLibrary == NULL ){
			ms_warning("[ogl_functions] Function : Fail to load plugin libGLESv2.so: %s", dlerror());
		}
		firstFallbackLibrary = dlopen("libGLEW.so", RTLD_LAZY);
		if( firstFallbackLibrary == NULL )
			ms_warning("[ogl_functions] Function : Fail to load plugin libGLEW.so: %s", dlerror());
	
#endif// _WIN32
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
	f->glGetIntegerv = CAST(resolveGlGetIntegerv, glGetIntegerv);
	f->glGetProgramInfoLog = CAST(resolveGlGetProgramInfoLog, glGetProgramInfoLog);
	f->glGetProgramiv = CAST(resolveGlGetProgramiv, glGetProgramiv);
	f->glGetShaderInfoLog = CAST(resolveGlGetShaderInfoLog, glGetShaderInfoLog);
	f->glGetShaderiv = CAST(resolveGlGetShaderiv, glGetShaderiv);
	f->glGetString = CAST(resolveGlGetString, glGetString);
	f->glGetUniformLocation = CAST(resolveGlGetUniformLocation, glGetUniformLocation);
	f->glLinkProgram = CAST(resolveGlLinkProgram, glLinkProgram);
	f->glPixelStorei = CAST(resolveGlPixelStorei, glPixelStorei);
	f->glShaderSource = CAST(resolveGlShaderSource, glShaderSource);
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
	//----------------------    EGL
#if defined(_WIN32)// On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to embedd "libEGL.dll" with the app
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	
#	ifdef UNICODE
		openglLibrary = LoadLibraryExW(L"libEGL.dll", NULL, 0);
#	else
		openglLibrary = LoadLibraryExA("libEGL.dll", NULL, 0);
#	endif
#else
		openglLibrary = LoadPackagedLibrary(L"libEGL.dll", 0);// UWP compatibility
#endif
		if( openglLibrary == NULL ){
			ms_warning("[ogl_functions] Function : Fail to load plugin libEGL.dll: error %i", (int)GetLastError());
		}
#else
		openglLibrary = dlopen("libEGL.so", RTLD_LAZY);
		if( openglLibrary == NULL ){
			ms_warning("[ogl_functions] Function : Fail to load plugin libEGL.so: %s", dlerror());
		}
#endif// _WIN32
	f->eglGetProcAddress = CAST(resolveEGLGetProcAddress, eglGetProcAddress);

	f->eglQueryAPI = CAST_EGL(resolveEGLQueryAPI, eglQueryAPI);
	f->eglBindAPI = CAST_EGL(resolveEGLBindAPI, eglBindAPI);
	f->eglQueryString = CAST_EGL(resolveEGLQueryString, eglQueryString);
	f->eglGetPlatformDisplayEXT = CAST_EGL(resolveEGLGetPlatformDisplayEXT, eglGetPlatformDisplayEXT);
	f->eglGetCurrentDisplay = CAST_EGL(resolveEGLGetCurrentDisplay, eglGetCurrentDisplay);
	f->eglGetCurrentContext= CAST_EGL(resolveEGLGetCurrentDisplay, eglGetCurrentContext);
	f->eglGetCurrentSurface = CAST_EGL(resolveEGLGetCurrentDisplay, eglGetCurrentSurface);
	f->eglInitialize = CAST_EGL(resolveEGLInitialize, eglInitialize);
	f->eglChooseConfig = CAST_EGL(resolveEGLChooseConfig, eglChooseConfig);
	f->eglCreateContext = CAST_EGL(resolveEGLCreateContext, eglCreateContext);
	f->eglCreateWindowSurface = CAST_EGL(resolveEGLCreateWindowSurface, eglCreateWindowSurface);
	f->eglMakeCurrent = CAST_EGL(resolveEGLMakeCurrent, eglMakeCurrent);
	f->eglGetError = CAST_EGL(resolveEGLGetError, eglGetError);
	f->eglSwapBuffers = CAST_EGL(resolveEGLSwapBuffers, eglSwapBuffers);
	f->eglQuerySurface = CAST_EGL(resolveEGLQuerySurface, eglQuerySurface);
	f->eglDestroySurface = CAST_EGL( resolveEGLDestroySurface, eglDestroySurface);
	f->eglDestroyContext = CAST_EGL( resolveEGLDestroyContext, eglDestroyContext);
	f->eglTerminate = CAST_EGL( resolveEGLTerminate, eglTerminate);
	
	f->initialized = TRUE;
}

#ifdef __cplusplus
}
#endif
