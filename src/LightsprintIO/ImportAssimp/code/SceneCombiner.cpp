/*
Open Asset Import Library (ASSIMP)
----------------------------------------------------------------------

Copyright (c) 2006-2008, ASSIMP Development Team
All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the 
following conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the ASSIMP team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the ASSIMP Development Team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/


// ----------------------------------------------------------------------------
/** @file Implements Assimp::SceneCombiner. This is a smart utility
 *    class that combines multiple scenes, meshes, ... into one. Currently 
 *    these utilities are used by the IRR and LWS loaders and the
 *    OptimizeGraph step.
 */
// ----------------------------------------------------------------------------
#include "AssimpPCH.h"
#include "SceneCombiner.h"
#include "fast_atof.h"
#include "Hash.h"
#include "time.h"

namespace Assimp	{

// ------------------------------------------------------------------------------------------------
// Add a prefix to a string
inline void PrefixString(aiString& string,const char* prefix, unsigned int len)
{
	// If the string is already prefixed, we won't prefix it a second time
	if (string.length >= 1 && string.data[0] == '$')
		return;

	// Add the prefix
	::memmove(string.data+len,string.data,string.length+1);
	::memcpy (string.data, prefix, len);

	// And update the string's length
	string.length += len;
}

// ------------------------------------------------------------------------------------------------
// Add node identifiers to a hashing set
void SceneCombiner::AddNodeHashes(aiNode* node, std::set<unsigned int>& hashes)
{
	// Add node name to hashing set if it is non-empty - empty nodes are allowed 
	// and they can't have any anims assigned so its absolutely safe to duplicate them.
	if (node->mName.length) {
		hashes.insert( SuperFastHash(node->mName.data,node->mName.length) );
	}

	// Process all children recursively
	for (unsigned int i = 0; i < node->mNumChildren;++i)
		AddNodeHashes(node->mChildren[i],hashes);
}

// ------------------------------------------------------------------------------------------------
// Add a name prefix to all nodes in a hierarchy
void SceneCombiner::AddNodePrefixes(aiNode* node, const char* prefix, unsigned int len)
{
	ai_assert(NULL != prefix);
	PrefixString(node->mName,prefix,len);

	// Process all children recursively
	for (unsigned int i = 0; i < node->mNumChildren;++i)
		AddNodePrefixes(node->mChildren[i],prefix,len);
}

// ------------------------------------------------------------------------------------------------
// Search for matching names
bool SceneCombiner::FindNameMatch(const aiString& name, std::vector<SceneHelper>& input, unsigned int cur)
{
	const unsigned int hash = SuperFastHash(name.data, name.length);

	// Check whether we find a positive match in one of the given sets
	for (unsigned int i = 0; i < input.size(); ++i) {

		if (cur != i && input[i].hashes.find(hash) != input[i].hashes.end()) {
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------------------------------------
// Add a name prefix to all nodes in a hierarchy if a hash match is found
void SceneCombiner::AddNodePrefixesChecked(aiNode* node, const char* prefix, unsigned int len,
	std::vector<SceneHelper>& input, unsigned int cur)
{
	ai_assert(NULL != prefix);
	const unsigned int hash = SuperFastHash(node->mName.data,node->mName.length);

	// Check whether we find a positive match in one of the given sets
	for (unsigned int i = 0; i < input.size(); ++i) {

		if (cur != i && input[i].hashes.find(hash) != input[i].hashes.end()) {
			PrefixString(node->mName,prefix,len);
			break;
		}
	}

	// Process all children recursively
	for (unsigned int i = 0; i < node->mNumChildren;++i)
		AddNodePrefixesChecked(node->mChildren[i],prefix,len,input,cur);
}

// ------------------------------------------------------------------------------------------------
// Add an offset to all mesh indices in a node graph
void SceneCombiner::OffsetNodeMeshIndices (aiNode* node, unsigned int offset)
{
	for (unsigned int i = 0; i < node->mNumMeshes;++i)
		node->mMeshes[i] += offset;

	for (unsigned int i = 0; i < node->mNumChildren;++i)
		OffsetNodeMeshIndices(node->mChildren[i],offset);
}

// ------------------------------------------------------------------------------------------------
// Merges two scenes. Currently only used by the LWS loader.
void SceneCombiner::MergeScenes(aiScene** _dest,std::vector<aiScene*>& src,
	unsigned int flags)
{
	ai_assert(NULL != _dest);

	// if _dest points to NULL allocate a new scene. Otherwise clear the old and reuse it
	if (src.empty())
	{
		if (*_dest)
		{
			(*_dest)->~aiScene();
			SceneCombiner::CopySceneFlat(_dest,src[0]);
		}
		else *_dest = src[0];
		return;
	}
	if (*_dest)(*_dest)->~aiScene();
	else *_dest = new aiScene();

	// Create a dummy scene to serve as master for the others
	aiScene* master = new aiScene();
	master->mRootNode = new aiNode();
	master->mRootNode->mName.Set("<MergeRoot>");

	std::vector<AttachmentInfo> srcList (src.size());
	for (unsigned int i = 0; i < srcList.size();++i)	{
		srcList[i] = AttachmentInfo(src[i],master->mRootNode);
	}

	// 'master' will be deleted afterwards
	MergeScenes (_dest, master, srcList, flags);
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::AttachToGraph (aiNode* attach, std::vector<NodeAttachmentInfo>& srcList)
{
	unsigned int cnt;
	for (cnt = 0; cnt < attach->mNumChildren;++cnt)
		AttachToGraph(attach->mChildren[cnt],srcList);

	cnt = 0;
	for (std::vector<NodeAttachmentInfo>::iterator it = srcList.begin();
		 it != srcList.end(); ++it)
	{
		if ((*it).attachToNode == attach && !(*it).resolved)
			++cnt;
	}

	if (cnt)	{
		aiNode** n = new aiNode*[cnt+attach->mNumChildren];
		if (attach->mNumChildren)	{
			::memcpy(n,attach->mChildren,sizeof(void*)*attach->mNumChildren);
			delete[] attach->mChildren;
		}
		attach->mChildren = n;

		n += attach->mNumChildren;
		attach->mNumChildren += cnt;

		for (unsigned int i = 0; i < srcList.size();++i)	{
			NodeAttachmentInfo& att = srcList[i];
			if (att.attachToNode == attach && !att.resolved)	{
				*n = att.node;
				(**n).mParent = attach;
				++n;

				// mark this attachment as resolved
				att.resolved = true;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::AttachToGraph ( aiScene* master, 
	std::vector<NodeAttachmentInfo>& src)
{
	ai_assert(NULL != master);
	AttachToGraph(master->mRootNode,src);
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::MergeScenes(aiScene** _dest, aiScene* master, 
	std::vector<AttachmentInfo>& srcList,
	unsigned int flags)
{
	ai_assert(NULL != _dest);

	// if _dest points to NULL allocate a new scene. Otherwise clear the old and reuse it
	if (srcList.empty())
	{
		if (*_dest)
		{
			(*_dest)->~aiScene();
			SceneCombiner::CopySceneFlat(_dest,master);
		}
		else *_dest = master;
		return;
	}
	if (*_dest)(*_dest)->~aiScene();
	else *_dest = new aiScene();

	aiScene* dest = *_dest;

	std::vector<SceneHelper> src (srcList.size()+1);
	src[0].scene = master;
	for (unsigned int i = 0; i < srcList.size();++i)	{
		src[i+1] = SceneHelper( srcList[i].scene );
	}

	// this helper array specifies which scenes are duplicates of others
	std::vector<unsigned int> duplicates(src.size(),0xffffffff);

	// this helper array is used as lookup table several times
	std::vector<unsigned int> offset(src.size());

	// Find duplicate scenes
	for (unsigned int i = 0; i < src.size();++i)
	{
		if (duplicates[i] != i && duplicates[i] != 0xffffffff)continue;
		duplicates[i] = i;
		for ( unsigned int a = i+1; a < src.size(); ++a)
		{
			if (src[i].scene == src[a].scene)
				duplicates[a] = i;
		}
	}

	// Generate unique names for all named stuff?
	if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES)
	{
#if 0
		// Construct a proper random number generator
		boost::mt19937 rng(  );
		boost::uniform_int<> dist(1u,1 << 24u);
		boost::variate_generator<boost::mt19937&, boost::uniform_int<> > rndGen(rng, dist);   
#endif
		for (unsigned int i = 1; i < src.size();++i)
		{
			//if (i != duplicates[i]) 
			//{
			//	// duplicate scenes share the same UID
			//	::strcpy( src[i].id, src[duplicates[i]].id );
			//	src[i].idlen = src[duplicates[i]].idlen;

			//	continue;
			//}

			src[i].idlen = ::sprintf(src[i].id,"$%.6X$_",i);

			if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
				
				// Compute hashes for all identifiers in this scene and store them
				// in a sorted table (for convenience I'm using std::set). We hash
				// just the node and animation channel names, all identifiers except
				// the material names should be caught by doing this.
				AddNodeHashes(src[i]->mRootNode,src[i].hashes);

				for (unsigned int a = 0; a < src[i]->mNumAnimations;++a) {
					aiAnimation* anim = src[i]->mAnimations[a];
					src[i].hashes.insert(SuperFastHash(anim->mName.data,anim->mName.length));
				}
			}
		}
	}
	
	unsigned int cnt;

	// First find out how large the respective output arrays must be
	for ( unsigned int n = 0; n < src.size();++n )
	{
		SceneHelper* cur = &src[n];

		if (n == duplicates[n] || flags & AI_INT_MERGE_SCENE_DUPLICATES_DEEP_CPY)	{
			dest->mNumTextures   += (*cur)->mNumTextures;
			dest->mNumMaterials  += (*cur)->mNumMaterials;
			dest->mNumMeshes     += (*cur)->mNumMeshes;
		}

		dest->mNumLights     += (*cur)->mNumLights;
		dest->mNumCameras    += (*cur)->mNumCameras;
		dest->mNumAnimations += (*cur)->mNumAnimations;

		// Combine the flags of all scenes
		// We need to process them flag-by-flag here to get correct results
		// dest->mFlags ; //|= (*cur)->mFlags;
		if ((*cur)->mFlags & AI_SCENE_FLAGS_NON_VERBOSE_FORMAT) {
			dest->mFlags |= AI_SCENE_FLAGS_NON_VERBOSE_FORMAT;
		}
	}

	// generate the output texture list + an offset table for all texture indices
	if (dest->mNumTextures)
	{
		aiTexture** pip = dest->mTextures = new aiTexture*[dest->mNumMaterials];
		cnt = 0;
		for ( unsigned int n = 0; n < src.size();++n )
		{
			SceneHelper* cur = &src[n];
			for (unsigned int i = 0; i < (*cur)->mNumTextures;++i)
			{
				if (n != duplicates[n])
				{
					if ( flags & AI_INT_MERGE_SCENE_DUPLICATES_DEEP_CPY)
						Copy(pip,(*cur)->mTextures[i]);

					else continue;
				}
				else *pip = (*cur)->mTextures[i];
				++pip;
			}

			offset[n] = cnt;
			cnt = (unsigned int)(pip - dest->mTextures);
		}
	}

	// generate the output material list + an offset table for all material indices
	if (dest->mNumMaterials)
	{ 
		aiMaterial** pip = dest->mMaterials = new aiMaterial*[dest->mNumMaterials];
		cnt = 0;
		for ( unsigned int n = 0; n < src.size();++n )	{
			SceneHelper* cur = &src[n];
			for (unsigned int i = 0; i < (*cur)->mNumMaterials;++i)
			{
				if (n != duplicates[n])
				{
					if ( flags & AI_INT_MERGE_SCENE_DUPLICATES_DEEP_CPY)
						Copy(pip,(*cur)->mMaterials[i]);

					else continue;
				}
				else *pip = (*cur)->mMaterials[i];

				if ((*cur)->mNumTextures != dest->mNumTextures)		{
					// We need to update all texture indices of the mesh. So we need to search for
					// a material property called '$tex.file'

					for (unsigned int a = 0; a < (*pip)->mNumProperties;++a)
					{
						aiMaterialProperty* prop = (*pip)->mProperties[a];
						if (!strncmp(prop->mKey.data,"$tex.file",9))
						{
							// Check whether this texture is an embedded texture.
							// In this case the property looks like this: *<n>,
							// where n is the index of the texture.
							aiString& s = *((aiString*)prop->mData);
							if ('*' == s.data[0])	{
								// Offset the index and write it back ..
								const unsigned int idx = strtol10(&s.data[1]) + offset[n];
								ASSIMP_itoa10(&s.data[1],sizeof(s.data)-1,idx);
							}
						}

						// Need to generate new, unique material names?
						else if (!::strcmp( prop->mKey.data,"$mat.name" ) && flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_MATNAMES)
						{
							aiString* pcSrc = (aiString*) prop->mData; 
							PrefixString(*pcSrc, (*cur).id, (*cur).idlen);
						}
					}
				}
				++pip;
			}

			offset[n] = cnt;
			cnt = (unsigned int)(pip - dest->mMaterials);
		}
	}

	// generate the output mesh list + again an offset table for all mesh indices
	if (dest->mNumMeshes)
	{
		aiMesh** pip = dest->mMeshes = new aiMesh*[dest->mNumMeshes];
		cnt = 0;
		for ( unsigned int n = 0; n < src.size();++n )
		{
			SceneHelper* cur = &src[n];
			for (unsigned int i = 0; i < (*cur)->mNumMeshes;++i)
			{
				if (n != duplicates[n])	{
					if ( flags & AI_INT_MERGE_SCENE_DUPLICATES_DEEP_CPY)
						Copy(pip, (*cur)->mMeshes[i]);

					else continue;
				}
				else *pip = (*cur)->mMeshes[i];

				// update the material index of the mesh
				(*pip)->mMaterialIndex +=  offset[n];
				++pip;
			}

			// reuse the offset array - store now the mesh offset in it
			offset[n] = cnt;
			cnt = (unsigned int)(pip - dest->mMeshes);
		}
	}

	std::vector <NodeAttachmentInfo> nodes;
	nodes.reserve(srcList.size());

	// ----------------------------------------------------------------------------
	// Now generate the output node graph. We need to make those
	// names in the graph that are referenced by anims or lights
	// or cameras unique. So we add a prefix to them ... $<rand>_
	// We could also use a counter, but using a random value allows us to
	// use just one prefix if we are joining multiple scene hierarchies recursively.
	// Chances are quite good we don't collide, so we try that ...
	// ----------------------------------------------------------------------------

	// Allocate space for light sources, cameras and animations
	aiLight** ppLights = dest->mLights = (dest->mNumLights 
		? new aiLight*[dest->mNumLights] : NULL);

	aiCamera** ppCameras = dest->mCameras = (dest->mNumCameras 
		? new aiCamera*[dest->mNumCameras] : NULL);

	aiAnimation** ppAnims = dest->mAnimations = (dest->mNumAnimations 
		? new aiAnimation*[dest->mNumAnimations] : NULL);

	for ( int n = src.size()-1; n >= 0 ;--n ) /* !!! important !!! */
	{
		SceneHelper* cur = &src[n];
		aiNode* node;

		// To offset or not to offset, this is the question
		if (n != (int)duplicates[n])
		{
			// Get full scenegraph copy
			Copy( &node, (*cur)->mRootNode );
			OffsetNodeMeshIndices(node,offset[duplicates[n]]);

			if (flags & AI_INT_MERGE_SCENE_DUPLICATES_DEEP_CPY)	{
				// (note:) they are already 'offseted' by offset[duplicates[n]] 
				OffsetNodeMeshIndices(node,offset[n] - offset[duplicates[n]]);
			}
		}
		else // if (n == duplicates[n])
		{
			node = (*cur)->mRootNode;
			OffsetNodeMeshIndices(node,offset[n]);
		}
		if (n) // src[0] is the master node
			nodes.push_back(NodeAttachmentInfo( node,srcList[n-1].attachToNode,n ));

		// add name prefixes?
		if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES) {

			// or the whole scenegraph
			if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
				AddNodePrefixesChecked(node,(*cur).id,(*cur).idlen,src,n);
			}
			else AddNodePrefixes(node,(*cur).id,(*cur).idlen);

			// meshes
			for (unsigned int i = 0; i < (*cur)->mNumMeshes;++i)	{
				aiMesh* mesh = (*cur)->mMeshes[i]; 

				// rename all bones
				for (unsigned int a = 0; a < mesh->mNumBones;++a)	{
					if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
						if (!FindNameMatch(mesh->mBones[a]->mName,src,n))
							continue;
					}
					PrefixString(mesh->mBones[a]->mName,(*cur).id,(*cur).idlen);
				}
			}
		}

		// --------------------------------------------------------------------
		// Copy light sources
		for (unsigned int i = 0; i < (*cur)->mNumLights;++i,++ppLights)
		{
			if (n != (int)duplicates[n]) // duplicate scene? 
			{
				Copy(ppLights, (*cur)->mLights[i]);
			}
			else *ppLights = (*cur)->mLights[i];


			// Add name prefixes?
			if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES) {
				if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
					if (!FindNameMatch((*ppLights)->mName,src,n))
						continue;
				}

				PrefixString((*ppLights)->mName,(*cur).id,(*cur).idlen);
			}
		}

		// --------------------------------------------------------------------
		// Copy cameras
		for (unsigned int i = 0; i < (*cur)->mNumCameras;++i,++ppCameras)	{
			if (n != (int)duplicates[n]) // duplicate scene? 
			{
				Copy(ppCameras, (*cur)->mCameras[i]);
			}
			else *ppCameras = (*cur)->mCameras[i];

			// Add name prefixes?
			if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES) {
				if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
					if (!FindNameMatch((*ppCameras)->mName,src,n))
						continue;
				}

				PrefixString((*ppCameras)->mName,(*cur).id,(*cur).idlen);
			}
		}

		// --------------------------------------------------------------------
		// Copy animations
		for (unsigned int i = 0; i < (*cur)->mNumAnimations;++i,++ppAnims)	{
			if (n != (int)duplicates[n]) // duplicate scene? 
			{
				Copy(ppAnims, (*cur)->mAnimations[i]);
			}
			else *ppAnims = (*cur)->mAnimations[i];

			// Add name prefixes?
			if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES) {
				if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
					if (!FindNameMatch((*ppAnims)->mName,src,n))
						continue;
				}

				PrefixString((*ppAnims)->mName,(*cur).id,(*cur).idlen);

				// don't forget to update all node animation channels
				for (unsigned int a = 0; a < (*ppAnims)->mNumChannels;++a) {
					if (flags & AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY) {
						if (!FindNameMatch((*ppAnims)->mChannels[a]->mNodeName,src,n))
							continue;
					}

					PrefixString((*ppAnims)->mChannels[a]->mNodeName,(*cur).id,(*cur).idlen);
				}
			}
		}
	}

	// Now build the output graph
	AttachToGraph ( master, nodes);
	dest->mRootNode = master->mRootNode;

	// Check whether we succeeded at building the output graph
	for (std::vector <NodeAttachmentInfo> ::iterator it = nodes.begin(); 
		it != nodes.end(); ++it)
	{
		if (!(*it).resolved) {
			if (flags & AI_INT_MERGE_SCENE_RESOLVE_CROSS_ATTACHMENTS) {
				// search for this attachment point in all other imported scenes, too.
				for ( unsigned int n = 0; n < src.size();++n ) {
					if (n != (*it).src_idx) {
						AttachToGraph(src[n].scene,nodes);
						if ((*it).resolved)
							break;
					}
				}
			}
			if (!(*it).resolved) {
				DefaultLogger::get()->error(std::string("SceneCombiner: Failed to resolve attachment ") 
					+ (*it).node->mName.data + " " + (*it).attachToNode->mName.data);
			}
		}
	}

	// now delete all input scenes. Make sure duplicate scenes aren't
	// deleted more than one time
	for ( unsigned int n = 0; n < src.size();++n )	{
		if (n != duplicates[n]) // duplicate scene?
			continue;

		aiScene* deleteMe = src[n].scene;

		// We need to delete the arrays before the destructor is called -
		// we are reusing the array members
		delete[] deleteMe->mMeshes;     deleteMe->mMeshes     = NULL;
		delete[] deleteMe->mCameras;    deleteMe->mCameras    = NULL;
		delete[] deleteMe->mLights;     deleteMe->mLights     = NULL;
		delete[] deleteMe->mMaterials;  deleteMe->mMaterials  = NULL;
		delete[] deleteMe->mAnimations; deleteMe->mAnimations = NULL;

		deleteMe->mRootNode = NULL;

		// Now we can safely delete the scene
		delete deleteMe;
	}

	// Check flags
	if (!dest->mNumMeshes || !dest->mNumMaterials) {
		dest->mFlags |= AI_SCENE_FLAGS_INCOMPLETE;
	}

	// We're finished
}

// ------------------------------------------------------------------------------------------------
// Build a list of unique bones
void SceneCombiner::BuildUniqueBoneList(std::list<BoneWithHash>& asBones,
	std::vector<aiMesh*>::const_iterator it,
	std::vector<aiMesh*>::const_iterator end)
{
	unsigned int iOffset = 0;
	for (; it != end;++it)	{
		for (unsigned int l = 0; l < (*it)->mNumBones;++l)	{
			aiBone* p = (*it)->mBones[l];
			uint32_t itml = SuperFastHash(p->mName.data,(unsigned int)p->mName.length);

			std::list<BoneWithHash>::iterator it2  = asBones.begin();
			std::list<BoneWithHash>::iterator end2 = asBones.end();

			for (;it2 != end2;++it2)	{
				if ((*it2).first == itml)	{
					(*it2).pSrcBones.push_back(BoneSrcIndex(p,iOffset));
					break;
				}
			}
			if (end2 == it2)	{
				// need to begin a new bone entry
				asBones.push_back(BoneWithHash());
				BoneWithHash& btz = asBones.back();

				// setup members
				btz.first = itml;
				btz.second = &p->mName;
				btz.pSrcBones.push_back(BoneSrcIndex(p,iOffset));
			}
		}
		iOffset += (*it)->mNumVertices;
	}
}

// ------------------------------------------------------------------------------------------------
// Merge a list of bones
void SceneCombiner::MergeBones(aiMesh* out,std::vector<aiMesh*>::const_iterator it,
	std::vector<aiMesh*>::const_iterator end)
{
	ai_assert(NULL != out && !out->mNumBones);

	// find we need to build an unique list of all bones.
	// we work with hashes to make the comparisons MUCH faster,
	// at least if we have many bones.
	std::list<BoneWithHash> asBones;
	BuildUniqueBoneList(asBones, it,end);
	
	// now create the output bones
	out->mNumBones = 0;
	out->mBones = new aiBone*[asBones.size()];

	for (std::list<BoneWithHash>::const_iterator it = asBones.begin(),end = asBones.end(); it != end;++it)	{
		// Allocate a bone and setup it's name
		aiBone* pc = out->mBones[out->mNumBones++] = new aiBone();
		pc->mName = aiString( *((*it).second ));

		std::vector< BoneSrcIndex >::const_iterator wend = (*it).pSrcBones.end();

		// Loop through all bones to be joined for this bone
		for (std::vector< BoneSrcIndex >::const_iterator wmit = (*it).pSrcBones.begin(); wmit != wend; ++wmit)	{
			pc->mNumWeights += (*wmit).first->mNumWeights;

			// NOTE: different offset matrices for bones with equal names
			// are - at the moment - not handled correctly. 
			if (wmit != (*it).pSrcBones.begin() && pc->mOffsetMatrix != (*wmit).first->mOffsetMatrix)	{
				DefaultLogger::get()->warn("Bones with equal names but different offset matrices can't be joined at the moment");
				continue;
			}
			pc->mOffsetMatrix = (*wmit).first->mOffsetMatrix;
		}

		// Allocate the vertex weight array
		aiVertexWeight* avw = pc->mWeights = new aiVertexWeight[pc->mNumWeights];

		// And copy the final weights - adjust the vertex IDs by the 
		// face index offset of the coresponding mesh.
		for (std::vector< BoneSrcIndex >::const_iterator wmit = (*it).pSrcBones.begin(); wmit != wend; ++wmit)	{
			aiBone* pip = (*wmit).first;
			for (unsigned int mp = 0; mp < pip->mNumWeights;++mp,++avw)	{
				const aiVertexWeight& vfi = pip->mWeights[mp];
				avw->mWeight = vfi.mWeight;
				avw->mVertexId = vfi.mVertexId + (*wmit).second;
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------
// Merge a list of meshes
void SceneCombiner::MergeMeshes(aiMesh** _out,unsigned int flags,
	std::vector<aiMesh*>::const_iterator begin,
	std::vector<aiMesh*>::const_iterator end)
{
	ai_assert(NULL != _out);

	if (begin == end)	{
		*_out = NULL; // no meshes ...
		return;
	}

	// Allocate the output mesh
	aiMesh* out = *_out = new aiMesh();
	out->mMaterialIndex = (*begin)->mMaterialIndex;

	// Find out how much output storage we'll need
	for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{
		out->mNumVertices	+= (*it)->mNumVertices;
		out->mNumFaces		+= (*it)->mNumFaces;
		out->mNumBones		+= (*it)->mNumBones;

		// combine primitive type flags
		out->mPrimitiveTypes |= (*it)->mPrimitiveTypes;
	}

	if (out->mNumVertices) {
		aiVector3D* pv2;

		// copy vertex positions
		if ((**begin).HasPositions())	{

			pv2 = out->mVertices = new aiVector3D[out->mNumVertices];
			for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{
				if ((*it)->mVertices)	{
					::memcpy(pv2,(*it)->mVertices,(*it)->mNumVertices*sizeof(aiVector3D));
				}
				else DefaultLogger::get()->warn("JoinMeshes: Positions expected but input mesh contains no positions");
				pv2 += (*it)->mNumVertices;
			}
		}
		// copy normals
		if ((**begin).HasNormals())	{

			pv2 = out->mNormals = new aiVector3D[out->mNumVertices];
			for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{
				if ((*it)->mNormals)	{
					::memcpy(pv2,(*it)->mNormals,(*it)->mNumVertices*sizeof(aiVector3D));
				}
				else DefaultLogger::get()->warn("JoinMeshes: Normals expected but input mesh contains no normals");
				pv2 += (*it)->mNumVertices;
			}
		}
		// copy tangents and bitangents
		if ((**begin).HasTangentsAndBitangents())	{

			pv2 = out->mTangents = new aiVector3D[out->mNumVertices];
			aiVector3D* pv2b = out->mBitangents = new aiVector3D[out->mNumVertices];

			for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{
				if ((*it)->mTangents)	{
					::memcpy(pv2, (*it)->mTangents,	 (*it)->mNumVertices*sizeof(aiVector3D));
					::memcpy(pv2b,(*it)->mBitangents,(*it)->mNumVertices*sizeof(aiVector3D));
				}
				else DefaultLogger::get()->warn("JoinMeshes: Tangents expected but input mesh contains no tangents");
				pv2  += (*it)->mNumVertices;
				pv2b += (*it)->mNumVertices;
			}
		}
		// copy texture coordinates
		unsigned int n = 0;
		while ((**begin).HasTextureCoords(n))	{
			out->mNumUVComponents[n] = (*begin)->mNumUVComponents[n];

			pv2 = out->mTextureCoords[n] = new aiVector3D[out->mNumVertices];
			for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{

				if ((*it)->mTextureCoords[n])	{
					::memcpy(pv2,(*it)->mTextureCoords[n],(*it)->mNumVertices*sizeof(aiVector3D));
				}
				else DefaultLogger::get()->warn("JoinMeshes: UVs expected but input mesh contains no UVs");
				pv2 += (*it)->mNumVertices;
			}
			++n;
		}
		// copy vertex colors
		n = 0;
		while ((**begin).HasVertexColors(n))	{
			aiColor4D* pv2 = out->mColors[n] = new aiColor4D[out->mNumVertices];
			for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{

				if ((*it)->mColors[n])	{
					::memcpy(pv2,(*it)->mColors[n],(*it)->mNumVertices*sizeof(aiColor4D));
				}
				else DefaultLogger::get()->warn("JoinMeshes: VCs expected but input mesh contains no VCs");
				pv2 += (*it)->mNumVertices;
			}
			++n;
		}
	}

	if (out->mNumFaces) // just for safety
	{
		// copy faces
		out->mFaces = new aiFace[out->mNumFaces];
		aiFace* pf2 = out->mFaces;

		unsigned int ofs = 0;
		for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)	{
			for (unsigned int m = 0; m < (*it)->mNumFaces;++m,++pf2)	{
				aiFace& face = (*it)->mFaces[m];
				pf2->mNumIndices = face.mNumIndices;
				pf2->mIndices = face.mIndices;

				if (ofs)	{
					// add the offset to the vertex
					for (unsigned int q = 0; q < face.mNumIndices; ++q)
						face.mIndices[q] += ofs;	
				}
				face.mIndices = NULL;
			}
			ofs += (*it)->mNumVertices;
		}
	}

	// bones - as this is quite lengthy, I moved the code to a separate function
	if (out->mNumBones)
		MergeBones(out,begin,end);

	// delete all source meshes
	for (std::vector<aiMesh*>::const_iterator it = begin; it != end;++it)
		delete *it;
}

// ------------------------------------------------------------------------------------------------
template <typename Type>
inline void CopyPtrArray (Type**& dest, Type** src, unsigned int num)
{
	if (!num)
	{
		dest = NULL;
		return;
	}
	dest = new Type*[num];
	for (unsigned int i = 0; i < num;++i)
		SceneCombiner::Copy(&dest[i],src[i]);
}

// ------------------------------------------------------------------------------------------------
template <typename Type>
inline void GetArrayCopy (Type*& dest, unsigned int num )
{
	if (!dest)return;
	Type* old = dest;

	dest = new Type[num];
	::memcpy(dest, old, sizeof(Type) * num);
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::CopySceneFlat(aiScene** _dest,aiScene* src)
{
	// reuse the old scene or allocate a new?
	if (*_dest)(*_dest)->~aiScene();
	else *_dest = new aiScene();

	::memcpy(*_dest,src,sizeof(aiScene));
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::CopyScene(aiScene** _dest,aiScene* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiScene* dest = *_dest = new aiScene();

	// copy animations
	dest->mNumAnimations = src->mNumAnimations;
	CopyPtrArray(dest->mAnimations,src->mAnimations,
		dest->mNumAnimations);

	// copy textures
	dest->mNumTextures = src->mNumTextures;
	CopyPtrArray(dest->mTextures,src->mTextures,
		dest->mNumTextures);

	// copy materials
	dest->mNumMaterials = src->mNumMaterials;
	CopyPtrArray(dest->mMaterials,src->mMaterials,
		dest->mNumMaterials);

	// copy lights
	dest->mNumLights = src->mNumLights;
	CopyPtrArray(dest->mLights,src->mLights,
		dest->mNumLights);

	// copy cameras
	dest->mNumCameras = src->mNumCameras;
	CopyPtrArray(dest->mCameras,src->mCameras,
		dest->mNumCameras);

	// copy meshes
	dest->mNumMeshes = src->mNumMeshes;
	CopyPtrArray(dest->mMeshes,src->mMeshes,
		dest->mNumMeshes);

	// now - copy the root node of the scene (deep copy, too)
	Copy( &dest->mRootNode, src->mRootNode);

	// and keep the flags ...
	dest->mFlags = src->mFlags;
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy     (aiMesh** _dest, const aiMesh* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiMesh* dest = *_dest = new aiMesh();

	// get a flat copy
	::memcpy(dest,src,sizeof(aiMesh));

	// and reallocate all arrays
	GetArrayCopy( dest->mVertices,   dest->mNumVertices );
	GetArrayCopy( dest->mNormals ,   dest->mNumVertices );
	GetArrayCopy( dest->mTangents,   dest->mNumVertices );
	GetArrayCopy( dest->mBitangents, dest->mNumVertices );

	unsigned int n = 0;
	while (dest->HasTextureCoords(n))
		GetArrayCopy( dest->mTextureCoords[n++],   dest->mNumVertices );

	n = 0;
	while (dest->HasVertexColors(n))
		GetArrayCopy( dest->mColors[n++],   dest->mNumVertices );

	// make a deep copy of all bones
	CopyPtrArray(dest->mBones,dest->mBones,dest->mNumBones);

	// make a deep copy of all faces
	GetArrayCopy(dest->mFaces,dest->mNumFaces);
	for (unsigned int i = 0; i < dest->mNumFaces;++i)
	{
		aiFace& f = dest->mFaces[i];
		GetArrayCopy(f.mIndices,f.mNumIndices);
	}
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy (aiMaterial** _dest, const aiMaterial* src)
{
	ai_assert(NULL != _dest && NULL != src);

	MaterialHelper* dest = (MaterialHelper*) ( *_dest = new MaterialHelper() );
	dest->mNumAllocated  =  src->mNumAllocated;
	dest->mNumProperties =  src->mNumProperties;
	dest->mProperties    =  new aiMaterialProperty* [dest->mNumAllocated];

	for (unsigned int i = 0; i < dest->mNumProperties;++i)
	{
		aiMaterialProperty* prop  = dest->mProperties[i] = new aiMaterialProperty();
		aiMaterialProperty* sprop = src->mProperties[i];

		prop->mDataLength = sprop->mDataLength;
		prop->mData = new char[prop->mDataLength];
		::memcpy(prop->mData,sprop->mData,prop->mDataLength);

		prop->mIndex    = sprop->mIndex;
		prop->mSemantic = sprop->mSemantic;
		prop->mKey      = sprop->mKey;
		prop->mType		= sprop->mType;
	}
}
	
// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy  (aiTexture** _dest, const aiTexture* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiTexture* dest = *_dest = new aiTexture();

	// get a flat copy
	::memcpy(dest,src,sizeof(aiTexture));

	// and reallocate all arrays. We must do it manually here
	const char* old = (const char*)dest->pcData;
	if (old)
	{
		unsigned int cpy;
		if (!dest->mHeight)cpy = dest->mWidth;
		else cpy = dest->mHeight * dest->mWidth * sizeof(aiTexel);

		if (!cpy)
		{
			dest->pcData = NULL;
			return;
		}
		// the cast is legal, the aiTexel c'tor does nothing important
		dest->pcData = (aiTexel*) new char[cpy];
		::memcpy(dest->pcData, old, cpy);
	}
}
	
// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy     (aiAnimation** _dest, const aiAnimation* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiAnimation* dest = *_dest = new aiAnimation();

	// get a flat copy
	::memcpy(dest,src,sizeof(aiAnimation));

	// and reallocate all arrays
	CopyPtrArray( dest->mChannels, src->mChannels, dest->mNumChannels );
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy     (aiNodeAnim** _dest, const aiNodeAnim* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiNodeAnim* dest = *_dest = new aiNodeAnim();

	// get a flat copy
	::memcpy(dest,src,sizeof(aiNodeAnim));

	// and reallocate all arrays
	GetArrayCopy( dest->mPositionKeys, dest->mNumPositionKeys );
	GetArrayCopy( dest->mScalingKeys,  dest->mNumScalingKeys );
	GetArrayCopy( dest->mRotationKeys, dest->mNumRotationKeys );
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy   (aiCamera** _dest,const  aiCamera* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiCamera* dest = *_dest = new aiCamera();

	// get a flat copy, that's already OK
	::memcpy(dest,src,sizeof(aiCamera));
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy   (aiLight** _dest, const aiLight* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiLight* dest = *_dest = new aiLight();

	// get a flat copy, that's already OK
	::memcpy(dest,src,sizeof(aiLight));
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy     (aiBone** _dest, const aiBone* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiBone* dest = *_dest = new aiBone();

	// get a flat copy
	::memcpy(dest,src,sizeof(aiBone));

	// and reallocate all arrays
	GetArrayCopy( dest->mWeights, dest->mNumWeights );
}

// ------------------------------------------------------------------------------------------------
void SceneCombiner::Copy     (aiNode** _dest, const aiNode* src)
{
	ai_assert(NULL != _dest && NULL != src);

	aiNode* dest = *_dest = new aiNode();

	// get a flat copy
	::memcpy(dest,src,sizeof(aiNode));

	// and reallocate all arrays
	GetArrayCopy( dest->mMeshes, dest->mNumMeshes );
	CopyPtrArray( dest->mChildren, src->mChildren,dest->mNumChildren);
}


}
