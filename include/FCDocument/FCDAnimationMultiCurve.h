/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/
/*
	Based on the FS Import classes:
	Copyright (C) 2005-2006 Feeling Software Inc
	Copyright (C) 2005-2006 Autodesk Media Entertainment
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/**
	@file FCDAnimationMultiCurve.h
	This file contains the FCDAnimationMultiCurve class.
*/

#ifndef _FCD_ANIMATION_MULTI_CURVE_H_
#define _FCD_ANIMATION_MULTI_CURVE_H_

class FCDocument;

#ifndef __FCD_OBJECT_H_
#include "FCDocument/FCDObject.h"
#endif // __FCD_OBJECT_H_

typedef float (*FCDConversionFunction)(float v); /**< A simple conversion function. */
typedef float (*FCDCollapsingFunction)(float* values, uint32 count); /**< A collapsing function. It converts multiple floating-point values into one floating-point value. */
typedef fm::pvector<FCDAnimationCurve> FCDAnimationCurveList; /**< A dynamically-sized array of animation curves. */
typedef fm::pvector<const FCDAnimationCurve> FCDAnimationCurveConstList; /**< A dynamically-sized array of constant animation curve pointers. */

/**
	A COLLADA multi-dimensional animation curve.

	This is a utility class that is used to convert multiple
	animation curves into one animation curve that has multiple
	dimensions, but only one list of key inputs.

	FCollada will never create a multi-dimensional animation curve
	during the import of a COLLADA document.

	@ingroup FCDocument
*/
class FCOLLADA_EXPORT FCDAnimationMultiCurve : public FCDObject
{
private:
	DeclareObjectType(FCDObject);

	// The number of merged curves
	uint32 dimension;

	// Target information
	int32 targetElement;
	fm::string* targetQualifiers;

	// Input information
	FloatList keys,* keyValues;
	FMVector2List* inTangents,* outTangents;
	FMVector3List* tcbParameters;
	FMVector2List* easeInOuts;
	FUDaeInfinity::Infinity preInfinity, postInfinity;

	// The interpolation values follow the FUDaeInterpolation enum (FUDaeEnum.h)
	UInt32List interpolations;

public:
	/** Constructor.
		The number of dimensions will not change in the lifetime of a
		multi-dimensional curve.
		@param document The COLLADA document that owns the animation curve.
		@param dimension The number of dimensions for the animation curve. */
	FCDAnimationMultiCurve(FCDocument* document, uint32 dimension);

	/** Destructor. */
	virtual ~FCDAnimationMultiCurve();

	/** Merges multiple single-dimensional animation curves into one
		multi-dimensional animation curve.
		For each NULL element found within the 'toMerge' list, the corresponding
		default value is used. If there are not enough default values provided, zero is assumed.
		The number of dimensions for the output animation curve is taken as the size of the 'toMerge' list.
		@param toMerge The list of single-dimensional animation curves to merge. This list may
			contain NULL elements, as explained above.
		@param defaultValues The list of default values to use when a NULL element is encountered.
			Default values should be provided even for the elements that are not NULL.
		@param defaultQualifiers The list of qualifiers to use when a NULL element is encountered.
			When no curve is provided: these qualifiers will be used for the associated default value. */
	static FCDAnimationMultiCurve* MergeCurves(const FCDAnimationCurveList& toMerge, const FloatList& defaultValues, const StringList& defaultQualifiers = StringList()) { return MergeCurves(*(FCDAnimationCurveConstList*)&toMerge, defaultValues, defaultQualifiers); }
	static FCDAnimationMultiCurve* MergeCurves(const FCDAnimationCurveConstList& toMerge, const FloatList& defaultValues, const StringList& defaultQualifiers = StringList()); /**< See above. */

	/** Retrieves the number of dimensions for the curve.
		@return The number of dimensions for the curve. */
	inline uint32 GetDimension() const { return dimension; }

	/** Retrieves the list of key inputs for the animation curve.
		@return The list of key inputs. */
	inline FloatList& GetKeys() { return keys; }
	inline const FloatList& GetKeys() const { return keys; } /**< See above. */

	/** Retrieves the lists of key outputs for the animation curve.
		There is one separate list of key outputs for each dimension of the curve.
		@return The lists of key outputs. */
	inline FloatList* GetKeyValues() { return keyValues; }
	inline const FloatList* GetKeyValues() const { return keyValues; } /**< See above. */

	/** Retrieves the lists of key in-tangent values for the animation curve.
		These lists have data only if the curve includes segments with the bezier interpolation.
		There is one separate list of key in-tangent values for each dimension of the curve.
		@return The lists of in-tangent values. */
	inline FMVector2List* GetInTangents() { return inTangents; }
	inline const FMVector2List* GetInTangents() const { return inTangents; } /**< See above. */

	/** Retrieves the lists of key out-tangent values for the animation curve.
		These lists have data only if the curve includes segments with the bezier interpolation.
		There is one separate list of key out-tangent values for each dimension of the curve.
		@return The lists of out-tangent values. */
	inline FMVector2List* GetOutTangents() { return outTangents; }
	inline const FMVector2List* GetOutTangents() const { return outTangents; } /**< See above. */

	/** Retrieves the list of key TCB values for the animation curve.
		This list has data only for curves that include segments with the TCB interpolation.
		@return The list of TCB values. */
	inline FMVector3List* GetTCBs() { return tcbParameters; }
	inline const FMVector3List* GetTCBs() const { return tcbParameters; } /**< See above. */

	/** Retrieves the list of key ease-in/ease-out values for the TCB animation curve.
		This list has data only for curves that include segments with the TCB interpolation.
		@return The list of ease-in/ease-out values. */
	inline FMVector2List* GetEaseInOuts() { return easeInOuts; }
	inline const FMVector2List* GetEaseInOuts() const { return easeInOuts; } /**< See above. */

	/** Retrieves the list of interpolation type for the segments of the animation curve.
		There is always one interpolation type for each key in the curve. The interpolation type
		of a segment of the curve is set at the key at which begins the segment.
		@see FUDaeInterpolation
		@return The list of interpolation types. */
	inline UInt32List& GetInterpolations() { return interpolations; }
	inline const UInt32List& GetInterpolations() const { return interpolations; } /**< See above. */

	/** Retrieves the type of behavior for the curve if the input value is
		outside the input interval defined by the curve keys and less than any key input value.
		@see FUDaeInfinity
		@return The pre-infinity behavior of the curve. */
	inline FUDaeInfinity::Infinity GetPreInfinity() const { return preInfinity; }

	/** Sets the behavior of the curve if the input value is
		outside the input interval defined by the curve keys and less than any key input value.
		@see FUDaeInfinity
		@param infinity The pre-infinity behavior of the curve. */
	inline void SetPreInfinity(FUDaeInfinity::Infinity infinity) { preInfinity = infinity; SetDirtyFlag(); }

	/** Retrieves the type of behavior for the curve if the input value is
		outside the input interval defined by the curve keys and greater than any key input value.
		@see FUDaeInfinity
		@return The post-infinity behavior of the curve. */
	inline FUDaeInfinity::Infinity GetPostInfinity() const { return postInfinity; }

	/** Sets the behavior of the curve if the input value is
		outside the input interval defined by the curve keys and greater than any key input value.
		@see FUDaeInfinity
		@param infinity The post-infinity behavior of the curve. */
	inline void SetPostInfinity(FUDaeInfinity::Infinity infinity) { postInfinity = infinity; SetDirtyFlag(); }

	/** Evaluates the animation curve.
		@param input An input value.
		@param output An array of floating-point values to fill in with the sampled values. */
	void Evaluate(float input, float* output) const;

	/** Collapses this multi-dimensional curve into a one-dimensional curve.
		@param collapse The function to use to collapse multiple floating-point
			values into one. Set this to NULL to use the default collapsing
			function, which averages all the values.
		@see Average TakeFirst */
	FCDAnimationCurve* Collapse(FCDCollapsingFunction collapse = NULL) const;

	/** [INTERNAL] Writes out the data sources necessary to import the animation curve
		to a given XML tree node.
		@param parentNode The XML tree node in which to create the data sources.
		@param baseId A COLLADA Id prefix to use when generating the source ids. */
	void WriteSourceToXML(xmlNode* parentNode, const fm::string& baseId);

	/** [INTERNAL] Writes out the sampler that puts together the data sources
		and generates a sampling function.
		@param parentNode The XML tree node in which to create the sampler.
		@param baseId The COLLADA id prefix used when generating the source ids.
			This prefix is also used to generate the sampler COLLADA id.
		@return The created XML tree node. */
	xmlNode* WriteSamplerToXML(xmlNode* parentNode, const fm::string& baseId);

	/** [INTERNAL] Writes out the animation channel that attaches the sampling function
		to the animatable value.
		@param parentNode The XML tree node in which to create the sampler.
		@param baseId The COLLADA Id prefix used when generating the source ids
			and the sampler id.
		@param pointer The target pointer prefix for the targeted animated element.
		@return The created XML tree node. */
	xmlNode* WriteChannelToXML(xmlNode* parentNode, const fm::string& baseId, const fm::string& pointer);

	/** [INTERNAL] Retrieves the target element suffix for the curve.
		This will be -1 if the animated element does not belong to an
		animated element list.
		@return The target element suffix. */
	inline int32 GetTargetElement() const { return targetElement; }

	/** [INTERNAL] Sets the target element suffix for the curve.
		@param e The target element suffix. Set to value to -1
			if the animated element does not belong to an animated element list. */
	inline void SetTargetElement(int32 e) { targetElement = e; SetDirtyFlag(); }
};

/**
	Retrieves the first floating-point value of a list of floating-point values.
	This is a typical conversion function.
	@param values The list of floating-point values.
	@param count The number of values within the given list.
*/
inline float TakeFirst(float* values, uint32 count) { return (count > 0) ? *values : 0.0f; }

/**
	Retrieves the average value of a list of floating-point values.
	This is a typical conversion function.
	@param values The list of floating-point values.
	@param count The number of values within the given list.
*/
inline float Average(float* values, uint32 count) { float v = 0.0f; for (uint32 i = 0; i < count; ++i) v += values[i]; v /= float(count); return v; }

#endif // _FCD_ANIMATION_MULTI_CURVE_H_
