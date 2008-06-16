#--------------------------------------------------------------------------
# This makefile may be used to build FCollada library in Linux
# environment. In order to use it, please copy it to the root
# directory of your downloaded package and type "make". Upon
# successful build, type "make install". We recommend to check
# the Lightsprint documentation, section "Third party libraries".
#--------------------------------------------------------------------------

# uncomment the following line if you wish to build the debug version
#DEBUG = 1

CPPFLAGS = -IFCollada -IFCollada/libxml/include -DLINUX

ifdef DEBUG
	CPPFLAGS += -O0 -g -D_DEBUG
else
	CPPFLAGS += -O2 -s -DNDEBUG -ffast-math -fomit-frame-pointer
endif

SOURCES = \
	FCollada/FCollada.cpp\
	FCollada/FColladaPlugin.cpp\
	FCollada/FCDocument/FCDAnimated.cpp\
	FCollada/FCDocument/FCDAnimationChannel.cpp\
	FCollada/FCDocument/FCDAnimationClip.cpp\
	FCollada/FCDocument/FCDAnimationClipTools.cpp\
	FCollada/FCDocument/FCDAnimation.cpp\
	FCollada/FCDocument/FCDAnimationCurve.cpp\
	FCollada/FCDocument/FCDAnimationCurveTools.cpp\
	FCollada/FCDocument/FCDAnimationKey.cpp\
	FCollada/FCDocument/FCDAnimationMultiCurve.cpp\
	FCollada/FCDocument/FCDAsset.cpp\
	FCollada/FCDocument/FCDCamera.cpp\
	FCollada/FCDocument/FCDController.cpp\
	FCollada/FCDocument/FCDControllerInstance.cpp\
	FCollada/FCDocument/FCDControllerTools.cpp\
	FCollada/FCDocument/FCDEffectCode.cpp\
	FCollada/FCDocument/FCDEffect.cpp\
	FCollada/FCDocument/FCDEffectParameter.cpp\
	FCollada/FCDocument/FCDEffectParameterFactory.cpp\
	FCollada/FCDocument/FCDEffectParameterSampler.cpp\
	FCollada/FCDocument/FCDEffectParameterSurface.cpp\
	FCollada/FCDocument/FCDEffectPass.cpp\
	FCollada/FCDocument/FCDEffectPassShader.cpp\
	FCollada/FCDocument/FCDEffectPassState.cpp\
	FCollada/FCDocument/FCDEffectProfile.cpp\
	FCollada/FCDocument/FCDEffectProfileFX.cpp\
	FCollada/FCDocument/FCDEffectStandard.cpp\
	FCollada/FCDocument/FCDEffectTechnique.cpp\
	FCollada/FCDocument/FCDEffectTools.cpp\
	FCollada/FCDocument/FCDEmitter.cpp\
	FCollada/FCDocument/FCDEmitterInstance.cpp\
	FCollada/FCDocument/FCDEmitterObject.cpp\
	FCollada/FCDocument/FCDEmitterParticle.cpp\
	FCollada/FCDocument/FCDEntity.cpp\
	FCollada/FCDocument/FCDEntityInstance.cpp\
	FCollada/FCDocument/FCDEntityReference.cpp\
	FCollada/FCDocument/FCDExternalReferenceManager.cpp\
	FCollada/FCDocument/FCDExtra.cpp\
	FCollada/FCDocument/FCDForceDeflector.cpp\
	FCollada/FCDocument/FCDForceDrag.cpp\
	FCollada/FCDocument/FCDForceField.cpp\
	FCollada/FCDocument/FCDForceGravity.cpp\
	FCollada/FCDocument/FCDForcePBomb.cpp\
	FCollada/FCDocument/FCDForceWind.cpp\
	FCollada/FCDocument/FCDGeometry.cpp\
	FCollada/FCDocument/FCDGeometryInstance.cpp\
	FCollada/FCDocument/FCDGeometryMesh.cpp\
	FCollada/FCDocument/FCDGeometryNURBSSurface.cpp\
	FCollada/FCDocument/FCDGeometryPolygons.cpp\
	FCollada/FCDocument/FCDGeometryPolygonsInput.cpp\
	FCollada/FCDocument/FCDGeometryPolygonsTools.cpp\
	FCollada/FCDocument/FCDGeometrySource.cpp\
	FCollada/FCDocument/FCDGeometrySpline.cpp\
	FCollada/FCDocument/FCDImage.cpp\
	FCollada/FCDocument/FCDLibrary.cpp\
	FCollada/FCDocument/FCDLight.cpp\
	FCollada/FCDocument/FCDLightTools.cpp\
	FCollada/FCDocument/FCDMaterial.cpp\
	FCollada/FCDocument/FCDMaterialInstance.cpp\
	FCollada/FCDocument/FCDMorphController.cpp\
	FCollada/FCDocument/FCDObject.cpp\
	FCollada/FCDocument/FCDObjectWithId.cpp\
	FCollada/FCDocument/FCDocument.cpp\
	FCollada/FCDocument/FCDocumentTools.cpp\
	FCollada/FCDocument/FCDParameterAnimatable.cpp\
	FCollada/FCDocument/FCDParticleModifier.cpp\
	FCollada/FCDocument/FCDPhysicsAnalyticalGeometry.cpp\
	FCollada/FCDocument/FCDPhysicsForceFieldInstance.cpp\
	FCollada/FCDocument/FCDPhysicsMaterial.cpp\
	FCollada/FCDocument/FCDPhysicsModel.cpp\
	FCollada/FCDocument/FCDPhysicsModelInstance.cpp\
	FCollada/FCDocument/FCDPhysicsRigidBody.cpp\
	FCollada/FCDocument/FCDPhysicsRigidBodyInstance.cpp\
	FCollada/FCDocument/FCDPhysicsRigidBodyParameters.cpp\
	FCollada/FCDocument/FCDPhysicsRigidConstraint.cpp\
	FCollada/FCDocument/FCDPhysicsRigidConstraintInstance.cpp\
	FCollada/FCDocument/FCDPhysicsScene.cpp\
	FCollada/FCDocument/FCDPhysicsShape.cpp\
	FCollada/FCDocument/FCDPlaceHolder.cpp\
	FCollada/FCDocument/FCDSceneNode.cpp\
	FCollada/FCDocument/FCDSceneNodeIterator.cpp\
	FCollada/FCDocument/FCDSceneNodeTools.cpp\
	FCollada/FCDocument/FCDSkinController.cpp\
	FCollada/FCDocument/FCDTargetedEntity.cpp\
	FCollada/FCDocument/FCDTexture.cpp\
	FCollada/FCDocument/FCDTransform.cpp\
	FCollada/FCDocument/FCDVersion.cpp\
	FCollada/FUtils/FUAssert.cpp\
	FCollada/FUtils/FUBase64.cpp\
	FCollada/FUtils/FUBoundingBox.cpp\
	FCollada/FUtils/FUBoundingSphere.cpp\
	FCollada/FUtils/FUBoundingTest.cpp\
	FCollada/FUtils/FUCrc32.cpp\
	FCollada/FUtils/FUCrc32Test.cpp\
	FCollada/FUtils/FUCriticalSection.cpp\
	FCollada/FUtils/FUDaeEnum.cpp\
	FCollada/FUtils/FUDateTime.cpp\
	FCollada/FUtils/FUDebug.cpp\
	FCollada/FUtils/FUError.cpp\
	FCollada/FUtils/FUErrorLog.cpp\
	FCollada/FUtils/FUEventTest.cpp\
	FCollada/FUtils/FUFile.cpp\
	FCollada/FUtils/FUFileManager.cpp\
	FCollada/FUtils/FUFileManagerTest.cpp\
	FCollada/FUtils/FUFunctorTest.cpp\
	FCollada/FUtils/FULogFile.cpp\
	FCollada/FUtils/FUObject.cpp\
	FCollada/FUtils/FUObjectTest.cpp\
	FCollada/FUtils/FUObjectType.cpp\
	FCollada/FUtils/FUParameter.cpp\
	FCollada/FUtils/FUParameterizable.cpp\
	FCollada/FUtils/FUPluginManager.cpp\
	FCollada/FUtils/FUSemaphore.cpp\
	FCollada/FUtils/FUStringBuilder.cpp\
	FCollada/FUtils/FUStringBuilderTest.cpp\
	FCollada/FUtils/FUStringConversion.cpp\
	FCollada/FUtils/FUStringConversionTest.cpp\
	FCollada/FUtils/FUStringTest.cpp\
	FCollada/FUtils/FUSynchronizableObject.cpp\
	FCollada/FUtils/FUTestBed.cpp\
	FCollada/FUtils/FUThread.cpp\
	FCollada/FUtils/FUTracker.cpp\
	FCollada/FUtils/FUUniqueStringMap.cpp\
	FCollada/FUtils/FUUniqueStringMapTest.cpp\
	FCollada/FUtils/FUUri.cpp\
	FCollada/FUtils/FUXmlDocument.cpp\
	FCollada/FUtils/FUXmlParser.cpp\
	FCollada/FUtils/FUXmlWriter.cpp\
	FCollada/FMath/FMAllocator.cpp\
	FCollada/FMath/FMAngleAxis.cpp\
	FCollada/FMath/FMArrayTest.cpp\
	FCollada/FMath/FMColor.cpp\
	FCollada/FMath/FMInterpolation.cpp\
	FCollada/FMath/FMLookAt.cpp\
	FCollada/FMath/FMMatrix33.cpp\
	FCollada/FMath/FMMatrix44.cpp\
	FCollada/FMath/FMQuaternion.cpp\
	FCollada/FMath/FMQuaternionTest.cpp\
	FCollada/FMath/FMRandom.cpp\
	FCollada/FMath/FMSkew.cpp\
	FCollada/FMath/FMTreeTest.cpp\
	FCollada/FMath/FMVector3.cpp\
	FCollada/FMath/FMVolume.cpp \
	FColladaPlugins/FArchiveXML/FArchiveXML.cpp\
	FColladaPlugins/FArchiveXML/FAXAnimationExport.cpp\
	FColladaPlugins/FArchiveXML/FAXAnimationImport.cpp\
	FColladaPlugins/FArchiveXML/FAXCameraExport.cpp\
	FColladaPlugins/FArchiveXML/FAXCameraImport.cpp\
	FColladaPlugins/FArchiveXML/FAXColladaParser.cpp\
	FColladaPlugins/FArchiveXML/FAXColladaWriter.cpp\
	FColladaPlugins/FArchiveXML/FAXControllerExport.cpp\
	FColladaPlugins/FArchiveXML/FAXControllerImport.cpp\
	FColladaPlugins/FArchiveXML/FAXEmitterExport.cpp\
	FColladaPlugins/FArchiveXML/FAXEmitterImport.cpp\
	FColladaPlugins/FArchiveXML/FAXEntityExport.cpp\
	FColladaPlugins/FArchiveXML/FAXEntityImport.cpp\
	FColladaPlugins/FArchiveXML/FAXForceFieldExport.cpp\
	FColladaPlugins/FArchiveXML/FAXForceFieldImport.cpp\
	FColladaPlugins/FArchiveXML/FAXGeometryExport.cpp\
	FColladaPlugins/FArchiveXML/FAXGeometryImport.cpp\
	FColladaPlugins/FArchiveXML/FAXImportLinking.cpp\
	FColladaPlugins/FArchiveXML/FAXInstanceExport.cpp\
	FColladaPlugins/FArchiveXML/FAXInstanceImport.cpp\
	FColladaPlugins/FArchiveXML/FAXLightExport.cpp\
	FColladaPlugins/FArchiveXML/FAXLightImport.cpp\
	FColladaPlugins/FArchiveXML/FAXMaterialExport.cpp\
	FColladaPlugins/FArchiveXML/FAXMaterialImport.cpp\
	FColladaPlugins/FArchiveXML/FAXPhysicsExport.cpp\
	FColladaPlugins/FArchiveXML/FAXPhysicsImport.cpp\
	FColladaPlugins/FArchiveXML/FAXSceneExport.cpp\
	FColladaPlugins/FArchiveXML/FAXSceneImport.cpp

OBJS = $(addsuffix .o, $(basename $(SOURCES)))

all: $(OBJS)
	@ar rc libfcollada.a $(OBJS)
	@ranlib libfcollada.a
	@echo Build OK. You may now use \"make install\".

%.o: %.cpp
	gcc -o $@ $(CPPFLAGS) -c $<

clean:
	find . -name "*.o" -delete

install:
	cp libfcollada.a /usr/lib/libfcollada.a
