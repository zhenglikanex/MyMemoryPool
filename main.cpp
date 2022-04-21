#include <iostream>
#include <vector>
#include <bitset>
#include <stdlib.h>
#include "Malloc.h"
#include "VirtualMalloc.h"
#include <chrono>

void* aligned_malloc(size_t size, size_t alignment)
{
	size_t offset = alignment - 1 + sizeof(void*);
	void * originalP = malloc(size + offset);
	size_t originalLocation = reinterpret_cast<size_t>(originalP);
	size_t realLocation = (originalLocation + offset) & ~(alignment - 1);
	void * realP = reinterpret_cast<void*>(realLocation);
	size_t originalPStorage = realLocation - sizeof(void*);
	*reinterpret_cast<void**>(originalPStorage) = originalP;
	return realP;
}

void aligned_free(void* p)
{
	size_t originalPStorage = reinterpret_cast<size_t>(p) - sizeof(void*);
	free(*reinterpret_cast<void**>(originalPStorage));
}

//alignas可以指定变量的地址对齐
//pack可以指定对象布局中字段的地址对齐
//地址对齐就是地址位置是N的倍数

struct Timer
{
	std::string name;
	std::chrono::system_clock::duration start;
	Timer(const std::string& name)
	{
		this->name = name;
		start = std::chrono::system_clock::now().time_since_epoch();
	}

	~Timer()
	{
		auto t = std::chrono::system_clock::now().time_since_epoch() - start;
		std::cout << name << std::chrono::duration_cast<std::chrono::milliseconds>(t).count() << "ms" << std::endl;
	}
};

VirtualMalloc& GetMyMalloc()
{
	MEMORYSTATUSEX memory_status_ex = {};
	memory_status_ex.dwLength = sizeof(memory_status_ex);
	::GlobalMemoryStatusEx(&memory_status_ex);

	SYSTEM_INFO system_info = {};
	::GetSystemInfo(&system_info);

	static VirtualMalloc my_malloc(system_info.dwAllocationGranularity, 1ull << 32);
	return my_malloc;
}

void Test1(uint32_t size)
{
	//

	int cnt = 1000000;
	std::cout << "BlockSize: " << size << std::endl;
	{
		auto& my_malloc = GetMyMalloc();
		auto ptr = my_malloc.Malloc(16);
		my_malloc.Free(ptr);
		bool b = false;
		Timer t("MyMalloc: ");

		for(int i = 0;i<cnt;++i)
		{
			auto ptr1 = my_malloc.Malloc(size);
			auto ptr2 = my_malloc.Malloc(size);

			my_malloc.Free(ptr2);
		
		}

	}

	{
		Timer t("malloc: ");

		for (int i = 0; i < cnt; ++i)
		{
			auto ptr1 = malloc(size);
			auto ptr2 = malloc(size);
			free(ptr2);
		}
	}
	std::cout << "---------------------------------------------"  << std::endl;
}

int main()
{
	Test1(16);
	Test1(32);
	Test1(64);
	Test1(333);
	Test1(1000);

	auto ptr = GetMyMalloc().Malloc(1000000);
	GetMyMalloc().Free(ptr);

	system("pause");
	return 0;
}