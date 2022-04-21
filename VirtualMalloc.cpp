#include "VirtualMalloc.h"

#include <cassert>
#include <iostream>
#include <new>

inline bool Is2Power(uint32_t size)
{
	return (size & (size - 1)) == 0;
}

template<class T>
inline T Align(T size, uint32_t alignment)
{
	return (size + alignment - 1) & ~(static_cast<T>(alignment) - 1);
}

template<class T>
inline T Align(T size, uint64_t alignment)
{
	return (size + alignment - 1) & ~(static_cast<T>(alignment) - 1);
}


inline uint32_t CeilLogTwo(uint64_t value)
{
	uint32_t bit = 0;
	while (value > 0)
	{
		value = value >> 1;
		++bit;
	}

	return bit - 1;
}

VirtualMalloc::VirtualMalloc(uint32_t page_size, uint64_t address_limit)
	: page_size_(page_size)
	, address_limit_(address_limit)
	, pool_info_hash_table_(nullptr)
	, page_size_bit_(0)
{
	assert(Is2Power(page_size_));
	assert(address_limit_ > page_size_);

	static const uint32_t kBlockSizes[kPoolTableSize] =
	{
		8,		16,		32,		48,		64,		80,		96,		112,
		128,	160,	192,	224,	256,	288,	320,	384,
		448,	512,	576,	640,	704,	768,	896,	1024,
		1168,	1360,	1632,	2048,	2336,	2720,	3264,	4096,
		4672,	5456,	6544,	8192,	9360,	10912,	13104,	16384,
		21840,	32768
	};

	for (uint32_t i = 0; i < kPoolTableSize; ++i)
	{
		pool_tables_[i].block_size = kBlockSizes[i];
	}

	for (uint32_t i = 0; i < kMaxPoolCount; ++i)
	{
		for (uint32_t pool_index = 0; pool_index < kPoolTableSize; ++pool_index)
		{

			if (i <= pool_tables_[pool_index].block_size)
			{
				mem_size_by_pool_map_[i] = &pool_tables_[pool_index];
				break;
			}
		}
	}

	page_size_bit_ = CeilLogTwo(page_size_);
	max_pool_info_count_ = address_limit_ >> page_size_bit_;
	address_limit_count_ = Align(kAddressLimit, address_limit_) / address_limit_;
}

VirtualMalloc::~VirtualMalloc()
{

}

void* VirtualMalloc::Malloc(size_t size, uint32_t alignment)
{
	assert((alignment == 0 || Is2Power(alignment)) && "alignment需要是2的幂!");

	if (size == 0)
	{
		return nullptr;
	}

	//是否需要对齐
	if(alignment != 0)
	{
		size = Align(size, alignment);
	}

	if(size <= kBlockSizeLimit)
	{
		auto pool_table = mem_size_by_pool_map_[size];

		// 该池子还未分配
		if(!pool_table->first_pool)
		{
			pool_table->first_pool = AllocBlockPoolInfo(size,pool_table->block_size, kPoolSize);
		}

		if(!pool_table->first_pool)
		{
			throw std::bad_alloc();
		}

		return AllocBlock(pool_table,pool_table->first_pool, pool_table->block_size);
	}
	else
	{
		//直接调用MALLOC
		size_t os_bytes = Align(size, page_size_);

		FreeBlock* free = static_cast<FreeBlock*>(MALLOC(os_bytes));

		PoolInfo* pool_info = GetPoolInfo(free);
		for (uint32_t page = 1, manager_size = page_size_; manager_size < os_bytes; ++page, manager_size += page_size_)
		{
			PoolInfo* group_info = GetPoolInfo( reinterpret_cast<char*>(free) + page );
			group_info->use_block_count = 0;	//当Free时use_block_count为nullptr时说明是多个pool_info管理一个pool
			group_info->table_index = kMaxPoolCount;
			group_info->pool_size = os_bytes;
			group_info->first_block = nullptr;		
		}

		++pool_info->use_block_count;
		pool_info->table_index = kMaxPoolCount;
		pool_info->pool_size = os_bytes;
		pool_info->first_block = reinterpret_cast<FreeBlock*>(os_bytes);	//存放os_bytes大小,uint32_t可能不够存放os_bytes

		return free;
	}
}

void VirtualMalloc::Free(void* ptr) const
{
	if(ptr == nullptr)
	{
		return;
	}

	// cache miss
	PoolInfo* pool_info = FindPoolInfo(ptr);
	if(pool_info)
	{
		--pool_info->use_block_count;

		if(pool_info->use_block_count == 0)
		{
			if(pool_info->prev_link)
			{
				*pool_info->prev_link = pool_info->next;
			}

			void* base_ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pool_info->first_block) >> page_size_bit_ << page_size_bit_);
			FREE(base_ptr, pool_info->pool_size);
			pool_info->first_block = nullptr;
		}
		else
		{
			//回收内存块
			FreeBlock* block = static_cast<FreeBlock*>(ptr);
			block->block_num = 1;
			block->next_block = pool_info->first_block;
			pool_info->first_block = block;

			auto pool_table = mem_size_by_pool_map_[pool_info->table_index];
			if (pool_info->prev_link && pool_info->use_block_count * pool_table->block_size <= pool_info->pool_size)
			{
				//从耗尽的链表中移除
				*pool_info->prev_link = (*pool_info->prev_link)->next;
				pool_info->prev_link = nullptr;

				//加入空闲链表
				pool_info->prev_link = &pool_table->first_pool;
				pool_info->next = pool_table->first_pool;
				pool_table->first_pool = pool_info;
			}
		}
	}
}

void* VirtualMalloc::AllocBlock(PoolTable* pool_table, PoolInfo* pool_info, uint32_t block_size) const
{
	//从后往前分配,防止每次AllocPool后都要重新构建FreeBlock的链表
	FreeBlock* block = reinterpret_cast<FreeBlock*>(reinterpret_cast<uint8_t*>(pool_info->first_block) + 
		--pool_info->first_block->block_num * pool_table->block_size);

	++pool_info->use_block_count;

	if(pool_info->first_block->block_num == 0)
	{
		pool_info->first_block = block->next_block;
	}

	if(!pool_info->first_block)
	{
		// 从空闲的内存池中移除
		pool_table->first_pool = pool_info->next;

		// 加入耗尽的链表
		if(pool_table->exhausted_pool)
		{
			pool_table->exhausted_pool->prev_link = &pool_info->next;
		}
		pool_info->prev_link = &pool_table->exhausted_pool;
		pool_info->next = pool_table->exhausted_pool;
		pool_table->exhausted_pool = pool_info;
	}

	return block;
}

VirtualMalloc::PoolInfo* VirtualMalloc::AllocBlockPoolInfo(uint32_t table_index, uint32_t block_size, uint32_t pool_size)
{
	uint32_t block_nums = pool_size / block_size;
	uint32_t bytes = block_nums * block_size;					// 真实可用的内存大小,因为pool_size不一定能被block_size整除
	uint32_t os_bytes = Align(bytes, page_size_);				// 对齐到page_size,os真实分配的内存大小(os按页分配内存)

	auto free = static_cast<FreeBlock*>(MALLOC(os_bytes));

	if(!free)
	{
		throw std::bad_alloc();
	}

	free->block_num = block_nums;

	/*
	 *太慢了
	 FreeBlock* p = free;
	for (uint32_t i = 1; i < block_count; ++i)
	{
		p->next_block = reinterpret_cast<FreeBlock*>((reinterpret_cast<char*>(p) + block_size));
	}*/

	auto pool = GetPoolInfo(free);

	// 当os_bytes大于page_size,用多个pool_info来配合管理
	if(os_bytes > page_size_)
	{
		for (uint32_t page = 1, manager_size = page_size_; manager_size < os_bytes; ++page, manager_size += page_size_)
		{
			PoolInfo* group_info = GetPoolInfo(free + page);
			group_info->use_block_count = 0;
			group_info->table_index = table_index;
			group_info->pool_size = os_bytes;
			group_info->first_block = nullptr;		//当first_block为nullptr时说明是多个pool_info管理一个pool
		}
	}

	pool->use_block_count = 0;
	pool->table_index = table_index;
	pool->pool_size = os_bytes;
	pool->first_block = free;
	pool->prev_link = &mem_size_by_pool_map_[table_index]->first_pool;
	
	return pool;
}

VirtualMalloc::PoolInfo* VirtualMalloc::GetPoolInfo(void* ptr)
{
	if(pool_info_hash_table_ == nullptr)
	{
		pool_info_hash_table_ = static_cast<PoolInfoHashTable*>(MALLOC(address_limit_count_ * sizeof(PoolInfoHashTable)));

		if(!pool_info_hash_table_)
		{
			throw std::bad_alloc();
		}

		for (uint64_t i = 0; i < address_limit_count_; ++i)
		{
			new (pool_info_hash_table_ + i) PoolInfoHashTable;
		}
	}

	size_t key = reinterpret_cast<size_t>(ptr);
	uint64_t hash = key / address_limit_;
	uint64_t pool_index = (key % address_limit_) >> page_size_bit_;

	auto& pool_info_table_node = pool_info_hash_table_[hash];
	if(!pool_info_table_node.first_pool_info)
	{
		//初始化所有PoolInfo
		PoolInfo* pool = static_cast<PoolInfo*>(MALLOC(max_pool_info_count_ * sizeof(PoolInfo)));
		pool_info_table_node.first_pool_info = static_cast<PoolInfo**>(MALLOC(max_pool_info_count_ * sizeof(PoolInfo*)));

		if (!pool || !pool_info_table_node.first_pool_info)
		{
			throw std::bad_alloc();
		}

		for (uint32_t i = 0; i < max_pool_info_count_; ++i)
		{
			new (pool) PoolInfo;
			pool_info_table_node.first_pool_info[i] = pool;
			++pool;
		}
	}

	return pool_info_table_node.first_pool_info[pool_index];
}

VirtualMalloc::PoolInfo* VirtualMalloc::FindPoolInfo(void* ptr) const
{
	size_t key = reinterpret_cast<size_t>(ptr);
	uint64_t hash = key / address_limit_;
	uint64_t pool_index = (key % address_limit_) >> page_size_bit_;
	auto& pool_info_hash_table_node = pool_info_hash_table_[hash];

	if (pool_info_hash_table_node.first_pool_info[pool_index]->use_block_count > 0)
	{
		return pool_info_hash_table_node.first_pool_info[pool_index];
	}

	for (uint32_t index = pool_index - 1; index > 0; --index)
	{
		if(pool_info_hash_table_node.first_pool_info[pool_index]->use_block_count > 0)
		{
			return pool_info_hash_table_node.first_pool_info[index];
		}
	}

	return nullptr;
}
