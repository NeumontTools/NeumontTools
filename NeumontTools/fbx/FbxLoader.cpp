#include "FbxLoader.h"
#include <vector>
#include "FbxExtractor.h"
#include "MyVertex.h"
#include "../AnimationKeySet.h"
#include "../Vertex.h"
#include <iostream>
using std::cout;
using std::endl;

namespace Neumont
{
	bool FbxLoader::averageNormals = true;

	void AverageNormals(Neumont::ShapeData& outData);
	void FbxLoader::ToggleAverage(bool toggle)
	{
		averageNormals = toggle;
	}

	bool FbxLoader::loadFbxFile(
		const char* filename, 
		Neumont::ShapeData*& shapeDatas,
		unsigned int& numShapeDatas)
	{
		// I know this can be cleanup up, especially ownership
		std::vector<GeometryToolData<MyVertex>> geometryToolData;
		std::vector<std::string> textureFileNames;
		std::vector<ushort> animIndicies;
		std::vector<AnimationKeyFrameSet> keySet;
		std::vector<skeletonBone> skeleton;
		FbxExtractor<MyVertex> extractor;
		bool b = extractor.pullScene(filename, geometryToolData, textureFileNames,animIndicies, keySet,skeleton);
		if(! b )
			return false;

		numShapeDatas = geometryToolData.size();
		shapeDatas = new ShapeData[numShapeDatas];

		for(uint shapeDataIndex = 0; shapeDataIndex < numShapeDatas; shapeDataIndex++)
		{
			GeometryToolData<MyVertex>& data = geometryToolData[shapeDataIndex];
			Neumont::ShapeData& outData = shapeDatas[shapeDataIndex];

			////////outData.animationKeySet = toArray(keySet);

			outData.numVerts = data.numVerts;
			outData.numIndices = data.numIndices;

			outData.verts = new Neumont::Vertex[data.numVerts * 4];
			for(uint i = 0; i < data.numVerts; i++)
			{
				const MyVertex& source = data.vertices[i];
				Neumont::Vertex& target = outData.verts[i];
				target.position.x = source.x;
				target.position.y = source.y;
				target.position.z = source.z;

				// Convert color range from 0-255 int to 0-1 float
				//switched to float from 0-255
				unsigned int color = source.color;
				target.color.r = ((color & 0xff000000)>> 24) / 1.0f;
				target.color.g = ((color & 0x00ff0000)>> 16) / 1.0f;
				target.color.b = ((color & 0x0000ff00)>> 8) / 1.0f; 
				target.color.a = 255;

				target.normal.x = source.normal.x;
				target.normal.y = source.normal.y;
				target.normal.z = source.normal.z;

				target.uv.x = source.u;
				target.uv.y = source.v;

			}
			outData.indices = data.indices; // Caller takes ownership
			delete [] data.vertices;

			outData.textureFileName = NULL;
			outData.normalMapFileName = NULL;
			uint check = -1;
			if(data.textureIndex != check)
			{
				outData.textureFileName = new char[textureFileNames[data.textureIndex].length()+1];
				strcpy(outData.textureFileName, textureFileNames[data.textureIndex].c_str());
			}
			else
			{
				outData.textureFileName = NULL;
			}
			if(data.normalMapIndex != check)
			{
				outData.normalMapFileName = new char[textureFileNames[data.normalMapIndex].length()+1];
				strcpy(outData.normalMapFileName, textureFileNames[data.normalMapIndex].c_str());
			}
			else
			{
				outData.normalMapFileName = NULL;
			}
			outData.numAnimations = keySet.size();
			if(outData.numAnimations != 0)
			{
				outData.animation = new AnimationKeyFrameSet[keySet.size()];
			}
			else
			{
				outData.animation = NULL;
			}
			if(outData.numAnimations !=0)
			{
				outData.frameCount = keySet[0].transKeys.size();
			}
			else
			{
				outData.frameCount =0;
			}
			for(uint i=0;i<keySet.size();i++)
			{
				outData.animation[i] = keySet[i];
			}
			outData.skeleton = new skeletonBone[skeleton.size()];
			outData.numBones = skeleton.size();
			for(uint i=0;i<skeleton.size();i++)
			{
				outData.skeleton[i] = skeleton[i];
			}
			calculateTangents(outData);
			if(averageNormals)
				AverageNormals(outData);
			findMinMaxPoints(outData);
			normalizeTangents(outData);
		}
		return true;
	}

	void FbxLoader::normalizeTangents(Neumont::ShapeData& shapeData)
	{
		for(uint i=0;i<shapeData.numVerts;i++)
		{
			Vertex& vert = shapeData.verts[i];
			vert.tangent = glm::normalize(vert.tangent); 
			vert.bitangent = glm::normalize(vert.bitangent); 
		}
	}

	void FbxLoader::calculateTangents(Neumont::ShapeData& shapeData)
	{
		// See comments in ShapeGenerator's version of this for understanding of the algorithm
		for(uint i=0;i<shapeData.numIndices;i+=3)
		{
			Neumont::Vertex& vert1 = shapeData.verts[shapeData.indices[i]];
			Neumont::Vertex& vert2 = shapeData.verts[shapeData.indices[i+1]];
			Neumont::Vertex& vert3 = shapeData.verts[shapeData.indices[i+2]];
			// Shortcuts for vertices
			glm::vec3 & v0 = vert1.position;
			glm::vec3 & v1 = vert2.position;
			glm::vec3 & v2 = vert3.position;

			// Shortcuts for UVs
			glm::vec2 & uv0 = vert1.uv;
			glm::vec2 & uv1 = vert2.uv;
			glm::vec2 & uv2 = vert3.uv;

			// Edges of the triangle : postion delta
			glm::vec3 deltaPos1 = v1-v0;
			glm::vec3 deltaPos2 = v2-v0;

			// UV delta
			glm::vec2 deltaUV1 = uv1-uv0;
			glm::vec2 deltaUV2 = uv2-uv0;
			float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
			glm::vec3 tangent = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y)*r;
			glm::vec3 bitangent = (deltaPos2 * deltaUV1.x   - deltaPos1 * deltaUV2.x)*r;

			//make sure it's right handed
			tangent = glm::normalize(tangent - vert1.normal * glm::dot(vert1.normal, tangent));
			if (glm::dot(glm::cross(vert1.normal, tangent), bitangent) > 0.0f)
				tangent = tangent * -1.0f;

			vert1.tangent = tangent;
			vert1.bitangent = bitangent;
			vert2.tangent = tangent;
			vert2.bitangent = bitangent;
			vert3.tangent = tangent;
			vert3.bitangent = bitangent;
		}
	}

	void FbxLoader::findMinMaxPoints(ShapeData& shapeData)
	{
		shapeData.minPoint = glm::vec3(HUGE_K);
		shapeData.maxPoint = -1.0f * shapeData.minPoint;
		for(uint i=0;i<shapeData.numVerts;i++)
		{
			glm::vec3& vertex = shapeData.verts[i].position;
			shapeData.minPoint.x = min(shapeData.minPoint.x,vertex.x);
			shapeData.minPoint.y = min(shapeData.minPoint.y,vertex.y);
			shapeData.minPoint.z = min(shapeData.minPoint.z,vertex.z);
			shapeData.maxPoint.x = max(shapeData.maxPoint.x,vertex.x);
			shapeData.maxPoint.y = max(shapeData.maxPoint.y,vertex.y);
			shapeData.maxPoint.z = max(shapeData.maxPoint.z,vertex.z);
		}
	}


	void FbxLoader::AverageNormals(Neumont::ShapeData& outData)
	{
		for(uint i= 0; i < outData.numVerts; i++)
		{
			int count =0;
			for(uint j = i+1; j < outData.numVerts;j++)
			{
				//find a matching vert
				if(outData.verts[i].position.x == outData.verts[j].position.x &&
					outData.verts[i].position.y == outData.verts[j].position.y &&
					outData.verts[i].position.z == outData.verts[j].position.z)
				{
					//fix up the normal
					outData.verts[i].normal += outData.verts[j].normal;
					outData.verts[i].tangent += outData.verts[j].tangent;
					outData.verts[i].bitangent += outData.verts[j].bitangent;
					count++;

					//fix indicies
					for(uint k=0;k<outData.numIndices;k++)
					{
						if(outData.indices[k] > j)
							outData.indices[k] -= 1;
						else if(outData.indices[k] == j)
							outData.indices[k] = i;
					}

					//delete the vert
					memcpy(&outData.verts[j],&outData.verts[j+1],(outData.numVerts - j) * sizeof(Neumont::Vertex));
					outData.numVerts--;

					j--;
				}
			}
			glm::normalize(outData.verts[i].normal);
			//Not doing this allows bigger triangle to have a larger effect on
			//bitangent and tangets of the averaged verts
			//glm::normalize(outData.verts[i].tangent);
			//glm::normalize(outData.verts[i].bitangent);
		}
		Neumont::Vertex* dataToBeCoppied =  outData.verts;
		outData.verts = new Neumont::Vertex[outData.numVerts];
		memcpy(outData.verts,dataToBeCoppied,outData.numVerts * sizeof(Neumont::Vertex));
	}
}