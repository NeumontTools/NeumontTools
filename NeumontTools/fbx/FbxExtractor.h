#ifndef FBX_EXTRACTOR_H
#define FBX_EXTRACTOR_H
#include "../AnimationKeySet.h"
#include <cassert>
#include "SDK_Utility.h"
#include "GeometryToolData.h"
#include "fbxsdk\fileio\fbxiobase.h"
#include <string>
#include <vector>
#include <iostream>
#include <set>
using std::vector;

template<class T>
T* toArray(const std::vector<T>& source)
{
	T* ret = new T[source.size()];
	for(size_t i = 0; i < source.size(); i++)
		ret[i] = source[i];
	return ret;
}


template <class TVert>
class FbxExtractor
{
	std::vector<GeometryToolData<TVert>> geometries;
	std::vector<std::string> textureFileNames;
	std::vector<AnimationKeyFrameSet> animKeySet;
	struct controlPointWeightMap{
		uint index;
		char* boneName;
		float weight;
		uint boneID;
		controlPointWeightMap(const int& id,char* bone,const float& weit):index(id),boneName(bone),weight(weit),boneID(0){}
	};
	struct vertexWeight{
		float weight;
		uint boneID;
		uint vertexID;
		vertexWeight(uint vid,uint bid, float weit): vertexID(vid),boneID(bid), weight(weit){}
	};
	struct vertexWeightData{
		uint boneID;
		glm::vec3 weight;
		vertexWeightData():boneID(0),weight(0.0f,0.0f,0.0f){}
	};
	std::vector<skeletonBone> skeleton;
	std::vector<uint> vertexMap;
	std::vector<controlPointWeightMap> weightMap;
	std::vector<vertexWeightData> vertexWeights;

	// Adds the indices to the MeshVector, calculating "fanning"
	// quads or polygons out from a single vertex
	void addIndices(size_t numVertsThisPolygon, 
		const std::vector<TVert>& verts,
		std::vector<unsigned short>& indices);
	void pullPosition(KFbxVector4* controlPoints, int controlPointIndex, TVert& vert);
	void pullUVs(KFbxMesh* mesh, int vertexId, int controlPointIndex, 
		int polygonVertex, int vertexIndex, TVert& vert);
	void pullTextureFilenameIndex(KFbxFileTexture* pTexture, int pBlendMode, size_t& textureDataIndex);
	void pullTextureFile(KFbxTexture* pTexture, int pBlendMode, size_t& textureDataIndex);
	void pullTextureInfo(KFbxGeometry* pGeometry, GeometryToolData<TVert>& meshInfo);
	void pullFbxData(const char* nodeName, KFbxMesh* mesh);
	void pullNodeInfo(KFbxNode* node);
	void pullSkinInfo(KFbxMesh* mesh);
	bool isDuplicateMesh(const GeometryToolData<TVert>& data) const;
	void AnimationTake(FbxAnimLayer* pAnimLayer,FbxNode* pNode, KFbxScene * pScene);
	bool AnimationExists(FbxAnimLayer *pAnimLayer,KFbxNode* node);
	void addWeights(vector<TVert>& verts);
	void pullSkeletonInfo(KFbxNode* node);

public:
	bool pullScene(	const char* fileName,
		std::vector<GeometryToolData<TVert>>& baseGeometries,
		std::vector<std::string>& textureFileNames,
		std::vector<ushort>& animIndicies,
		std::vector<AnimationKeyFrameSet>& animationKeySet,
		std::vector<skeletonBone>& skeleton);
};

template<class TCollectionType, class UReturnType>
bool pullT(KFbxMesh* mesh, int vertexId, int controlPointIndex, 
	const TCollectionType* (KFbxLayer::*getCollectionMethod)() const,
	UReturnType& returnVal)
{
	//check for vertex control points
	const TCollectionType* collection = NULL;
	for (int j = 0; j < mesh->GetLayerCount() && collection == NULL; j++)
		collection = (mesh->GetLayer(j)->*getCollectionMethod)();

	UReturnType sourceValue;

	if(collection == NULL)
		return false;

	KFbxLayerElement::EMappingMode mappingMode = collection->GetMappingMode();
	//collection->SetMappingMode( KFbxLayerElementNormal::eByControlPoint);

	//check the different mapping modes that the verticies might be stored in
	switch (mappingMode)
	{
	case KFbxLayerElement::eByControlPoint:
		switch (collection->GetReferenceMode())
		{
		case KFbxLayerElement::eDirect:
			sourceValue = collection->GetDirectArray().GetAt(controlPointIndex);
			break;
		case KFbxLayerElement::eIndexToDirect:
			{
				int id = collection->GetIndexArray().GetAt(controlPointIndex);
				sourceValue = collection->GetDirectArray().GetAt(id);
			}
			break;
		}
		break;

	case KFbxLayerElement::eByPolygonVertex:
		{
			switch (collection->GetReferenceMode())
			{
			case KFbxLayerElement::eDirect:
				sourceValue = collection->GetDirectArray().GetAt(vertexId);
				break;
			case KFbxLayerElement::eIndexToDirect:
				{
					int id = collection->GetIndexArray().GetAt(vertexId);
					sourceValue = collection->GetDirectArray().GetAt(id);
				}
				break;
			}
		}
		break;
	}
	returnVal = sourceValue;
	return true;
}

// Adds the indices to the MeshVector, calculating "fanning"
// quads or polygons out from a single vertex
template <class TVert>
void FbxExtractor<TVert>::addIndices(
	size_t numVertsThisPolygon, 
	const std::vector<TVert>& verts,
	std::vector<unsigned short>& indices)
{
	size_t baseIndex = verts.size() - numVertsThisPolygon;
	for(size_t i = baseIndex + 1; i < baseIndex + numVertsThisPolygon - 1; i++)
	{
		indices.push_back(baseIndex);
		indices.push_back(i);
		indices.push_back(i + 1);
	}
}

template <class TVert>
void FbxExtractor<TVert>::pullPosition(KFbxVector4* controlPoints, int controlPointIndex, TVert& vert)
{
	const KFbxVector4& sourceVert = controlPoints[controlPointIndex];
	vert.x = sourceVert[0];
	vert.y = sourceVert[1];
	vert.z = sourceVert[2];
}

template <class TVert>
void FbxExtractor<TVert>::pullUVs(KFbxMesh* mesh, int vertexId, int controlPointIndex, 
	int polygonVertex, int vertexIndex, TVert& vert)
{
	// Unfortunately, getting UVs is too different 
	// to use my template trick,
	// so some duplicate code in this function:

	KFbxLayerElementUV* leUV = NULL;
	for (int j = 0; j < mesh->GetLayerCount() && leUV == NULL; j++)
		leUV = mesh->GetLayer(j)->GetUVs();
	if(leUV == NULL)
		return;

	KFbxVector2 sourceUV;
	KFbxLayerElement::EMappingMode mappingMode = leUV->GetMappingMode(); 
	//based on the mapping mode of the mesh the UVs are stored slightly differently
	switch (mappingMode)
	{
	case KFbxLayerElement::eByControlPoint:
		switch (leUV->GetReferenceMode())
		{
		case KFbxLayerElement::eDirect:
			sourceUV = leUV->GetDirectArray().GetAt(controlPointIndex);
			break;
		case KFbxLayerElement::eIndexToDirect:
			{
				int id = leUV->GetIndexArray().GetAt(controlPointIndex);
				sourceUV = leUV->GetDirectArray().GetAt(id);
			}
			break;
		}
		break;

	case KFbxLayerElement::eByPolygonVertex:
		{
			switch (leUV->GetReferenceMode())
			{
			case KFbxLayerElement::eDirect:
				sourceUV = leUV->GetDirectArray().GetAt(vertexId);
				break;
			case KFbxLayerElement::eIndexToDirect:
				{
					int id = leUV->GetIndexArray().GetAt(vertexId);
					sourceUV = leUV->GetDirectArray().GetAt(id);

				}
				break;
			}
		}
		break;
	}

	vert.u = sourceUV[0];
	vert.v = sourceUV[1];
}

template <class TVert>
void FbxExtractor<TVert>::pullTextureFilenameIndex(KFbxFileTexture* pTexture, int pBlendMode, size_t& textureDataIndex)
{
	// if a texture was found gets the file name
	const char* fileName = pTexture->GetFileName();

	if( ! fileName)
		return;
	for(size_t i = 0; i < textureFileNames.size(); i++)
		if(textureFileNames[i] == std::string(fileName))
		{
			textureDataIndex = i;
			return;
		}
		textureDataIndex = textureFileNames.size();
		textureFileNames.push_back(fileName);
}

template <class TVert>
void FbxExtractor<TVert>::pullTextureFile(KFbxTexture* pTexture, int pBlendMode, size_t& textureDataIndex)
{
	//TODO
	//use the file name that is pulled to go find the image
	//READ IN THE IMPORTANT BITS <this is a lot more complicated than it sounds>
	//without the removal of the headers on the image file we will still need to reinterpret them somehow
	//then save those bits to be read out late
	textureDataIndex = -1;
}

template <class TVert>
void FbxExtractor<TVert>::pullTextureInfo(KFbxGeometry* pGeometry, GeometryToolData<TVert>& meshInfo)
{
	int lMaterialIndex;
	KFbxProperty kfbxProperty;    
	int numMaterials = pGeometry->GetNode()->GetSrcObjectCount(KFbxSurfaceMaterial::ClassId);
	for (lMaterialIndex = 0; lMaterialIndex < numMaterials; lMaterialIndex++){
		KFbxSurfaceMaterial *material = 
			(KFbxSurfaceMaterial *)pGeometry->GetNode()->
			GetSrcObject(KFbxSurfaceMaterial::ClassId, lMaterialIndex);

		if(! material)
			continue;
		meshInfo.textureIndex = -1;
		for(int lTextureIndex = 0; lTextureIndex < KFbxLayerElement::sTypeTextureCount;lTextureIndex++)
		{
			const char* textureChannelName = KFbxLayerElement::sTextureChannelNames[lTextureIndex];
			kfbxProperty = material->FindProperty(textureChannelName);			
			if(kfbxProperty.IsValid())
			{
				// No layered texture simply get on the property
				int numTextures = kfbxProperty.GetSrcObjectCount(KFbxTexture::ClassId);
				for(int j =0; j < numTextures; ++j)
				{
					//pulls texture
					KFbxTexture* texture = FbxCast <KFbxFileTexture> (
						kfbxProperty.GetSrcObject(KFbxTexture::ClassId, j));

					if(texture)
					{
						pullTextureFile(texture, -1, meshInfo.textureIndex);
					}
					//pulls texture file name
					KFbxFileTexture* textureFile = FbxCast <KFbxFileTexture> (
						kfbxProperty.GetSrcObject(KFbxTexture::ClassId, j));
					if(textureFile)
					{
						KString propertyName = kfbxProperty.GetName();
						int textureNum = j;

						FbxLayerElement::EType typeTextureValueOffsetted = ((FbxLayerElement::EType)
							(FbxLayerElement::sTypeTextureStartIndex + lTextureIndex));
						if(typeTextureValueOffsetted == FbxLayerElement::EType::eTextureDiffuse)
							pullTextureFilenameIndex(textureFile, -1, meshInfo.textureIndex);
						else if(typeTextureValueOffsetted == FbxLayerElement::EType::eTextureNormalMap)
							pullTextureFilenameIndex(textureFile, -1, meshInfo.normalMapIndex);
						else 
						{ /*assert(false);*/
							meshInfo.textureIndex = -1;
						} // It's a different texture type than these ones, Look at FbxLayerElement::EType to see other possibilities
					}
				}
			}
		}
	}
}

template <class TVert>
void FbxExtractor<TVert>::pullFbxData(const char* nodeName, KFbxMesh* mesh)
{
	// Skinning
	pullSkinInfo(mesh);
	
	// Textures
	GeometryToolData<TVert> meshInfo;
	pullTextureInfo(mesh, meshInfo);

	int numPolygons = mesh->GetPolygonCount();

	KFbxVector4* controlPoints = mesh->GetControlPoints(); 
	// Polygons
	int vertexId = 0;

	//temporary storage before theyre placed into the geometryinfo
	vector<TVert> vertss;
	vector<unsigned short> indicess;
	for (int polygonIndex = 0; polygonIndex < numPolygons; polygonIndex++)
	{
		int numVertsThisPolygon = mesh->GetPolygonSize(polygonIndex);
		for (int vertexIndex = 0; vertexIndex < numVertsThisPolygon; vertexIndex++)
		{
			//ANIMATIONS
			//use this value to map vertex to wieghts
			int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, vertexIndex);

			TVert vert;
			pullPosition(controlPoints, controlPointIndex, vert);

			KFbxVector4 sourceNormal;
			bool b = pullT<KFbxLayerElementNormal, KFbxVector4>(mesh, vertexId, 
				controlPointIndex, &KFbxLayer::GetNormals, sourceNormal);
			if(b)
			{
				vert.normal.x = sourceNormal[0];
				vert.normal.y = sourceNormal[1];
				vert.normal.z = sourceNormal[2];
			}

			//////////////////////////////////////////
			//pull the color for the vertex
			//however these are overwritten later with a random color
			KFbxColor color;
			b = pullT<KFbxLayerElementVertexColor, KFbxColor>(
				mesh, vertexId, controlPointIndex, &KFbxLayer::GetVertexColors, color);
			if(b)
				vert.color = ColorToInt(color.mRed, color.mGreen, color.mBlue, color.mAlpha);
			//////////////////////////////////////////

			//UV Info
			pullUVs(mesh, vertexId, controlPointIndex, polygonIndex, vertexIndex, vert);

			if(meshInfo.textureIndex == -1)
			{
				// No textures, and I want more than a white 
				// box to show up in the scene, so random colors:
				vert.color = ColorToInt(rand() % 255, rand() % 255, rand() % 255);
				//vert.color = ColorToInt(255, 0, 0);
			}
			vertexMap.push_back(controlPointIndex);
			vertss.push_back(vert);

			vertexId++;
		} 
		addIndices(numVertsThisPolygon, vertss, indicess);
	} 
	addWeights(vertss);
	meshInfo.indices = toArray(indicess);
	meshInfo.numIndices = indicess.size();
	meshInfo.vertices = toArray(vertss);
	meshInfo.numVerts = vertss.size();
	if(isDuplicateMesh(meshInfo))
		meshInfo.destroy();
	else
		geometries.push_back(meshInfo);
}

template <class TVert>
void FbxExtractor<TVert>::pullNodeInfo(KFbxNode* node)
{
	const char* nodeName= node->GetName();
	pullSkeletonInfo(node);
	KFbxMesh* mesh = node->GetMesh();
	if(mesh != NULL)
		pullFbxData(nodeName, mesh);
}

template <class TVert>
void FbxExtractor<TVert>::pullSkinInfo(KFbxMesh* mesh)
{
	//pull the control points connected to each bone as well as the weights associated with those
	KFbxCluster* lCluster;
	for(int i=0;i<mesh->GetDeformerCount(KFbxDeformer::eSkin);i++)
	{
		for(int j=0;j<((FbxSkin *) mesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();j++)
		{
			//pull the weight
			lCluster = ((FbxSkin *) mesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
			double *weights = lCluster->GetControlPointWeights();
			const char* boneName = lCluster->GetLink()->GetName();

			char* boneNameLocation = new char[strlen(boneName)+1];
			strcpy_s(boneNameLocation,strlen(boneName)+1,boneName);

			//pull the control points which are used to match to the verticies
			int *lControlPointIndex=lCluster->GetControlPointIndices();
			//place all the data pulled into the weight map
			//so they can be mapped later
			for(int x=0;x<lCluster->GetControlPointIndicesCount();x++)
			{
				weightMap.push_back(controlPointWeightMap(lControlPointIndex[x],NULL,weights[x]));
			}
			boneNameLocation = 0;
		}
	}
}

template <class TVert>
bool FbxExtractor<TVert>::pullScene(
	const char* fileName,
	std::vector<GeometryToolData<TVert>>& baseGeometries,
	std::vector<std::string>& textureFileNames,
	std::vector<ushort>& animIndicies,
	std::vector<AnimationKeyFrameSet>& animationKeySet,
	std::vector<skeletonBone>& skele)
{
	KFbxScene* gScene = InitializeSdkManagerAndScene();

	if(!LoadFBXScene(fileName))
		return false;

	size_t numNodes = gScene->GetNodeCount();

	for(size_t i = 0; i < numNodes; i++)
	{
		pullNodeInfo(gScene->GetNode(i));

		//extract animatnions
		int numStacks = gScene->GetSrcObjectCount(FBX_TYPE(FbxAnimStack));
		FbxAnimStack* pAnimStack;
		for(int k=0;k<numStacks;k++)
		{
			FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(gScene->GetSrcObject(FBX_TYPE(FbxAnimStack), k));
			int numAnimLayers = pAnimStack->GetMemberCount(FBX_TYPE(FbxAnimLayer));
			for(int j=0;j<numAnimLayers;j++)
			{
				FbxAnimLayer* lAnimLayer = pAnimStack->GetMember(FBX_TYPE(FbxAnimLayer), j);
				//quickly test if there is any animations to even be pulled
				//since creating animation sets for bones that never move would
				//be extremely expensive to save in memory
				if(AnimationExists(lAnimLayer,gScene->GetNode(i)))
				{
					AnimationTake(lAnimLayer,gScene->GetNode(i), gScene);
				}
			}
		}
	}
	for(int i=0;i<animKeySet.size();i++)
	{
		animationKeySet.push_back(animKeySet[i]);

	}
	textureFileNames = this->textureFileNames; // :) tool code
	baseGeometries = geometries;
	skele = skeleton;

	this->skeleton.clear();
	this->geometries.clear();
	this->textureFileNames.clear();
	this->vertexMap.clear();
	this->animKeySet.clear();
	this->weightMap.clear();
	DestroySdkObjects();
	return true;
}

template<class TVert>
bool FbxExtractor<TVert>::isDuplicateMesh(const GeometryToolData<TVert>& data) const
{
	for(uint i = 0; i < geometries.size(); i++)
	{
		const GeometryToolData<TVert>& g = geometries[i];
		if(g.numIndices != data.numIndices || g.numVerts != data.numVerts)
			continue;
		for(uint j = 0; j < data.numIndices; j++)
			if(g.indices[i] != data.indices[i])
				goto EndLoop;
		for(uint j = 0; j < data.numVerts; j++)
			if(memcmp(data.vertices + j, g.vertices + j, sizeof(TVert)) != 0)
				goto EndLoop;
		return true;
EndLoop: {}
	}
	return false;
}

template<class TVert>
bool FbxExtractor<TVert>::AnimationExists(FbxAnimLayer *pAnimLayer,KFbxNode* pNode)
{
	bool success = false;
	//check for translation

	FbxAnimCurve *pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	if (pCurve)
	{
		if (pCurve->KeyGetCount() > 0)
			success = true;
	}

	pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
			success = true;
	}

	pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
			success = true;
	}     

	//check for scaling

	pCurve = pNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
			success = true;
	}

	pCurve = pNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
			success = true;
	}

	pCurve = pNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
		{
			success = true;   
		}
	}

	//check for roation

	pCurve = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
		{
			success = true;
		}
	}

	pCurve = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
		{
			success = true;
		}
	}

	pCurve = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	if (pCurve && !success)
	{
		if (pCurve->KeyGetCount() > 0)
		{
			success = true;
		}
	}  
	return success;
}

template<class TVert>
void FbxExtractor<TVert>::AnimationTake(FbxAnimLayer* pAnimLayer,FbxNode* pNode, KFbxScene * pScene)
{
	animKeySet.push_back(AnimationKeyFrameSet());
	AnimationKeyFrameSet& animationKeySet = animKeySet[animKeySet.size()-1];
	animationKeySet.clear();
	const char * pNodeName = pNode->GetName();
	animKeySet[animKeySet.size()-1].index = skeleton.size();
	//bool bAnimationPresent = AnimationPresent(pAnimLayer, pNode);

	FbxTakeInfo* lCurrentTakeInfo = pScene->GetTakeInfo(std::string(pAnimLayer->GetName()).c_str());
	FbxTime tStart, tStop;

	if (lCurrentTakeInfo)
	{
		tStart = lCurrentTakeInfo->mLocalTimeSpan.GetStart();
		tStop = lCurrentTakeInfo->mLocalTimeSpan.GetStop();
	}
	else
	{
		// filled in by animation
		tStart.SetSecondDouble(FLT_MAX);
		tStop.SetSecondDouble(-FLT_MAX);
	} 

	FbxTime::EMode pTimeMode = pScene->GetGlobalSettings().GetTimeMode();
	double frameRate = FbxTime::GetFrameRate(pTimeMode);

	std::set<FbxTime> timeSet;

	FbxAnimCurve* pCurve = 0;
	uint keyCount =0;

	std::vector<RotateKey> raw_rx;
	std::vector<RotateKey> raw_ry;
	std::vector<RotateKey> raw_rz;

	//pull translation frames
	//pull trans X
	pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;

		for(uint i = 0; i < keyCount; i++)
		{
			FbxTime   lKeyTime = pCurve->KeyGetTime(i);   
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);
		}
	}

	//pull trans Y
	pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;

		for(uint lCount = 0; lCount < keyCount; lCount++)
		{
			FbxTime   lKeyTime = pCurve->KeyGetTime(lCount);    
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);
		}
	}

	//pull trans Z
	pCurve = pNode->LclTranslation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;

		for(uint i = 0; i < keyCount; i++)
		{
			FbxTime   lKeyTime = pCurve->KeyGetTime(i);   
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);
		}
	}     

	//pull scaling frames
	//pull scaling X
	pCurve = pNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;

		for(uint i = 0; i < keyCount; i++)
		{
			FbxTime   lKeyTime = pCurve->KeyGetTime(i);    
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);
		}
	}

	//pull scaling Y
	pCurve = pNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;

		for(uint i = 0; i < keyCount; i++)
		{
			FbxTime   lKeyTime = pCurve->KeyGetTime(i);    
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);        
		}
	}

	//pull scaling Z
	pCurve = pNode->LclScaling.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;

		for(int i = 0; i < keyCount; i++)
		{
			FbxTime   lKeyTime = pCurve->KeyGetTime(i);    
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);        
		}     
	}

	//pull roation
	//pulling rotation is a little different. Rotation is stored within the curve not the matrix.
	//pull rot X
	pCurve = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;
		for(uint i = 0; i < keyCount; i++)
		{
			//pull time for each frame
			FbxTime lKeyTime = pCurve->KeyGetTime(i);    
			//pull the rotation value
			float lKeyValue = static_cast<float>(pCurve->KeyGetValue(i));
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);

			RotateKey rk;
			rk.time = lKeyTime.GetSecondDouble() * frameRate;   
			rk.value = lKeyValue;   
			raw_rx.push_back(rk);

		}
	}

	//pull rot Y
	pCurve = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;
		for(uint i = 0; i < keyCount; i++)
		{
			//pull time for each frame
			FbxTime lKeyTime = pCurve->KeyGetTime(i);
			//pull the roation value
			float lKeyValue = static_cast<float>(pCurve->KeyGetValue(i));
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);        

			RotateKey rk;
			rk.time = lKeyTime.GetSecondDouble() * frameRate;   
			rk.value = lKeyValue;   
			raw_ry.push_back(rk);
		}
	}

	//pull rot Z
	pCurve = pNode->LclRotation.GetCurve(pAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	if (pCurve)
	{
		keyCount = pCurve->KeyGetCount()-1;
		for(uint i = 0; i < keyCount; i++)
		{
			//pull time for each frame
			FbxTime lKeyTime = pCurve->KeyGetTime(i); 
			//pull rotation value
			float lKeyValue = static_cast<float>(pCurve->KeyGetValue(i));
			//set start time to the lowest time
			if (lKeyTime < tStart)
			{
				tStart = lKeyTime;
			}
			//set stop time to the greatest time
			if (lKeyTime > tStop)
			{
				tStop = lKeyTime;
			}

			timeSet.insert(lKeyTime);   

			RotateKey rk;
			rk.time = lKeyTime.GetSecondDouble() * frameRate;   
			rk.value = lKeyValue;   
			raw_rz.push_back(rk);
		}  
	}   

	double timeStartSecondsTimesRate = tStart.GetSecondDouble() * frameRate;
	double timeStopSecondsTimesRate = tStop.GetSecondDouble() * frameRate;  

	timeSet.insert(tStart);
	timeSet.insert(tStop);

	const char * name = pNode->GetName();




	//Check for rotational overflow.
	FbxVector4 lastRot;

	FbxVector4 localT, localR, localS;


	localT  = pNode->LclTranslation.Get();
	localR  = pNode->LclRotation.Get();
	localS  = pNode->LclScaling.Get();


	glm::vec4 lastQuat;

	uint i=0;
	for (std::set<FbxTime>::iterator where = timeSet.begin(); where != timeSet.end(); where++, i++)
	{
		const FbxTime &pTime = *where;

		FbxAMatrix localMatrix = pNode->GetScene()->GetEvaluator()->GetNodeLocalTransform(pNode, pTime);;
		FbxVector4 scale = localMatrix.GetS();
		
		TimeKey scaleKey;
		scaleKey.time = pTime.GetSecondDouble() * frameRate;   

		//create scaling keys
		scaleKey.value.x = scale[0];
		scaleKey.value.y = scale[1];
		scaleKey.value.z = scale[2];

		//is_valid(scaleKey.value);
		animationKeySet.scaleKeys.push_back(scaleKey);

		//need to check if the translation is valid
		FbxVector4 trans = localMatrix.GetT();
		TimeKey transKey;

		transKey.time = pTime.GetSecondDouble() * frameRate;   

		//if the translation isnt valid set it to the local translation
		if (_isnan(trans[0]) || !_finite(trans[0]))
		{
			trans[0] = localT[0];
		}
		if (_isnan(trans[1]) || !_finite(trans[1]))
		{
			trans[1] = localT[1];
		}

		if (_isnan(trans[2]) || !_finite(trans[2]))
		{
			trans[2] = localT[2];
		}

		transKey.value.x = trans[0];
		transKey.value.y = trans[1];
		transKey.value.z = trans[2];

		animationKeySet.transKeys.push_back(transKey);


		FbxQuaternion localQ = localMatrix.GetQ();
		FbxVector4 localRot = localMatrix.GetR();

		TimeKey rotKey;
		rotKey.time = pTime.GetSecondDouble() * frameRate;   

		rotKey.value.x = raw_rx[i].value;
		rotKey.value.y = raw_ry[i].value;
		rotKey.value.z = raw_rz[i].value;

		
		animationKeySet.rotKeys.push_back(rotKey);
	}
}


template<typename TVert>
void FbxExtractor<TVert>::addWeights(vector<TVert>& verts)
{
	//needs to compare the weights.
	//anything past the 3rd smallest one should be thrown out
	//we then only store the top 3 and assume the smallest is w4 = 1 - (w1 + w2 + w3)

	//match up the bone names from weightmap and skeleton
	//to figure out each bones ID
	vector<vertexWeight> weights;
	for(int i=0;i<weightMap.size();i++)
	{
		for(int j=0;j<skeleton.size();j++)
		{
			if(strcmp(skeleton[j].boneName,weightMap[i].boneName) ==0)
			{
				weightMap[i].boneID = j+1;
			}
		}
	}
	for(int i=0;i<vertexMap.size();i++)
	{
		vertexWeights.push_back(vertexWeightData());
		for(int j=0;j<weightMap.size();j++)
		{
			if(vertexMap[i] == weightMap[j].index)
			{
				weights.push_back(vertexWeight(weightMap[j].index,weightMap[j].boneID,weightMap[j].weight));
			}
		}
		if(weights.size() > 4)
		{
			
			//find the 3 most important weights and bit shift them
			//to the appropriate positions to be saved in data later
			for(uint j=0;j<weights.size();j++)
			{
				int count=0;
				for(uint x=0;x<weights.size();x++)
				{					
					if(weights[j].weight == weights[x].weight && i != j)
					{
						//adjust the value of one weight on the off chance 2 are the same
						weights[x].weight += 0.000000000000000000000000000001f;
						weights[j].weight -= 0.000000000000000000000000000001f;
					}
					if(weights[j].weight < weights[x].weight)
					{
						count++;
					}
				}
				//check if weight is in the top 3
				if (count < 4)
				{
					//set up the indicies of skeleton weights
					vertexWeights[vertexWeights.size()-1].boneID |= weights[j].boneID<<(count*8);
					vertexWeights[vertexWeights.size()-1].weight[count] = weights[j].weight;
				}
			}
		}
		else
		{
			//add weights
			for(uint j=0;j<weights.size();j++)
			{				
				int count=0;
				for(uint x=0;x<weights.size();x++)
				{
					if(weights[j].weight == weights[x].weight && i != j)
					{
						//adjust the value of one weight on the off chance 2 are the same
						weights[x].weight += 0.000000000000000000000000000001f;
						weights[j].weight -= 0.000000000000000000000000000001f;
					}
					//compare each weight to every other weight in the array
					if(weights[j].weight < weights[x].weight)
					{
						count++;
					}
					vertexWeights[vertexWeights.size()-1].boneID |= weights[j].boneID<<(count*8);
					vertexWeights[vertexWeights.size()-1].weight[count] = weights[j].weight;
				}
				//if there was only 1 weight it wont be able to gain a count in the loop to be added
				//so it must be added manually
				if(weights.size() == 1)
				{
					vertexWeights[vertexWeights.size()-1].boneID |= weights[j].boneID;
					vertexWeights[vertexWeights.size()-1].weight[count] = weights[j].weight;
				}
			}
			weights.clear();
		}
	}
	for(int i=0;i<verts.size();i++)
	{
		//add the weight data into the skeletonID
		verts[i].skeletonIDs = vertexWeights[i].boneID;
		verts[i].w1 = vertexWeights[i].weight[0];
		verts[i].w2 = vertexWeights[i].weight[1];
		verts[i].w3 = vertexWeights[i].weight[2];
	}
	vertexWeights.clear();
	weightMap.clear();
}

template<class TVert>
void FbxExtractor<TVert>::pullSkeletonInfo(KFbxNode* node)
{
	KFbxSkeleton* lSkeleton = (KFbxSkeleton*) node->GetNodeAttribute();
	if(lSkeleton)
	{
		if(lSkeleton->GetSkeletonType() == KFbxSkeleton::eRoot || lSkeleton->GetSkeletonType() == KFbxSkeleton::eLimbNode || lSkeleton->GetSkeletonType() == KFbxSkeleton::eEffector)
		{
			//eEffector is the root node
			const char* name = node->GetName();
			skeleton.push_back(skeletonBone());
			//to grab the size of a skeletonbone name
			skeletonBone temp;
			memcpy(skeleton[skeleton.size()-1].boneName,name,sizeof(temp.boneName));
			skeleton[skeleton.size()-1].boneName[sizeof(temp.boneName)-1] = '\0';
			assert(skeleton.size()<255);
			//TODO::
			//need to store the parentIDs once I find them.
			// the current id of each bone is the vector index+1
		}
	}
}


#endif
