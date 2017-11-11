#include "stdafx.h"
#include "Program.h"
#include <process.h>
#include <psapi.h>
#include <random>
#include <memory>
#include <array>
#include <mutex>

#include "Stopwatch.h"

using namespace FastPageFault;

Program::Program(std::queue<std::wstring> &&args)
{
	_Args = std::move(args);

}

void Program::Help()
{
	const wchar_t* HelpStr =
		L"FastPageFault [-N dd [-lock] [-touchthreads n] [-file xxx [-flush] [-mapthreads n]]] [-filemap xxx [-flush]] [-createfile dd xxx]\n" \
		L"By Alois Kraus 2017 v1.0\n" \
		L"  -N ddd          Allocate and touch ddd MB of memory\n" \
		L"  -touchthreads n Touch allocated memory by 1 up to n threads where each thread touches N/n bytes of memory to simulate a concurrent touch. Use n=all to run from 1-n hardware threads.\n" \
		L"  -lock           Lock allocated memory (-N ddd) with VirtualLock before touching pages\n" \
		L"  -file xxx       Execute map/touch/unmap in a loop until the touch threads have finished measuring the soft page fault performance\n" \
		L"   -flush         Flush the file system cache for the file before reading memory mapped file contents.\n" \
		L"   -mapthreads n  Read the memory mapped file from n threads in a loop until the main touch operation has completed.\n" \
		L"  ===== Memory Copy Tests =====\n" \
		L"  -memcopy N        Copy from an equally sized source buffer data to a destination buffer which is on first copy soft faulted into the current process\n" \
		L"  -memcopythreads n Copy from 1 up to n threads N/n bytes from its own thread to determine when the soft page fault spin lock overhead becomes bigger than the gains from a parallel memcpy\n" \
		L"                    If n=all then the test is repeated performed in steps from 1 up to all physical cores.\n" \
		L"  ===== File Mapping Tests =====\n" \
		L"  -filemap xxx    Read a memory mapped file via page faults into memory\n" \
		L"    -prefetch     Execute PrefetchVirtualMemory and sleep for 10s before touching the pages\n" \
		L"  ===== Test Data Generation =====\n" \
		L"  -createfile dd xxx Create a test data file of dd MB of size. The written data is random and not repeated.\n" \
		L"\n" \
		L"Allocate dd MB of memory and touch the memory pages several times to measure the cost of soft page faults.\n" \
		L"Additionally you can read a large file from disk as memory mapped file from 1 or more threads several times to measure the\n" \
		L"impact of file mapping induced soft and hard page faults\n" \
		L"Examples\n" \
		L"Allocate 2 GB of memory and access the first byte of every page (4096 bytes) from 1 up to as many threads as the CPU has cores.\n" \
		L"  FastPageFault -N 2000 -touchthreads all\n" \
		L"Create a 1000 MB data file with random data for later usage.\n" \
		L"  FastPageFault -createfile 1000 c:\\1000MB.data\n" \
		L"Allocate 2 GB of memory and read at the same time from 1-2 threads the 1GB file in a loop\n" \
		L"  FastPageFault -N 2000 -file c:\\1GB.data -threads all\n" \
		L"Allocate 2 GB of memory and lock it with VirtualLock while reading a memory mapped file in a loop\n" \
		L"  FastPageFault -N 2000 -lock -file c:\\1GB.data\n" \
		L"Allocate 2 GB of memory and read a 1GB file from the physical disk as memory mapped file in a loop\n" \
		L"  FastPageFault -N 2000 -file c:\\1GB.data -flush\n";

	wprintf(HelpStr);
	if (_Errors.size() > 0)
	{
		wprintf(L"");
	}
	for (auto error : _Errors)
	{
		wprintf(error.c_str());
	}
}


void Program::Execute()
{
	switch (_Action)
	{
	case Action::CreateFile:
		CreateTestFile();
		break;
	case Action::Memory:
		AllocateTest();
		break;
	case Action::FileMap:
		FileMappingTest();
		break;
	case Action::MemCpy:
		MemCopyTest();
		break;
	default:
		wprintf(L"Invalid Execution Action: %d\n", _Action);
	}
}
void Program::AllocateTest()
{
	std::vector<std::thread> mapThreads;

	// Execute concurrently a file mapping operation which will map the entire file 
	// and touch all mapped pages as soft or hard faults depending on the settings
	// This is done in a loop until the main thread has finished soft faulting all of its pages via the _bFinishTouching flag
	// to ensure that we measure the file map/touch/unmap overhead for the complete duration while we are soft faulting memory pages
	// into our working set
	if (!_FileName.empty())
	{
		for (int i = 0; i < _MapThreadCount; i++)
		{
			mapThreads.push_back(std::thread([=]()
			{
				while (!_bFinishTouching)
				{
					MemoryMappedFile file(_FileName, _bFlushFileSystemCache);
					Stopwatch dummy;
					file.TouchPages(dummy);
				}
			}));
		}
	}

	::Sleep(10); // Give the threads some time to start

	AllocateAndTouchMemory(_BytesToAllocate);

	_bFinishTouching = true; // stop file map/touch/unmap cycle when we have finished measuring the soft page fault performance
	// wait for running reader threads
	for (auto &t : mapThreads)
	{
		t.join();
	}
}

void Program::AllocateAndTouchMemory(size_t N)
{
	Stopwatch sw;
	sw.Start();

	wprintf(L"Threads\tSize_MB\tTime_ms\tus/Page\tMB/s\tScenario\n");

	for (int nTouch = 1; nTouch <= _TouchThreads; nTouch++)
	{
		void *pBuffer = VirtualAlloc(N);
		if (pBuffer == nullptr)
		{
			return;
		}

		if (_bLockPages)
		{
			LockMemory(pBuffer, N);
		}
		auto AllocTime = sw.Stop();

		sw.Start();
		// The overhead to create a new thread and get it running is normally
		// well below 1ms. We can do this also in the single threaded case without loosing accuracy.
		std::vector<std::thread> touchThreads;
		__int64 bytesPerThread = _BytesToAllocate / nTouch;

		for (int i = 0; i < nTouch; i++)
		{
			touchThreads.push_back(std::thread([=]
			{
				Touch(((int *)pBuffer) + i* bytesPerThread / 4, bytesPerThread);
			}
			));
		}

		for (auto &t : touchThreads)
		{
			t.join();
		}

		auto touchTime = sw.Stop();
		float MB = (float)(N / (1024LL * 1024));
		float s = (float)touchTime.count() / 1000.0f;
		printf("%d\t%.0f\t%lld\t%.3f\t%.0f\tTouch 1\n", nTouch, MB, touchTime.count(), AveragePageAccessTimeInus(touchTime, N), MB / s);

		sw.Start();
		Touch(pBuffer, N);
		auto touchTime2 = sw.Stop();
		printf("%d\t%.0f\t%lld\t%.3f\tN.a.\tTouch 2\n", nTouch, MB, touchTime2.count(), AveragePageAccessTimeInus(touchTime2, N));
		VirtualFree(pBuffer);
	}

}

// Touch is from the compiler point of view a nop operation with no observable side effect 
// This is true from a pure data content point of view but performance wise there is a huge
// difference. Turn optimizations off to prevent the compiler to outsmart us.
#pragma optimize( "", off )
void Program::Touch(void *p, size_t N)
{
	char *pB = (char *)p;
	char tmp;
	for (size_t i = 0; i < N; i += 4096)
	{
		tmp = pB[i];
	}

}
#pragma optimize("", on)

// Copy memory from a source to a destination buffer where the source buffer is fully initialized and zeroed. 
// The destination buffer is not yet touched and the first time subject to soft page faults.
// To speed up the sequential memcpy we use 1-nThread threads to copy from each thread a portion of the array to the destination
// to measure the cost of concurrent page faults along with concurrent memcpy.
// This will increase the soft page fault time due to internal locks in the page fault implementation while the added concurrency will 
// reduce the overall memcpy time.
// On the second run we will effectively measure the memory bandwidth with this test.
void Program::MemCopyTest()
{
	wprintf(L"Threads\tSize_MB\tTime_ms\tus/Page\tMB/s\tScenario\n");

	float maxMBs = 0.f;

	for (int nThread = 1; nThread <= _MemCopyThreads; nThread++)
	{
		void *pSource = VirtualAlloc(_BytesToMemCopy);
		void *pDest = VirtualAlloc(_BytesToMemCopy);

		::ZeroMemory(pSource, _BytesToMemCopy);

		for (int run = 0; run < 2; run++)
		{
			std::vector<std::thread> copyThreads;

			Stopwatch sw;
			__int64 sizePerThread = _BytesToMemCopy / nThread;

			for (int i = 0; i < nThread; i++)
			{
				copyThreads.push_back(std::thread([=]
				{
					memcpy(((byte *)pDest) + i*sizePerThread, ((byte *)pSource) + i*sizePerThread, sizePerThread);
				}));
			}

			for (auto &t : copyThreads)
			{
				t.join();
			}
			auto ms = sw.Stop();
			auto MB = _BytesToMemCopy / (1024LL * 1024LL);
			float MBs = MB / (ms.count() / 1000.0f);
			wprintf(L"%d\t%lld\t%lld\t%.3f\t%.0f\tTouch_%d\n", nThread, MB, ms.count(), AveragePageAccessTimeInus(ms, _BytesToMemCopy), MBs, run + 1);
			maxMBs = max(maxMBs, MBs);
		}

		VirtualFree(pSource);
		VirtualFree(pDest);
	}

	if (_MemCopyThreads > 3)
	{
		wprintf(L"Estimated duplex memory bandwidth: %.0f MB/s\n", maxMBs);
	}
}


/// Create test file with random data 
void Program::CreateTestFile()
{
	HANDLE h = FileExtensions::CreateWriteableFile(_FileName);
	const int BufferSize = 1 * 1024 * 1024;

	std::unique_ptr<byte> buffer(new byte[BufferSize]);
	std::random_device rand;

	DWORD dwWritten = 0;
	BOOL lWrite;
	for (__int64 offset = 0; offset < _BytesToAllocate; offset += BufferSize)
	{
		// fill in random data to prevent dirty tricks of OS to treat it as empty pages or 
		// to employ memory compression, page sharing and such things
		for (int i = 0; i < BufferSize/4; i++)
		{
			((int *)buffer.get())[i] = rand();
		}

		lWrite = ::WriteFile(h, buffer.get(), BufferSize, &dwWritten, nullptr);
		if (lWrite == FALSE)
		{
			wprintf(L"Error: Could not write to file %s, LastError: %d\n", _FileName.c_str(), ::GetLastError());
			break;
		}
	}

	if (lWrite)
	{
		wprintf(L"Created file %s of size %lld MB\n", _FileName.c_str(), _BytesToAllocate / (1024LL * 1024LL));
	}
	::CloseHandle(h);
}

///
void Program::FileMappingTest()
{
	MemoryMappedFile mem(_FileName, _bFlushFileSystemCache);
	Stopwatch sw;
	mem.TouchPages(sw, _bPrefetch);
	auto ms = sw.Stop();
	float MB = (float)(mem.GetFileSize() / (1024LL * 1024LL));
	float s = ((float)ms.count() / 1000.0f);
	wprintf(L"Read file %s in %lldms with %.0f MB/s, %.3fus/page", _FileName.c_str(), ms.count(), MB/s, AveragePageAccessTimeInus(ms, mem.GetFileSize()));
}



void Program::LockMemory(void *pBuffer, const size_t N)
{
	PROCESS_MEMORY_COUNTERS counters;
	BOOL ret = ::GetProcessMemoryInfo(::GetCurrentProcess(), &counters, sizeof(counters));
	if (!ret)
	{
		wprintf(L"GetProcessMemory failed: %d\n", ::GetLastError());
		return;
	}

	auto setws = SetProcessWorkingSetSize(::GetCurrentProcess(), counters.WorkingSetSize + 2500uL * 1024 * 1024, 3000uL * 1024 * 1024);

	Stopwatch sw;
	sw.Start();
	BOOL lLock = VirtualLock((byte *)pBuffer, N);
	auto lockTime = sw.Stop();

	wprintf(L"Locked %lld MB in %llums, %.3fus/page\n",
		N / (1024LL * 1024), 
		lockTime.count(),
		AveragePageAccessTimeInus(lockTime, N));

	if (lLock == FALSE)
	{
		wprintf(L"VirtualLock failed with %d\n", ::GetLastError());
	}
}

bool Program::Parse()
{
	bool lret = true;
	int nAllCores = std::thread::hardware_concurrency(); // up to all cores

	auto argsMap = std::map<std::wstring, std::function<void()>>{
		{ L"-flush",  [=]() { _bFlushFileSystemCache = true; } },
		{ L"-file", [=]() { _FileName = GetNextArg(); } },
		{ L"-createfile", [=]() {  _BytesToAllocate = 1024LL * 1024LL * ConvertToInt(GetNextArg());
								   _FileName = GetNextArg();
								   _Action = Action::CreateFile;
								} },
		{ L"-filemap", [=]() {  _FileName = GetNextArg();
								_Action = Action::FileMap;
							 } },

		{ L"-prefetch", [=]() { _bPrefetch = true; } },
		{ L"-lock", [=]() { _bLockPages = true; } },
		{ L"-N", [=]() { _BytesToAllocate = 1024LL * 1024LL * ConvertToInt(GetNextArg());  } },
		{ L"-memcopy", [=]() { _BytesToMemCopy = 1024LL * 1024LL * ConvertToInt(GetNextArg());
							 _Action = Action::MemCpy;
							 } },
		{ L"-memcopythreads", [=]() { _MemCopyThreads = ConvertToInt(GetNextArg(), L"all", nAllCores); } },
		{ L"-touchthreads", [=]() { _TouchThreads = ConvertToInt(GetNextArg(), L"all", nAllCores); } },
		{ L"-mapthreads", [=]() { _MapThreadCount = ConvertToInt(GetNextArg()); } },
	};

	while (_Args.size() > 0)
	{
		auto currentArg = _Args.front();
		_Args.pop();
		auto action = argsMap[currentArg];
		if (action == nullptr)
		{
			_Errors.push_back(StringExtensions::Format(L"Error: Invalid argument: %s detected\n", currentArg.c_str()));
			lret = false;
		}
		else
		{
			action();
		}
	}

	if (_Action == Action::None)
	{
		_Action = Action::Memory;
	}

	if (_BytesToAllocate == 0 &&  _Action == Action::Memory)
	{
		lret = false;
		_Errors.push_back(L"Error: Invalid parameter passed to -N\n");
	}

	if (_BytesToAllocate == 0 && _Action == Action::CreateFileW  )
	{
		lret = false;
		_Errors.push_back(L"Error: Invalid parameter passed to -createfile as file size\n");
	}

	if (_TouchThreads == 0 && _Action == Action::Memory)
	{
		lret = false;
		_Errors.push_back(L"Error: Invalid or no parameter passed to -touchthreads\n");
	}

	if (_BytesToMemCopy == 0 && _Action == Action::MemCpy)
	{
		lret = false;
		_Errors.push_back(L"Error: Invalid parameter passed to -memcopy which was not a valid size of buffer to copy in MB\n");
	}

	if (_MemCopyThreads == 0 && _Action == Action::MemCpy)
	{
		lret = false;
		_Errors.push_back(L"Error: Invalid or no parameter passed to memcopythreads\n");
	}

	if (!_FileName.empty() && _Action == Action::Memory && !FileExtensions::FileExists(_FileName))
	{
		lret = false;
		_Errors.push_back( StringExtensions::Format(L"Error: File %s was not found to read\n", _FileName.c_str()) );
	}


	return lret;
}

int Program::ConvertToInt(const std::wstring &arg, const std::wstring &specialStr, int specialInt)
{
	int lret = 0;
	if (arg == specialStr)
	{
		lret = specialInt;
	}
	else
	{
		lret = _wtoi(arg.c_str());
	}

	return lret;
}

std::wstring Program::GetNextArg()
{
	std::wstring str;
	if (_Args.size() > 0)
	{
		str = _Args.front();
		_Args.pop();
	}
	return str;
}

void * Program::VirtualAlloc(size_t n)
{
	void *lret = ::VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	if (lret == NULL)
	{
		wprintf(L"VirtualAlloc failed. Error: %ld\n", GetLastError());
	}

	return lret;
}

void Program::VirtualFree(void *pMemory)
{
	BOOL lret = ::VirtualFree(pMemory, 0, MEM_RELEASE);
	if (lret == FALSE)
	{
		wprintf(L"Error: Could not free memory. LastError: %d\n", ::GetLastError());
	}
}

Program::~Program()
{
}
