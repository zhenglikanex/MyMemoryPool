#pragma once

#include <new>

namespace kanex
{
	template<class T,class ... Args>
	inline void construct(T* p, const Args& ...args)
	{
		new (p) T(args...);
	}

	template<class T>
	inline void destory(T* pointer)
	{
		pointer->~T();
	}

	inline void destroy(char*, char*) {}
	inline void destroy(wchar_t*, wchar_t*) {}

	template<class ForwardIterator,class T>
	inline void __destory(ForwardIterator first, ForwardIterator last, T*)
	{
		if constexpr (std::is_trivially_destructible_v<T>)
		{
			while (first != last)
			{
				destory(&*first);
				++first;
			}
		}
	}
}