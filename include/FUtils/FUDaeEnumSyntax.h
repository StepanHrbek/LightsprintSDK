/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef _FU_DAE_ENUM_SYNTAX_H_
#define _FU_DAE_ENUM_SYNTAX_H_

#define	DAE_STEP_INTERPOLATION						"STEP"
#define DAE_LINEAR_INTERPOLATION					"LINEAR"
#define DAE_BEZIER_INTERPOLATION					"BEZIER"
#define DAE_TCB_INTERPOLATION						"TCB"

// spline types
#define DAE_LINEAR_SPLINE_TYPE						"LINEAR"
#define DAE_BEZIER_SPLINE_TYPE						"BEZIER"
#define DAE_TCB_SPLINE_TYPE							"TCB"
#define DAE_NURBS_SPLINE_TYPE						"NURBS"
#define DAE_UNKNOWN_SPLINE_TYPE						"UNKNOWN"

// spline forms
#define DAE_OPEN_SPLINE_FORM						"OPEN"
#define DAE_CLOSED_SPLINE_FORM						"CLOSED"
#define DAE_UNKNOWN_SPLINE_FORM						"UNKNOWN"


#define DAE_CONSTANT_FUNCTION						"CONSTANT"
#define DAE_LINEAR_FUNCTION							"LINEAR"
#define DAE_QUADRATIC_FUNCTION						"QUADRATIC"

#define DAE_AMBIENT_TEXTURE_CHANNEL					"AMBIENT"
#define DAE_BUMP_TEXTURE_CHANNEL					"BUMP"
#define DAE_DIFFUSE_TEXTURE_CHANNEL					"DIFFUSE"
#define DAE_DISPLACEMENT_TEXTURE_CHANNEL			"DISPLACEMENT"
#define DAE_EMISSION_TEXTURE_CHANNEL				"GLOW"
#define DAE_FILTER_TEXTURE_CHANNEL					"FILTER"
#define	DAE_OPACITY_TEXTURE_CHANNEL					"OPACITY"
#define DAE_REFLECTION_TEXTURE_CHANNEL				"REFLECTION"
#define DAE_REFRACTION_TEXTURE_CHANNEL				"REFRACTION"
#define DAE_SHININESS_TEXTURE_CHANNEL				"SHININESS"
#define DAE_SPECULAR_TEXTURE_CHANNEL				"SPECULAR"
#define DAE_SPECULARLEVEL_TEXTURE_CHANNEL			"SPECULAR-LEVEL"
#define DAE_TRANSPARENT_TEXTURE_CHANNEL				"TRANSPARENT"

#define DAE_NORMALIZED_MORPH_METHOD					"NORMALIZED"
#define DAE_RELATIVE_MORPH_METHOD					"RELATIVE"

#define DAE_COLOR_INPUT								"COLOR"
#define DAE_GEOBINORMAL_INPUT						"BINORMAL"
#define DAE_GEOTANGENT_INPUT						"TANGENT"
#define DAE_MAPPING_INPUT							"UV"
#define DAE_NORMAL_INPUT							"NORMAL"
#define DAE_POSITION_INPUT							"POSITION"
#define DAE_TEXCOORD_INPUT							"TEXCOORD"
#define DAE_TEXBINORMAL_INPUT						"TEXBINORMAL"
#define DAE_TEXTANGENT_INPUT						"TEXTANGENT"
#define DAE_VERTEX_INPUT							"VERTEX"
#define DAEMAYA_EXTRA_INPUT							"EXTRA"					// ColladaMaya-specific

#define DAE_FX_PROFILE_COMMON_ELEMENT				"profile_COMMON"
#define DAE_FX_PROFILE_CG_ELEMENT					"profile_CG"
#define DAE_FX_PROFILE_HLSL_ELEMENT					"profile_HLSL"
#define DAE_FX_PROFILE_GLSL_ELEMENT					"profile_GLSL"
#define DAE_FX_PROFILE_GLES_ELEMENT					"profile_GLES"

#define DAE_FX_FUNCTION_NEVER						"NEVER"
#define DAE_FX_FUNCTION_LESS						"LESS"
#define DAE_FX_FUNCTION_EQUAL						"EQUAL"
#define DAE_FX_FUNCTION_LEQUAL						"LEQUAL"
#define DAE_FX_FUNCTION_GREATER						"GREATER"
#define DAE_FX_FUNCTION_NEQUAL						"NOTEQUAL"
#define DAE_FX_FUNCTION_GEQUAL						"GEQUAL"
#define DAE_FX_FUNCTION_ALWAYS						"ALWAYS"

#define DAE_FX_STATE_STENCILOP_KEEP					"KEEP"
#define DAE_FX_STATE_STENCILOP_ZERO					"ZERO"
#define DAE_FX_STATE_STENCILOP_REPLACE				"REPLACE"
#define DAE_FX_STATE_STENCILOP_INCREMENT			"INCR"
#define DAE_FX_STATE_STENCILOP_DECREMENT			"DECR"
#define DAE_FX_STATE_STENCILOP_INVERT				"INVERT"
#define DAE_FX_STATE_STENCILOP_INCREMENT_WRAP		"INCR_WRAP"
#define DAE_FX_STATE_STENCILOP_DECREMENT_WRAP		"DECR_WRAP"

#define DAE_FX_STATE_BLENDTYPE_ZERO							"ZERO"
#define DAE_FX_STATE_BLENDTYPE_ONE							"ONE"
#define DAE_FX_STATE_BLENDTYPE_SOURCE_COLOR					"SRC_COLOR"
#define DAE_FX_STATE_BLENDTYPE_ONE_MINUS_SOURCE_COLOR		"ONE_MINUS_SRC_COLOR"
#define DAE_FX_STATE_BLENDTYPE_DESTINATION_COLOR			"DEST_COLOR"
#define DAE_FX_STATE_BLENDTYPE_ONE_MINUS_DESTINATION_COLOR	"ONE_MINUS_DEST_COLOR"
#define DAE_FX_STATE_BLENDTYPE_SOURCE_ALPHA					"SRC_ALPHA"
#define DAE_FX_STATE_BLENDTYPE_ONE_MINUS_SOURCE_ALPHA		"ONE_MINUS_SRC_ALPHA"
#define DAE_FX_STATE_BLENDTYPE_DESTINATION_ALPHA			"DEST_ALPHA"
#define DAE_FX_STATE_BLENDTYPE_ONE_MINUS_DESTINATION_ALPHA	"ONE_MINUS_DEST_ALPHA"
#define DAE_FX_STATE_BLENDTYPE_CONSTANT_COLOR				"CONSTANT_COLOR"
#define DAE_FX_STATE_BLENDTYPE_ONE_MINUS_CONSTANT_COLOR		"ONE_MINUS_CONSTANT_COLOR"
#define DAE_FX_STATE_BLENDTYPE_CONSTANT_ALPHA				"CONSTANT_ALPHA"
#define DAE_FX_STATE_BLENDTYPE_ONE_MINUS_CONSTANT_ALPHA		"ONE_MINUS_CONSTANT_ALPHA"
#define DAE_FX_STATE_BLENDTYPE_SOURCE_ALPHA_SATURATE		"SRC_ALPHA_SATURATE"

#define DAE_FX_STATE_FACETYPE_FRONT					"FRONT"
#define DAE_FX_STATE_FACETYPE_BACK					"BACK"
#define DAE_FX_STATE_FACETYPE_FRONT_AND_BACK		"FRONT_AND_BACK"

#define DAE_FX_STATE_BLENDEQ_ADD					"ADD"
#define DAE_FX_STATE_BLENDEQ_SUBTRACT				"SUBTRACT"
#define DAE_FX_STATE_BLENDEQ_REVERSE_SUBTRACT		"REVERSE_SUBTRACT"
#define DAE_FX_STATE_BLENDEQ_MIN					"MIN"
#define DAE_FX_STATE_BLENDEQ_MAX					"MAX"

#define DAE_FX_STATE_MATERIALTYPE_EMISSION			"EMISSION"
#define DAE_FX_STATE_MATERIALTYPE_AMBIENT			"AMBIENT"
#define DAE_FX_STATE_MATERIALTYPE_DIFFUSE			"DIFFUSE"
#define DAE_FX_STATE_MATERIALTYPE_SPECULAR			"SPECULAR"
#define DAE_FX_STATE_MATERIALTYPE_AMBDIFF			"AMBIENT_AND_DIFFUSE"

#define DAE_FX_STATE_FOGTYPE_LINEAR					"LINEAR"
#define DAE_FX_STATE_FOGTYPE_EXP					"EXP"
#define DAE_FX_STATE_FOGTYPE_EXP2					"EXP2"

#define DAE_FX_STATE_FOGCOORD_FOG_COORDINATE		"FOG_COORDINATE"
#define DAE_FX_STATE_FOGCOORD_FRAGMENT_DEPTH		"FRAGMENT_DEPTH"

#define DAE_FX_STATE_FFACE_CW						"CW"
#define DAE_FX_STATE_FFACE_CCW						"CCW"

#define DAE_FX_STATE_LOGICOP_CLEAR					"CLEAR"
#define DAE_FX_STATE_LOGICOP_AND					"AND"
#define DAE_FX_STATE_LOGICOP_AND_REVERSE			"AND_REVERSE"
#define DAE_FX_STATE_LOGICOP_COPY					"COPY"
#define DAE_FX_STATE_LOGICOP_AND_INVERTED			"AND_INVERTED"
#define DAE_FX_STATE_LOGICOP_NOOP					"NOOP"
#define DAE_FX_STATE_LOGICOP_XOR					"XOR"
#define DAE_FX_STATE_LOGICOP_OR						"OR"
#define DAE_FX_STATE_LOGICOP_NOR					"NOR"
#define DAE_FX_STATE_LOGICOP_EQUIV					"EQUIV"
#define DAE_FX_STATE_LOGICOP_INVERT					"INVERT"
#define DAE_FX_STATE_LOGICOP_OR_REVERSE				"OR_REVERSE"
#define DAE_FX_STATE_LOGICOP_COPY_INVERTED			"COPY_INVERTED"
#define DAE_FX_STATE_LOGICOP_NAND					"NAND"
#define DAE_FX_STATE_LOGICOP_SET					"SET"

#define DAE_FX_STATE_POLYMODE_POINT					"POINT"
#define DAE_FX_STATE_POLYMODE_LINE					"LINE"
#define DAE_FX_STATE_POLYMODE_FILL					"FILL"

#define DAE_FX_STATE_SHADEMODEL_FLAT				"FLAT"
#define DAE_FX_STATE_SHADEMODEL_SMOOTH				"SMOOTH"

#define DAE_FX_STATE_LMCCT_SINGLE_COLOR				"SINGLE_COLOR"
#define DAE_FX_STATE_LMCCT_SEPARATE_SPECULAR_COLOR	"SEPARATE_SPECULAR_COLOR"

#define DAE_FX_STATE_ALPHA_FUNC						"alpha_func"
#define DAE_FX_STATE_BLEND_FUNC						"blend_func"
#define DAE_FX_STATE_BLEND_FUNC_SEPARATE			"blend_func_separate"
#define DAE_FX_STATE_BLEND_EQUATION					"blend_equation"
#define DAE_FX_STATE_BLEND_EQUATION_SEPARATE		"blend_equation_separate"
#define DAE_FX_STATE_COLOR_MATERIAL					"color_material"
#define DAE_FX_STATE_CULL_FACE						"cull_face"
#define DAE_FX_STATE_DEPTH_FUNC						"depth_func"
#define DAE_FX_STATE_FOG_MODE						"fog_mode"
#define DAE_FX_STATE_FOG_COORD_SRC					"fog_coord_src"
#define DAE_FX_STATE_FRONT_FACE						"front_face"
#define DAE_FX_STATE_LIGHT_MODEL_COLOR_CONTROL		"light_model_color_control"
#define DAE_FX_STATE_LOGIC_OP						"logic_op"
#define DAE_FX_STATE_POLYGON_MODE					"polygon_mode"
#define DAE_FX_STATE_SHADE_MODEL					"shade_model"
#define DAE_FX_STATE_STENCIL_FUNC					"stencil_func"
#define DAE_FX_STATE_STENCIL_OP						"stencil_op"
#define DAE_FX_STATE_STENCIL_FUNC_SEPARATE			"stencil_func_separate"
#define DAE_FX_STATE_STENCIL_OP_SEPARATE			"stencil_op_separate"
#define DAE_FX_STATE_STENCIL_MASK_SEPARATE			"stencil_mask_separate"
#define DAE_FX_STATE_LIGHT_ENABLE					"light_enable"
#define DAE_FX_STATE_LIGHT_AMBIENT					"light_ambient"
#define DAE_FX_STATE_LIGHT_DIFFUSE					"light_diffuse"
#define DAE_FX_STATE_LIGHT_SPECULAR					"light_specular"
#define DAE_FX_STATE_LIGHT_POSITION					"light_position"
#define DAE_FX_STATE_LIGHT_CONSTANT_ATTENUATION		"light_constant_attenuation"
#define DAE_FX_STATE_LIGHT_LINEAR_ATTENUATION		"light_linear_attenuation"
#define DAE_FX_STATE_LIGHT_QUADRATIC_ATTENUATION	"light_quadratic_attenuation"
#define DAE_FX_STATE_LIGHT_SPOT_CUTOFF				"light_spot_cutoff"
#define DAE_FX_STATE_LIGHT_SPOT_DIRECTION			"light_spot_direction"
#define DAE_FX_STATE_LIGHT_SPOT_EXPONENT			"light_spot_exponent"
#define DAE_FX_STATE_TEXTURE1D						"texture1D"
#define DAE_FX_STATE_TEXTURE2D						"texture2D"
#define DAE_FX_STATE_TEXTURE3D						"texture3D"
#define DAE_FX_STATE_TEXTURECUBE					"textureCUBE"
#define DAE_FX_STATE_TEXTURERECT					"textureRECT"
#define DAE_FX_STATE_TEXTUREDEPTH					"textureDEPTH"
#define DAE_FX_STATE_TEXTURE1D_ENABLE				"texture1D_enable"
#define DAE_FX_STATE_TEXTURE2D_ENABLE				"texture2D_enable"
#define DAE_FX_STATE_TEXTURE3D_ENABLE				"texture3D_enable"
#define DAE_FX_STATE_TEXTURECUBE_ENABLE				"textureCUBE_enable"
#define DAE_FX_STATE_TEXTURERECT_ENABLE				"textureRECT_enable"
#define DAE_FX_STATE_TEXTUREDEPTH_ENABLE			"textureDEPTH_enable"
#define DAE_FX_STATE_TEXTURE_ENV_COLOR				"texture_env_color"
#define DAE_FX_STATE_TEXTURE_ENV_MODE				"texture_env_mode"
#define DAE_FX_STATE_CLIP_PLANE						"clip_plane"
#define DAE_FX_STATE_CLIP_PLANE_ENABLE				"clip_plane_enable"
#define DAE_FX_STATE_BLEND_COLOR					"blend_color"
#define DAE_FX_STATE_CLEAR_COLOR					"clear_color"
#define DAE_FX_STATE_CLEAR_STENCIL					"clear_stencil"
#define DAE_FX_STATE_CLEAR_DEPTH					"clear_depth"
#define DAE_FX_STATE_COLOR_MASK						"color_mask"
#define DAE_FX_STATE_DEPTH_BOUNDS					"depth_bounds"
#define DAE_FX_STATE_DEPTH_MASK						"depth_mask"
#define DAE_FX_STATE_DEPTH_RANGE					"depth_range"
#define DAE_FX_STATE_FOG_DENSITY					"fog_density"
#define DAE_FX_STATE_FOG_START						"fog_start"
#define DAE_FX_STATE_FOG_END						"fog_end"
#define DAE_FX_STATE_FOG_COLOR						"fog_color"
#define DAE_FX_STATE_LIGHT_MODEL_AMBIENT			"light_model_ambient"
#define DAE_FX_STATE_LIGHTING_ENABLE				"lighting_enable"
#define DAE_FX_STATE_LINE_STIPPLE					"line_stipple"
#define DAE_FX_STATE_LINE_STIPPLE_ENABLE			"line_stipple_enable"
#define DAE_FX_STATE_LINE_WIDTH						"line_width"
#define DAE_FX_STATE_MATERIAL_AMBIENT				"material_ambient"
#define DAE_FX_STATE_MATERIAL_DIFFUSE				"material_diffuse"
#define DAE_FX_STATE_MATERIAL_EMISSION				"material_emission"
#define DAE_FX_STATE_MATERIAL_SHININESS				"material_shininess"
#define DAE_FX_STATE_MATERIAL_SPECULAR				"material_specular"
#define DAE_FX_STATE_MODEL_VIEW_MATRIX				"model_view_matrix"
#define DAE_FX_STATE_POINT_DISTANCE_ATTENUATION		"point_distance_attenuation"
#define DAE_FX_STATE_POINT_FADE_THRESHOLD_SIZE		"point_fade_threshold_size"
#define DAE_FX_STATE_POINT_SIZE						"point_size"
#define DAE_FX_STATE_POINT_SIZE_MIN					"point_size_min"
#define DAE_FX_STATE_POINT_SIZE_MAX					"point_size_max"
#define DAE_FX_STATE_POLYGON_OFFSET					"polygon_offset"
#define DAE_FX_STATE_PROJECTION_MATRIX				"projection_matrix"
#define DAE_FX_STATE_SCISSOR						"scissor"
#define DAE_FX_STATE_STENCIL_MASK					"stencil_mask"
#define DAE_FX_STATE_ALPHA_TEST_ENABLE				"alpha_test_enable"
#define DAE_FX_STATE_AUTO_NORMAL_ENABLE				"auto_normal_enable"
#define DAE_FX_STATE_BLEND_ENABLE					"blend_enable"
#define DAE_FX_STATE_COLOR_LOGIC_OP_ENABLE			"color_logic_op_enable"
#define DAE_FX_STATE_COLOR_MATERIAL_ENABLE			"color_material_enable"
#define DAE_FX_STATE_CULL_FACE_ENABLE				"cull_face_enable"
#define DAE_FX_STATE_DEPTH_BOUNDS_ENABLE			"depth_bounds_enable"
#define DAE_FX_STATE_DEPTH_CLAMP_ENABLE				"depth_clamp_enable"
#define DAE_FX_STATE_DEPTH_TEST_ENABLE				"depth_test_enable"
#define DAE_FX_STATE_DITHER_ENABLE					"dither_enable"
#define DAE_FX_STATE_FOG_ENABLE						"fog_enable"
#define DAE_FX_STATE_LIGHT_MODEL_LOCAL_VIEWER_ENABLE "light_model_local_viewer_enable"
#define DAE_FX_STATE_LIGHT_MODEL_TWO_SIDE_ENABLE	"light_model_two_side_enable"
#define DAE_FX_STATE_LINE_SMOOTH_ENABLE				"line_smooth_enable"
#define DAE_FX_STATE_LOGIC_OP_ENABLE				"logic_op_enable"
#define DAE_FX_STATE_MULTISAMPLE_ENABLE				"multisample_enable"
#define DAE_FX_STATE_NORMALIZE_ENABLE				"normalize_enable"
#define DAE_FX_STATE_POINT_SMOOTH_ENABLE			"point_smooth_enable"
#define DAE_FX_STATE_POLYGON_OFFSET_FILL_ENABLE		"polygon_offset_fill_enable"
#define DAE_FX_STATE_POLYGON_OFFSET_LINE_ENABLE		"polygon_offset_line_enable"
#define DAE_FX_STATE_POLYGON_OFFSET_POINT_ENABLE	"polygon_offset_point_enable"
#define DAE_FX_STATE_POLYGON_SMOOTH_ENABLE			"polygon_smooth_enable"
#define DAE_FX_STATE_POLYGON_STIPPLE_ENABLE			"polygon_stipple_enable"
#define DAE_FX_STATE_RESCALE_NORMAL_ENABLE			"rescale_normal_enable"
#define DAE_FX_STATE_SAMPLE_ALPHA_TO_COVERAGE_ENABLE "sample_alpha_to_coverage_enable"
#define DAE_FX_STATE_SAMPLE_ALPHA_TO_ONE_ENABLE		"sample_alpha_to_one_enable"
#define DAE_FX_STATE_SAMPLE_COVERAGE_ENABLE			"sample_coverage_enable"
#define DAE_FX_STATE_SCISSOR_TEST_ENABLE			"scissor_test_enable"
#define DAE_FX_STATE_STENCIL_TEST_ENABLE			"stencil_test_enable"

// ColladaMaya enumerated types

#define DAEMAYA_CONSTANT_INFINITY						"CONSTANT"			
#define DAEMAYA_LINEAR_INFINITY							"LINEAR"			
#define DAEMAYA_CYCLE_INFINITY							"CYCLE"				
#define DAEMAYA_CYCLE_RELATIVE_INFINITY					"CYCLE_RELATIVE"	
#define DAEMAYA_OSCILLATE_INFINITY						"OSCILLATE"			

#define DAEMAYA_NONE_BLENDMODE							"NONE"
#define DAEMAYA_OVER_BLENDMODE							"OVER"
#define DAEMAYA_IN_BLENDMODE							"IN"
#define DAEMAYA_OUT_BLENDMODE							"OUT"
#define DAEMAYA_ADD_BLENDMODE							"ADD"
#define DAEMAYA_SUBSTRACT_BLENDMODE						"SUBSTRACT"
#define DAEMAYA_MULTIPLY_BLENDMODE						"MULTIPLY"
#define DAEMAYA_DIFFERENCE_BLENDMODE					"DIFFERENCE"
#define DAEMAYA_LIGHTEN_BLENDMODE						"LIGHTEN"
#define DAEMAYA_DARKEN_BLENDMODE						"DARKEN"
#define DAEMAYA_SATURATE_BLENDMODE						"SATURATE"
#define DAEMAYA_DESATURATE_BLENDMODE					"DESATURATE"
#define DAEMAYA_ILLUMINATE_BLENDMODE					"ILLUMINATE"

#endif // _FU_DAE_ENUM_SYNTAX_H_
