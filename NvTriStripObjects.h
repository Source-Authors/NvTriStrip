#ifndef NV_TRISTRIP_OBJECTS_H
#define NV_TRISTRIP_OBJECTS_H

#include "VertexCache.h"

#include <cassert>
#include <cstddef>
#include <vector>
#include <list>

/////////////////////////////////////////////////////////////////////////////////
//
// Types defined for stripification
//
/////////////////////////////////////////////////////////////////////////////////

namespace nv::tristrip::internal {

struct MyVertex {
	float x, y, z;
	float nx, ny, nz;
};

using MyVector = MyVertex;

struct MyFace {
	int v1, v2, v3;
	float nx, ny, nz;
};


class NvFaceInfo {
public:
	// vertex indices
	NvFaceInfo(int v0, int v1, int v2){
		m_v0 = v0; m_v1 = v1; m_v2 = v2;
		m_stripId      = -1;
		m_testStripId  = -1;
		m_experimentId = -1;
	}
	
	// data members are left public
	int   m_v0, m_v1, m_v2;
	int   m_stripId;      // real strip Id
	int   m_testStripId;  // strip Id in an experiment
	int   m_experimentId; // in what experiment was it given an experiment Id?
};

// nice and dumb edge class that points knows its
// indices, the two faces, and the next edge using
// the lesser of the indices
class NvEdgeInfo {
public:
	// constructor puts 1 ref on us
	NvEdgeInfo (int v0, int v1){
		m_v0       = v0;
		m_v1       = v1;
		m_face0    = nullptr;
		m_face1    = nullptr;
		m_nextV0   = nullptr;
		m_nextV1   = nullptr;
		
		// we will appear in 2 lists.  this is a good
		// way to make sure we delete it the second time
		// we hit it in the edge infos
		m_refCount = 2;    
		
	}
	
	// ref and unref
	void Unref () { if (--m_refCount == 0) delete this; }
	
	// data members are left public
	unsigned int m_refCount;
	NvFaceInfo  *m_face0, *m_face1;
	int          m_v0, m_v1;
	NvEdgeInfo  *m_nextV0, *m_nextV1;
};


// This class is a quick summary of parameters used
// to begin a triangle strip.  Some operations may
// want to create lists of such items, so they were
// pulled out into a class
class NvStripStartInfo {
public:
	NvStripStartInfo(NvFaceInfo *startFace, NvEdgeInfo *startEdge, bool toV1){
		m_startFace    = startFace;
		m_startEdge    = startEdge;
		m_toV1         = toV1;
	}
	NvFaceInfo    *m_startFace;
	NvEdgeInfo    *m_startEdge;
	bool           m_toV1;      
};


using NvFaceInfoVec = std::vector<NvFaceInfo*>;
using NvFaceInfoList = std::list<NvFaceInfo*>;
using NvStripList = std::list<NvFaceInfoVec*>;
using NvEdgeInfoVec = std::vector<NvEdgeInfo*>;

using WordVec = std::vector<unsigned short>;
using UIntVec = std::vector<unsigned int>;
using IntVec = std::vector<int>;
using MyVertexVec = std::vector<MyVertex>;
using MyFaceVec = std::vector<MyFace>;

// This is a summary of a strip that has been built
class NvStripInfo {
public:
	// A little information about the creation of the triangle strips
	NvStripInfo(const NvStripStartInfo &startInfo, int stripId, int experimentId = -1) :
	  m_startInfo(startInfo)
	{
		m_stripId      = stripId;
		m_experimentId = experimentId;
		visited = false;
		m_numDegenerates = 0;
	}
	  
	// This is an experiment if the experiment id is >= 0
	inline bool IsExperiment () const { return m_experimentId >= 0; }
	  
	inline bool IsInStrip (const NvFaceInfo *faceInfo) const 
	{
		if(faceInfo == nullptr)
			return false;
		  
		return (m_experimentId >= 0 ? faceInfo->m_testStripId == m_stripId : faceInfo->m_stripId == m_stripId);
	}
	  
	bool SharesEdge(const NvFaceInfo* faceInfo, NvEdgeInfoVec &edgeInfos) const;
	  
	// take the given forward and backward strips and combine them together
	void Combine(const NvFaceInfoVec &forward, const NvFaceInfoVec &backward);
	  
	//returns true if the face is "unique", i.e. has a vertex which doesn't exist in the faceVec
	bool Unique(const NvFaceInfoVec& faceVec, NvFaceInfo* face) const;
	  
	// mark the triangle as taken by this strip
	bool IsMarked    (NvFaceInfo *faceInfo) const;
	void MarkTriangle(NvFaceInfo *faceInfo);
	  
	// build the strip
	void Build(NvEdgeInfoVec &edgeInfos, NvFaceInfoVec &faceInfos);
	  
	// public data members
	NvStripStartInfo m_startInfo;
	NvFaceInfoVec    m_faces;
	int              m_stripId;
	int              m_experimentId;
	  
	bool visited;

	int m_numDegenerates;

// START EPIC MOD: Fix memory leak
	NvFaceInfoVec m_Degenerates;
// END EPIC MOD: Fix memory leak
};

using NvStripInfoVec = std::vector<NvStripInfo*>;


//The actual stripifier
class NvStripifier {
public:
	
	// Constructor
	NvStripifier();
	~NvStripifier();
	
	//the target vertex cache size, the structure to place the strips in, and the input indices
	void Stripify(const UIntVec &in_indices, const int in_cacheSize, const size_t in_minStripLength, 
				  const size_t maxIndex, NvStripInfoVec &allStrips, NvFaceInfoVec &allFaces);
	void CreateStrips(const NvStripInfoVec& allStrips, IntVec& stripIndices, const bool bStitchStrips, size_t& numSeparateStrips);
	
	static int GetUniqueVertexInB(NvFaceInfo *faceA, NvFaceInfo *faceB);
	//static int GetSharedVertex(NvFaceInfo *faceA, NvFaceInfo *faceB);
	static void GetSharedVertices(NvFaceInfo *faceA, NvFaceInfo *faceB, int* vertex0, int* vertex1);

	static bool IsDegenerate(const NvFaceInfo* face);
	
protected:
	
	UIntVec indices;
	int cacheSize;
	size_t minStripLength;
	float meshJump;
	bool bFirstTimeResetPoint;
	
	/////////////////////////////////////////////////////////////////////////////////
	//
	// Big mess of functions called during stripification
	//
	/////////////////////////////////////////////////////////////////////////////////

	//********************
	bool IsCW(NvFaceInfo *faceInfo, int v0, int v1);
	bool NextIsCW(const int numIndices);
	
	bool IsDegenerate(const unsigned int v0, const unsigned int v1, const unsigned int v2);
	
	static int  GetNextIndex(const UIntVec &indices, NvFaceInfo *face);
	static NvEdgeInfo *FindEdgeInfo(const NvEdgeInfoVec &edgeInfos, int v0, int v1);
	static NvFaceInfo *FindOtherFace(NvEdgeInfoVec &edgeInfos, int v0, int v1, const NvFaceInfo *faceInfo);
	NvFaceInfo *FindGoodResetPoint(NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos);
	
	void FindAllStrips(NvStripInfoVec &allStrips, NvFaceInfoVec &allFaceInfos, NvEdgeInfoVec &allEdgeInfos, int numSamples);
	void SplitUpStripsAndOptimize(NvStripInfoVec &allStrips, NvStripInfoVec &outStrips, NvEdgeInfoVec& edgeInfos, NvFaceInfoVec& outFaceList);
	void RemoveSmallStrips(NvStripInfoVec& allStrips, NvStripInfoVec& allBigStrips, NvFaceInfoVec& faceList);
	
	bool FindTraversal(NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos, NvStripInfo *strip, NvStripStartInfo &startInfo);
	
	void CommitStrips(NvStripInfoVec &allStrips, const NvStripInfoVec &strips);
	
	float AvgStripSize(const NvStripInfoVec &strips);
	std::ptrdiff_t FindStartPoint(const NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos);
	
	void UpdateCacheStrip(VertexCache* vcache, NvStripInfo* strip);
	void UpdateCacheFace(VertexCache* vcache, NvFaceInfo* face);
	float CalcNumHitsStrip(VertexCache* vcache, NvStripInfo* strip);
	int CalcNumHitsFace(VertexCache* vcache, NvFaceInfo* face);
	int NumNeighbors(const NvFaceInfo* face, NvEdgeInfoVec& edgeInfoVec);
	
	void BuildStripifyInfo(NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos, const size_t maxIndex);
	bool AlreadyExists(NvFaceInfo* faceInfo, const NvFaceInfoVec& faceInfos);
	
	// let our strip info classes and the other classes get
	// to these protected stripificaton methods if they want
	friend class NvStripInfo;
};

}  // namespace nv::tristrip::internal

#endif
