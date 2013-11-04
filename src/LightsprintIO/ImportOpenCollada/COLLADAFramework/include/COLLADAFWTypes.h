/*
    Copyright (c) 2008-2009 NetAllied Systems GmbH

    This file is part of COLLADAFramework.

    Licensed under the MIT Open Source License, 
    for details please see LICENSE file or the website
    http://www.opensource.org/licenses/mit-license.php
*/

#ifndef __COLLADAFW_TYPES_H__
#define __COLLADAFW_TYPES_H__

#include "COLLADAFWPrerequisites.h"
#include "COLLADAFWArrayPrimitiveType.h"
#include "Math/COLLADABUMathMatrix4.h"


namespace COLLADAFW
{
    /** An array of unsigned int values. */
	typedef ArrayPrimitiveType<unsigned int> UIntValuesArray;
	typedef ArrayPrimitiveType<int> IntValuesArray;

	typedef ArrayPrimitiveType<size_t> SizeTValuesArray;

	typedef ArrayPrimitiveType<unsigned long long> ULongLongValuesArray;
	typedef ArrayPrimitiveType<long long> LongLongValuesArray;

	typedef ArrayPrimitiveType<float> FloatArray;
	typedef ArrayPrimitiveType<double> DoubleArray;


	typedef Array<COLLADABU::Math::Matrix4> Matrix4Array;

	//  typedef xsNCName String;

	typedef unsigned long long ObjectId;

	typedef unsigned long FileId;

	typedef unsigned long MaterialId;

	typedef unsigned long TextureMapId;

	/** Data type to reference sampler. Used by texture.*/
	typedef size_t SamplerID;

	static const SamplerID INVALID_SAMPLER_ID = -1;
	static const TextureMapId INVALID_MAP_ID = -1;

	/** Enum listing all physical dimensions used in the data model.*/
	enum PhysicalDimension
	{
		PHYSICAL_DIMENSION_UNKNOWN,
		PHYSICAL_DIMENSION_TIME,
		PHYSICAL_DIMENSION_LENGTH,
		PHYSICAL_DIMENSION_ANGLE,
		PHYSICAL_DIMENSION_COLOR,
		PHYSICAL_DIMENSION_NUMBER
	};

	typedef ArrayPrimitiveType<PhysicalDimension> PhysicalDimensionArray;

    // Element Type Enum
    namespace COLLADA_TYPE
    {
        enum ClassId
		{
            NO_TYPE = 0,
            ANY = 1,
            INPUTGLOBAL = 2,
            INPUTLOCAL = 3,
            INPUTLOCALOFFSET = 4,
            INSTANCEWITHEXTRA = 5,
            TARGETABLEFLOAT = 6,
            TARGETABLEFLOAT3 = 7,
            FX_SURFACE_FORMAT_HINT_COMMON = 8,
            CHANNELS = 9,
            RANGE = 10,
            PRECISION = 11,
            OPTION = 12,
            FX_SURFACE_INIT_PLANAR_COMMON = 13,
            ALL = 14,
            FX_SURFACE_INIT_VOLUME_COMMON = 15,
            PRIMARY = 16,
            FX_SURFACE_INIT_CUBE_COMMON = 17,
            ORDER = 18,
            FACE = 19,
            FX_SURFACE_INIT_FROM_COMMON = 20,
            FX_SURFACE_COMMON = 21,
            FORMAT = 22,
            SIZE = 23,
            VIEWPORT_RATIO = 24,
            MIP_LEVELS = 25,
            MIPMAP_GENERATE = 26,
            FX_SAMPLER1D_COMMON = 27,
            SOURCE = 28,
            WRAP_S = 29,
            MINFILTER = 30,
            MAGFILTER = 31,
            MIPFILTER = 32,
            BORDER_COLOR = 33,
            MIPMAP_MAXLEVEL = 34,
            MIPMAP_BIAS = 35,
            FX_SAMPLER2D_COMMON = 36,
            WRAP_T = 37,
            FX_SAMPLER3D_COMMON = 38,
            WRAP_P = 39,
            FX_SAMPLERCUBE_COMMON = 40,
            FX_SAMPLERRECT_COMMON = 41,
            FX_SAMPLERDEPTH_COMMON = 42,
            FX_COLORTARGET_COMMON = 43,
            FX_DEPTHTARGET_COMMON = 44,
            FX_STENCILTARGET_COMMON = 45,
            FX_CLEARCOLOR_COMMON = 46,
            FX_CLEARDEPTH_COMMON = 47,
            FX_CLEARSTENCIL_COMMON = 48,
            FX_ANNOTATE_COMMON = 49,
            FX_INCLUDE_COMMON = 50,
            FX_NEWPARAM_COMMON = 51,
            SEMANTIC = 52,
            MODIFIER = 53,
            FX_CODE_PROFILE = 54,
            GL_SAMPLER1D = 55,
            GL_SAMPLER2D = 56,
            GL_SAMPLER3D = 57,
            GL_SAMPLERCUBE = 58,
            GL_SAMPLERRECT = 59,
            GL_SAMPLERDEPTH = 60,
            GLSL_NEWARRAY_TYPE = 61,
            GLSL_SETARRAY_TYPE = 62,
            GLSL_SURFACE_TYPE = 63,
            GENERATOR = 64,
            NAME = 65,
            GLSL_NEWPARAM = 66,
            GLSL_SETPARAM_SIMPLE = 67,
            GLSL_SETPARAM = 68,
            COMMON_FLOAT_OR_PARAM_TYPE = 69,
            FLOAT = 70,
            PARAM = 71,
            COMMON_COLOR_OR_TEXTURE_TYPE = 72,
            COLOR = 73,
            TEXTURE = 74,
            COMMON_TRANSPARENT_TYPE = 75,
            COMMON_NEWPARAM_TYPE = 76,
            FLOAT2 = 77,
            FLOAT3 = 78,
            FLOAT4 = 79,
            CG_SAMPLER1D = 80,
            CG_SAMPLER2D = 81,
            CG_SAMPLER3D = 82,
            CG_SAMPLERCUBE = 83,
            CG_SAMPLERRECT = 84,
            CG_SAMPLERDEPTH = 85,
            CG_CONNECT_PARAM = 86,
            CG_NEWARRAY_TYPE = 87,
            CG_SETARRAY_TYPE = 88,
            CG_SETUSER_TYPE = 89,
            CG_SURFACE_TYPE = 90,
            CG_NEWPARAM = 91,
            CG_SETPARAM_SIMPLE = 92,
            CG_SETPARAM = 93,
            GLES_TEXTURE_CONSTANT_TYPE = 94,
            GLES_TEXENV_COMMAND_TYPE = 95,
            GLES_TEXCOMBINER_ARGUMENTRGB_TYPE = 96,
            GLES_TEXCOMBINER_ARGUMENTALPHA_TYPE = 97,
            GLES_TEXCOMBINER_COMMANDRGB_TYPE = 98,
            GLES_TEXCOMBINER_COMMANDALPHA_TYPE = 99,
            GLES_TEXCOMBINER_COMMAND_TYPE = 100,
            GLES_TEXTURE_PIPELINE = 101,
            GLES_TEXTURE_UNIT = 102,
            SURFACE = 103,
            SAMPLER_STATE = 104,
            TEXCOORD = 105,
            GLES_SAMPLER_STATE = 106,
            GLES_NEWPARAM = 107,
            FX_SURFACE_INIT_COMMON = 108,
            INIT_AS_NULL = 109,
            INIT_AS_TARGET = 110,
            FX_ANNOTATE_TYPE_COMMON = 111,
            BOOL = 112,
            BOOL2 = 113,
            BOOL3 = 114,
            BOOL4 = 115,
            INT = 116,
            INT2 = 117,
            INT3 = 118,
            INT4 = 119,
            FLOAT2X2 = 120,
            FLOAT3X3 = 121,
            FLOAT4X4 = 122,
            STRING = 123,
            FX_BASIC_TYPE_COMMON = 124,
            FLOAT1X1 = 125,
            FLOAT1X2 = 126,
            FLOAT1X3 = 127,
            FLOAT1X4 = 128,
            FLOAT2X1 = 129,
            FLOAT2X3 = 130,
            FLOAT2X4 = 131,
            FLOAT3X1 = 132,
            FLOAT3X2 = 133,
            FLOAT3X4 = 134,
            FLOAT4X1 = 135,
            FLOAT4X2 = 136,
            FLOAT4X3 = 137,
            ENUM = 138,
            GL_PIPELINE_SETTINGS = 139,
            ALPHA_FUNC = 140,
            FUNC = 141,
            VALUE = 142,
            BLEND_FUNC = 143,
            SRC = 144,
            DEST = 145,
            BLEND_FUNC_SEPARATE = 146,
            SRC_RGB = 147,
            DEST_RGB = 148,
            SRC_ALPHA = 149,
            DEST_ALPHA = 150,
            BLEND_EQUATION = 151,
            BLEND_EQUATION_SEPARATE = 152,
            RGB = 153,
            ALPHA = 154,
            COLOR_MATERIAL = 155,
            MODE = 156,
            CULL_FACE = 157,
            DEPTH_FUNC = 158,
            FOG_MODE = 159,
            FOG_COORD_SRC = 160,
            FRONT_FACE = 161,
            LIGHT_MODEL_COLOR_CONTROL = 162,
            LOGIC_OP = 163,
            POLYGON_MODE = 164,
            SHADE_MODEL = 165,
            STENCIL_FUNC = 166,
            REF = 167,
            MASK = 168,
            STENCIL_OP = 169,
            FAIL = 170,
            ZFAIL = 171,
            ZPASS = 172,
            STENCIL_FUNC_SEPARATE = 173,
            FRONT = 174,
            BACK = 175,
            STENCIL_OP_SEPARATE = 176,
            STENCIL_MASK_SEPARATE = 177,
            LIGHT_ENABLE = 178,
            LIGHT_AMBIENT = 179,
            LIGHT_DIFFUSE = 180,
            LIGHT_SPECULAR = 181,
            LIGHT_POSITION = 182,
            LIGHT_CONSTANT_ATTENUATION = 183,
            LIGHT_LINEAR_ATTENUATION = 184,
            LIGHT_QUADRATIC_ATTENUATION = 185,
            LIGHT_SPOT_CUTOFF = 186,
            LIGHT_SPOT_DIRECTION = 187,
            LIGHT_SPOT_EXPONENT = 188,
            TEXTURE1D = 189,
            TEXTURE2D = 190,
            TEXTURE3D = 191,
            TEXTURECUBE = 192,
            TEXTURERECT = 193,
            TEXTUREDEPTH = 194,
            TEXTURE1D_ENABLE = 195,
            TEXTURE2D_ENABLE = 196,
            TEXTURE3D_ENABLE = 197,
            TEXTURECUBE_ENABLE = 198,
            TEXTURERECT_ENABLE = 199,
            TEXTUREDEPTH_ENABLE = 200,
            TEXTURE_ENV_COLOR = 201,
            TEXTURE_ENV_MODE = 202,
            CLIP_PLANE = 203,
            CLIP_PLANE_ENABLE = 204,
            BLEND_COLOR = 205,
            CLEAR_COLOR = 206,
            CLEAR_STENCIL = 207,
            CLEAR_DEPTH = 208,
            COLOR_MASK = 209,
            DEPTH_BOUNDS = 210,
            DEPTH_MASK = 211,
            DEPTH_RANGE = 212,
            FOG_DENSITY = 213,
            FOG_START = 214,
            FOG_END = 215,
            FOG_COLOR = 216,
            LIGHT_MODEL_AMBIENT = 217,
            LIGHTING_ENABLE = 218,
            LINE_STIPPLE = 219,
            LINE_WIDTH = 220,
            MATERIAL_AMBIENT = 221,
            MATERIAL_DIFFUSE = 222,
            MATERIAL_EMISSION = 223,
            MATERIAL_SHININESS = 224,
            MATERIAL_SPECULAR = 225,
            MODEL_VIEW_MATRIX = 226,
            POINT_DISTANCE_ATTENUATION = 227,
            POINT_FADE_THRESHOLD_SIZE = 228,
            POINT_SIZE = 229,
            POINT_SIZE_MIN = 230,
            POINT_SIZE_MAX = 231,
            POLYGON_OFFSET = 232,
            PROJECTION_MATRIX = 233,
            SCISSOR = 234,
            STENCIL_MASK = 235,
            ALPHA_TEST_ENABLE = 236,
            AUTO_NORMAL_ENABLE = 237,
            BLEND_ENABLE = 238,
            COLOR_LOGIC_OP_ENABLE = 239,
            COLOR_MATERIAL_ENABLE = 240,
            CULL_FACE_ENABLE = 241,
            DEPTH_BOUNDS_ENABLE = 242,
            DEPTH_CLAMP_ENABLE = 243,
            DEPTH_TEST_ENABLE = 244,
            DITHER_ENABLE = 245,
            FOG_ENABLE = 246,
            LIGHT_MODEL_LOCAL_VIEWER_ENABLE = 247,
            LIGHT_MODEL_TWO_SIDE_ENABLE = 248,
            LINE_SMOOTH_ENABLE = 249,
            LINE_STIPPLE_ENABLE = 250,
            LOGIC_OP_ENABLE = 251,
            MULTISAMPLE_ENABLE = 252,
            NORMALIZE_ENABLE = 253,
            POINT_SMOOTH_ENABLE = 254,
            POLYGON_OFFSET_FILL_ENABLE = 255,
            POLYGON_OFFSET_LINE_ENABLE = 256,
            POLYGON_OFFSET_POINT_ENABLE = 257,
            POLYGON_SMOOTH_ENABLE = 258,
            POLYGON_STIPPLE_ENABLE = 259,
            RESCALE_NORMAL_ENABLE = 260,
            SAMPLE_ALPHA_TO_COVERAGE_ENABLE = 261,
            SAMPLE_ALPHA_TO_ONE_ENABLE = 262,
            SAMPLE_COVERAGE_ENABLE = 263,
            SCISSOR_TEST_ENABLE = 264,
            STENCIL_TEST_ENABLE = 265,
            GLSL_PARAM_TYPE = 266,
            CG_PARAM_TYPE = 267,
            BOOL1 = 268,
            BOOL1X1 = 269,
            BOOL1X2 = 270,
            BOOL1X3 = 271,
            BOOL1X4 = 272,
            BOOL2X1 = 273,
            BOOL2X2 = 274,
            BOOL2X3 = 275,
            BOOL2X4 = 276,
            BOOL3X1 = 277,
            BOOL3X2 = 278,
            BOOL3X3 = 279,
            BOOL3X4 = 280,
            BOOL4X1 = 281,
            BOOL4X2 = 282,
            BOOL4X3 = 283,
            BOOL4X4 = 284,
            FLOAT1 = 285,
            INT1 = 286,
            INT1X1 = 287,
            INT1X2 = 288,
            INT1X3 = 289,
            INT1X4 = 290,
            INT2X1 = 291,
            INT2X2 = 292,
            INT2X3 = 293,
            INT2X4 = 294,
            INT3X1 = 295,
            INT3X2 = 296,
            INT3X3 = 297,
            INT3X4 = 298,
            INT4X1 = 299,
            INT4X2 = 300,
            INT4X3 = 301,
            INT4X4 = 302,
            HALF = 303,
            HALF1 = 304,
            HALF2 = 305,
            HALF3 = 306,
            HALF4 = 307,
            HALF1X1 = 308,
            HALF1X2 = 309,
            HALF1X3 = 310,
            HALF1X4 = 311,
            HALF2X1 = 312,
            HALF2X2 = 313,
            HALF2X3 = 314,
            HALF2X4 = 315,
            HALF3X1 = 316,
            HALF3X2 = 317,
            HALF3X3 = 318,
            HALF3X4 = 319,
            HALF4X1 = 320,
            HALF4X2 = 321,
            HALF4X3 = 322,
            HALF4X4 = 323,
            FIXED = 324,
            FIXED1 = 325,
            FIXED2 = 326,
            FIXED3 = 327,
            FIXED4 = 328,
            FIXED1X1 = 329,
            FIXED1X2 = 330,
            FIXED1X3 = 331,
            FIXED1X4 = 332,
            FIXED2X1 = 333,
            FIXED2X2 = 334,
            FIXED2X3 = 335,
            FIXED2X4 = 336,
            FIXED3X1 = 337,
            FIXED3X2 = 338,
            FIXED3X3 = 339,
            FIXED3X4 = 340,
            FIXED4X1 = 341,
            FIXED4X2 = 342,
            FIXED4X3 = 343,
            FIXED4X4 = 344,
            GLES_PIPELINE_SETTINGS = 345,
            TEXTURE_PIPELINE = 346,
            LIGHT_LINEAR_ATTENUTATION = 347,
            TEXTURE_PIPELINE_ENABLE = 348,
            GLES_BASIC_TYPE_COMMON = 349,
            COLLADA = 350,
            SCENE = 351,
            IDREF_ARRAY = 352,
            NAME_ARRAY = 353,
            BOOL_ARRAY = 354,
            FLOAT_ARRAY = 355,
            INT_ARRAY = 356,
            ACCESSOR = 357,
            TECHNIQUE_COMMON = 358,
            GEOMETRY = 359,
            MESH = 360,
            SPLINE = 361,
            CONTROL_VERTICES = 362,
            P = 363,
            LINES = 364,
            LINESTRIPS = 365,
            POLYGONS = 366,
            PH = 367,
            H = 368,
            POLYLIST = 369,
            VCOUNT = 370,
            TRIANGLES = 371,
            TRIFANS = 372,
            TRISTRIPS = 373,
            VERTICES = 374,
            LOOKAT = 375,
            MATRIX = 376,
            ROTATE = 377,
            SCALE = 378,
            SKEW = 379,
            TRANSLATE = 380,
            IMAGE = 381,
            DATA = 382,
            INIT_FROM = 383,
            LIGHT = 384,
            AMBIENT = 385,
            DIRECTIONAL = 386,
            POINT = 387,
            SPOT = 388,
            MATERIAL = 389,
            CAMERA = 390,
            OPTICS = 391,
            ORTHOGRAPHIC = 392,
            PERSPECTIVE = 393,
            IMAGER = 394,
            ANIMATION = 395,
            ANIMATION_CLIP = 396,
            CHANNEL = 397,
            SAMPLER = 398,
            CONTROLLER = 399,
            SKIN = 400,
            BIND_SHAPE_MATRIX = 401,
            JOINTS = 402,
            VERTEX_WEIGHTS = 403,
            V = 404,
            MORPH = 405,
            TARGETS = 406,
            ASSET = 407,
            CONTRIBUTOR = 408,
            AUTHOR = 409,
            AUTHORING_TOOL = 410,
            COMMENTS = 411,
            COPYRIGHT = 412,
            SOURCE_DATA = 413,
            CREATED = 414,
            KEYWORDS = 415,
            MODIFIED = 416,
            REVISION = 417,
            SUBJECT = 418,
            TITLE = 419,
            UNIT = 420,
            UP_AXIS = 421,
            EXTRA = 422,
            TECHNIQUE = 423,
            NODE = 424,
            VISUAL_SCENE = 425,
            EVALUATE_SCENE = 426,
            RENDER = 427,
            LAYER = 428,
            BIND_MATERIAL = 429,
            INSTANCE_CAMERA = 430,
            INSTANCE_CONTROLLER = 431,
            SKELETON = 432,
            INSTANCE_EFFECT = 433,
            TECHNIQUE_HINT = 434,
            SETPARAM = 435,
            INSTANCE_FORCE_FIELD = 436,
            INSTANCE_GEOMETRY = 437,
            INSTANCE_LIGHT = 438,
            INSTANCE_MATERIAL = 439,
            BIND = 440,
            BIND_VERTEX_INPUT = 441,
            INSTANCE_NODE = 442,
            INSTANCE_PHYSICS_MATERIAL = 443,
            INSTANCE_PHYSICS_MODEL = 444,
            INSTANCE_RIGID_BODY = 445,
            ANGULAR_VELOCITY = 446,
            VELOCITY = 447,
            DYNAMIC = 448,
            MASS_FRAME = 449,
            SHAPE = 450,
            HOLLOW = 451,
            INSTANCE_RIGID_CONSTRAINT = 452,
            LIBRARY_ANIMATIONS = 453,
            LIBRARY_ANIMATION_CLIPS = 454,
            LIBRARY_CAMERAS = 455,
            LIBRARY_CONTROLLERS = 456,
            LIBRARY_GEOMETRIES = 457,
            LIBRARY_EFFECTS = 458,
            LIBRARY_FORCE_FIELDS = 459,
            LIBRARY_IMAGES = 460,
            LIBRARY_LIGHTS = 461,
            LIBRARY_MATERIALS = 462,
            LIBRARY_NODES = 463,
            LIBRARY_PHYSICS_MATERIALS = 464,
            LIBRARY_PHYSICS_MODELS = 465,
            LIBRARY_PHYSICS_SCENES = 466,
            LIBRARY_VISUAL_SCENES = 467,
            FX_PROFILE_ABSTRACT = 468,
            EFFECT = 469,
            GL_HOOK_ABSTRACT = 470,
            PROFILE_GLSL = 471,
            PASS = 472,
            DRAW = 473,
            SHADER = 474,
            COMPILER_TARGET = 475,
            COMPILER_OPTIONS = 476,
            PROFILE_COMMON = 477,
            CONSTANT = 478,
            LAMBERT = 479,
            PHONG = 480,
            BLINN = 481,
            PROFILE_CG = 482,
            PROFILE_GLES = 483,
            COLOR_TARGET = 484,
            DEPTH_TARGET = 485,
            STENCIL_TARGET = 486,
            COLOR_CLEAR = 487,
            DEPTH_CLEAR = 488,
            STENCIL_CLEAR = 489,
            BOX = 490,
            HALF_EXTENTS = 491,
            PLANE = 492,
            EQUATION = 493,
            SPHERE = 494,
            RADIUS = 495,
            ELLIPSOID = 496,
            CYLINDER = 497,
            HEIGHT = 498,
            TAPERED_CYLINDER = 499,
            RADIUS1 = 500,
            RADIUS2 = 501,
            CAPSULE = 502,
            TAPERED_CAPSULE = 503,
            CONVEX_MESH = 504,
            FORCE_FIELD = 505,
            PHYSICS_MATERIAL = 506,
            PHYSICS_SCENE = 507,
            RIGID_BODY = 508,
            RIGID_CONSTRAINT = 509,
            REF_ATTACHMENT = 510,
            ATTACHMENT = 511,
            ENABLED = 512,
            INTERPENETRATE = 513,
            LIMITS = 514,
            SWING_CONE_AND_TWIST = 515,
            LINEAR = 516,
            SPRING = 517,
            ANGULAR = 518,
            PHYSICS_MODEL = 519,

            PRIMITIVE_ELEMENT = 520,

            INSTANCE_VISUAL_SCENE = 521,
            INSTANCE_SCENE_GRAPH = 522,

			ANIMATIONLIST = 1000,
			SKIN_DATA = 1001,

			FORMULA = 1002,
			FORMULAS = 1003,

			JOINT = 1004,
			JOINTPRIMITIVE = 1005,

			KINEMATICS_MODEL = 1006,
			KINEMATICS_CONTROLLER = 1007,
			INSTANCE_KINEMATICS_SCENE = 1008

			}
            ;



    }

	typedef COLLADA_TYPE::ClassId ClassId;

	/** Holding effect maps parameters loaded from <extra>. */
	struct TextureSamplerAndCoordsId
	{
		TextureSamplerAndCoordsId() : samplerId( INVALID_SAMPLER_ID ), textureMapId( INVALID_MAP_ID ) {}
		TextureSamplerAndCoordsId( const TextureSamplerAndCoordsId& pre ) : samplerId( pre.samplerId ), textureMapId( pre.textureMapId ) {}
		virtual ~TextureSamplerAndCoordsId() {}

		SamplerID samplerId;
		TextureMapId textureMapId;
	};
	struct TextureAttributes : public TextureSamplerAndCoordsId
	{
		TextureAttributes() : TextureSamplerAndCoordsId(), textureSampler(), texCoord() {}
		TextureAttributes(const TextureAttributes& pre) : TextureSamplerAndCoordsId(pre), textureSampler( pre.textureSampler ), texCoord( pre.texCoord ) {}
		String textureSampler;
		String texCoord;

		TextureAttributes* clone() const { return new TextureAttributes(*this); }
	};

}

#endif // __COLLADAFW_TYPES_H__
