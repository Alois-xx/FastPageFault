#include "stdafx.h"
#include "MemoryMappedFile.h"


MemoryMappedFile::MemoryMappedFile(std::wstring &file, bool bFlushFileSystemCacheOfFile)
{
	hFile = nullptr;
	hFileMapping = nullptr;
	pFile = nullptr;

	if (bFlushFileSystemCacheOfFile)
	{
		FlushFSCache(file);
	}

	hFile = ::CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		throw std::exception("Could not open file");
	}

	hFileMapping = ::CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (hFileMapping == NULL)
	{
		throw std::exception("Could not create file mapping");
	}

	pFile = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
	if (pFile == nullptr)
	{
		throw std::exception("MapViewOfFile failed");
	}
}

void MemoryMappedFile::FlushFSCache(std::wstring &file)
{
	HANDLE hUnbufferedHandle = ::CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr);
	if (hUnbufferedHandle == INVALID_HANDLE_VALUE)
	{
		throw std::exception("Could not open file in unbuffered mode.");
	}
	::CloseHandle(hUnbufferedHandle);
}

size_t MemoryMappedFile::GetFileSize()
{
	BY_HANDLE_FILE_INFORMATION fileInfo;

	BOOL lGetFileRet = ::GetFileInformationByHandle(hFile, &fileInfo);
	if (lGetFileRet == FALSE)
	{
		throw std::exception("Could not get file information");
	}

	size_t fileSize = 1;
	fileSize = (((size_t)fileInfo.nFileSizeHigh) << 32) + (size_t)fileInfo.nFileSizeLow;
	return fileSize;
}

#pragma optimize( "", off )
void MemoryMappedFile::TouchPages(Stopwatch &sw, bool bPrefetch, int sleepBeforeTouchMs)
{
	size_t fileSize = GetFileSize();

	if (bPrefetch)
	{
		WIN32_MEMORY_RANGE_ENTRY range;
		range.NumberOfBytes = fileSize;
		range.VirtualAddress = pFile;

		BOOL lPrefecth = ::PrefetchVirtualMemory(::GetCurrentProcess(), 1, &range, 0);
		if (!lPrefecth)
		{
			wprintf(L"PrefetchVirtualMemory failed with error code: %d\n", ::GetLastError());
		}
		::Sleep(2000);
	}

	sw.Start();

	byte *pStart = (byte *)pFile;
	int tmp = 0;
	for (size_t i = 0; i < fileSize; i+= 4096) // touch in 4 K blocks
	{
		tmp = *(pStart + i);
	}
}
#pragma optimize( "", on )

MemoryMappedFile::~MemoryMappedFile()
{
	if (hFile != nullptr && hFile != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(hFile);
	}

	if (hFileMapping != NULL)
	{
		::CloseHandle(hFileMapping);
	}

	if (pFile != NULL)
	{
		::UnmapViewOfFile(pFile);
	}
}
