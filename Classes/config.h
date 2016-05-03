/*
 *  config.h
 *  urMus
 *
 *  Created by Jong Wook Kim on 5/26/10.\
 *
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

// If you compile for pre 4.3 devices (2nd gen iPod Touches for example)
#undef LEGACY42

// Enable or disable Camera code. Needs to be disabled for devices without camera
#define USECAMERA

// Enable or disable urMus handling mirror displays. Disabling this currently also disables separate external page display.
// If disabled, SetExternalOrientation() allows to set the mirror display orientation and uses Apple's centering code
#undef HANDLEEXTERNALDISPLAYS

// OpenGLES2 enabled. Many code parts such as the camera filter code require opengles2 shaders. This flag will enable opengles1 versions of the code as they exist. Usually newer features are only written in opengles2.
#define OPENGLES2
// GPUIMAGE enables GPUIMAGE based shader/camera filters. Only guaranteed to work when OPENGLES2 is enabled.
#ifdef OPENGLES2
#define GPUIMAGE
#else
//#define GPUIMAGE
#endif
// Enabled Apple's font rendering rather than FreeType2/Glyph Atlas code. The Apple rendering is stable but not portable.
#undef UISTRINGS

#ifndef UISTRINGS
// Use FTGL library for gylph atlas code. Recommended for speed.
#define FTGL
// Flag that forces complete rerendering of glyphs when string is redrawn. Slow, bad etc...
#define CACHESTRINGTEXTURE
#endif

// To use MoMu's audio handling use this call. (Recommended, alternative is not currently maintained)
#define USEMUMOAUDIO



#if defined( __WIN32__ ) || defined( _WIN32 )
	#define TARGET_WIN32
#elif defined( __APPLE_CC__)
	#define TARGET_APPLE
	#include <TargetConditionals.h>

	#if (TARGET_OF_IPHONE_SIMULATOR) || (TARGET_OS_IPHONE) || (TARGET_IPHONE)
		#define TARGET_IPHONE
		#define TARGET_OPENGLES
	#else
		#define TARGET_OSX
	#endif
#else
	#ifdef ANDROID
		#define TARGET_ANDROID
		#define SO_NOSIGPIPE 0
		#include <stdio.h>
	#endif
	#define TARGET_LINUX
#endif

//-------------------------------


// then the the platform specific includes:
#ifdef TARGET_WIN32
//this is for TryEnterCriticalSection
//http://www.zeroc.com/forums/help-center/351-ice-1-2-tryentercriticalsection-problem.html
#ifndef _WIN32_WINNT
#   define _WIN32_WINNT 0x400
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#if (_MSC_VER)       // microsoft visual studio
#pragma warning(disable : 4996)     // disable all deprecation warnings
#pragma warning(disable : 4068)     // unknown pragmas
#pragma warning(disable : 4101)     // unreferenced local variable
#pragma	warning(disable : 4312)		// type cast conversion (in qt vp)
#pragma warning(disable : 4311)		// type cast pointer truncation (qt vp)
#pragma warning(disable : 4018)		// signed/unsigned mismatch (since vector.size() is a size_t)
#pragma warning(disable : 4267)		// conversion from size_t to Size warning... possible loss of data
#pragma warning(disable : 4800)		// 'Boolean' : forcing value to bool 'true' or 'false'
#pragma warning(disable : 4099)		// for debug, PDB 'vc80.pdb' was not found with...
#endif

#define TARGET_LITTLE_ENDIAN			// intel cpu

// some gl.h files, like dev-c++, are old - this is pretty universal
#ifndef GL_BGR_EXT
#define GL_BGR_EXT 0x80E0
#endif
#endif

#ifdef TARGET_OSX
#ifndef __MACOSX_CORE__
#define __MACOSX_CORE__
#endif
#include <unistd.h>
#include "GLee.h"
#include <OpenGL/glu.h>
#include <ApplicationServices/ApplicationServices.h>

#if defined(__LITTLE_ENDIAN__)
#define TARGET_LITTLE_ENDIAN		// intel cpu
#endif
#endif

#ifdef TARGET_ANDROID
#include <unistd.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#define GL_BGRA 0x80E1
#endif

#ifdef TARGET_IPHONE
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#endif


#ifndef __APPLE_CC__

	#ifndef NULL
		#define NULL 0
	#endif

	#ifndef nil
		#define nil 0
	#endif

	typedef signed int SInt32;
	typedef unsigned int UInt32;
	typedef signed short SInt16;
	typedef unsigned short UInt16;

	#define MAX(x,y) (((x)>(y))?(x):(y))


	#endif

#endif
	