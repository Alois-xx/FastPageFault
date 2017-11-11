#pragma once
#include <string>
#include <windows.h>

struct FileExtensions
{
	static bool FileExists(std::wstring &szPath)
	{
		DWORD dwAttrib = GetFileAttributes(szPath.c_str());

		return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	}

	static HANDLE CreateWriteableFile(std::wstring &szPath)
	{
		HANDLE hFile = ::CreateFile(szPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_RANDOM_ACCESS, nullptr);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			throw std::exception("Could not open file");
		}

		return hFile;
	}
private:

	FileExtensions();
	~FileExtensions();
};

