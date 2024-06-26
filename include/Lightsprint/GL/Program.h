//----------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
//! \file Program.h
//! \brief LightsprintGL | GLSL program with vertex and fragment shaders
//----------------------------------------------------------------------------

#ifndef PROGRAM_H
#define PROGRAM_H

#include "Lightsprint/RRMemory.h"
#include <string> // locationCache
#include <unordered_map> // locationCache
#ifdef __ANDROID__
	#include <GLES2/gl2.h>
#else
	#include <GL/glew.h>
#endif

// define RR_GL_API
#ifdef _MSC_VER
	#ifdef RR_GL_STATIC
		// build or use static library
		#define RR_GL_API
	#elif defined(RR_GL_BUILD)
		// build dll
		#define RR_GL_API __declspec(dllexport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#else
		// use dll
		#define RR_GL_API __declspec(dllimport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#endif
#else
	// build or use static library
	#define RR_GL_API
#endif

// autolink library when external project includes any of its headers
#ifdef _MSC_VER
	// for opengl
	#pragma comment(lib,"opengl32.lib")
	#pragma comment(lib,"glu32.lib")
	#pragma comment(lib,"glew32.lib")

	#if !defined(RR_GL_MANUAL_LINK) && !defined(RR_GL_BUILD)
		#ifdef RR_GL_STATIC
			// use static library
			#ifdef NDEBUG
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER "_sr.lib")
			#else
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER "_sd.lib")
			#endif
		#else
			// use dll
			#ifdef NDEBUG
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER ".lib")
			#else
				#pragma comment(lib,"LightsprintGL." RR_LIB_COMPILER "_dd.lib")
			#endif
		#endif
	#endif
#endif

#ifdef __ANDROID__
	#define RR_GL_ES2 // use OpenGL ES 2.0, not OpenGL. if not defined, both OpenGL and OpenGL ES paths are supported via bool s_es
#endif

namespace rr_gl
{

// vertex attrib array indices
// numbers are hardcoded in "layout(location = ...)"
enum
{
	VAA_POSITION = 0,
	VAA_NORMAL = 1,
	VAA_UV_MATERIAL_DIFFUSE = 2,
	VAA_UV_MATERIAL_SPECULAR = 3,
	VAA_UV_MATERIAL_EMISSIVE = 4,
	VAA_UV_MATERIAL_TRANSPARENT = 5,
	VAA_UV_MATERIAL_BUMP = 6,
	VAA_UV_UNWRAP = 7,
	VAA_UV_FORCED_2D = 8,
	VAA_UV_LAST = 8, // If you increase it, see [#17]
	VAA_COLOR = 9,
	VAA_TANGENT = 10,
	VAA_BITANGENT = 11,
	VAA_COUNT = 12
};

/////////////////////////////////////////////////////////////////////////////
//
// Program

//! GLSL program.
//
//! GLSL is language used by OpenGL API for writing shaders.
//! Program is one or more shaders linked together, see OpenGL for more details.
class RR_GL_API Program : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Creates GLSL program by linking one vertex and one fragment shader together.
	//! Shaders are created by concatenating string defines
	//! and contents of file vertexShader / fragmentShader.
	//! \param defines
	//!  String inserted before compilation at the beginning of shader source code. May be nullptr.
	//! \param vertexShader
	//!  Name of text file with GLSL vertex shader. May be empty for fixed pipeline.
	//! \param fragmentShader
	//!  Name of text file with GLSL fragment shader. May be empty for fixed pipeline.
	static Program* create(const char* defines, const rr::RRString& vertexShader, const rr::RRString& fragmentShader);

	~Program();

	//! Validates program and returns true only when it was successful,
	//! see OpenGL for more details on validation.
	//! Validation is optional process used mainly for debugging.
	bool isValid();
	//! Sets rendering pipeline so that program will be used for
	//! following primitives rendered.
	//! Program must be linked before use.
	void useIt();

	//! Sets uniform texture (activates texture unit, binds texture), returns texture unit used.
	//
	//! First call after useIt() uses texture unit 0, next one uses unit 1 etc.
	//! If code in range 0..15 is entered, all textures with the same code are sent to the same unit.
	//!
	//! Following fragments of code do the same
	//! - sendTexture(name,tex);
	//! - sendTexture(name,nullptr); tex->bindTexture();
	//! - unsigned unit = sendTexture(name,nullptr); glActiveTexture(GL_TEXTURE0+unit); tex->bindTexture();
	//!
	//! When doing multiple renders that differ only in textures, use codes
	//! - useIt(); for (int i=0;i<100;i++) { sendTexture(name1,...,1); sendTexture(name2,...,2); draw(); }
	//! With codes, the same 2 texture units are used 100 times.
	//! Without codes, 200 texture units would be allocated.
	unsigned sendTexture(const char* name, const class Texture* t, int code = -1);

	//! Unbinds texture previously sent via sendTexture(,,code);
	void unsendTexture(int code);

	//! Sets uniform of type float.
	void sendUniform(const char* name, float x);
	//! Sets uniform of type vec2.
	void sendUniform(const char* name, float x, float y);
	//! Sets uniform of type vec3.
	void sendUniform(const char* name, const rr::RRVec3& xyz);
	//! Sets uniform of type vec4.
	void sendUniform(const char* name, const rr::RRVec4& xyzw);
	//! Sets uniform of type int or sampler2D or samplerCube or sampler2DShadow.
	void sendUniform(const char* name, int x);
	//! Sets uniform of type int2.
	void sendUniform(const char* name, int x, int y);
	//! Sets uniform of type mat2, mat3 or mat4.
	void sendUniform(const char* name, const float *m, bool transp=false, int size=4);

	//! Sets array of uniforms of type int or sampler2D or samplerCube or sampler2DShadow.
	void sendUniformArray(const char* name, int count, const GLint* x);
	void sendUniformArray(const char* name, int count, const GLfloat* x);
	void sendUniformArray(const char* name, int count, const rr::RRVec2* x);
	void sendUniformArray(const char* name, int count, const rr::RRVec3* x);
	void sendUniformArray(const char* name, int count, const rr::RRVec4* x);

	//! Does uniform exist in program? 
	bool uniformExists(const char* uniformName);

	//! Print OpenGL log to console. False by default. Set true for debugging.
	static void logMessages(bool enable);
private:
	Program(const char* defines, const rr::RRString& vertexShader, const rr::RRString& fragmentShader);
	int getLoc(const char* name);
	bool isLinked();
	bool logLooksSafe();

	class Shader *vertex, *fragment;
	unsigned handle;
	bool linked;
	unsigned nextTextureUnit; // texture unit to be used in next setTexture()
	signed char assignedTextureUnit[16]; // indexed by code
	std::unordered_map<const char*,int> locationCache; // accelerates getLoc(). [#66] hashing string was slow, now we hash pointer only, so it must be unique, don't generate different uniform names to single buffer
	std::unordered_map<const char*,bool> existsCache; // accelerates uniformExists(). -"-
};

}; // namespace

#endif
