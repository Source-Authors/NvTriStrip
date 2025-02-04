#include "NvTriStripObjects.h"

#include "VertexCache.h"

#include <cassert>
#include <cstddef>
#include <cstdio>

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <set>

namespace nv::tristrip::internal {

constexpr inline int CACHE_INEFFICIENCY{6};

NvStripifier::NvStripifier()
{
  cacheSize = 0;
  minStripLength = 0;
  meshJump = 0;
  bFirstTimeResetPoint = false;
}

NvStripifier::~NvStripifier() = default;

///////////////////////////////////////////////////////////////////////////////////////////
// FindEdgeInfo()
//
// find the edge info for these two indices
//
NvEdgeInfo * NvStripifier::FindEdgeInfo(const NvEdgeInfoVec &edgeInfos, int v0, int v1){
	
	// we can get to it through either array
	// because the edge infos have a v0 and v1
	// and there is no order except how it was
	// first created.
	NvEdgeInfo *infoIter = edgeInfos[v0];
	while (infoIter != nullptr){
		if (infoIter->m_v0 == v0){
			if (infoIter->m_v1 == v1)
				return infoIter;

			infoIter = infoIter->m_nextV0;
		}
		else {
			assert(infoIter->m_v1 == v0);
			if (infoIter->m_v0 == v1)
				return infoIter;

			infoIter = infoIter->m_nextV1;
		}
	}
	return nullptr;
}


///////////////////////////////////////////////////////////////////////////////////////////
// FindOtherFace
//
// find the other face sharing these vertices
// exactly like the edge info above
//
NvFaceInfo * NvStripifier::FindOtherFace(NvEdgeInfoVec &edgeInfos, int v0, int v1, const NvFaceInfo *faceInfo){
	NvEdgeInfo *edgeInfo = FindEdgeInfo(edgeInfos, v0, v1);

	if( (edgeInfo == nullptr) && (v0 == v1))
	{
		//we've hit a degenerate
		return nullptr;
	}

	assert(edgeInfo != nullptr);
	return (edgeInfo->m_face0 == faceInfo ? edgeInfo->m_face1 : edgeInfo->m_face0);
}


bool NvStripifier::AlreadyExists(NvFaceInfo* faceInfo, const NvFaceInfoVec& faceInfos)
{
	return std::any_of(std::begin(faceInfos),
		std::end(faceInfos),
		[faceInfo](const NvFaceInfo* f) noexcept {
			return
				(f->m_v0 == faceInfo->m_v0) &&
				(f->m_v1 == faceInfo->m_v1) &&
				(f->m_v2 == faceInfo->m_v2);
		});
}

///////////////////////////////////////////////////////////////////////////////////////////
// BuildStripifyInfo()
//
// Builds the list of all face and edge infos
//
void NvStripifier::BuildStripifyInfo(NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos,
									 const size_t maxIndex)
{
	// reserve space for the face infos, but do not resize them.
	size_t numIndices = indices.size();
	faceInfos.reserve(numIndices / 3);
	
	// we actually resize the edge infos, so we must initialize to nullptr
	edgeInfos.resize(maxIndex + 1);
	for (size_t i = 0; i < maxIndex + 1; i++)
		edgeInfos[i] = nullptr;
	
	// iterate through the triangles of the triangle list
	size_t numTriangles = numIndices / 3;
	size_t index        = 0;
	bool bFaceUpdated[3];

	for (size_t i = 0; i < numTriangles; i++)
	{
		bool bMightAlreadyExist = true;
		bFaceUpdated[0] = false;
		bFaceUpdated[1] = false;
		bFaceUpdated[2] = false;

		// grab the indices
		unsigned int v0 = indices[index++];
		unsigned int v1 = indices[index++];
		unsigned int v2 = indices[index++];

		//we disregard degenerates
		if(IsDegenerate(v0, v1, v2))
			continue;
		
		// create the face info and add it to the list of faces, but only if this exact face doesn't already 
		//  exist in the list
		NvFaceInfo *faceInfo = new NvFaceInfo(v0, v1, v2);

		// grab the edge infos, creating them if they do not already exist
		NvEdgeInfo *edgeInfo01 = FindEdgeInfo(edgeInfos, v0, v1);
		if (edgeInfo01 == nullptr)
		{
			//since one of it's edges isn't in the edge data structure, it can't already exist in the face structure
			bMightAlreadyExist = false;

			// create the info
			edgeInfo01 = new NvEdgeInfo(v0, v1);
			
			// update the linked list on both 
			edgeInfo01->m_nextV0 = edgeInfos[v0];
			edgeInfo01->m_nextV1 = edgeInfos[v1];
			edgeInfos[v0] = edgeInfo01;
			edgeInfos[v1] = edgeInfo01;
			
			// set face 0
			edgeInfo01->m_face0 = faceInfo;
		}
		else 
		{
			if (edgeInfo01->m_face1 != nullptr)
			{
				fprintf(stderr, "BuildStripifyInfo: > 2 triangles on an edge... uncertain consequences\n");
			}
			else
			{
				edgeInfo01->m_face1 = faceInfo;
				bFaceUpdated[0] = true;
			}
		}
		
		// grab the edge infos, creating them if they do not already exist
		NvEdgeInfo *edgeInfo12 = FindEdgeInfo(edgeInfos, v1, v2);
		if (edgeInfo12 == nullptr)
		{
			bMightAlreadyExist = false;
			
			// create the info
			edgeInfo12 = new NvEdgeInfo(v1, v2);
			
			// update the linked list on both 
			edgeInfo12->m_nextV0 = edgeInfos[v1];
			edgeInfo12->m_nextV1 = edgeInfos[v2];
			edgeInfos[v1] = edgeInfo12;
			edgeInfos[v2] = edgeInfo12;
			
			// set face 0
			edgeInfo12->m_face0 = faceInfo;
		}
		else 
		{
			if (edgeInfo12->m_face1 != nullptr)
			{
				fprintf(stderr, "BuildStripifyInfo: > 2 triangles on an edge... uncertain consequences\n");
			}
			else
			{
				edgeInfo12->m_face1 = faceInfo;
				bFaceUpdated[1] = true;
			}
		}
		
		// grab the edge infos, creating them if they do not already exist
		NvEdgeInfo *edgeInfo20 = FindEdgeInfo(edgeInfos, v2, v0);
		if (edgeInfo20 == nullptr)
		{
			bMightAlreadyExist = false;

			// create the info
			edgeInfo20 = new NvEdgeInfo(v2, v0);
			
			// update the linked list on both 
			edgeInfo20->m_nextV0 = edgeInfos[v2];
			edgeInfo20->m_nextV1 = edgeInfos[v0];
			edgeInfos[v2] = edgeInfo20;
			edgeInfos[v0] = edgeInfo20;
			
			// set face 0
			edgeInfo20->m_face0 = faceInfo;
		}
		else 
		{
			if (edgeInfo20->m_face1 != nullptr)
			{
				fprintf(stderr, "BuildStripifyInfo: > 2 triangles on an edge... uncertain consequences\n");
			}
			else
			{
				edgeInfo20->m_face1 = faceInfo;
				bFaceUpdated[2] = true;
			}
		}

		if(bMightAlreadyExist)
		{
			if(!AlreadyExists(faceInfo, faceInfos))
				faceInfos.emplace_back(faceInfo);
			else
			{
				delete faceInfo;

				//cleanup pointers that point to this deleted face
				if(bFaceUpdated[0])
					edgeInfo01->m_face1 = nullptr;
				if(bFaceUpdated[1])
					edgeInfo12->m_face1 = nullptr;
				if(bFaceUpdated[2])
					edgeInfo20->m_face1 = nullptr;
			}
		}
		else
		{
			faceInfos.emplace_back(faceInfo);
		}

	}
}


///////////////////////////////////////////////////////////////////////////////////////////
// FindStartPoint()
//
// Finds a good starting point, namely one which has only one neighbor
//
std::ptrdiff_t NvStripifier::FindStartPoint(const NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos)
{
	int bestCtr = -1;
	std::ptrdiff_t bestIndex = -1, i = 0;

	for(auto &f : faceInfos)
	{
		int ctr = 0;
		
		if(FindOtherFace(edgeInfos, f->m_v0, f->m_v1, f) == nullptr)
			ctr++;
		if(FindOtherFace(edgeInfos, f->m_v1, f->m_v2, f) == nullptr)
			ctr++;
		if(FindOtherFace(edgeInfos, f->m_v2, f->m_v0, f) == nullptr)
			ctr++;

		if(ctr > bestCtr)
		{
			bestCtr = ctr;
			bestIndex = i;
		}

		++i;
	}
	
	if(bestCtr == 0)
		return -1;
	else
		return bestIndex;
}


///////////////////////////////////////////////////////////////////////////////////////////
// FindGoodResetPoint()
//  
// A good reset point is one near other commited areas so that
// we know that when we've made the longest strips its because
// we're stripifying in the same general orientation.
//
NvFaceInfo* NvStripifier::FindGoodResetPoint(NvFaceInfoVec &faceInfos, NvEdgeInfoVec &edgeInfos){
	// we hop into different areas of the mesh to try to get
	// other large open spans done.  Areas of small strips can
	// just be left to triangle lists added at the end.
	NvFaceInfo *result = nullptr;
	
	{
		size_t numFaces   = faceInfos.size();
		std::ptrdiff_t startPoint;
		if(bFirstTimeResetPoint)
		{
			//first time, find a face with few neighbors (look for an edge of the mesh)
			startPoint = FindStartPoint(faceInfos, edgeInfos);
			bFirstTimeResetPoint = false;
		}
		else
// START EPIC MOD: Fix incorrect bracket placement
			startPoint = (std::ptrdiff_t)(float( numFaces - 1 ) * meshJump );
// END EPIC MOD: Fix incorrect bracket placement
		
		if(startPoint == -1)
		{
// START EPIC MOD: Fix incorrect bracket placement
			startPoint = (std::ptrdiff_t)(float( numFaces - 1 ) * meshJump );
// END EPIC MOD: Fix incorrect bracket placement
			
			//meshJump += 0.1f;
			//if (meshJump > 1.0f)
			//	meshJump = .05f;
		}

		std::ptrdiff_t i = startPoint;
		do {
			
			// if this guy isn't visited, try him
			if (faceInfos[i]->m_stripId < 0){
				result = faceInfos[i];
				break;
			}
			
			// update the index and clamp to 0-(numFaces-1)
			if (static_cast<size_t>(++i) >= numFaces)
				i = 0;
			
		} while (i != startPoint);
		
		// update the meshJump
		meshJump += 0.1f;
		if (meshJump > 1.0f)
			meshJump = .05f;
	}
	
	// return the best face we found
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////
// GetUniqueVertexInB()
//
// Returns the vertex unique to faceB
//
int NvStripifier::GetUniqueVertexInB(NvFaceInfo *faceA, NvFaceInfo *faceB){
	
	int facev0 = faceB->m_v0;
	if (facev0 != faceA->m_v0 &&
		facev0 != faceA->m_v1 &&
		facev0 != faceA->m_v2)
		return facev0;
	
	int facev1 = faceB->m_v1;
	if (facev1 != faceA->m_v0 &&
		facev1 != faceA->m_v1 &&
		facev1 != faceA->m_v2)
		return facev1;
	
	int facev2 = faceB->m_v2;
	if (facev2 != faceA->m_v0 &&
		facev2 != faceA->m_v1 &&
		facev2 != faceA->m_v2)
		return facev2;
	
	// nothing is different
	return -1;
}


///////////////////////////////////////////////////////////////////////////////////////////
// GetSharedVertices()
//
// Returns the (at most) two vertices shared between the two faces
//
void NvStripifier::GetSharedVertices(NvFaceInfo *faceA, NvFaceInfo *faceB, int* vertex0, int* vertex1)
{
	*vertex0 = -1;
	*vertex1 = -1;

	int facev0 = faceB->m_v0;
	if (facev0 == faceA->m_v0 ||
		facev0 == faceA->m_v1 ||
		facev0 == faceA->m_v2)
	{
		if(*vertex0 == -1)
			*vertex0 = facev0;
	}
	
	int facev1 = faceB->m_v1;
	if (facev1 == faceA->m_v0 ||
		facev1 == faceA->m_v1 ||
		facev1 == faceA->m_v2)
	{
		if(*vertex0 == -1)
			*vertex0 = facev1;
		else
		{
			*vertex1 = facev1;
			return;
		}
	}	

	int facev2 = faceB->m_v2;
	if (facev2 == faceA->m_v0 ||
		facev2 == faceA->m_v1 ||
		facev2 == faceA->m_v2)
	{
		if(*vertex0 == -1)
			*vertex0 = facev2;
		else
		{
			*vertex1 = facev2;
			return;
		}
	}

}


///////////////////////////////////////////////////////////////////////////////////////////
// GetNextIndex()
//
// Returns vertex of the input face which is "next" in the input index list
//
inline int NvStripifier::GetNextIndex(const UIntVec &indices, NvFaceInfo *face){
	
	size_t numIndices = indices.size();
	assert(numIndices >= 2);
	
	int v0  = indices[numIndices-2];
	int v1  = indices[numIndices-1];
	
	int fv0 = face->m_v0;
	int fv1 = face->m_v1;
	int fv2 = face->m_v2;
	
	if (fv0 != v0 && fv0 != v1){
		if ((fv1 != v0 && fv1 != v1) || (fv2 != v0 && fv2 != v1)){
			fprintf(stderr, "GetNextIndex: Triangle doesn't have all of its vertices\n");
			fprintf(stderr, "GetNextIndex: Duplicate triangle probably got us derailed\n");
		}
		return fv0;
	}
	if (fv1 != v0 && fv1 != v1){
		if ((fv0 != v0 && fv0 != v1) || (fv2 != v0 && fv2 != v1)){
			fprintf(stderr, "GetNextIndex: Triangle doesn't have all of its vertices\n");
			fprintf(stderr, "GetNextIndex: Duplicate triangle probably got us derailed\n");
		}
		return fv1;
	}
	if (fv2 != v0 && fv2 != v1){
		if ((fv0 != v0 && fv0 != v1) || (fv1 != v0 && fv1 != v1)){
			fprintf(stderr, "GetNextIndex: Triangle doesn't have all of its vertices\n");
			fprintf(stderr, "GetNextIndex: Duplicate triangle probably got us derailed\n");
		}
		return fv2;
	}
	
	// shouldn't get here, but let's try and fail gracefully
	if( (fv0 == fv1) || (fv0 == fv2) )
		return fv0;
	else if( (fv1 == fv0) || (fv1 == fv2) )
		return fv1;
	else if( (fv2 == fv0) || (fv2 == fv1) )
		return fv2;
	else
		return -1;
}


///////////////////////////////////////////////////////////////////////////////////////////
// IsMarked()
//
// If either the faceInfo has a real strip index because it is
// already assign to a committed strip OR it is assigned in an
// experiment and the experiment index is the one we are building
// for, then it is marked and unavailable
inline bool NvStripInfo::IsMarked(NvFaceInfo *faceInfo) const {
	return (faceInfo->m_stripId >= 0) || (IsExperiment() && faceInfo->m_experimentId == m_experimentId);
}


///////////////////////////////////////////////////////////////////////////////////////////
// MarkTriangle()
//
// Marks the face with the current strip ID
//
inline void NvStripInfo::MarkTriangle(NvFaceInfo *faceInfo){
	assert(!IsMarked(faceInfo));
	if (IsExperiment()){
		faceInfo->m_experimentId = m_experimentId;
		faceInfo->m_testStripId  = m_stripId;
    }
	else{
		assert(faceInfo->m_stripId == -1);
		faceInfo->m_experimentId = -1;
		faceInfo->m_stripId      = m_stripId;
	}
}


bool NvStripInfo::Unique(const NvFaceInfoVec& faceVec, NvFaceInfo* face) const
{
	bool bv0, bv1, bv2; //bools to indicate whether a vertex is in the faceVec or not
	bv0 = bv1 = bv2 = false;

	for(auto &f : faceVec)
	{
		if(!bv0)
		{
			if( (f->m_v0 == face->m_v0) || 
				(f->m_v1 == face->m_v0) ||
				(f->m_v2 == face->m_v0) )
				bv0 = true;
		}

		if(!bv1)
		{
			if( (f->m_v0 == face->m_v1) || 
				(f->m_v1 == face->m_v1) ||
				(f->m_v2 == face->m_v1) )
				bv1 = true;
		}

		if(!bv2)
		{
			if( (f->m_v0 == face->m_v2) || 
				(f->m_v1 == face->m_v2) ||
				(f->m_v2 == face->m_v2) )
				bv2 = true;
		}

		//the face is not unique, all it's vertices exist in the face vector
		if(bv0 && bv1 && bv2)
			return false;
	}
	
	//if we get out here, it's unique
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////
// Build()
//
// Builds a strip forward as far as we can go, then builds backwards, and joins the two lists
//
void NvStripInfo::Build(NvEdgeInfoVec &edgeInfos, NvFaceInfoVec &)
{
	// used in building the strips forward and backward
	UIntVec scratchIndices;
	
	// build forward... start with the initial face
	NvFaceInfoVec forwardFaces, backwardFaces;
	forwardFaces.emplace_back(m_startInfo.m_startFace);

	MarkTriangle(m_startInfo.m_startFace);
	
	int v0 = (m_startInfo.m_toV1 ? m_startInfo.m_startEdge->m_v0 : m_startInfo.m_startEdge->m_v1);
	int v1 = (m_startInfo.m_toV1 ? m_startInfo.m_startEdge->m_v1 : m_startInfo.m_startEdge->m_v0);
	
	// easiest way to get v2 is to use this function which requires the
	// other indices to already be in the list.
	scratchIndices.emplace_back(v0);
	scratchIndices.emplace_back(v1);
	int v2 = NvStripifier::GetNextIndex(scratchIndices, m_startInfo.m_startFace);
	scratchIndices.emplace_back(v2);

	//
	// build the forward list
	//
	int nv0 = v1;
	int nv1 = v2;

	NvFaceInfo *nextFace = NvStripifier::FindOtherFace(edgeInfos, nv0, nv1, m_startInfo.m_startFace);
	while (nextFace != nullptr && !IsMarked(nextFace))
	{
		//check to see if this next face is going to cause us to die soon
		int testnv0 = nv1;
		int testnv1 = NvStripifier::GetNextIndex(scratchIndices, nextFace);
		
		NvFaceInfo* nextNextFace = NvStripifier::FindOtherFace(edgeInfos, testnv0, testnv1, nextFace);

		if( (nextNextFace == nullptr) || (IsMarked(nextNextFace)) )
		{
			//uh, oh, we're following a dead end, try swapping
			NvFaceInfo* testNextFace = NvStripifier::FindOtherFace(edgeInfos, nv0, testnv1, nextFace);

			if( ((testNextFace != nullptr) && !IsMarked(testNextFace)) )
			{
				//we only swap if it buys us something
				
				//add a "fake" degenerate face
				NvFaceInfo* tempFace = new NvFaceInfo(nv0, nv1, nv0);
				
// START EPIC MOD: Fix memory leak
				// We need to store this pointer to avoid a memory leak from the experiments that didn't make it. Only the experiment
				// that passes has its degenerates deleted. Add the pointer to the member vector for this StripInfo
				m_Degenerates.emplace_back( tempFace );
// END EPIC MOD: Fix memory leak

				forwardFaces.emplace_back(tempFace);
				MarkTriangle(tempFace);

				scratchIndices.emplace_back(nv0);
				testnv0 = nv0;

				++m_numDegenerates;
			}

		}

		// add this to the strip
// EPIC NOTE: Is this correct? If we've just pushed back tempFace then surely this is wrong...
		forwardFaces.emplace_back(nextFace);

		MarkTriangle(nextFace);
		
		// add the index
		//nv0 = nv1;
		//nv1 = NvStripifier::GetNextIndex(scratchIndices, nextFace);
// EPIC NOTE: This is OK in either case though...
		scratchIndices.emplace_back(testnv1);
		
		// and get the next face
// EPIC NOTE: As is this...
		nv0 = testnv0;
		nv1 = testnv1;

		nextFace = NvStripifier::FindOtherFace(edgeInfos, nv0, nv1, nextFace);
	
	}
	
	// tempAllFaces is going to be forwardFaces + backwardFaces
	// it's used for Unique()
	NvFaceInfoVec tempAllFaces;
	std::copy(std::begin(forwardFaces), std::end(forwardFaces), std::back_inserter(tempAllFaces));

	//
	// reset the indices for building the strip backwards and do so
	//
	scratchIndices.resize(0);
	scratchIndices.emplace_back(v2);
	scratchIndices.emplace_back(v1);
	scratchIndices.emplace_back(v0);
	nv0 = v1;
	nv1 = v0;
	nextFace = NvStripifier::FindOtherFace(edgeInfos, nv0, nv1, m_startInfo.m_startFace);
	while (nextFace != nullptr && !IsMarked(nextFace))
	{
		//this tests to see if a face is "unique", meaning that its vertices aren't already in the list
		// so, strips which "wrap-around" are not allowed
		if(!Unique(tempAllFaces, nextFace))
			break;

		//check to see if this next face is going to cause us to die soon
		int testnv0 = nv1;
		int testnv1 = NvStripifier::GetNextIndex(scratchIndices, nextFace);
		
		NvFaceInfo* nextNextFace = NvStripifier::FindOtherFace(edgeInfos, testnv0, testnv1, nextFace);

		if( (nextNextFace == nullptr) || (IsMarked(nextNextFace)) )
		{
			//uh, oh, we're following a dead end, try swapping
			NvFaceInfo* testNextFace = NvStripifier::FindOtherFace(edgeInfos, nv0, testnv1, nextFace);
			if( ((testNextFace != nullptr) && !IsMarked(testNextFace)) )
			{
				//we only swap if it buys us something
				
				//add a "fake" degenerate face
				NvFaceInfo* tempFace = new NvFaceInfo(nv0, nv1, nv0);

// START EPIC MOD: Fix memory leak
				// We need to store this pointer to avoid a memory leak from the experiments that didn't make it. Only the experiment
				// that passes has its degenerates deleted. Add the pointer to the member vector for this StripInfo - Paul Fotheringham
				m_Degenerates.emplace_back( tempFace );
// END EPIC MOD: Fix memory leak

				backwardFaces.emplace_back(tempFace);
				MarkTriangle(tempFace);
				scratchIndices.emplace_back(nv0);
				testnv0 = nv0;

				++m_numDegenerates;
			}
				
		}

		// add this to the strip
		backwardFaces.emplace_back(nextFace);
		
		//this is just so Unique() will work
		tempAllFaces.emplace_back(nextFace);

		MarkTriangle(nextFace);
		
		// add the index
		//nv0 = nv1;
		//nv1 = NvStripifier::GetNextIndex(scratchIndices, nextFace);
		scratchIndices.emplace_back(testnv1);
		
		// and get the next face
		nv0 = testnv0;
		nv1 = testnv1;
		nextFace = NvStripifier::FindOtherFace(edgeInfos, nv0, nv1, nextFace);
	}
	
	// Combine the forward and backwards stripification lists and put into our own face vector
	Combine(forwardFaces, backwardFaces);
}


///////////////////////////////////////////////////////////////////////////////////////////
// Combine()
//
// Combines the two input face vectors and puts the result into m_faces
//
void NvStripInfo::Combine(const NvFaceInfoVec &forward, const NvFaceInfoVec &backward){
	
	// add backward faces
	ptrdiff_t numFaces = (ptrdiff_t)backward.size();
	for (ptrdiff_t i = numFaces - 1; i >= 0; i--)
		m_faces.emplace_back(backward[i]);
	
	// add forward faces
	for (auto &f : forward)
		m_faces.emplace_back(f);
}


///////////////////////////////////////////////////////////////////////////////////////////
// SharesEdge()
//
// Returns true if the input face and the current strip share an edge
//
bool NvStripInfo::SharesEdge(const NvFaceInfo* faceInfo, NvEdgeInfoVec &edgeInfos) const
{
	//check v0->v1 edge
	NvEdgeInfo* currEdge = NvStripifier::FindEdgeInfo(edgeInfos, faceInfo->m_v0, faceInfo->m_v1);
	
	if(IsInStrip(currEdge->m_face0) || IsInStrip(currEdge->m_face1))
		return true;
	
	//check v1->v2 edge
	currEdge = NvStripifier::FindEdgeInfo(edgeInfos, faceInfo->m_v1, faceInfo->m_v2);
	
	if(IsInStrip(currEdge->m_face0) || IsInStrip(currEdge->m_face1))
		return true;
	
	//check v2->v0 edge
	currEdge = NvStripifier::FindEdgeInfo(edgeInfos, faceInfo->m_v2, faceInfo->m_v0);
	
	if(IsInStrip(currEdge->m_face0) || IsInStrip(currEdge->m_face1))
		return true;
	
	return false;
	
}


///////////////////////////////////////////////////////////////////////////////////////////
// CommitStrips()
//
// "Commits" the input strips by setting their m_experimentId to -1 and adding to the allStrips
//  vector
//
void NvStripifier::CommitStrips(NvStripInfoVec &allStrips, const NvStripInfoVec &strips)
{
	// Iterate through strips
	for (auto *strip : strips){

		// Tell the strip that it is now real
		strip->m_experimentId = -1;
		
		// add to the list of real strips
		allStrips.emplace_back(strip);
		
		// Iterate through the faces of the strip
		// Tell the faces of the strip that they belong to a real strip now
		for (auto &f : strip->m_faces)
		{
			strip->MarkTriangle(f);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////
// FindTraversal()
//
// Finds the next face to start the next strip on.
//
bool NvStripifier::FindTraversal(NvFaceInfoVec    &,
								 NvEdgeInfoVec    &edgeInfos,
								 NvStripInfo      *strip,
								 NvStripStartInfo &startInfo){
	
	// if the strip was v0->v1 on the edge, then v1 will be a vertex in the next edge.
	int v = (strip->m_startInfo.m_toV1 ? strip->m_startInfo.m_startEdge->m_v1 : strip->m_startInfo.m_startEdge->m_v0);
	
	NvFaceInfo *untouchedFace = nullptr;
	NvEdgeInfo *edgeIter      = edgeInfos[v];
	while (edgeIter != nullptr){
		NvFaceInfo *face0 = edgeIter->m_face0;
		NvFaceInfo *face1 = edgeIter->m_face1;
		if ((face0 != nullptr && !strip->IsInStrip(face0)) && face1 != nullptr && !strip->IsMarked(face1))
		{
			untouchedFace = face1;
			break;
		}
		if ((face1 != nullptr && !strip->IsInStrip(face1)) && face0 != nullptr && !strip->IsMarked(face0)){
			untouchedFace = face0;
			break;
		}
		
		// find the next edgeIter
		edgeIter = (edgeIter->m_v0 == v ? edgeIter->m_nextV0 : edgeIter->m_nextV1);
	}
	
	startInfo.m_startFace = untouchedFace;
	startInfo.m_startEdge = edgeIter;
	if (edgeIter != nullptr)
	{
		if(strip->SharesEdge(startInfo.m_startFace, edgeInfos))
			startInfo.m_toV1 = (edgeIter->m_v0 == v);  //note! used to be m_v1
		else
			startInfo.m_toV1 = (edgeIter->m_v1 == v);
	}
	return (startInfo.m_startFace != nullptr);
}


////////////////////////////////////////////////////////////////////////////////////////
// RemoveSmallStrips()
//
// allStrips is the whole strip vector...all small strips will be deleted from this list, to avoid leaking mem
// allBigStrips is an out parameter which will contain all strips above minStripLength
// faceList is an out parameter which will contain all faces which were removed from the striplist
//
void NvStripifier::RemoveSmallStrips(NvStripInfoVec& allStrips, NvStripInfoVec& allBigStrips, NvFaceInfoVec& faceList)
{
	faceList.clear();
	allBigStrips.clear();  //make sure these are empty
	NvFaceInfoVec tempFaceList;
	
	for(auto &as : allStrips)
	{
		if(as->m_faces.size() < minStripLength)
		{
			//strip is too small, add faces to faceList
			for(auto &f : as->m_faces)
				tempFaceList.emplace_back(f);
			
			//and free memory
			delete as;
		}
		else
		{
			allBigStrips.emplace_back(as);
		}
	}
	
	if(tempFaceList.size())
	{
		std::unique_ptr<bool[]> bVisitedList = std::make_unique<bool[]>(tempFaceList.size());
		memset(bVisitedList.get(), 0, tempFaceList.size()*sizeof(bool));

		std::unique_ptr<VertexCache> vcache = std::make_unique<VertexCache>(cacheSize);

		int numHits;
		std::ptrdiff_t bestIndex = 0;

		while(1)
		{
			int bestNumHits = -1;

			//find best face to add next, given the current cache
			for(std::ptrdiff_t i = 0; i < static_cast<ptrdiff_t>(tempFaceList.size()); i++)
			{
				if(bVisitedList[i])
					continue;

				numHits = CalcNumHitsFace(vcache.get(), tempFaceList[i]);
				if(numHits > bestNumHits)
				{
					bestNumHits = numHits;
					bestIndex = i;
				}
			}

			if(bestNumHits == -1)
				break;

			bVisitedList[bestIndex] = true;

			UpdateCacheFace(vcache.get(), tempFaceList[bestIndex]);

			faceList.emplace_back(tempFaceList[bestIndex]);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////
// NextIsCW()
//
// Returns true if the next face should be ordered in CW fashion
//
bool NvStripifier::NextIsCW(const int numIndices)
{
	return ((numIndices % 2) == 0);
}


///////////////////////////////////////////////////////////////////////////////////////////
// IsCW()
//
// Returns true if the face is ordered in CW fashion
//
bool NvStripifier::IsCW(NvFaceInfo *faceInfo, int v0, int v1)
{
	if (faceInfo->m_v0 == v0)
		return (faceInfo->m_v1 == v1);
	if (faceInfo->m_v1 == v0)
		return (faceInfo->m_v2 == v1);
	
	return (faceInfo->m_v0 == v1);
}

////////////////////////////////////////////////////////////////////////////////////////
// CreateStrips()
//
// Generates actual strips from the list-in-strip-order.
//
void NvStripifier::CreateStrips(const NvStripInfoVec& allStrips, IntVec& stripIndices, 
								const bool bStitchStrips, size_t& numSeparateStrips)
{
	assert(numSeparateStrips == 0);

	NvFaceInfo tLastFace(0, 0, 0);
	NvFaceInfo tPrevStripLastFace(0, 0, 0);
	size_t nStripCount = allStrips.size();
	assert(nStripCount > 0);

	//we infer the cw/ccw ordering depending on the number of indices
	//this is screwed up by the fact that we insert -1s to denote changing strips
	//this is to account for that
	int accountForNegatives = 0;

	for (size_t i = 0; i < nStripCount; i++)
	{
		NvStripInfo *strip = allStrips[i];
		size_t nStripFaceCount = strip->m_faces.size();
		assert(nStripFaceCount > 0);

		// Handle the first face in the strip
		{
			NvFaceInfo tFirstFace(strip->m_faces[0]->m_v0, strip->m_faces[0]->m_v1, strip->m_faces[0]->m_v2);

			// If there is a second face, reorder vertices such that the
			// unique vertex is first
			if (nStripFaceCount > 1)
			{
				int nUnique = NvStripifier::GetUniqueVertexInB(strip->m_faces[1], &tFirstFace);
				if (nUnique == tFirstFace.m_v1)
				{
					std::swap(tFirstFace.m_v0, tFirstFace.m_v1);
				}
				else if (nUnique == tFirstFace.m_v2)
				{
					std::swap(tFirstFace.m_v0, tFirstFace.m_v2);
				}

				// If there is a third face, reorder vertices such that the
				// shared vertex is last
				if (nStripFaceCount > 2)
				{
					if(IsDegenerate(strip->m_faces[1]))
					{
						int pivot = strip->m_faces[1]->m_v1;
						if(tFirstFace.m_v1 == pivot)
						{
							std::swap(tFirstFace.m_v1, tFirstFace.m_v2);
						}
					}
					else
					{
						int nShared0, nShared1;
						GetSharedVertices(strip->m_faces[2], &tFirstFace, &nShared0, &nShared1);
						if ( (nShared0 == tFirstFace.m_v1) && (nShared1 == -1) )
						{
							std::swap(tFirstFace.m_v1, tFirstFace.m_v2);
						}
					}
				}
			}

			if( (i == 0) || !bStitchStrips)
			{
				if(!IsCW(strip->m_faces[0], tFirstFace.m_v0, tFirstFace.m_v1))
					stripIndices.emplace_back(tFirstFace.m_v0);
			}
			else
			{
				// Double tap the first in the new strip
				stripIndices.emplace_back(tFirstFace.m_v0);
	
				// Check CW/CCW ordering
				if (NextIsCW((int)(stripIndices.size()) - accountForNegatives) != IsCW(strip->m_faces[0], tFirstFace.m_v0, tFirstFace.m_v1))
				{
					stripIndices.emplace_back(tFirstFace.m_v0);
				}
			}

			stripIndices.emplace_back(tFirstFace.m_v0);
			stripIndices.emplace_back(tFirstFace.m_v1);
			stripIndices.emplace_back(tFirstFace.m_v2);

			// Update last face info
			tLastFace = tFirstFace;
		}

		for (size_t j = 1; j < nStripFaceCount; j++)
		{
			int nUnique = GetUniqueVertexInB(&tLastFace, strip->m_faces[j]);
			if (nUnique != -1)
			{
				stripIndices.emplace_back(nUnique);

				// Update last face info
				tLastFace.m_v0 = tLastFace.m_v1;
				tLastFace.m_v1 = tLastFace.m_v2;
				tLastFace.m_v2 = nUnique;
			}
			else
			{
				//we've hit a degenerate
				stripIndices.emplace_back(strip->m_faces[j]->m_v2);
				tLastFace.m_v0 = strip->m_faces[j]->m_v0;//tLastFace.m_v1;
				tLastFace.m_v1 = strip->m_faces[j]->m_v1;//tLastFace.m_v2;
				tLastFace.m_v2 = strip->m_faces[j]->m_v2;//tLastFace.m_v1;

			}
		}

		// Double tap between strips.
		if(bStitchStrips)
		{
			if(i != nStripCount - 1)
				stripIndices.emplace_back(tLastFace.m_v2);
		}
		else
		{
			//-1 index indicates next strip
			stripIndices.emplace_back(-1);
			accountForNegatives++;
			numSeparateStrips++;
		}

		// Update last face info
		tLastFace.m_v0 = tLastFace.m_v1;
		tLastFace.m_v1 = tLastFace.m_v2;
		tLastFace.m_v2 = tLastFace.m_v2;
	}
	
	if(bStitchStrips)
		numSeparateStrips = 1;
}


///////////////////////////////////////////////////////////////////////////////////////////
// Stripify()
//
//
// in_indices are the input indices of the mesh to stripify
// in_cacheSize is the target cache size 
//
void NvStripifier::Stripify(const UIntVec &in_indices, const int in_cacheSize, 
							const size_t in_minStripLength, const size_t maxIndex, 
							NvStripInfoVec &outStrips, NvFaceInfoVec& outFaceList)
{
	meshJump = 0.0f;
	bFirstTimeResetPoint = true; //used in FindGoodResetPoint()

	//the number of times to run the experiments
	constexpr int numSamples = 10;
	
	//the cache size, clamped to one
	cacheSize = std::max(1, in_cacheSize - CACHE_INEFFICIENCY);
	
	minStripLength = in_minStripLength;  //this is the strip size threshold below which we dump the strip into a list
	
	indices = in_indices;
	
	// build the stripification info
	NvFaceInfoVec allFaceInfos;
	NvEdgeInfoVec allEdgeInfos;
	
	BuildStripifyInfo(allFaceInfos, allEdgeInfos, maxIndex);
	
	NvStripInfoVec allStrips;

	// stripify
	FindAllStrips(allStrips, allFaceInfos, allEdgeInfos, numSamples);
	
	//split up the strips into cache friendly pieces, optimize them, then dump these into outStrips
	SplitUpStripsAndOptimize(allStrips, outStrips, allEdgeInfos, outFaceList);

	//clean up
	for (auto &as : allStrips)
	{
		delete as;
	}
	
	std::ptrdiff_t i = 0;
	for (auto *info : allEdgeInfos)
	{
		while (info != nullptr)
		{
			NvEdgeInfo *next = (info->m_v0 == i ? info->m_nextV0 : info->m_nextV1);
			info->Unref();
			info = next;
		}

		i++;
	}
	
}


bool NvStripifier::IsDegenerate(const NvFaceInfo* face)
{
	if(face->m_v0 == face->m_v1)
		return true;
	if(face->m_v0 == face->m_v2)
		return true;
	if(face->m_v1 == face->m_v2)
		return true;
	return false;
}

bool NvStripifier::IsDegenerate(const unsigned int v0, const unsigned int v1, const unsigned int v2)
{
	if(v0 == v1)
		return true;
	if(v0 == v2)
		return true;
	if(v1 == v2)
		return true;
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////
// SplitUpStripsAndOptimize()
//
// Splits the input vector of strips (allBigStrips) into smaller, cache friendly pieces, then
//  reorders these pieces to maximize cache hits
// The final strips are output through outStrips
//
void NvStripifier::SplitUpStripsAndOptimize(NvStripInfoVec &allStrips, NvStripInfoVec &outStrips,
                                            NvEdgeInfoVec& edgeInfos, NvFaceInfoVec& outFaceList)
{
	int threshold = cacheSize;
	NvStripInfoVec tempStrips;
	
	//split up strips into threshold-sized pieces
	for(auto &as : allStrips)
	{
		NvStripInfo* currentStrip;
		NvStripStartInfo startInfo(nullptr, nullptr, false);
	
		ptrdiff_t actualStripSize = std::count_if(std::begin(as->m_faces),
			std::end(as->m_faces),
			[*this](const NvFaceInfo *f) noexcept
			{
				return !IsDegenerate(f);
			});

		if(actualStripSize > threshold)
		{
			
			ptrdiff_t numTimes = actualStripSize / threshold;
			ptrdiff_t numLeftover = actualStripSize % threshold;

			ptrdiff_t degenerateCount = 0;
			ptrdiff_t j;
			for(j = 0; j < numTimes; j++)
			{
				currentStrip = new NvStripInfo(startInfo, 0, -1);
				
				ptrdiff_t faceCtr = j*threshold + degenerateCount;
				bool bFirstTime = true;
				while(faceCtr < threshold+(j*threshold)+degenerateCount)
				{
					if(IsDegenerate(as->m_faces[faceCtr]))
					{
						degenerateCount++;
						
						//last time or first time through, no need for a degenerate
						if( (((faceCtr + 1) != threshold+(j*threshold)+degenerateCount) ||
							 ((j == numTimes - 1) && (numLeftover < 4) && (numLeftover > 0))) && 
							 !bFirstTime)
						{
							currentStrip->m_faces.emplace_back(as->m_faces[faceCtr++]);
						}
						else
// START EPIC MOD: Fix memory leak
						{
							delete as->m_faces[ faceCtr ];
							++faceCtr;
						}
// END EPIC MOD: Fix memory leak
					}
					else
					{
						currentStrip->m_faces.emplace_back(as->m_faces[faceCtr++]);
						bFirstTime = false;
					}
				}
				///*
				if(j == numTimes - 1) //last time through
				{
					if( (numLeftover < 4) && (numLeftover > 0) ) //way too small
					{
						//just add to last strip
						ptrdiff_t ctr = 0;
						while(ctr < numLeftover)
						{
							if(!IsDegenerate(as->m_faces[faceCtr]))
							{	
								currentStrip->m_faces.emplace_back(as->m_faces[faceCtr++]);
								++ctr;
							}
							else
							{
								currentStrip->m_faces.emplace_back(as->m_faces[faceCtr++]);
								++degenerateCount;
							}
						}
						numLeftover = 0;
					}
				}
				//*/
				tempStrips.emplace_back(currentStrip);
			}
			
			ptrdiff_t leftOff = j * threshold + degenerateCount;
			
			if(numLeftover != 0)
			{
				currentStrip = new NvStripInfo(startInfo, 0, -1);   
				
				ptrdiff_t ctr = 0;
				bool bFirstTime = true;
				while(ctr < numLeftover)
				{
					if( !IsDegenerate(as->m_faces[leftOff]) )
					{
						ctr++;
						bFirstTime = false;
						currentStrip->m_faces.emplace_back(as->m_faces[leftOff++]);
					}
					else if(!bFirstTime)
						currentStrip->m_faces.emplace_back(as->m_faces[leftOff++]);
					else
// START EPIC MOD: Fix memory leak
					{
						delete as->m_faces[ leftOff ];
						++leftOff;
					}
// END EPIC MOD: Fix memory leak
				}
				
				tempStrips.emplace_back(currentStrip);
			}
		}
		else
		{
			//we're not just doing a tempStrips.emplace_back(allBigStrips[i]) because
			// this way we can delete allBigStrips later to free the memory
			currentStrip = new NvStripInfo(startInfo, 0, -1);
			
			for(auto &f : as->m_faces)
				currentStrip->m_faces.emplace_back(f);
			
			tempStrips.emplace_back(currentStrip);
		}
	}

	//add small strips to face list
	NvStripInfoVec tempStrips2;
	RemoveSmallStrips(tempStrips, tempStrips2, outFaceList);
	
	outStrips.clear();
	//screw optimization for now
//	for(i = 0; i < tempStrips.size(); ++i)
//    outStrips.emplace_back(tempStrips[i]);
	
	if(tempStrips2.size() != 0)
	{
		//Optimize for the vertex cache
		VertexCache* vcache = new VertexCache(cacheSize);
		
		float numHits;
		std::ptrdiff_t bestIndex = 0;
		
		size_t firstIndex = 0, j = 0;
		float minCost = 10000.0f;
		
		for(auto &ts : tempStrips2)
		{
			//find strip with least number of neighbors per face
			int numNeighbors = std::accumulate(std::begin(ts->m_faces), std::end(ts->m_faces),
				0,
				[&](int acc, const NvFaceInfo* right) noexcept {
					return acc + NumNeighbors(right, edgeInfos);
				});
			
			float currCost = (float)numNeighbors / (float)ts->m_faces.size();
			if(currCost < minCost)
			{
				minCost = currCost;
				firstIndex = j;
			}

			++j;
		}
		
		UpdateCacheStrip(vcache, tempStrips2[firstIndex]);
		outStrips.emplace_back(tempStrips2[firstIndex]);
		
		tempStrips2[firstIndex]->visited = true;
		
		bool bWantsCW = (tempStrips2[firstIndex]->m_faces.size() % 2) == 0;
		
		float bestNumHits = -1.0f;

		//this n^2 algo is what slows down stripification so much....
		// needs to be improved
		while(1)
		{
			bestNumHits = -1.0f;
			
			//find best strip to add next, given the current cache
			for(std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(tempStrips2.size()); i++)
			{
				if(tempStrips2[i]->visited)
					continue;

				numHits = CalcNumHitsStrip(vcache, tempStrips2[i]);
				if(numHits > bestNumHits)
				{
					bestNumHits = numHits;
					bestIndex = i;
				}
				else if(numHits >= bestNumHits)
				{
					//check previous strip to see if this one requires it to switch polarity
					NvStripInfo *strip = tempStrips2[i];
					size_t nStripFaceCount = strip->m_faces.size();
					
					NvFaceInfo tFirstFace(strip->m_faces[0]->m_v0, strip->m_faces[0]->m_v1, strip->m_faces[0]->m_v2);
					
					// If there is a second face, reorder vertices such that the
					// unique vertex is first
					if (nStripFaceCount > 1)
					{
						int nUnique = NvStripifier::GetUniqueVertexInB(strip->m_faces[1], &tFirstFace);
						if (nUnique == tFirstFace.m_v1)
						{
							std::swap(tFirstFace.m_v0, tFirstFace.m_v1);
						}
						else if (nUnique == tFirstFace.m_v2)
						{
							std::swap(tFirstFace.m_v0, tFirstFace.m_v2);
						}
						
						// If there is a third face, reorder vertices such that the
						// shared vertex is last
						if (nStripFaceCount > 2)
						{
// EPIC NOTE: No degeneracy check here like in CreateStrips???
							int nShared0, nShared1;
							GetSharedVertices(strip->m_faces[2], &tFirstFace, &nShared0, &nShared1);
							if ( (nShared0 == tFirstFace.m_v1) && (nShared1 == -1) )
							{
								std::swap(tFirstFace.m_v1, tFirstFace.m_v2);
							}
						}
					}
						
					// Check CW/CCW ordering
					if (bWantsCW == IsCW(strip->m_faces[0], tFirstFace.m_v0, tFirstFace.m_v1))
					{
						//I like this one!
						bestIndex = i;
					}
				}
			}
		
			if(bestNumHits == -1.0f)
				break;

			tempStrips2[bestIndex]->visited = true;
			UpdateCacheStrip(vcache, tempStrips2[bestIndex]);
			outStrips.emplace_back(tempStrips2[bestIndex]);
			bWantsCW = (tempStrips2[bestIndex]->m_faces.size() % 2 == 0) ? bWantsCW : !bWantsCW;
		}
		
		delete vcache;	
	}
}


///////////////////////////////////////////////////////////////////////////////////////////
// UpdateCacheStrip()
//
// Updates the input vertex cache with this strip's vertices
//
void NvStripifier::UpdateCacheStrip(VertexCache* vcache, NvStripInfo* strip)
{
	for(auto &f : strip->m_faces)
	{
		if(!vcache->InCache(f->m_v0))
			vcache->AddEntry(f->m_v0);
		
		if(!vcache->InCache(f->m_v1))
			vcache->AddEntry(f->m_v1);
		
		if(!vcache->InCache(f->m_v2))
			vcache->AddEntry(f->m_v2);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
// UpdateCacheFace()
//
// Updates the input vertex cache with this face's vertices
//
void NvStripifier::UpdateCacheFace(VertexCache* vcache, NvFaceInfo* face)
{
	if(!vcache->InCache(face->m_v0))
		vcache->AddEntry(face->m_v0);
		
	if(!vcache->InCache(face->m_v1))
		vcache->AddEntry(face->m_v1);
		
	if(!vcache->InCache(face->m_v2))
		vcache->AddEntry(face->m_v2);
}


///////////////////////////////////////////////////////////////////////////////////////////
// CalcNumHitsStrip()
//
// returns the number of cache hits per face in the strip
//
float NvStripifier::CalcNumHitsStrip(VertexCache* vcache, NvStripInfo* strip)
{
	std::ptrdiff_t numHits = 0;
	std::ptrdiff_t numFaces = 0;
	
	for(auto &f : strip->m_faces)
	{
		if(vcache->InCache(f->m_v0))
			++numHits;
		
		if(vcache->InCache(f->m_v1))
			++numHits;
		
		if(vcache->InCache(f->m_v2))
			++numHits;
		
		numFaces++;
	}
	
	return numFaces != 0 ? ((float)numHits / (float)numFaces) : 0;
}


///////////////////////////////////////////////////////////////////////////////////////////
// CalcNumHitsFace()
//
// returns the number of cache hits in the face
//
int NvStripifier::CalcNumHitsFace(VertexCache* vcache, NvFaceInfo* face)
{
	int numHits = 0;

	if(vcache->InCache(face->m_v0))
		numHits++;
		
	if(vcache->InCache(face->m_v1))
		numHits++;
		
	if(vcache->InCache(face->m_v2))
		numHits++;
		
	return numHits;
}


///////////////////////////////////////////////////////////////////////////////////////////
// NumNeighbors()
//
// Returns the number of neighbors that this face has
//
int NvStripifier::NumNeighbors(const NvFaceInfo* face, NvEdgeInfoVec& edgeInfoVec)
{
	int numNeighbors = 0;
	
	if(FindOtherFace(edgeInfoVec, face->m_v0, face->m_v1, face) != nullptr)
	{
		numNeighbors++;
	}
	
	if(FindOtherFace(edgeInfoVec, face->m_v1, face->m_v2, face) != nullptr)
	{
		numNeighbors++;
	}
	
	if(FindOtherFace(edgeInfoVec, face->m_v2, face->m_v0, face) != nullptr)
	{
		numNeighbors++;
	}
	
	return numNeighbors;
}


///////////////////////////////////////////////////////////////////////////////////////////
// AvgStripSize()
//
// Finds the average strip size of the input vector of strips
//
float NvStripifier::AvgStripSize(const NvStripInfoVec &strips){
	std::ptrdiff_t sizeAccum = 0;
	size_t numStrips = strips.size();
	for (auto *strip : strips){
		sizeAccum += strip->m_faces.size();
		sizeAccum -= strip->m_numDegenerates;
	}
	return ((float)sizeAccum) / ((float)numStrips);
}


///////////////////////////////////////////////////////////////////////////////////////////
// FindAllStrips()
//
// Does the stripification, puts output strips into vector allStrips
//
// Works by setting runnning a number of experiments in different areas of the mesh, and
//  accepting the one which results in the longest strips.  It then accepts this, and moves
//  on to a different area of the mesh.  We try to jump around the mesh some, to ensure that
//  large open spans of strips get generated.
//
void NvStripifier::FindAllStrips(NvStripInfoVec &allStrips,
								 NvFaceInfoVec &allFaceInfos,
								 NvEdgeInfoVec &allEdgeInfos,
								 int numSamples){
	// the experiments
	int experimentId = 0;
	int stripId      = 0;
	bool done        = false;

	while (!done)
	{
		//
		// PHASE 1: Set up numSamples * numEdges experiments
		//
		NvStripInfoVec *experiments = new NvStripInfoVec [numSamples * 6];
		int experimentIndex = 0;
		std::set   <NvFaceInfo*>  resetPoints;
		for (int i = 0; i < numSamples; i++)
		{
			
			// Try to find another good reset point.
			// If there are none to be found, we are done
			NvFaceInfo *nextFace = FindGoodResetPoint(allFaceInfos, allEdgeInfos);
			if (nextFace == nullptr){
				done = true;
				break;
			}
			// If we have already evaluated starting at this face in this slew
			// of experiments, then skip going any further
			else if (resetPoints.find(nextFace) != resetPoints.end()){
				continue;
			}
			
			// trying it now...
			resetPoints.insert(nextFace);
			
			// otherwise, we shall now try experiments for starting on the 01,12, and 20 edges
			assert(nextFace->m_stripId < 0);
			
			// build the strip off of this face's 0-1 edge
			NvEdgeInfo *edge01 = FindEdgeInfo(allEdgeInfos, nextFace->m_v0, nextFace->m_v1);
			NvStripInfo *strip01 = new NvStripInfo(NvStripStartInfo(nextFace, edge01, true), stripId++, experimentId++);
			experiments[experimentIndex++].emplace_back(strip01);
			
			// build the strip off of this face's 1-0 edge
			NvEdgeInfo *edge10 = FindEdgeInfo(allEdgeInfos, nextFace->m_v0, nextFace->m_v1);
			NvStripInfo *strip10 = new NvStripInfo(NvStripStartInfo(nextFace, edge10, false), stripId++, experimentId++);
			experiments[experimentIndex++].emplace_back(strip10);
			
			// build the strip off of this face's 1-2 edge
			NvEdgeInfo *edge12 = FindEdgeInfo(allEdgeInfos, nextFace->m_v1, nextFace->m_v2);
			NvStripInfo *strip12 = new NvStripInfo(NvStripStartInfo(nextFace, edge12, true), stripId++, experimentId++);
			experiments[experimentIndex++].emplace_back(strip12);
			
			// build the strip off of this face's 2-1 edge
			NvEdgeInfo *edge21 = FindEdgeInfo(allEdgeInfos, nextFace->m_v1, nextFace->m_v2);
			NvStripInfo *strip21 = new NvStripInfo(NvStripStartInfo(nextFace, edge21, false), stripId++, experimentId++);
			experiments[experimentIndex++].emplace_back(strip21);
			
			// build the strip off of this face's 2-0 edge
			NvEdgeInfo *edge20 = FindEdgeInfo(allEdgeInfos, nextFace->m_v2, nextFace->m_v0);
			NvStripInfo *strip20 = new NvStripInfo(NvStripStartInfo(nextFace, edge20, true), stripId++, experimentId++);
			experiments[experimentIndex++].emplace_back(strip20);
			
			// build the strip off of this face's 0-2 edge
			NvEdgeInfo *edge02 = FindEdgeInfo(allEdgeInfos, nextFace->m_v2, nextFace->m_v0);
			NvStripInfo *strip02 = new NvStripInfo(NvStripStartInfo(nextFace, edge02, false), stripId++, experimentId++);
			experiments[experimentIndex++].emplace_back(strip02);
		}
		
		//
		// PHASE 2: Iterate through that we setup in the last phase
		// and really build each of the strips and strips that follow to see how
		// far we get
		//
		int numExperiments = experimentIndex;
		for (int i = 0; i < numExperiments; i++){
			
			// get the strip set
			
			// build the first strip of the list
			experiments[i][0]->Build(allEdgeInfos, allFaceInfos);
			int experimentId2 = experiments[i][0]->m_experimentId;
			
			NvStripInfo *stripIter = experiments[i][0];
			NvStripStartInfo startInfo(nullptr, nullptr, false);
			while (FindTraversal(allFaceInfos, allEdgeInfos, stripIter, startInfo)){
				
				// create the new strip info
				stripIter = new NvStripInfo(startInfo, stripId++, experimentId2);
				
				// build the next strip
				stripIter->Build(allEdgeInfos, allFaceInfos);
				
				// add it to the list
				experiments[i].emplace_back(stripIter);
			}
		}
		
		//
		// Phase 3: Find the experiment that has the most promise
		//
		int bestIndex = 0;
		double bestValue = 0;
		for (int i = 0; i < numExperiments; i++)
		{
			constexpr float avgStripSizeWeight = 1.0f;
			constexpr float numStripsWeight    = 0.0f;
			float avgStripSize = AvgStripSize(experiments[i]);
			float numStrips    = (float) experiments[i].size();
			float value        = avgStripSize * avgStripSizeWeight + (numStrips * numStripsWeight);

			if (value > bestValue)
			{
				bestValue = value;
				bestIndex = i;
			}
		}
		
		//
		// Phase 4: commit the best experiment of the bunch
		//
		CommitStrips(allStrips, experiments[bestIndex]);
		
		// and destroy all of the others
		for (int i = 0; i < numExperiments; i++)
		{
			if (i != bestIndex)
			{
				size_t numStrips = experiments[i].size();
				for (size_t j = 0; j < numStrips; j++)
				{
// START EPIC MOD: Fix memory leak
					for ( int iDegenerate = 0; iDegenerate < experiments[ i ][ j ]->m_numDegenerates; ++iDegenerate )
					{
						delete experiments[ i ][ j ]->m_Degenerates[ iDegenerate ];
					}
// END EPIC MOD: Fix memory leak

					delete experiments[i][j];
				}
			}
		}
		
		// delete the array that we used for all experiments
		delete [] experiments;
  }
}

}  // namespace nv::tristrip::internal

