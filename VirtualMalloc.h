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

	// �����
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
	 *��֯Pool��Block��
	 */
	struct FreeBlock
	{
		FreeBlock* next_block = nullptr;
		uint32_t block_num = 0;
	};

	/*��������һ��pool,ÿ��pool��size���Բ�ͬ
	 *��������page_size�����
	 *��pool_size����page_sizeʱ
	 *��pool_size/page_size��pool_info������һ��pool
	 *pool_info��Ƴɿ�������page_size,�������ڴ��˷�
	 */
	struct alignas(32) PoolInfo
	{
		//�����ȥ��block����
		uint16_t use_block_count = 0;				//0 , 2

		//ָ��������pool_info��table	
		uint16_t table_index = 0;					//2 , 2

		//�����pool��С,
		uint32_t pool_size = 0;						//4 , 8

		//ָ��Pool�ĵ�һ����ַ
		FreeBlock* first_block = nullptr;			//8 , 16

		//ָ����һ��pool_info
		PoolInfo* next = nullptr;					//16 , 24

		//ָ����һ��pool_infoָ���ָ��,�������ٴ��������Ƴ�
		PoolInfo** prev_link = nullptr;				//24 , 32
	};

	/*
	 *��������ͬһ��block
	 */
	struct PoolTable
	{
		//�����block��С
		uint16_t block_size = 0;

		//ָ���һ�����Է����pool
		PoolInfo* first_pool = nullptr;

		//ָ������pool
		PoolInfo* exhausted_pool = nullptr;
	};



	/*
	 * 
	 */
	struct PoolInfoHashTable
	{
		//ָ������
		PoolInfo** first_pool_info = nullptr;
	};

public:
	void* AllocBlock(PoolTable* pool_table,PoolInfo* pool_info,uint32_t block_size) const;
	PoolInfo* AllocBlockPoolInfo(uint32_t table_index,uint32_t block_size, uint32_t pool_size);
	PoolInfo* GetPoolInfo(void* ptr);
	PoolInfo* FindPoolInfo(void* ptr) const;

	//����ϵͳ���������ҳ��С
	uint32_t page_size_;

	//os�������ַ
	uint64_t address_limit_;

	//pool_info_hash_array_
	uint32_t max_pool_info_count_;

	PoolTable pool_tables_[kPoolTableSize];

	//���ݷ�����ڴ��С��ӳ�䵽pool_table
	PoolTable* mem_size_by_pool_map_[kMaxPoolCount];

	//����pool��address��ϣ�õ��������е�index
	PoolInfoHashTable* pool_info_hash_table_;

	uint16_t page_size_bit_;
	uint64_t address_limit_count_;
};
