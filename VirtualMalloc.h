#pragma once
#include <cstdint>

//#if WINDOWS
#include <Windows.h>

#define MALLOC(size) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
#define FREE(ptr,size) VirtualFree(ptr, 0, MEM_RELEASE);

//#endif

class VirtualMalloc
{
public:
	static constexpr uint32_t kPoolSize = 65536;

	// 管理的
	static constexpr uint16_t kPoolTableSize = 42;

	// 
	static constexpr uint32_t kMaxPoolCount = kPoolSize / 2 + 1;

	static constexpr uint32_t kBlockSizeLimit = kPoolSize / 2;

	static constexpr uint64_t kAddressLimit = 1ull << 48;
public:
	VirtualMalloc(uint32_t page_size, uint64_t address_limit);
	~VirtualMalloc();

	VirtualMalloc(const VirtualMalloc&) = delete;
	VirtualMalloc(VirtualMalloc&&) = delete;
	bool operator=(const VirtualMalloc&) = delete;
	bool operator=(VirtualMalloc&&) = delete;

	void* Malloc(size_t size,uint32_t alignment = 0);
	void Free(void* ptr) const;
private:

	/*
	 *组织Pool的Block块
	 */
	struct FreeBlock
	{
		FreeBlock* next_block = nullptr;
		uint32_t block_num = 0;
	};

	/*用来管理一个pool,每个pool的size可以不同
	 *但必须是page_size对齐的
	 *当pool_size大于page_size时
	 *用pool_size/page_size个pool_info来管理一个pool
	 *pool_info设计成可以整除page_size,来减少内存浪费
	 */
	struct alignas(32) PoolInfo
	{
		//分配出去的block数量
		uint16_t use_block_count = 0;				//0 , 2

		//指向管理这个pool_info的table	
		uint16_t table_index = 0;					//2 , 2

		//管理的pool大小,
		uint32_t pool_size = 0;						//4 , 8

		//指向Pool的第一个地址
		FreeBlock* first_block = nullptr;			//8 , 16

		//指向下一个pool_info
		PoolInfo* next = nullptr;					//16 , 24

		//指向上一个pool_info指针的指针,用来加速从链表中移除
		PoolInfo** prev_link = nullptr;				//24 , 32
	};

	/*
	 *用来管理同一个block
	 */
	struct PoolTable
	{
		//管理的block大小
		uint16_t block_size = 0;

		//指向第一个可以分配的pool
		PoolInfo* first_pool = nullptr;

		//指向耗完的pool
		PoolInfo* exhausted_pool = nullptr;
	};



	/*
	 * 
	 */
	struct PoolInfoHashTable
	{
		//指针数组
		PoolInfo** first_pool_info = nullptr;
	};

public:
	void* AllocBlock(PoolTable* pool_table,PoolInfo* pool_info,uint32_t block_size) const;
	PoolInfo* AllocBlockPoolInfo(uint32_t table_index,uint32_t block_size, uint32_t pool_size);
	PoolInfo* GetPoolInfo(void* ptr);
	PoolInfo* FindPoolInfo(void* ptr) const;

	//操作系统分配的虚拟页大小
	uint32_t page_size_;

	//os最大分配地址
	uint64_t address_limit_;

	//pool_info_hash_array_
	uint32_t max_pool_info_count_;

	PoolTable pool_tables_[kPoolTableSize];

	//根据分配的内存大小，映射到pool_table
	PoolTable* mem_size_by_pool_map_[kMaxPoolCount];

	//根据pool的address哈希得到在数组中的index
	PoolInfoHashTable* pool_info_hash_table_;

	uint16_t page_size_bit_;
	uint64_t address_limit_count_;
};
