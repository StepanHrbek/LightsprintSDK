/*
    Copyright (c) 2008-2009 NetAllied Systems GmbH

    This file is part of COLLADASaxFrameworkLoader.

    Licensed under the MIT Open Source License, 
    for details please see LICENSE file or the website
    http://www.opensource.org/licenses/mit-license.php
*/

#include "COLLADASaxFWLStableHeaders.h"
#include "COLLADASaxFWLIFilePartLoader.h"
#include "COLLADASaxFWLLoader.h"
#include "COLLADASaxFWLFileLoader.h"
#include "COLLADASaxFWLIErrorHandler.h"
#include "COLLADASaxFWLIParserImpl.h"
#include "COLLADASaxFWLIParserImpl14.h"
#include "COLLADASaxFWLIParserImpl15.h"
#include "COLLADASaxFWLIExtraDataCallbackHandler.h"


namespace COLLADASaxFWL
{

    //------------------------------
	IFilePartLoader::IFilePartLoader ()
		: mPartLoader(0)
        , mParserImpl(0)
	{
	}
	
    //------------------------------
	IFilePartLoader::~IFilePartLoader()
	{
		deleteFilePartLoader();
        if ( mParserImpl )
        {
            delete mParserImpl;
            mParserImpl = 0;
        }
	}

	//-----------------------------
	COLLADAFW::IWriter* IFilePartLoader::writer()
	{
		return getColladaLoader()->writer();
	}

	//-----------------------------
	const COLLADAFW::UniqueId& IFilePartLoader::createUniqueId( const String& uriString, COLLADAFW::ClassId classId )
	{
		assert( getColladaLoader() );

		COLLADABU::URI uri(getFileUri(), uriString);

		return getColladaLoader()->getUniqueId(uri, classId);
	}

	//-----------------------------
	const COLLADAFW::UniqueId& IFilePartLoader::getUniqueIdByUrl( const String& uriString)
	{
		assert( getColladaLoader() );

		COLLADABU::URI uri(getFileUri(), uriString);

		return getColladaLoader()->getUniqueId(uri);
	}

	//-----------------------------
	COLLADAFW::UniqueId IFilePartLoader::createUniqueIdFromId( const ParserChar* colladaId, COLLADAFW::ClassId classId )
	{
		assert( getColladaLoader() );

		if ( !colladaId || !(*colladaId) )
			return createUniqueId(classId);

		COLLADABU::URI uri(getFileUri(), String("#") + String((const char *)colladaId));

		COLLADAFW::UniqueId createdUniqueId = createUniqueIdFromUrl(uri, classId, false);
		return createdUniqueId;
	}

	//------------------------------
	const COLLADAFW::UniqueId& IFilePartLoader::getUniqueIdById( const ParserChar* colladaId )
	{
		assert( getColladaLoader() );

		if ( !colladaId || !(*colladaId) )
			return COLLADAFW::UniqueId::INVALID;

		COLLADABU::URI uri(getFileUri(), String("#") + String((const char *)colladaId));

		return getColladaLoader()->getUniqueId(uri);
	}


	//-----------------------------
	const COLLADAFW::UniqueId& IFilePartLoader::createUniqueIdFromUrl( const ParserChar* url, COLLADAFW::ClassId classId )
	{
		assert( getColladaLoader() );
		if ( !url || !(*url) )
			return COLLADAFW::UniqueId::INVALID;

		COLLADABU::URI uri(getFileUri(), String((const char *)url));

		return getColladaLoader()->getUniqueId(uri, classId);
	}

	//-----------------------------
	const COLLADAFW::UniqueId& IFilePartLoader::createUniqueIdFromUrl( const COLLADABU::URI& url, COLLADAFW::ClassId classId, bool isAbsolute  )
	{
		assert( getColladaLoader() );

		if ( isAbsolute )
		{
			return getColladaLoader()->getUniqueId(url, classId);
		}
		else
		{
			COLLADABU::URI absoluteUri(getFileUri(), url.getURIString());

			return getColladaLoader()->getUniqueId(absoluteUri, classId);
		}
	}

	//-----------------------------
	const COLLADAFW::UniqueId& IFilePartLoader::getUniqueIdByUrl( const COLLADABU::URI& url, bool isAbsolute )
	{
		assert( getColladaLoader() );
		
		if ( isAbsolute )
		{
			return getColladaLoader()->getUniqueId(url);
		}
		else
		{
			COLLADABU::URI absoluteUri(getFileUri(), url.getURIString());

			return getColladaLoader()->getUniqueId(absoluteUri);
		}
	}


	//-----------------------------
	COLLADAFW::UniqueId IFilePartLoader::createUniqueId( COLLADAFW::ClassId classId )
	{
		assert( getColladaLoader() );
		return getColladaLoader()->getUniqueId(classId);
	}

	//-----------------------------
	void IFilePartLoader::deleteFilePartLoader()
	{
		if ( mPartLoader )
		{
			delete mPartLoader;
			mPartLoader = 0;
		}
	}

	//------------------------------
	void IFilePartLoader::setMeAsParser()
	{
        assert(mParserImpl);
        switch ( mParserImpl->getCOLLADAVersion() )
        {
        case COLLADA_14:
            {
                IParserImpl14* impl14 = (IParserImpl14*)mParserImpl;
                setParser( impl14->getGeneratedParser() );
                break;
            }
        case COLLADA_15:
            {
                IParserImpl15* impl15 = (IParserImpl15*)mParserImpl;
                setParser( impl15->getGeneratedParser() );
                break;
            }
        }
		
	}

	//------------------------------
	GeometryMaterialIdInfo& IFilePartLoader::getMeshMaterialIdInfo( )
	{
		assert(getColladaLoader());
		return getColladaLoader()->getMeshMaterialIdInfo();
	}

	//------------------------------
	COLLADAFW::TextureMapId IFilePartLoader::getTextureMapIdBySematic( const String& semantic )
	{
		assert( getColladaLoader() );
		return getColladaLoader()->getTextureMapIdBySematic(semantic);
	}

	//------------------------------
	SidTreeNode* IFilePartLoader::addToSidTree( const char* colladaId, const char* colladaSid )
	{
		return getFileLoader()->addToSidTree( colladaId, colladaSid );
	}

	//------------------------------
	SidTreeNode*  IFilePartLoader::addToSidTree( const char* colladaId, const char* colladaSid, COLLADAFW::Object* target )
	{
		return getFileLoader()->addToSidTree( colladaId, colladaSid, target );
	}

	//------------------------------
	SidTreeNode*  IFilePartLoader::addToSidTree( const char* colladaId, const char* colladaSid, COLLADAFW::Animatable* target )
	{
		return getFileLoader()->addToSidTree( colladaId, colladaSid, target );
	}

	//------------------------------
	SidTreeNode*  IFilePartLoader::addToSidTree( const char* colladaId, const char* colladaSid, IntermediateTargetable* target )
	{
		return getFileLoader()->addToSidTree( colladaId, colladaSid, target );
	}

	//------------------------------
	void IFilePartLoader::moveUpInSidTree()
	{
		getFileLoader()->moveUpInSidTree();
	}

	//------------------------------
	const SidTreeNode* IFilePartLoader::resolveSid( const SidAddress& sidAddress )
	{
		return getFileLoader()->resolveSid(sidAddress);
	}

	//------------------------------
	void IFilePartLoader::addToAnimationSidAddressBindings( const AnimationInfo& animationInfo, const SidAddress& targetSidAddress )
	{
		getFileLoader()->addToAnimationSidAddressBindings( animationInfo, targetSidAddress );
	}

	//------------------------------
	COLLADAFW::AnimationList*& IFilePartLoader::getAnimationListByUniqueId( const COLLADAFW::UniqueId& animationListUniqueId )
	{
		return getFileLoader()->getAnimationListByUniqueId( animationListUniqueId );
	}

	//-----------------------------
	void IFilePartLoader::addSkinDataJointSidsPair( const COLLADAFW::UniqueId& skinDataUniqueId, const StringList& sidsOrIds, bool areIds )
	{
		getFileLoader()->addSkinDataJointSidsPair( skinDataUniqueId, sidsOrIds, areIds );
	}

	//-----------------------------
	void IFilePartLoader::addSkinDataSkinSourcePair( const COLLADAFW::UniqueId& skinDataUniqueId, const COLLADABU::URI& skinSource )
	{
		getFileLoader()->addSkinDataSkinSourcePair( skinDataUniqueId, skinSource );
	}

	//-----------------------------
	void IFilePartLoader::addMorphController( COLLADAFW::MorphController* morphController )
	{
		getColladaLoader()->getMorphControllerList().push_back(morphController);
	}

	//-----------------------------
	const Loader::JointSidsOrIds& IFilePartLoader::getJointSidsOrIdsBySkinDataUniqueId( const COLLADAFW::UniqueId& skinDataUniqueId ) const
	{
		return getFileLoader()->getJointSidsOrIdsBySkinDataUniqueId( skinDataUniqueId );
	}

	//------------------------------
	const Loader::InstanceControllerDataListMap& IFilePartLoader::getInstanceControllerDataListMap() const
	{
		return getFileLoader()->getInstanceControllerDataListMap();
	}

	//------------------------------
	const Loader::InstanceControllerDataList& IFilePartLoader::getInstanceControllerDataListByControllerUniqueId( const COLLADAFW::UniqueId& controllerUniqueId ) const
	{
		return getFileLoader()->getInstanceControllerDataListByControllerUniqueId(controllerUniqueId);
	}

	//------------------------------
	Loader::InstanceControllerDataList& IFilePartLoader::getInstanceControllerDataListByControllerUniqueId( const COLLADAFW::UniqueId& controllerUniqueId )
	{
		return getFileLoader()->getInstanceControllerDataListByControllerUniqueId(controllerUniqueId);
	}

	//------------------------------
	bool IFilePartLoader::begin__technique( const technique__AttributeData& attributeData )
	{
        //SaxVirtualFunctionTest(begin__technique(attributeData))

        return getFileLoader ()->base__begin__technique ( attributeData, getUniqueId () );
    }

	//------------------------------
	bool IFilePartLoader::end__technique()
	{
        return true;
	}

    //------------------------------
	bool IFilePartLoader::handleFWLError( const SaxFWLError& saxFWLError )
	{
		IErrorHandler* errorHandler = getColladaLoader()->getErrorHandler();
		bool stopParsing = false; 
		if ( errorHandler )
		{
			stopParsing = errorHandler->handleError( &saxFWLError );
		}
		return (saxFWLError.getSeverity() == IError::SEVERITY_CRITICAL) ? true : stopParsing;
	}

	//------------------------------
	bool IFilePartLoader::handleFWLError( SaxFWLError::ErrorType errorType, String errorMessage, IError::Severity severity /*= IError::SEVERITY_ERROR_NONCRITICAL */ )
	{
		SaxFWLError saxFWLError(errorType, errorMessage, severity);
		if ( getFileLoader() && (getFileLoader()->getParsingStatus() == FileLoader::PARSING_PARSING) )
		{
			const GeneratedSaxParser::SaxParser* saxParser = getFileLoader()->getSaxParser();
			if ( saxParser )
			{
				saxFWLError.setLineNumber( saxParser->getLineNumer() );
				saxFWLError.setColumnNumber( saxParser->getColumnNumer() );
			}
		}
		return handleFWLError( saxFWLError );
	}

} // namespace COLLADASaxFWL
