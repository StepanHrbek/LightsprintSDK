// --------------------------------------------------------------------------
// Chunk list - single linked list allocated from non-global pool
// Copyright 2008 Stepan Hrbek, Lightsprint. All rights reserved.
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
// allocates only, delete pool to delete all previousuly allocated memory

template<class C>
class Pool
{
public:
	Pool()
	{
		block = NULL;
		elementsUsed = 0;
	}
	C* allocate()
	{
		if(!block || elementsUsed==Block::BLOCK_SIZE)
		{
			Block* oldBlock = block;
			try // ignore warning, std::bad_alloc exception is catched properly
			{
				block = new Block;
			}
			catch(...)
			{
				LIMITED_TIMES(1,RRReporter::report(ERRO,"Not enough memory, radiosity job interrupted.\n"));
				return NULL;
			}
			block->next = oldBlock;
			elementsUsed = 0;
		}
		block->element[elementsUsed].next = NULL;
		return block->element + elementsUsed++;
	}
	~Pool()
	{
		delete block;
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
		enum {BLOCK_SIZE=1000000/sizeof(C)};
		C element[BLOCK_SIZE];
		Block* next;
	};

	Block* block;
	unsigned elementsUsed;
};


//////////////////////////////////////////////////////////////////////////////
//
// ChunkList
//
// singly linked list with pooling and grouping. supports
// - insert all elements at once
// - read all elements at once
// - clear
// - separated pools for separated sets of instances
//
// STL would be inefficient here
// - list pointers waste memory when elements are small (we group elements to reduce number of pointers)
// - vector pooling is inefficient, needs blocks of different sizes (we are happy with single size)
// - all instances share one pool

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

	void clear()
	{
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

	// the only way to insert new elements, overwrites old elements
	class InsertIterator
	{
	public:
		InsertIterator(ChunkList& _chunkList, Allocator& _allocator) : chunkList(_chunkList), allocator(_allocator)
		{
			chunkList.numElements = 0; // insertIterator overwrites old elements, this line simulates clear
			chunk = chunkList.firstChunk;
			elementInChunk = 0;
		}
		bool insert(C element)
		{
			if(!chunk)
			{
				chunk = chunkList.firstChunk = allocator.allocate();
				if(!chunk) return false;
			}
			if(elementInChunk==Chunk::CHUNK_SIZE)
			{
				if(!chunk->next)
					chunk->next = allocator.allocate();
				chunk = chunk->next;
				if(!chunk) return false;
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

	// the only way to read elements
	class ReadIterator
	{
	public:
		ReadIterator(const ChunkList& _chunkList)
		{
			chunk = _chunkList.firstChunk;
			elementInChunk = 0;
			remainingElements = _chunkList.numElements;
		}
		const C* operator *()
		{
			return remainingElements ? chunk->element+elementInChunk : NULL;
		}
		void operator ++()
		{
			RR_ASSERT(remainingElements);
			remainingElements--;
			elementInChunk++;
			if(elementInChunk>=Chunk::CHUNK_SIZE)
			{
				chunk = chunk->next;
				elementInChunk = 0;
			}
		}
	private:
		Chunk* chunk;
		unsigned elementInChunk;
		unsigned remainingElements;
	};

private:
	Chunk* firstChunk;
	unsigned numElements;
};

} // namespace

#endif
