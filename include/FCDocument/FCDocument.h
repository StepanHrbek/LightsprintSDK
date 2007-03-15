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
	@file FCDocument.h
	This file declares the COLLADA document object model top class: FCDocument.
*/

#ifndef _FC_DOCUMENT_H_
#define _FC_DOCUMENT_H_

template <class T> class FCDLibrary;
template <class T> class FUUniqueStringMapT;

class FCDAnimation;
class FCDAnimationChannel;
class FCDAnimationClip;
class FCDAnimated;
class FCDAsset;
class FCDCamera;
class FCDController;
class FCDEffect;
class FCDEntity;
class FCDEmitter;
class FCDExternalReferenceManager;
class FCDForceField;
class FCDGeometry;
class FCDImage;
class FCDLight;
class FCDMaterial;
class FCDObject;
class FCDPhysicsMaterial;
class FCDPhysicsModel;
class FCDPhysicsScene;
class FCDTexture;
class FCDSceneNode;
class FUFileManager;

/**
	A structure to keep the version of the COLLADA file
	Format is major.minor.revision
*/

class FCOLLADA_EXPORT FCDVersion
{
public:
	uint8 major;
	uint8 minor;
	uint8 revision;

	FCDVersion() {major = minor = revision = 0;}
	virtual ~FCDVersion() {};

	/** Extracts the version number from a string in the form of
		major.minor.revision (ex: 1.4.1)
		@param v A string containing the version numbers */
	void ParseVersionNumbers(const fm::string& v);

	/** Comparison functions for version numbers.
		@param v A string containing the version numbers
		@return The result of the comparison */
	bool IsLowerThan(const fm::string& v) const;
	bool IsHigherThan(const fm::string& v) const;
	bool Is(const fm::string& v) const;
};

/**
	A layer declaration.
	Contains a name for the layer and the ids of all the entities within the layer.
*/
class FCOLLADA_EXPORT FCDLayer
{
public:
	fm::string name; /**< The layer name. There is no guarantee of uniqueness. */
	StringList objects; /**< The list of COLLADA entity ids which are contained by this layer. */
};

typedef fm::map<FCDAnimated*, FCDAnimated*> FCDAnimatedSet; /**< A set of animated values. */
typedef fm::pvector<FCDAnimationChannel> FCDAnimationChannelList;  /**< A dynamically-sized array of animation channels. */
typedef fm::map<const float*, FCDAnimated*> FCDAnimatedValueMap;  /**< A map of animatable values to animated values. */
typedef fm::pvector<FCDLayer> FCDLayerList; /**< A dynamically-sized array of layer declarations. */

typedef FCDLibrary<FCDAnimation> FCDAnimationLibrary; /**< A COLLADA library of animation entities. */
typedef FCDLibrary<FCDAnimationClip> FCDAnimationClipLibrary; /**< A COLLADA library of animation clip entities. */
typedef FCDLibrary<FCDCamera> FCDCameraLibrary; /**< A COLLADA library of camera entities. */
typedef FCDLibrary<FCDController> FCDControllerLibrary; /**< A COLLADA library of controller entities. */
typedef FCDLibrary<FCDEffect> FCDEffectLibrary; /**< A COLLADA library of effect entities. */
typedef FCDLibrary<FCDEmitter> FCDEmitterLibrary; /**< A non-standard library of generic emitters. */
typedef	FCDLibrary<FCDForceField> FCDForceFieldLibrary; /**< A COLLADA library of force fields. */
typedef FCDLibrary<FCDGeometry> FCDGeometryLibrary; /**< A COLLADA library of geometric entities. */
typedef FCDLibrary<FCDImage> FCDImageLibrary; /**< A COLLADA library of images. */
typedef FCDLibrary<FCDLight> FCDLightLibrary; /**< A COLLADA library of light entities. */
typedef FCDLibrary<FCDMaterial> FCDMaterialLibrary; /**< A COLLADA library of visual material entities. */
typedef FCDLibrary<FCDSceneNode> FCDVisualSceneNodeLibrary; /**< A COLLADA library of visual scene nodes. */
typedef FCDLibrary<FCDPhysicsModel> FCDPhysicsModelLibrary; /**< A COLLADA library of physics model entities. */
typedef FCDLibrary<FCDPhysicsMaterial> FCDPhysicsMaterialLibrary; /**< A COLLADA library of physics material entities. */
typedef	FCDLibrary<FCDPhysicsScene> FCDPhysicsSceneLibrary; /**< A COLLADA library of physics scene nodes. */
typedef FUUniqueStringMapT<FUSStringBuilder> FUSUniqueStringMap; /**< A set of unique strings. */

/** @defgroup FCDocument COLLADA Document Object Model. */

/** The top class for the COLLADA object model.

	This class holds all the COLLADA libraries, the scene graphs and the
	document's asset tag.

	It also holds some global information, such as the animation start and end
	time and the layers. This global information is only exported by ColladaMaya
	right now.

	@ingroup FCDocument COLLADA Document Object Model
*/
class FCOLLADA_EXPORT FCDocument : public FUObject
{
private:
	DeclareObjectType(FUObject);

	FUFileManager* fileManager;
	FUObjectRef<FCDExternalReferenceManager> externalReferenceManager;
	fstring fileUrl;
	FCDVersion version;

	FUSUniqueStringMap* uniqueNameMap;
	FUObjectPtr<FCDSceneNode> visualSceneRoot;
	FUObjectPtr<FCDPhysicsScene> physicsSceneRoot;

	// Document parameters
	FUObjectRef<FCDAsset> asset;
	float lengthUnitWanted, lengthUnitConversion;
	bool hasStartTime, hasEndTime;
	float startTime, endTime; // Maya-only
	FCDLayerList layers; // Maya-only

	// Parsed and merged libraries
	FUObjectRef<FCDAnimationLibrary> animationLibrary;
	FUObjectRef<FCDAnimationClipLibrary> animationClipLibrary;
	FUObjectRef<FCDCameraLibrary> cameraLibrary;
	FUObjectRef<FCDControllerLibrary> controllerLibrary;
	FUObjectRef<FCDEffectLibrary> effectLibrary;
	FUObjectRef<FCDForceFieldLibrary> forceFieldLibrary;
	FUObjectRef<FCDGeometryLibrary> geometryLibrary;
	FUObjectRef<FCDImageLibrary> imageLibrary;
	FUObjectRef<FCDLightLibrary> lightLibrary;
	FUObjectRef<FCDMaterialLibrary> materialLibrary;
	FUObjectRef<FCDPhysicsModelLibrary> physicsModelLibrary;
	FUObjectRef<FCDPhysicsMaterialLibrary> physicsMaterialLibrary;
	FUObjectRef<FCDPhysicsSceneLibrary> physicsSceneLibrary;
	FUObjectRef<FCDVisualSceneNodeLibrary> visualSceneLibrary;
	FUObjectRef<FCDEmitterLibrary> emitterLibrary;

	// Animated values
	FCDAnimatedSet animatedValues;
	FCDAnimatedValueMap animatedValueMap;

public:
	/** Construct a new COLLADA document. */
	FCDocument();

	/** COLLADA document destructor. This clears out all the memory related to the document. */
	virtual ~FCDocument();

	/** Retrieves the asset information for this COLLADA document. The asset information should always be present.
		@return A pointer to the asset information structure. This pointer should never be NULL. */
	FCDAsset* GetAsset() { return asset; }
	const FCDAsset* GetAsset() const { return asset; }	/**< See above. */

	/** Retrieves the version numbers for this COLLADA document. The version numbers should always be present.
		@return The version number structure.*/
	FCDVersion GetVersion() { return version; }
	const FCDVersion& GetVersion() const { return version; }	/**< See above. */

	/** [INTERNAL] Retrieves the local file manager for the COLLADA document. Used to resolve URIs and transform file
		paths into their relative or absolute equivalent. May be deprecated in future versions.
		@return The file manager for this COLLADA document. This pointer should never be NULL. */
	FUFileManager* GetFileManager() { return fileManager; }
	const FUFileManager* GetFileManager() const { return fileManager; }	/**< See above. */

	/** Retrieves the currently selected visual scene.
		@return The currently selected visual scene structure. */
	FCDSceneNode* GetVisualSceneRoot() { return visualSceneRoot; }
	const FCDSceneNode* GetVisualSceneRoot() const { return visualSceneRoot; } /**< See above. */

	/** Retrieves the currently selected physics scene.
		@return The currently selected physics scene structure. */
	FCDPhysicsScene* GetPhysicsSceneRoot() { return physicsSceneRoot; }
	const FCDPhysicsScene* GetPhysicsSceneRoot() const { return physicsSceneRoot; } /**< See above. */

	/** [INTERNAL] Retrieves the map of unique ids for this document.
		@return The map of unique ids for this document. */
	FUSUniqueStringMap* GetUniqueNameMap() { return uniqueNameMap; }
	const FUSUniqueStringMap* GetUniqueNameMap() const { return uniqueNameMap; } /**< See above. */

	/** Retrieves the external reference manager.
		@return The external reference manager. */
	FCDExternalReferenceManager* GetExternalReferenceManager() { return externalReferenceManager; }
	const FCDExternalReferenceManager* GetExternalReferenceManager() const { return externalReferenceManager; } /**< See above. */

	/** Retrieves the file URL for this document.
		@return The file URL for the document. */
	const fstring& GetFileUrl() const { return fileUrl; }

	/** Sets the file URL for this document.
		Useful when working with external references.
		@param filepath The filename for the document. */
	void SetFileUrl(const fstring& filename);

	/** @deprecated Retrieves the vector pre-determined by the document as the up-axis.
		This information is now contained within the asset structure. Please use GetAsset()->GetUpAxis().
		@return A 3D vector for the up axis direction. */
	const FMVector3& GetUpAxis() const;

	/** @deprecated Retrieves the length of 1 distance unit for this document, in meters. The default is 1.0,
		which means that the 1 unit in the document is equal to 1 meter. This information is 
		now contained within the asset structure. Please use GetAsset()->GetLengthUnit().
		@return The length of 1 distance unit for this document. */
	float GetLengthUnit() const;

	/** [INTERNAL] Retrieves the conversion factor between the requested distance unit
		and the document's distance unit.
		@return The distance unit conversion factor. */
	inline float GetLengthUnitConversion() const { return lengthUnitConversion; }

	/** Sets the wanted distance unit factor, in meters, for this document.
		For example, Maya uses centimeters internally and sets the wanted distance unit factor to 0.01.
		Use this parameter on export as well, to enforce a length conversion:
		set the value of the actual engine distance unit.
		@param wantedLengthUnit The wanted distance unit, in meters. */
	inline void SetLengthUnitWanted(float wantedLengthUnit) { lengthUnitWanted = wantedLengthUnit; }

	/** Returns whether a start time is being enforced for the document.
		@return Whether the document has a start time. */
	inline bool HasStartTime() const { return hasStartTime; }
	/** Retrieves the start time set for the document.
		@return The document start time. */
	inline float GetStartTime() const { return startTime; }
	/** Enforces a certain time as the start time for the document.
		@param time The document start time. */
	inline void SetStartTime(float time) { startTime = time; hasStartTime = true; }

	/** Returns whether a end time is being enforced for the document.
		@return Whether the document has a end time. */
	inline bool HasEndTime() const { return hasEndTime; }
	/** Retrieves the end time set for the document.
		@return The document end time. */
	inline float GetEndTime() const { return endTime; }
	/** Enforces a certain time as the end time for the document.
		@param time The document end time. */
	inline void SetEndTime(float time) { endTime = time; hasEndTime = true; }

	/** Retrieves the list of entity layers.
		@return The list of entity layers. */
	inline FCDLayerList& GetLayers() { return layers; }
	inline const FCDLayerList& GetLayers() const { return layers; } /**< See above. */

	/** Retrieves the number of entity layers contained within the document.
		@return The number of layers. */
	inline size_t GetLayerCount() const { return layers.size(); }

	/** Retrieves a specific entity layer contained within the document.
		@param index The index of the layer.
		@return The entity layer. This pointer will be NULL if the index
			is out-of-bounds. */
	inline FCDLayer* GetLayer(size_t index) { FUAssert(index < GetLayerCount(), return NULL); return layers.at(index); }
	inline const FCDLayer* GetLayer(size_t index) const { FUAssert(index < GetLayerCount(), return NULL); return layers.at(index); }

	/** Adds an entity layer to the document.
		@return The new layer. */
	FCDLayer* AddLayer();

	/** Releases an entity layer from the document
		@param layer The layer to release. */
	void ReleaseLayer(FCDLayer* layer);

	/** Retrieves the animation library. The animation library contains the animation curves
		within a tree structure. To create and find animation curves, do not use the animation
		library directly: use the FCDAnimated class, the FindAnimatedValue() function and the
		RegisterAnimatedValue() function.
		@return The animation library. */
	inline FCDAnimationLibrary* GetAnimationLibrary() { return animationLibrary; }
	inline const FCDAnimationLibrary* GetAnimationLibrary() const { return animationLibrary; } /**< See above. */

	/** Retrieves the animation clip library. The animation clip library contains a list of animation clips.
		Each animation clip instantiates nodes from the animation library. Sections of the animation curves
		belonging to the instantiated animation nodes are thereby packaged together as animation clips.
		@return The animation clip library. */
	inline FCDAnimationClipLibrary* GetAnimationClipLibrary() { return animationClipLibrary; }
	inline const FCDAnimationClipLibrary* GetAnimationClipLibrary() const { return animationClipLibrary; } /**< See above. */

	/** Retrieves the camera library. The camera library contains a list of cameras, which may be
		instantiated within the scene graph. COLLADA supports two camera types: perspective and orthographic.
		@return The camera library. */
	inline FCDCameraLibrary* GetCameraLibrary() { return cameraLibrary; }
	inline const FCDCameraLibrary* GetCameraLibrary() const { return cameraLibrary; } /**< See above. */

	/** Retrieves the controller library. The controller library contains a list of controllers, which may
		be instantiated within the scene graph. COLLADA supports two controller types: skin and morph.
		@return The controller library. */
	inline FCDControllerLibrary* GetControllerLibrary() { return controllerLibrary; }
	inline const FCDControllerLibrary* GetControllerLibrary() const { return controllerLibrary; } /**< See above. */

	/** Retrieves the geometry library. The geometry library contains a list of basic geometries, which may
		be instantiated within the scene graph and may be used by controllers.
		COLLADA supports two geometry types: mesh and spline.
		@return The geometry library. */
	inline FCDGeometryLibrary* GetGeometryLibrary() { return geometryLibrary; }
	inline const FCDGeometryLibrary* GetGeometryLibrary() const { return geometryLibrary; } /**< See above. */

	/** Retrieves the physics force field library.
		Force fields are emitters of physical force and have no COMMON profile in COLLADA.
		@return The force field library. */
	inline FCDForceFieldLibrary* GetForceFieldLibrary() { return forceFieldLibrary; }
	inline const FCDForceFieldLibrary* GetForceFieldLibrary() const { return forceFieldLibrary; }

	/** Retrieves the image library. The image library contains a list of images. Images are used
		by effects for textures.
		@return The image library. */
	inline FCDImageLibrary* GetImageLibrary() { return imageLibrary; }
	inline const FCDImageLibrary* GetImageLibrary() const { return imageLibrary; } /**< See above. */

	/** Retrieves the light library. The light library contains a list of light, which may be
		instantiated within the scene graph. COLLADA supports four light types: ambient, directional,
		point and spot lights.
		@return The light library. */
	inline FCDLightLibrary* GetLightLibrary() { return lightLibrary; }
	inline const FCDLightLibrary* GetLightLibrary() const { return lightLibrary; } /**< See above. */

	/** Retrieves the visual material library. The visual material library contains a list of visual materials,
		which are bound to mesh polygons within the scene graph. A visual material instantiates an effect and
		presets the effect parameters for a given visual result.
		@return The visual material library. */
	inline FCDMaterialLibrary* GetMaterialLibrary() { return materialLibrary; }
	inline const FCDMaterialLibrary* GetMaterialLibrary() const { return materialLibrary; } /**< See above. */

	/** Retrieves the effect library. The effect library contains a list of effects, which may be instantiated
		by materials. An effect defines an interface for a rendering shader. A ColladaFX effect may contain multiple
		passes and techniques for different platforms or level of details.
		@return The effect library. */
	inline FCDEffectLibrary* GetEffectLibrary() { return effectLibrary; } 
	inline const FCDEffectLibrary* GetEffectLibrary() const { return effectLibrary; } /**< See above. */

	/** Retrieves the visual scene library. The visual scene library contains an acyclic directed graph of
		visual scene nodes: a visual scene node contains one or more parent nodes and zero or more child nodes.
		A visual scene node also contains 3D transformations: translation, rotation, scale, skew, as well as
		the compound transformations: lookAt and matrix. A visual scene node also contains instances of
		geometries, controllers, cameras and/or lights. Only one visual scene should be used at one time
		by the global scene.
		@return The visual scene library. */
	inline FCDVisualSceneNodeLibrary* GetVisualSceneLibrary() { return visualSceneLibrary; }
	inline const FCDVisualSceneNodeLibrary* GetVisualSceneLibrary() const { return visualSceneLibrary; } /**< See above. */

	/** Retrieves the physics model library.
		The physics model library contains a list of physics models.
		@return The physics model library. */
	inline FCDPhysicsModelLibrary* GetPhysicsModelLibrary() { return physicsModelLibrary; }
	inline const FCDPhysicsModelLibrary* GetPhysicsModelLibrary() const { return physicsModelLibrary; } /**< See above. */

	/** Retrieves the physics material library.
		The physics material library contains a list of physics material.
		@return The physics material library. */
	inline FCDPhysicsMaterialLibrary* GetPhysicsMaterialLibrary() { return physicsMaterialLibrary; }
	inline const FCDPhysicsMaterialLibrary* GetPhysicsMaterialLibrary() const { return physicsMaterialLibrary; } /**< See above. */

	/** Retrieves the physics scene library.
		The physics scene library contains a list of physics scene nodes.
		@return The physics scene library. */
	inline FCDPhysicsSceneLibrary* GetPhysicsSceneLibrary() { return physicsSceneLibrary;}
	inline const FCDPhysicsSceneLibrary* GetPhysicsSceneLibrary() const { return physicsSceneLibrary;} /**< See above. */

	/** Retrieves the emitter library.
		The emitter library contains a list of emitter definitions.
		@return The emitter library. */
	inline FCDEmitterLibrary* GetEmitterLibrary() { return emitterLibrary;}
	inline const FCDEmitterLibrary* GetEmitterLibrary() const { return emitterLibrary;} /**< See above. */

	/** Insert a new visual scene within the visual scene library.
		The new visual scene will be used as the root visual scene.
		@return The newly created visual scene. */
	FCDSceneNode* AddVisualScene();

	/** Insert a new physics scene within the physics material library.
		The new physics scene will be used as the root physics scene.
		@return The newly created physics scene. */
	FCDPhysicsScene* AddPhysicsScene();
	
	/** [INTERNAL] Retrieves all the animation channels which include a given target pointer.
		@param pointer A valid COLLADA target pointer. Example: "node01/translate.X".
		@param channels A list to be filled with the animation channels which target the given pointer.
			This list is not emptied by the function. If no animation channels are found, this list will be empty. */
	void FindAnimationChannels(const fm::string& pointer, FCDAnimationChannelList& channels);

	/** [INTERNAL] Retrieves the array indices of animation channels which target the given XML node.
		@param targetArray A XML node that contains an array of animatable values.
		@param animatedIndices A list to be filled with the array indices of the animation channels
			which target the given XML node. If no animation channel indices are found, this list will be empty. */
	void FindAnimationChannelsArrayIndices(xmlNode* targetArray, Int32List& animatedIndices);

	/** Retrieves the animation tree node that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The animation tree node. This pointer will be NULL if
			no matching animation tree node was found. */
	FCDAnimation* FindAnimation(const fm::string& daeId);

	/** Retrieves the animation clip that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The animation clip. This pointer will be NULL if
			no matching animation clip was found. */
	FCDAnimationClip* FindAnimationClip(const fm::string& daeId);

	/** Retrieves the camera that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The camera. This pointer will be NULL if no matching camera was found. */
	FCDCamera* FindCamera(const fm::string& daeId);

	/** Retrieves the controller that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The controller. This pointer will be NULL if no matching controller was found. */
	FCDController* FindController(const fm::string& daeId);

	/** Retrieves the effect that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The effect. This pointer will be NULL if no matching effect was found. */
	FCDEffect* FindEffect(const fm::string& daeId);

	/** Retrieves the entity that matches the given COLLADA id.
		This function will look through all the libraries for any entity
		with the given COLLADA id.
		@return The entity. This pointer will be NULL if no matching entity was found. */
	FCDEntity* FindEntity(const fm::string& daeId);

	/** Retrieves the emitter that matches the given COLLADA id. 
		@param daeId A valid COLLADA id.
		@return The emitter. This pointer will be NULL if no matching emitter was found. */
	FCDEmitter* FindEmitter(const fm::string& daeId);

	/** Retrieves the force field that matches the given COLLADA id. 
		@param daeId A valid COLLADA id.
		@return The force field. This pointer will be NULL if no matching force field was found. */
	FCDForceField* FindForceField(const fm::string& daeId);

	/** Retrieves the geometry that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The geometry. This pointer will be NULL if no matching geometry was found. */
	FCDGeometry* FindGeometry(const fm::string& daeId);

	/** Retrieves the image that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The image. This pointer will be NULL if no matching image was found. */
	FCDImage* FindImage(const fm::string& daeId);

	/** Retrieves the layer that matches the given name.
		Note that there are no checks for uniqueness in layer names.
		@param name A layer name.
		@return The layer. This pointer will be NULL if no matching layer was found. */
	FCDLayer* FindLayer(const fm::string& name);

	/** Retrieves the light that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The light. This pointer will be NULL if no matching light was found. */
	FCDLight* FindLight(const fm::string& daeId);

	/** Retrieves the visual material that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The visual material. This pointer will be NULL if no matching visual material was found. */
	FCDMaterial* FindMaterial(const fm::string& daeId);

	/** Retrieves the visual scene that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The visual scene. This pointer will be NULL if no matching visual scene was found. */
	FCDSceneNode* FindVisualScene(const fm::string& daeId);

	/** Retrieves the physics scene that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The physics scene. This pointer will be NULL if no matching physics scene was found. */
	FCDPhysicsScene* FindPhysicsScene(const fm::string& daeId);

	/** Retrieves the physics material that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The physics material. This pointer will be NULL if no matching physics material was found. */
	FCDPhysicsMaterial* FindPhysicsMaterial(const fm::string& daeId);

	/** Retrieves the physics model that matches the given COLLADA id.
		@param daeId A valid COLLADA id.
		@return The physics model. This pointer will be NULL if no matching physics model was found. */
	FCDPhysicsModel* FindPhysicsModel(const fm::string& daeId);

	/** Retrieves the visual scene node that matches the given COLLADA id. 
		This method searches through all the visual scenes within the visual scene library and
		their child visual scene nodes to find the correct visual scene node.
		@param daeId A valid COLLADA id.
		@return The visual scene node. This pointer will be NULL if no matching visual scene node was found. */
	FCDSceneNode* FindSceneNode(const fm::string& daeId);

	/** [INTERNAL] Registers an animated value with the document. All animated values are
		listed within the document.
		@param animated The new animated value to list within the document. */
	void RegisterAnimatedValue(FCDAnimated* animated);

	/** [INTERNAL] Unregisters an animated value of the document. All animated values are
		listed within the document. This function must be called before deleting an animated value.
		@param animated The animated value to un-list from the document. */
	void UnregisterAnimatedValue(FCDAnimated* animated);

	/** [INTERNAL] Links the given animated value as a possible driver to other animated value(s). This step is 
		done during the import, after new animated values are imported. The whole list of animated values is checked
		for potential driven values.
		@param animated The animated value to verify.
		@return Whether a driver was found. */
	bool LinkDriver(FCDAnimated* animated);

	/** Retrieves an animated value given an animatable value.
		@param ptr A pointer to an animatable value contained within the document.
		@return The animated value. This pointer will NULL if no matching animated value was found. */
	FCDAnimated* FindAnimatedValue(float* ptr);
	const FCDAnimated* FindAnimatedValue(const float* ptr) const; /**< See above. */

	/** [INTERNAL] Retrieves an animated value given a COLLADA target pointer. Used during the resolving of drivers/driven
		animated values.
		@param fullyQualifiedTarget A valid COLLADA target pointer.
		@return A pointer to the animatable value. */
	const float* FindAnimatedTarget(const fm::string& fullyQualifiedTarget);

	/** Retrieves whether a given animatable value is animated.
		@param ptr The animatable value.
		@return Whether the animatable value is animated. */
	bool IsValueAnimated(const float* ptr) const;

	/** Loads a COLLADA file into this document object. This function appends into the document object all
		the COLLADA assets found within the file identified by the given filename.
		@param filename An OS-dependant filename.
		@return The status of the import. If the status is 'false', it may be dangerous to
			extract information from the document. */
	bool LoadFromFile(const fstring& filename);

	/** Loads a COLLADA string into this document object. This function appends into the document object all
		the COLLADA assets found within the given string. The string should be in XML format.
		@param basePath The base file path for this import. Used when transforming the relative filenames
			found within the COLLADA string into absolute and OS-dependent filenames.
		@param text The COLLADA string.
		@param textLength The COLLADA string length. Set this value to zero for NULL-terminated strings.
		@return The status of the import. If the status is 'false', it may be dangerous to
			extract information from the document. */
	bool LoadFromText(const fstring& basePath, const char* text, size_t textLength = 0);
#ifdef UNICODE
	bool LoadFromText(const fstring& basePath, const fchar* text, size_t textLength = 0); /**< See above. */
#endif

	/** [INTERNAL] Reads the full document information from the given XML node tree. This step is done after
		a successfull import, by LibXML2, of the COLLADA string/file into a full XML node tree.
		@param colladaNode The base XML node tree to parse.
		@return The status of the import. If the status is 'false', it may be dangerous to
			extract information from the document. */
	bool LoadDocumentFromXML(xmlNode* colladaNode);

	/** Writes the document out to a file identified by its OS-dependent filename. This function is done
		in two steps. First, the document is fully translated into a XML node tree. Then, the XML node tree
		is written to a file by LibXML2.
		@param filename The OS-dependent filename.
		@return The status of the import. If the status is 'false', it may be dangerous to
			extract information from the document. */
	bool WriteToFile(const fstring& filename) const;

	/** [INTERNAL] Writes out the document to a XML node tree. This is the first step of the document
		export. This function traverses the full COLLADA document, writing all the entities into
		the given XML node tree.
		@param colladaNode The base XML node tree to write to.
		@return The status of the import. If the status is 'false', it may be dangerous to
			extract information from the document. */
	bool WriteDocumentToXML(xmlNode* colladaNode) const;

	/** [INTERNAL] Writes out the animation curves contained within an animated value to the given XML node tree.
		@param value An animatable value.
		@param valueNode The XML node associated with the animatable value. This XML node is used to generate a
			valid COLLADA target pointer.
		@param wantedSid The sub-id for the animatable value. If the animatable value is animated, this sub-id
			will be added as an attribute to the XML node. If another node, within the sid range already uses this sub-id,
			a unique sub-id will be generated using the wanted sub-id as a base.
		@param arrayElement The array index for this animatable value. Defaults to -1, which implies that the
			animatable value does not belong to an array.
		@return Whether any animated value was found for the given value pointer. */
	bool WriteAnimatedValueToXML(const float* value, xmlNode* valueNode, const char* wantedSid, int32 arrayElement = -1) const;

	/** [INTERNAL] Writes out the animation curves contained within an animated value to the given XML node tree.
		@param animated An animated element.
		@param valueNode The XML node associated with the animatable value. This XML node is used to generate a
			valid COLLADA target pointer.
		@param wantedSid The sub-id for the animatable value. If the animatable value is animated, this sub-id
			will be added as an attribute to the XML node. If another node, within the sid range already uses this sub-id,
			a unique sub-id will be generated using the wanted sub-id as a base. */
	void WriteAnimatedValueToXML(const FCDAnimated* animated, xmlNode* valueNode, const char* wantedSid) const;
};

#endif //_FC_DOCUMENT_H_
