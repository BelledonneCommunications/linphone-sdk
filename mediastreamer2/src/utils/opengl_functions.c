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
#include "opengl_functions.h"
#include "mediastreamer2/mscommon.h"

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h" // ENABLE_OPENGL_PROFILING
#endif

#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "bctoolbox/defs.h"

// =============================================================================

#if defined(__ANDROID__) || defined(__APPLE__)
#define CAST(type, fn) (f->getProcAddress && f->getProcAddress(#fn) ? (type)f->getProcAddress(#fn) : (type)fn)
#elif defined(WIN32)

#ifndef MS2_WINDOWS_UWP
//[Desktop app only]
#include <wingdi.h>

#pragma comment(lib, "Opengl32.lib") // Opengl32 symbols for wglGetProcAddress

void *GetAnyGLFuncAddress(HMODULE library, HMODULE firstFallback, const char *name) {
	void *p = (void *)wglGetProcAddress(name);
	if (p == 0 || (p == (void *)0x1) || (p == (void *)0x2) || (p == (void *)0x3) || (p == (void *)-1)) {
		if (library) p = (void *)GetProcAddress(library, name);
		if (!p && firstFallback) p = (void *)GetProcAddress(firstFallback, name);
	}

	return p;
}
#endif
#include <windows.h>
#ifdef MS2_WINDOWS_UWP
#define CAST(type, fn)                                                                                                 \
	(f->getProcAddress && f->getProcAddress(#fn) ? (type)f->getProcAddress(#fn)                                        \
	                                             : (type)GetProcAddress(openglLibrary, #fn))
#else
#define CAST(type, fn)                                                                                                 \
	(f->getProcAddress && f->getProcAddress(#fn)                                                                       \
	     ? (type)f->getProcAddress(#fn)                                                                                \
	     : (type)GetAnyGLFuncAddress(openglLibrary, firstFallbackLibrary, #fn))
#endif
#else

void *getAnyGLFuncAddress(void *library, void *firstFallback, const char *name) {
	void *p = NULL;
	if (library) p = (void *)dlsym(library, name);
	if (!p && firstFallback) p = (void *)dlsym(firstFallback, name);
	return p;
}

#define CAST(type, fn)                                                                                                 \
	(f->getProcAddress && f->getProcAddress(#fn)                                                                       \
	     ? (type)f->getProcAddress(#fn)                                                                                \
	     : (type)getAnyGLFuncAddress(openglLibrary, firstFallbackLibrary, #fn))
#endif

// Remove EGL from Android
#if defined(__ANDROID__) || defined(__APPLE__)
#define CAST_EGL(type, fn) NULL
#elif defined(__APPLE__)
#define CAST_EGL(type, fn) (f->eglGetProcAddress && f->eglGetProcAddress(#fn) ? (type)f->eglGetProcAddress(#fn) : NULL)
#else
#define CAST_EGL(type, fn)                                                                                             \
	(f->eglGetProcAddress && f->eglGetProcAddress(#fn) ? (type)f->eglGetProcAddress(#fn) : CAST(type, fn))
#endif
#ifdef __cplusplus
extern "C" {
#endif

void opengl_functions_load_gl(void **openglLibrary, void **firstFallbackLibrary, BCTBX_UNUSED(bool_t loadQtlibs)) {
	//----------------------    GL
#if defined(_WIN32) // On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to
                    // embedd "libGLESv2.dll" or "opengl32sw.dll" with the app
	bool_t haveOpengl32sw = FALSE;
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)
	DWORD searchFlags = LOAD_LIBRARY_SEARCH_APPLICATION_DIR | DONT_RESOLVE_DLL_REFERENCES;
// Load without resolving to check only local libraries. Then free library memory and effectively load the library
// with resolving. This way, we ensure that the best library comes from the appication.
#ifdef UNICODE
	*openglLibrary =
	    (loadQtlibs ? LoadLibraryExW(L"opengl32sw.dll", NULL, searchFlags) : NULL); // From Qt6: Software implementation
	if (*openglLibrary) {
		FreeLibrary((HMODULE)*openglLibrary);
		*openglLibrary = LoadLibraryW(L"opengl32sw.dll");
	}
	if (!*openglLibrary) {
		*openglLibrary = LoadLibraryExW(L"libGLESv2.dll", NULL, searchFlags);
		if (*openglLibrary) {
			FreeLibrary((HMODULE)*openglLibrary);
			*openglLibrary = LoadLibraryW(L"libGLESv2.dll");
		}
	} else {
		haveOpengl32sw = TRUE;
	}
	*firstFallbackLibrary = LoadLibraryW(L"opengl32.dll"); // From System32

#else
	*openglLibrary =
	    (loadQtlibs ? LoadLibraryExA("opengl32sw.dll", NULL, searchFlags) : NULL); // From Qt6: Software implementation
	if (*openglLibrary) {
		FreeLibrary((HMODULE)*openglLibrary);
		*openglLibrary = LoadLibraryA("opengl32sw.dll");
	}
	if (!*openglLibrary) {
		*openglLibrary = LoadLibraryExA("libGLESv2.dll", NULL, searchFlags);
		if (*openglLibrary) {
			FreeLibrary((HMODULE)*openglLibrary);
			*openglLibrary = LoadLibraryA("libGLESv2.dll");
		}
	} else {
		haveOpengl32sw = TRUE;
	}
	*firstFallbackLibrary = LoadLibraryA("opengl32.dll"); // From System32
#endif
#else
	*openglLibrary = LoadPackagedLibrary(L"libGLESv2.dll", 0); // UWP compatibility
#endif
	if (*openglLibrary == NULL && *firstFallbackLibrary == NULL) {
		ms_warning(
		    "[ogl_functions] Function : Fail to load OpenGL plugin opengl32sw.dll/libGLESv2.dll/opengl32.dll: error %i",
		    (int)GetLastError());
	} else {
		ms_message("[ogl_functions] OpenGL plugin loaded (%s)", (haveOpengl32sw           ? "opengl32sw.dll"
		                                                         : *openglLibrary == NULL ? "opengl32.dll"
		                                                                                  : "libGLESv2.dll"));
	}

#elif !defined(__APPLE__)
	if (*openglLibrary == NULL) *openglLibrary = dlopen("libGLESv2.so", RTLD_LAZY);
	if (*openglLibrary == NULL) {
		ms_warning("[ogl_functions] Function : Fail to load OpenGL plugin libGLESv2.so: %s", dlerror());
	}
	*firstFallbackLibrary = dlopen("libGLEW.so", RTLD_LAZY);
	if (*firstFallbackLibrary == NULL)
		ms_warning("[ogl_functions] Function : Fail to load OpenGL plugin libGLEW.so: %s", dlerror());
#else
	*openglLibrary = NULL;
	*firstFallbackLibrary = NULL;
#endif // _WIN32
}

void opengl_functions_load_egl(void **openglLibrary) {
#if defined(_WIN32) // On Windows, load dynamically library as is it not deployed by the system. Clients must be sure to
                    // embedd "libEGL.dll" with the app
#if defined(MS2_WINDOWS_DESKTOP) && !defined(MS2_WINDOWS_UWP)

#ifdef UNICODE
	*openglLibrary = LoadLibraryExW(L"libEGL.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
#else
	*openglLibrary = LoadLibraryExA("libEGL.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
#endif
#else
	*openglLibrary = LoadPackagedLibrary(L"libEGL.dll", 0); // UWP compatibility
#endif
	if (*openglLibrary == NULL) {
		ms_warning("[ogl_functions] Function : Fail to load EGL plugin libEGL.dll: error %i", (int)GetLastError());
	} else ms_message("[ogl_functions] EGL plugin loaded");
#elif !defined(__APPLE__)
	*openglLibrary = dlopen("libEGL.so", RTLD_LAZY);
	if (*openglLibrary == NULL) {
		ms_warning("[ogl_functions] Function : Fail to load EGL plugin libEGL.so: %s", dlerror());
	} else ms_message("[ogl_functions] EGL plugin loaded");
#else
	*openglLibrary = NULL;
#endif // _WIN32
}

void opengl_functions_default_init(OpenGlFunctions *f) {
#if defined(_WIN32)
	HMODULE openglLibrary = NULL, firstFallbackLibrary = NULL;
#else
	void *openglLibrary = NULL, *firstFallbackLibrary = NULL;
#endif
	// No need to load libraries if getProcAddress is defined.
	if (!f->getProcAddress)
		opengl_functions_load_gl((void **)&openglLibrary, (void **)&firstFallbackLibrary, f->loadQtLibs);
	// User cases: functions already loaded or coming from libraries
#if !defined(__ANDROID__) && !defined(__APPLE__)
	if (f->getProcAddress || firstFallbackLibrary != NULL || openglLibrary != NULL) {
#endif
		f->glInitialized = TRUE;
		f->glInitialized &= ((f->glActiveTexture = CAST(resolveGlActiveTexture, glActiveTexture)) != NULL);
		f->glInitialized &= ((f->glAttachShader = CAST(resolveGlAttachShader, glAttachShader)) != NULL);
		f->glInitialized &=
		    ((f->glBindAttribLocation = CAST(resolveGlBindAttribLocation, glBindAttribLocation)) != NULL);
		f->glInitialized &= ((f->glBindBuffer = CAST(resolveGlBindBuffer, glBindBuffer)) != NULL);
		f->glInitialized &= ((f->glBindTexture = CAST(resolveGlBindTexture, glBindTexture)) != NULL);
		f->glInitialized &= ((f->glBufferData = CAST(resolveGlBufferData, glBufferData)) != NULL);
		f->glInitialized &= ((f->glBufferSubData = CAST(resolveGlBufferSubData, glBufferSubData)) != NULL);
		f->glInitialized &= ((f->glClear = CAST(resolveGlClear, glClear)) != NULL);
		f->glInitialized &= ((f->glClearColor = CAST(resolveGlClearColor, glClearColor)) != NULL);
		f->glInitialized &= ((f->glCompileShader = CAST(resolveGlCompileShader, glCompileShader)) != NULL);
		f->glInitialized &= ((f->glCreateProgram = CAST(resolveGlCreateProgram, glCreateProgram)) != NULL);
		f->glInitialized &= ((f->glCreateShader = CAST(resolveGlCreateShader, glCreateShader)) != NULL);
		f->glInitialized &= ((f->glDeleteProgram = CAST(resolveGlDeleteProgram, glDeleteProgram)) != NULL);
		f->glInitialized &= ((f->glDeleteShader = CAST(resolveGlDeleteShader, glDeleteShader)) != NULL);
		f->glInitialized &= ((f->glDeleteTextures = CAST(resolveGlDeleteTextures, glDeleteTextures)) != NULL);
		f->glInitialized &= ((f->glDisable = CAST(resolveGlDisable, glDisable)) != NULL);
		f->glInitialized &= ((f->glDrawArrays = CAST(resolveGlDrawArrays, glDrawArrays)) != NULL);
		f->glInitialized &= ((f->glEnableVertexAttribArray =
		                          CAST(resolveGlEnableVertexAttribArray, glEnableVertexAttribArray)) != NULL);
		f->glInitialized &= ((f->glFinish = CAST(resolveGlFinish, glFinish)) != NULL);
		f->glInitialized &= ((f->glGenBuffers = CAST(resolveGlGenBuffers, glGenBuffers)) != NULL);
		f->glInitialized &= ((f->glGenTextures = CAST(resolveGlGenTextures, glGenTextures)) != NULL);
		f->glInitialized &= ((f->glGetError = CAST(resolveGlGetError, glGetError)) != NULL);
		f->glInitialized &= ((f->glGetIntegerv = CAST(resolveGlGetIntegerv, glGetIntegerv)) != NULL);
		f->glInitialized &= ((f->glGetProgramInfoLog = CAST(resolveGlGetProgramInfoLog, glGetProgramInfoLog)) != NULL);
		f->glInitialized &= ((f->glGetProgramiv = CAST(resolveGlGetProgramiv, glGetProgramiv)) != NULL);
		f->glInitialized &= ((f->glGetShaderInfoLog = CAST(resolveGlGetShaderInfoLog, glGetShaderInfoLog)) != NULL);
		f->glInitialized &= ((f->glGetShaderiv = CAST(resolveGlGetShaderiv, glGetShaderiv)) != NULL);
		f->glInitialized &= ((f->glGetString = CAST(resolveGlGetString, glGetString)) != NULL);
		f->glInitialized &=
		    ((f->glGetUniformLocation = CAST(resolveGlGetUniformLocation, glGetUniformLocation)) != NULL);
		f->glInitialized &= ((f->glLinkProgram = CAST(resolveGlLinkProgram, glLinkProgram)) != NULL);
		f->glInitialized &= ((f->glPixelStorei = CAST(resolveGlPixelStorei, glPixelStorei)) != NULL);
		f->glInitialized &= ((f->glShaderSource = CAST(resolveGlShaderSource, glShaderSource)) != NULL);
		f->glInitialized &= ((f->glTexImage2D = CAST(resolveGlTexImage2D, glTexImage2D)) != NULL);
		f->glInitialized &= ((f->glTexParameteri = CAST(resolveGlTexParameteri, glTexParameteri)) != NULL);
		f->glInitialized &= ((f->glTexSubImage2D = CAST(resolveGlTexSubImage2D, glTexSubImage2D)) != NULL);
		f->glInitialized &= ((f->glUniform1f = CAST(resolveGlUniform1f, glUniform1f)) != NULL);
		f->glInitialized &= ((f->glUniform1i = CAST(resolveGlUniform1i, glUniform1i)) != NULL);
		f->glInitialized &= ((f->glUniformMatrix4fv = CAST(resolveGlUniformMatrix4fv, glUniformMatrix4fv)) != NULL);
		f->glInitialized &= ((f->glUseProgram = CAST(resolveGlUseProgram, glUseProgram)) != NULL);
		f->glInitialized &= ((f->glValidateProgram = CAST(resolveGlValidateProgram, glValidateProgram)) != NULL);
		f->glInitialized &=
		    ((f->glVertexAttribPointer = CAST(resolveGlVertexAttribPointer, glVertexAttribPointer)) != NULL);
		f->glInitialized &= ((f->glViewport = CAST(resolveGlViewport, glViewport)) != NULL);

		// Only needed for OpenGL 3.0+
		f->glGenVertexArrays = CAST(resolveGlGenVertexArrays, glGenVertexArrays);
		f->glBindVertexArray = CAST(resolveGlBindVertexArray, glBindVertexArray);

#ifdef ENABLE_OPENGL_PROFILING
		f->glGenQueries = CAST(glGenQueriesSignature, glGenQueries);
		f->glBeginQuery = CAST(glBeginQuerySignature, glBeginQuery);
		f->glEndQuery = CAST(glEndQuerySignature, glEndQuery);
		f->glGetQueryObjectui64v = CAST(glGetQueryObjectui64vSignature, glGetQueryObjectui64v);
#else
	f->glGenQueries = NULL;
	f->glBeginQuery = NULL;
	f->glEndQuery = NULL;
	f->glGetQueryObjectui64v = NULL;
#endif

#if !defined(__ANDROID__) && !defined(__APPLE__)
	} else {
		ms_error("[ogl_functions] GL functions cannot be initialized.");
		return;
	}
#endif
	//----------------------    EGL

	openglLibrary = NULL;
	firstFallbackLibrary = NULL;
	// No need to load EGL libraries if getProcAddress is defined: EGL availability will be determined by getProcAddress
	// or from eglGetProcAddress if set.
	if (!f->getProcAddress && !f->eglGetProcAddress) {
		opengl_functions_load_egl((void **)&openglLibrary);
	}
	// Set eglGetProcAddress from getProcAddress if defined, or libraries if not.
	if (!f->eglGetProcAddress) f->eglGetProcAddress = CAST_EGL(resolveEGLGetProcAddress, eglGetProcAddress);
	// User cases: eglGetProcAddress already loaded, coming from getProcAddress or egl libraries loaded.
	if (f->eglGetProcAddress || openglLibrary != NULL) {
		ms_message("[ogl_functions] EGL is enabled.");
		f->eglInitialized = TRUE;
		f->eglInitialized &= ((f->eglQueryAPI = CAST_EGL(resolveEGLQueryAPI, eglQueryAPI)) != NULL);
		f->eglInitialized &= ((f->eglBindAPI = CAST_EGL(resolveEGLBindAPI, eglBindAPI)) != NULL);
		f->eglInitialized &= ((f->eglQueryString = CAST_EGL(resolveEGLQueryString, eglQueryString)) != NULL);
		f->eglInitialized &= ((f->eglGetPlatformDisplayEXT =
		                           CAST_EGL(resolveEGLGetPlatformDisplayEXT, eglGetPlatformDisplayEXT)) != NULL);
		f->eglInitialized &= ((f->eglGetDisplay = CAST_EGL(resolveEGLGetDisplay, eglGetDisplay)) != NULL);
		f->eglInitialized &=
		    ((f->eglGetCurrentDisplay = CAST_EGL(resolveEGLGetCurrentDisplay, eglGetCurrentDisplay)) != NULL);
		f->eglInitialized &=
		    ((f->eglGetCurrentContext = CAST_EGL(resolveEGLGetCurrentContext, eglGetCurrentContext)) != NULL);
		f->eglInitialized &=
		    ((f->eglGetCurrentSurface = CAST_EGL(resolveEGLGetCurrentSurface, eglGetCurrentSurface)) != NULL);
		f->eglInitialized &= ((f->eglInitialize = CAST_EGL(resolveEGLInitialize, eglInitialize)) != NULL);
		f->eglInitialized &= ((f->eglChooseConfig = CAST_EGL(resolveEGLChooseConfig, eglChooseConfig)) != NULL);
		f->eglInitialized &= ((f->eglCreateContext = CAST_EGL(resolveEGLCreateContext, eglCreateContext)) != NULL);
		f->eglInitialized &=
		    ((f->eglCreateWindowSurface = CAST_EGL(resolveEGLCreateWindowSurface, eglCreateWindowSurface)) != NULL);
		f->eglInitialized &= ((f->eglMakeCurrent = CAST_EGL(resolveEGLMakeCurrent, eglMakeCurrent)) != NULL);
		f->eglInitialized &= ((f->eglGetError = CAST_EGL(resolveEGLGetError, eglGetError)) != NULL);
		f->eglInitialized &= ((f->eglSwapBuffers = CAST_EGL(resolveEGLSwapBuffers, eglSwapBuffers)) != NULL);
		f->eglInitialized &= ((f->eglQuerySurface = CAST_EGL(resolveEGLQuerySurface, eglQuerySurface)) != NULL);
		f->eglInitialized &= ((f->eglDestroySurface = CAST_EGL(resolveEGLDestroySurface, eglDestroySurface)) != NULL);
		f->eglInitialized &= ((f->eglDestroyContext = CAST_EGL(resolveEGLDestroyContext, eglDestroyContext)) != NULL);
		f->eglInitialized &= ((f->eglReleaseThread = CAST_EGL(resolveEGLReleaseThread, eglReleaseThread)) != NULL);
		f->eglInitialized &= ((f->eglTerminate = CAST_EGL(resolveEGLTerminate, eglTerminate)) != NULL);
	} else ms_message("[ogl_functions] EGL is disabled");
}

#ifdef __cplusplus
}
#endif
