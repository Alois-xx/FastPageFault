#pragma once
#include <queue>
#include <string>
#include <tchar.h>
#include <chrono>

namespace FastPageFault
{
	class Program
	{
	public:
		Program(std::queue<std::wstring> &&args);
		bool Parse();
		void Execute();
		void Help();
		~Program();
	private: // Program dependent methods
		void LockMemory(void *pBuffer, const size_t N);
		void Touch(void *p, size_t N);
		float AveragePageAccessTimeInus(std::chrono::milliseconds ms, const size_t NBytes)
		{
			return std::chrono::duration_cast<std::chrono::microseconds>(ms).count() * 1.0f / (1.0f * (NBytes / 4096));
		}
		void AllocateAndTouchMemory(size_t N);
		void AllocateTest();
		void FileMappingTest();
		void MemCopyTest();

		void CreateTestFile();
		void *VirtualAlloc(size_t n);
		void VirtualFree(void *pMemory);

	private: // Helper Methods
		int ConvertToInt(const std::wstring &arg, const std::wstring &specialStr=L"", int specialInt=0);
		std::wstring GetNextArg();

	private: // Program dependent flags 
		std::wstring _FileName;
		bool _bFlushFileSystemCache = false;
		bool _bLockPages =false;
		bool _bPrefetch = false;
		__int64 _BytesToAllocate =0;
		int _TouchThreads = 1;
		int _MemCopyThreads = 1;
		__int64 _BytesToMemCopy = 0;
		volatile bool _bFinishTouching = false;

		int _MapThreadCount;
		
		enum Action
		{
			None = 0,
			Memory = 1,
			CreateFile = 2,
			FileMap = 3,
			MemCpy = 4,
		};

		Action _Action = Action::None;

	private: // program independent variables
		std::queue<std::wstring> _Args;
		std::vector<std::wstring> _Errors;
	};

}