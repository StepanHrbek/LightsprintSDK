// --------------------------------------------------------------------------
// Chunk list - single linked list allocated from local pool
// Copyright (c) 2008-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef CHUNK_LIST_H
#define CHUNK_LIST_H

#include "Lightsprint/RRDebug.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// Pool
//
// used internally by ChunkList
// reset() frees all previously allocated elements, single element can't be freed

template<class C>
class Pool
{
public:
	Pool()
	{
		firstBlock = NULL;
		freeBlock = NULL;
		elementsUsed = 0;
		allocationFailed = false;
	}
	C* allocate()
	{
		if (!reserve(1))
			return NULL;
		if (elementsUsed==Block::BLOCK_SIZE)
		{
			freeBlock = freeBlock->next;
			elementsUsed = 0;
		}
		freeBlock->element[elementsUsed].next = NULL;
		return freeBlock->element + elementsUsed++;
	}
	// free everything
	void reset()
	{
		this->~Pool();
		new(this) Pool();
	}
	// did allocation fail since last constructor or reset?
	bool failed()
	{
		return allocationFailed;
	}
	// preallocates space, ensures that next n allocate() calls won't fail
	// true = ok
	bool reserve(unsigned elementsToReserve)
	{
		allocationFailed |= !Block::reserve(freeBlock,elementsToReserve+elementsUsed);
		if (!firstBlock)
			firstBlock = freeBlock;
		return !allocationFailed;
	}
	~Pool()
	{
		delete firstBlock;
	}

private:
	// block of chunks (allocation unit)
	class Block
	{
	public:
		Block()
		{
			next = NULL;
		}
		~Block()
		{
			delete next;
		}
		// preallocates space, ensures that blocks contain at least elementsToReserve
		// true = ok
		static bool reserve(Block*& block, unsigned elementsToReserve)
		{
			if (!block)
			{
				try // ignore warning, std::bad_alloc exception is catched properly
				{
					block = new Block;
				}
				catch(...)
				{
#if defined(_M_X64) || defined(_LP64)
					RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"Not enough memory, radiosity job interrupted.\n"));
#else
					RR_LIMITED_TIMES(1,RRReporter::report(ERRO,"Out of address space, radiosity job interrupted. Use 64bit version.\n"));
#endif
					return false;
				}
			}
			return (elementsToReserve>BLOCK_SIZE) ? reserve(block->next,elementsToReserve-BLOCK_SIZE) : true;
		}
		enum {BLOCK_SIZE=1000000/sizeof(C)};
		C element[BLOCK_SIZE];
		Block* next; // allocated after us
	};

	Block* firstBlock; // pointer to first block
	Block* freeBlock; // pointer to first block with free elements
	unsigned elementsUsed; // number of elements used in freeBlock
	bool allocationFailed; // previous allocation or reservation failed
};


//////////////////////////////////////////////////////////////////////////////
//
// ChunkList
//
// singly linked list with pooling and grouping. supports
// - insert (fast when using InsertIterator to insert all elements at once, push_back is slow)
// - read all elements at once using const_iterator
// - clear
// - separated pools for separated sets of instances
//
// STL (with boost pool) would be inefficient here
// - list pointers waste memory when elements are small (we group elements to reduce number of pointers)
// - vector pooling is inefficient, needs blocks of different sizes (we are happy with single size)
// - all instances share one pool
//
// beware: it's compatible with STL only for our use case, it won't work elsewhere

template<class C>
class ChunkList
{
public:

	ChunkList()
	{
		firstChunk = NULL;
		numElements = 0;
	}
	// no need for destructor, pool takes care of memory

	unsigned size() const
	{
		return numElements;
	}

	// Discards ChunkList content without deleting it. To be called when pool is deleted.
	void clear()
	{
		firstChunk = NULL;
		numElements = 0;
	}

	// chunk - for internal use
	class Chunk
	{
	public:
		enum {CHUNK_SIZE=10};
		C element[CHUNK_SIZE];
		Chunk* next;
	};

	// allocator - user must manually create at least one
	typedef Pool<Chunk> Allocator;

	// the only way to append new elements
	class InsertIterator
	{
	public:
		InsertIterator(ChunkList& _chunkList, Allocator& _allocator) : chunkList(_chunkList), allocator(_allocator)
		{
			chunk = chunkList.firstChunk;
			elementInChunk = chunkList.numElements;
			while (elementInChunk>Chunk::CHUNK_SIZE)
			{
				elementInChunk -= Chunk::CHUNK_SIZE;
				chunk = chunk->next;
			}
		}
		bool insert(C element)
		{
			if (!chunk)
			{
				chunk = chunkList.firstChunk = allocator.allocate();
				if (!chunk) return false;
			}
			if (elementInChunk==Chunk::CHUNK_SIZE)
			{
				if (!chunk->next)
					chunk->next = allocator.allocate();
				chunk = chunk->next;
				if (!chunk) return false;
				elementInChunk = 0;
			}
			chunk->element[elementInChunk++] = element;
			chunkList.numElements++;
			return true;
		}
	private:
		ChunkList& chunkList;
		Allocator& allocator;
		Chunk* chunk;
		unsigned elementInChunk;
	};

	void push_back(const C& _c, Allocator& _allocator) // InsertIterator is faster when inserting multiple elements
	{
		InsertIterator i(*this,_allocator);
		i.insert(_c);
	}

	// the only way to read elements
	class const_iterator
	{
	public:
		const_iterator(const ChunkList& _chunkList)
		{
			chunk = _chunkList.firstChunk;
			elementInChunk = 0;
			remainingElements = _chunkList.numElements;
		}
		const C* operator *() const
		{
			return remainingElements ? chunk->element+elementInChunk : NULL;
		}
		const C* operator ->() const
		{
			return remainingElements ? chunk->element+elementInChunk : NULL;
		}
		void operator ++()
		{
			RR_ASSERT(remainingElements);
			remainingElements--;
			elementInChunk++;
			if (elementInChunk>=Chunk::CHUNK_SIZE)
			{
				chunk = chunk->next;
				elementInChunk = 0;
			}
		}
		const bool operator ==(void*) const // i==end()
		{
			return remainingElements==0;
		}
		const bool operator !=(void*) const // i!=end()
		{
			return remainingElements!=0;
		}
	private:
		Chunk* chunk;
		unsigned elementInChunk;
		unsigned remainingElements;
	};
	const ChunkList& begin() const // const_iterator = begin()
	{
		return *this;
	}
	void* end() const // i!=end()
	{
		return NULL;
	}
	C* operator ->() const // begin()->counter = 1;
	{
		RR_ASSERT(numElements);
		return firstChunk->element;
	}

private:
	Chunk* firstChunk;
	unsigned numElements;
};

} // namespace

#endif
