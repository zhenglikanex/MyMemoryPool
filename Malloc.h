#pragma once

#include <new>

#include "Config.h"

class CMalloc
{
public:
	static void* Malloc(size_t n)
	{
		return malloc(n);
	}

	static void* Realloc(void* p,size_t n)
	{
		return realloc(p, n);
	}

	static void Free(void* p)
	{
		free(p);
	}
};

template<size_t ALIGN,size_t MAX_BYTES,size_t FREE_LISTS = MAX_BYTES / ALIGN>
class FreeListMalloc
{
public:
	//static_assert(ALIGN & (ALIGN - 1) == 0, "!");
	//static_assert(ALIGN >= alignof(std::max_align_t), "!");
	////static_assert(MAX_BYTES & (MAX_BYTES - 1) == 0, "!");
	//static_assert(MAX_BYTES >= ALIGN, "!");

	static void* Malloc(size_t n)
	{
		//超过MAX_BYTES直接使用malloc
		if (n > MAX_BYTES)
		{
			return CMalloc::Malloc(n);
		}

		FreeListNode** free_list = free_lists_ + FreeListIndex(n);
		if (!*free_list)
		{
			*free_list = AllocFreeList(AlignUp(n),2);
		}

		if (free_list)
		{
			void* result = *free_list;

			*free_list = (*free_list)->next_node;

			return result;
		}

		return nullptr;
	}

	static void* Realloc(void* p, size_t n)
	{
		return realloc(p, n);
	}

	static void Free(void* p)
	{
		free(p);
	}
private:
	struct FreeListNode {
		FreeListNode* next_node;
	};

	//向上对齐到ALIGN的倍数
	static size_t AlignUp(size_t n)
	{
		return ~(ALIGN - 1) & (n + ALIGN - 1);
	}

	// 计算n在FreeList中的位置
	static size_t FreeListIndex(size_t n)
	{
		return (n + ALIGN - 1) / ALIGN - 1;
	}

public:
	static size_t FreeListIndex(void* p)
	{
		size_t align = (reinterpret_cast<uintptr_t>(p) >> 16) & MAX_BYTES;
		return align;
	}

	static FreeListNode* AllocFreeList(size_t bytes, size_t n)
	{
		FreeListNode* free_list = reinterpret_cast<FreeListNode*>(AllocChunk(bytes, n));
		FreeListNode* result = free_list;

		if (free_list)
		{
			for (size_t index = 1; index < n; ++index)
			{
				free_list->next_node = reinterpret_cast<FreeListNode*>((reinterpret_cast<char*>(free_list) + bytes));
				free_list = free_list->next_node;
			}

			free_list->next_node = nullptr;
		}

		return result;
	}
	
	static char* AllocChunk(size_t bytes,size_t& n)
	{
		size_t total_bytes = bytes * n;
		size_t chunk_size = end_index_ - start_index_;

		if (chunk_size >= total_bytes) //有足够chunk分配内存
		{
			char* result = start_index_;
			start_index_ += total_bytes;

			return result;
		}
		else if (chunk_size > bytes) //将剩余的chunk都分配出去
		{
			
		}
		else
		{
			// 从malloc中分配chunk
			size_t expand_chunk_size = 64 * 1024;
			start_index_ = static_cast<char*>(malloc(expand_chunk_size));
			end_index_ = start_index_ + expand_chunk_size;

			return AllocChunk(bytes, n);
		}

		return static_cast<char*>(malloc(total_bytes));
	}

	static char* start_index_;
	static char* end_index_;
	static size_t heap_size_;
	static FreeListNode* free_lists_[FREE_LISTS];
};

template<size_t ALIGN, size_t MAX_BYTES, size_t FREE_LISTS> char* FreeListMalloc<ALIGN, MAX_BYTES, FREE_LISTS>::start_index_ = nullptr;
template<size_t ALIGN, size_t MAX_BYTES, size_t FREE_LISTS> char* FreeListMalloc<ALIGN, MAX_BYTES, FREE_LISTS>::end_index_ = nullptr;
template<size_t ALIGN, size_t MAX_BYTES, size_t FREE_LISTS> size_t FreeListMalloc<ALIGN, MAX_BYTES, FREE_LISTS>::heap_size_ = 0;
template<size_t ALIGN, size_t MAX_BYTES, size_t FREE_LISTS>
typename FreeListMalloc<ALIGN, MAX_BYTES, FREE_LISTS>::FreeListNode* FreeListMalloc<ALIGN, MAX_BYTES, FREE_LISTS>::free_lists_[FREE_LISTS] = { nullptr };


#if USE_CUSTOM_MALLOC
using DefaultMalloc = FreeListMalloc<8, 256>;
#else
using DefaultMalloc = CMalloc;
#endif

#if USE_CUSTOM_MALLOC && USE_CUSTOM_NEW

//inline void* operator new(size_t n)
//{
//	return DefaultMalloc::Malloc(n);
//}
//
//inline void* operator new[](size_t n)
//{
//	return DefaultMalloc::Malloc(n);
//}
//
//inline void operator delete (void* n)
//{
//	DefaultMalloc::Free(n);
//}
//
//inline void operator delete[](void* n)
//{
//	DefaultMalloc::Free(n);
//}

#endif