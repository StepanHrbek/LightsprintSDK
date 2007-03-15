/*
    Copyright (C) 2005-2007 Feeling Software Inc.
    MIT License: http://www.opensource.org/licenses/mit-license.php
*/

/**
	@file FCDEffectTechnique.h
	This file declares the FCDEffectTechnique class.
*/

#ifndef _FCD_EFFECT_TECHNIQUE_H_
#define _FCD_EFFECT_TECHNIQUE_H_

#ifndef __FCD_OBJECT_H_
#include "FCDocument/FCDObject.h"
#endif // __FCD_OBJECT_H_

class FCDEffectCode;
class FCDEffectPass;
class FCDEffectParameter;
class FCDEffectParameterList;
class FCDEffectProfileFX;

typedef fm::pvector<FCDEffectPass> FCDEffectPassList; /**< A dynamically-sized array of effect passes. */
typedef fm::pvector<FCDEffectCode> FCDEffectCodeList; /**< A dynamically-sized array of effect code inclusions. */
typedef FUObjectContainer<FCDEffectPass> FCDEffectPassContainer; /**< A dynamically-sized containment array for effect passes. */
typedef FUObjectContainer<FCDEffectCode> FCDEffectCodeContainer; /**< A dynamically-sized containment array for effect code inclusions. */

/**
	A COLLADA effect technique.

	The COLLADA effect technique contains the passes to be used in the rendering of
	polygon sets.
	
	It also contains a list of effect parameters: both generators and overrides
	and it is the lowest level of abstraction in which you can access effect parameters. For 
	flattened materials, this means that all the effect parameters will be accessible at this level.

	It also contains a list of effect code inclusions.

	@ingroup FCDEffect
*/
class FCOLLADA_EXPORT FCDEffectTechnique : public FCDObject
{
private:
	DeclareObjectType(FCDObject);
	FCDEffectProfileFX* parent;

	fstring name;
	FCDEffectCodeContainer codes;
	FCDEffectPassContainer passes;
	FCDEffectParameterList* parameters;

public:
	/** Constructor: do not use directly.
		Instead, use the FCDEffectProfileFX::AddTechnique function. 
		@param document The COLLADA document which owns this technique.
		@param _parent The effect profile which contains the technique. */
	FCDEffectTechnique(FCDEffectProfileFX *_parent);

	/** Destructor. */
	virtual ~FCDEffectTechnique();

	/** Retrieves the effect profile that contains this technique.
		@return The parent effect profile. */
	FCDEffectProfileFX* GetParent() { return parent; }
	const FCDEffectProfileFX* GetParent() const { return parent; } /**< See above. */

	/** Retrieves the COLLADA id of the parent effect.
		This function is mostly useful as a shortcut for debugging and reporting.
		@return The COLLADA id of the parent effect. */
	const fm::string& GetDaeId() const;

	/** Retrieves the sub-id of the technique.
		@return The sub-id of the technique. */
	const fstring& GetName() const { return name; }

	/** Sets the sub-id of the technique.
		The effect technique must have a valid sub-id that is unique
		within its scope. Otherwise, one will be provided on XML export.
		@param _name A valid sub-id. */
	void SetName(const fstring& _name) { name = _name; SetDirtyFlag(); }

	/** Retrieves the list of passes.
		@return The list of passes. */
	FCDEffectPassContainer& GetPassList() { return passes; }
	const FCDEffectPassContainer& GetPassList() const { return passes; } /**< See above. */

	/** Retrieves the number of passes contained within this effect technique.
		@return The number of passes. */
	size_t GetPassCount() const { return passes.size(); }

	/** Retrieves a specific pass contained within this effect technique.
		@param index The index of the pass.
		@return The pass. This pointer will be NULL if the index is out-of-bounds. */
	FCDEffectPass* GetPass(size_t index) { FUAssert(index < GetPassCount(), return NULL); return passes.at(index); }
	const FCDEffectPass* GetPass(size_t index) const { FUAssert(index < GetPassCount(), return NULL); return passes.at(index); } /**< See above. */

	/** Adds a new pass to this effect technique.
		@return The new pass. */
	FCDEffectPass* AddPass();

	/** Retrieves the list of code inclusions.
		@return The list of code inclusions. */		
	FCDEffectCodeContainer& GetCodeList() { return codes; }
	const FCDEffectCodeContainer& GetCodeList() const { return codes; } /**< See above. */

	/** Retrieves the number of code inclusions contained within the effect profile.
		@return The number of code inclusions. */
	size_t GetCodeCount() const { return codes.size(); }

	/** Retrieves a code inclusion contained within the effect profile.
		@param index The index of the code inclusion.
		@return The code inclusion. This pointer will be NULL if the index is out-of-bounds. */
	FCDEffectCode* GetCode(size_t index) { FUAssert(index < GetCodeCount(), return NULL); return codes.at(index); }
	const FCDEffectCode* GetCode(size_t index) const { FUAssert(index < GetCodeCount(), return NULL); return codes.at(index); } /**< See above. */

	/** Retrieves the code inclusion with the given sub-id.
		@param sid A COLLADA sub-id.
		@return The code inclusion with the given sub-id. This pointer will be NULL,
			if there are no code inclusions that match the given sub-id. */
	FCDEffectCode* FindCode(const fm::string& sid);
	const FCDEffectCode* FindCode(const fm::string& sid) const; /**< See above. */

	/** Adds a new code inclusion to this effect profile.
		@return The new code inclusion. */
	FCDEffectCode* AddCode();

	/** Retrieves the list of effect parameters contained within the effect profile.
		This is the lowest level of abstraction and may contain either effect parameter
		generators or effect parameter overrides.
		@return The list of effect parameters. */
	FCDEffectParameterList* GetParameterList() { return parameters; }
	const FCDEffectParameterList* GetParameterList() const { return parameters; } /**< See above. */

	/** [INTERNAL] Inserts an existing parameter into the list of effect parameters
		at this abstraction level. This function is used during the flattening of a material.
		@param parameter The effect parameter to insert. */
	void AddParameter(FCDEffectParameter* parameter);

	/** Retrieves an effect parameter.
		Looks for the effect parameter with the correct reference, in order to bind or override its value.
		@param reference The reference to match. In the case of effect parameter generators,
			the sub-id is used to match.
		@return The first effect parameter that matches the reference.
			This pointer will be NULL if no effect parameter matches the given semantic. */
	const FCDEffectParameter* FindParameter(const char* reference) const;

	/** Retrieves an effect parameter.
		Looks for the effect parameter with the correct semantic, in order to bind or override its value.
		@param semantic The effect parameter semantic to match.
		@return The first effect parameter that matches the semantic.
			This pointer will be NULL if no effect parameter matches the given semantic. */
	virtual FCDEffectParameter* FindParameterBySemantic(const fm::string& semantic);

	/** Retrieves a subset of the effect parameter list.
		Look for the effect parameter generators with the correct semantic.
		@param semantic The effect parameter semantic to match.
		@param parameters The list of parameters to fill in. This list is not cleared. */
	virtual void FindParametersBySemantic(const fm::string& semantic, FCDEffectParameterList& parameters);

	/** Retrieves a subset of the effect parameter list.
		Look for the effect parameter generators with the correct reference.
		@param reference The effect parameter reference to match. In the case of effect
			parameter generators, the reference is replaced by the sub-id.
		@param parameters The list of parameters to fill in. This list is not cleared. */
	virtual void FindParametersByReference(const fm::string& reference, FCDEffectParameterList& parameters);

	/** [INTERNAL] Clones the full effect technique.
		@param clone The cloned technique.
			If this pointer is NULL, a new technique is created and
			you will need to release this new techniquetechnique.
		@return The cloned technique. This pointer will never be NULL. */
	FCDEffectTechnique* Clone(FCDEffectTechnique* clone = NULL) const;

	/** [INTERNAL] Flattens this effect technique.
		Merges the parameter overrides into the parameter generators. */
	void Flatten();

	/** [INTERNAL] Reads in the effect technique from a given COLLADA XML tree node.
		@param techniqueNode The COLLADA XML tree node.
		@param profileNode X @deprecated bad interface : this dependency must be taken out.
		@return The status of the import. If the status is 'false',
			it may be dangerous to extract information from the effect technique.*/
	bool LoadFromXML(xmlNode* techniqueNode, xmlNode* profileNode);

	/** [INTERNAL] Links the effect technique and its parameters. This is done after
		the whole COLLADA document has been processed once. */
	void Link();

	/** [INTERNAL] Writes out the effect technique to the given COLLADA XML tree node.
		@param parentNode The COLLADA XML parent node in which to insert the effect technique.
		@return The created element XML tree node. */
	xmlNode* WriteToXML(xmlNode* parentNode) const;
};

#endif
