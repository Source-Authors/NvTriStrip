#include "NvTriStrip.h"
#include "NvTriStripObjects.h"

#include <vector>

namespace nv::tristrip {

////////////////////////////////////////////////////////////////////////////////////////
//private data
static unsigned int cacheSize    = CACHESIZE_GEFORCE1_2;
static bool bStitchStrips        = true;
static unsigned int minStripSize = 0;
static bool bListsOnly           = false;

////////////////////////////////////////////////////////////////////////////////////////
// SetListsOnly()
//
// If set to true, will return an optimized list, with no strips at all.
//
// Default value: false
//
void SetListsOnly(const bool _bListsOnly)
{
	bListsOnly = _bListsOnly;
}

////////////////////////////////////////////////////////////////////////////////////////
// SetCacheSize()
//
// Sets the cache size which the stripfier uses to optimize the data.
// Controls the length of the generated individual strips.
// This is the "actual" cache size, so 24 for GeForce3 and 16 for GeForce1/2
// You may want to play around with this number to tweak performance.
//
// Default value: 16
//
void SetCacheSize(const unsigned int _cacheSize)
{
	cacheSize = _cacheSize;
}


////////////////////////////////////////////////////////////////////////////////////////
// SetStitchStrips()
//
// bool to indicate whether to stitch together strips into one huge strip or not.
// If set to true, you'll get back one huge strip stitched together using degenerate
//  triangles.
// If set to false, you'll get back a large number of separate strips.
//
// Default value: true
//
void SetStitchStrips(const bool _bStitchStrips)
{
	bStitchStrips = _bStitchStrips;
}


////////////////////////////////////////////////////////////////////////////////////////
// SetMinStripSize()
//
// Sets the minimum acceptable size for a strip, in triangles.
// All strips generated which are shorter than this will be thrown into one big, separate list.
//
// Default value: 0
//
void SetMinStripSize(const unsigned int _minStripSize)
{
	minStripSize = _minStripSize;
}

////////////////////////////////////////////////////////////////////////////////////////
// GenerateStrips()
//
// in_indices: input index list, the indices you would use to render
// in_numIndices: number of entries in in_indices
// primGroups: array of optimized/stripified PrimitiveGroups
// numGroups: number of groups returned
//
// Be sure to call delete[] on the returned primGroups to avoid leaking mem
//
void GenerateStrips(const unsigned int* in_indices, const size_t in_numIndices,
					PrimitiveGroup** primGroups, size_t* numGroups)
{
	//put data in format that the stripifier likes
	internal::UIntVec tempIndices;
	tempIndices.resize(in_numIndices);

	size_t maxIndex = 0;
	for(size_t i = 0; i < in_numIndices; i++)
	{
		tempIndices[i] = in_indices[i];
		if(in_indices[i] > maxIndex)
			maxIndex = in_indices[i];
	}

	internal::NvStripInfoVec tempStrips;
	internal::NvFaceInfoVec tempFaces;

	internal::NvStripifier stripifier;
	
	//do actual stripification
	stripifier.Stripify(tempIndices, cacheSize, minStripSize, maxIndex, tempStrips, tempFaces);

	//stitch strips together
	internal::IntVec stripIndices;
	size_t numSeparateStrips = 0;

	if(bListsOnly)
	{
		//if we're outputting only lists, we're done
		*numGroups = 1;
		auto* primGroupArray = new PrimitiveGroup[*numGroups];
		(*primGroups) = primGroupArray;

		//count the total number of indices
		size_t numIndices = 0;
		for(auto &s : tempStrips)
		{
			numIndices += s->m_faces.size() * 3;
		}

		//add in the list
		numIndices += tempFaces.size() * 3;

		auto &first      = primGroupArray[0];

		first.type       = PrimType::PT_LIST;
		first.numIndices = numIndices;
		first.indices    = new size_t[numIndices];

		//do strips
		size_t indexCtr = 0;
		for(auto &s : tempStrips)
		{
			for(auto &f : s->m_faces)
			{
				//degenerates are of no use with lists
				if(!internal::NvStripifier::IsDegenerate(f))
				{
					first.indices[indexCtr++] = f->m_v0;
					first.indices[indexCtr++] = f->m_v1;
					first.indices[indexCtr++] = f->m_v2;
				}
				else
				{
					//we've removed a tri, reduce the number of indices
					first.numIndices -= 3;
				}
			}
		}

		//do lists
		for(auto &f : tempFaces)
		{
			first.indices[indexCtr++] = f->m_v0;
			first.indices[indexCtr++] = f->m_v1;
			first.indices[indexCtr++] = f->m_v2;
		}
	}
	else
	{
		stripifier.CreateStrips(tempStrips, stripIndices, bStitchStrips, numSeparateStrips);

		//if we're stitching strips together, we better get back only one strip from CreateStrips()
		assert( (bStitchStrips && (numSeparateStrips == 1)) || !bStitchStrips);
		
		//convert to output format
		*numGroups = numSeparateStrips; //for the strips
		if(tempFaces.size() != 0)
			(*numGroups)++;  //we've got a list as well, increment
		(*primGroups) = new PrimitiveGroup[*numGroups];
		
		PrimitiveGroup* primGroupArray = *primGroups;
		
		//first, the strips
		size_t startingLoc = 0;
		for(size_t stripCtr = 0; stripCtr < numSeparateStrips; stripCtr++)
		{
			size_t stripLength = 0;

			if(!bStitchStrips)
			{
				//if we've got multiple strips, we need to figure out the correct length
				size_t i;
				for(i = startingLoc; i < stripIndices.size(); i++)
				{
					if(stripIndices[i] == -1)
						break;
				}
				
				assert(i >= startingLoc);
				stripLength = i - startingLoc;
			}
			else
				stripLength = stripIndices.size();
			
			auto &strip      = primGroupArray[stripCtr];

			strip.type       = PrimType::PT_STRIP;
			strip.indices    = new size_t[stripLength];
			strip.numIndices = stripLength;
			
			size_t indexCtr = 0;
			for(size_t i = startingLoc; i < stripLength + startingLoc; i++)
				strip.indices[indexCtr++] = stripIndices[i];

			//we add 1 to account for the -1 separating strips
			//this doesn't break the stitched case since we'll exit the loop
			startingLoc += stripLength + 1; 
		}
		
		//next, the list
		if(tempFaces.size() != 0)
		{
			assert(*numGroups >= 1);

			size_t faceGroupLoc = (*numGroups) - 1;    //the face group is the last one
			
			auto &strip      = primGroupArray[faceGroupLoc];

			strip.type       = PrimType::PT_LIST;
			strip.indices    = new size_t[tempFaces.size() * 3];
			strip.numIndices = tempFaces.size() * 3;

			size_t indexCtr = 0;
			for(auto &f : tempFaces)
			{
				strip.indices[indexCtr++] = f->m_v0;
				strip.indices[indexCtr++] = f->m_v1;
				strip.indices[indexCtr++] = f->m_v2;
			}
		}
	}

	//clean up everything

	//delete strips
	for(auto &s : tempStrips)
	{
		for(auto &f : s->m_faces)
		{
			delete f;
			f = nullptr;
		}
		delete s;
		s = nullptr;
	}

	//delete faces
	for(auto &f : tempFaces)
	{
		delete f;
		f = nullptr;
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// RemapIndices()
//
// Function to remap your indices to improve spatial locality in your vertex buffer.
//
// in_primGroups: array of PrimitiveGroups you want remapped
// numGroups: number of entries in in_primGroups
// numVerts: number of vertices in your vertex buffer, also can be thought of as the range
//  of acceptable values for indices in your primitive groups.
// remappedGroups: array of remapped PrimitiveGroups
//
// Note that, according to the remapping handed back to you, you must reorder your 
//  vertex buffer.
//
void RemapIndices(const PrimitiveGroup* in_primGroups, const size_t numGroups,
				  const size_t numVerts, PrimitiveGroup** remappedGroups)
{
	(*remappedGroups) = new PrimitiveGroup[numGroups];

	//caches oldIndex --> newIndex conversion
	std::vector<size_t> indexCache{numVerts, std::numeric_limits<size_t>::max(), std::allocator<size_t>()};
	
	//loop over primitive groups
	size_t indexCtr = 0;
	for(size_t i = 0; i < numGroups; i++)
	{
		size_t numIndices = in_primGroups[i].numIndices;

		//init remapped group
		(*remappedGroups)[i].type       = in_primGroups[i].type;
		(*remappedGroups)[i].numIndices = numIndices;
		(*remappedGroups)[i].indices    = new size_t[numIndices];

		for(size_t j = 0; j < numIndices; j++)
		{
			size_t cachedIndex = indexCache[in_primGroups[i].indices[j]];
			if(cachedIndex == std::numeric_limits<size_t>::max()) //we haven't seen this index before
			{
				//point to "last" vertex in VB
				(*remappedGroups)[i].indices[j] = indexCtr;

				//add to index cache, increment
				indexCache[in_primGroups[i].indices[j]] = indexCtr++;
			}
			else
			{
				//we've seen this index before
				(*remappedGroups)[i].indices[j] = cachedIndex;
			}
		}
	}
}

}  // namespace nv::tristrip
