// FastPageFault.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <queue>
#include "MemoryMappedFile.h"
#include "Program.h"

using namespace FastPageFault;

using namespace std::chrono_literals;


int wmain(int argc, wchar_t **argv)
{
	std::queue<std::wstring> args;
	for (int i = 1; i < argc; i++)
	{
		args.push(argv[i]);
	}

	Program p(std::move(args));
	if (p.Parse())
	{
		p.Execute();
	}
	else
	{
		p.Help();
	}

	return 0;;
}
#pragma optimize( "", on )
