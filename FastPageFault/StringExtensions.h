#pragma once
#include <memory>
#include <iostream>
#include <string>
#include <cstdio>

#pragma warning(disable : 4996)

struct StringExtensions
{
public:
	template<typename ... Args>
	static std::wstring Format(const wchar_t  *pFormat, Args ... args)
	{
		size_t size = _snwprintf(nullptr, 0, pFormat, args ...) + 1; // Extra space for '\0'
		std::unique_ptr<wchar_t[]> buf(new wchar_t[size]);
		_snwprintf(buf.get(), size, pFormat, args ...);
		return std::wstring(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	}

	StringExtensions();
	~StringExtensions();
};

