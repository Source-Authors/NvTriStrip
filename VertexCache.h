#ifndef VERTEX_CACHE_H
#define VERTEX_CACHE_H

#include <cassert>
#include <cstring>

namespace nvidia::tristrip::internal {

class VertexCache
{
public:
	explicit VertexCache(size_t size)
	{
		numEntries = size;
		
		entries = new int[numEntries];
		
		for(size_t i = 0; i < numEntries; i++)
			entries[i] = -1;
	}
		
	VertexCache() : VertexCache(16) { }
	~VertexCache() { delete[] entries; entries = nullptr; }
	
	bool InCache(int entry) const
	{
		for(size_t i = 0; i < numEntries; i++)
		{
			if(entries[i] == entry)
			{
				return true;
			}
		}
		
		return false;
	}
	
	int AddEntry(int entry)
	{
		int removed = entries[numEntries - 1];
		
		//push everything right one
		for(ptrdiff_t i = numEntries - 2; i >= 0; i--)
		{
			entries[i + 1] = entries[i];
		}
		
		entries[0] = entry;
		
		return removed;
	}

	void Clear()
	{
		memset(entries, -1, sizeof(int) * numEntries);
	}
	
	void Copy(VertexCache* inVcache) const
	{
		for(size_t i = 0; i < numEntries; i++)
		{
			inVcache->Set(i, entries[i]);
		}
	}

	int At(size_t index) const { assert(index < numEntries); return entries[index]; }
	void Set(size_t index, int value) { assert(index < numEntries); entries[index] = value; }

private:
  int *entries;
  size_t numEntries;
};

}  // namespace nvidia::tristrip::internal

#endif
