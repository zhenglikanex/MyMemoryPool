#pragma once

#include "Malloc.h"

template<size_t N>
class SizeAllocator
{
public:
	static void* Allocate()
	{
		
	}

	static void* Allocate(size_t n)
	{

	}

	static void Deallocate(void* p, size_t n)
	{
		
	}

	static void Deallocate(void* p)
	{

	}
};

template<class T>
class TypeAllocator
{
	static void* Allocate()
	{

	}

	static void* Allocate(size_t n)
	{

	}

	static void Deallocate(void* p, size_t n)
	{

	}

	static void Deallocate(void* p)
	{

	}
};

template<size_t N,size_t ALIGN = alignof(std::max_align_t)>
class AlignAllocator
{
	static void* Allocate()
	{

	}

	static void* Allocate(size_t n)
	{

	}

	static void Deallocate(void* p, size_t n)
	{

	}

	static void Deallocate(void* p)
	{

	}
};