#ifndef FBX_LOADER_H
#define FBX_LOADER_H
#include "../ExportImportHeader.h"
#include "../Vertex.h"
#include "../ShapeData.h"

namespace Neumont
{
	class DLL_SHARED FbxLoader
	{
		static bool averageNormals;
		static void findMinMaxPoints(Neumont::ShapeData& shapeData);
		static void AverageNormals(Neumont::ShapeData& outData);
		static void calculateTangents(Neumont::ShapeData& shapeData);
		static void normalizeTangents(Neumont::ShapeData& shapeData);
	public:
		static bool loadFbxFile(
			const char* filename,
			Neumont::ShapeData*& shapeDatas,
			unsigned int& numShapeDatas);
		static void ToggleAverage(bool);
	};
}

#endif