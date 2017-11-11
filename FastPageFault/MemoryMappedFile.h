#pragma once
#include <windows.h>
#include <string>
#include "Stopwatch.h"

class MemoryMappedFile
{
public:
	MemoryMappedFile(std::wstring &file, bool bFlushFileSystemCacheOfFile = false);
	void TouchPages(Stopwatch &sw, bool bPrefetch=false, int sleepBeforeTouchMs=10000);
	size_t GetFileSize();
	~MemoryMappedFile();
private:
	void FlushFSCache(std::wstring &file);
private:
	HANDLE hFile;
	HANDLE hFileMapping;
	void *pFile;

};

