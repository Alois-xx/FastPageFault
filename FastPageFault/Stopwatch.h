#pragma once

#include <chrono>

class Stopwatch
{
public:
	Stopwatch()
	{
		_Start = std::chrono::high_resolution_clock::now();
	}

	void Start()
	{
		_Start = std::chrono::high_resolution_clock::now();
	}

	std::chrono::milliseconds Stop()
	{
		_Stop = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(_Stop - _Start);
	}
private:
	std::chrono::high_resolution_clock::time_point _Start;
	std::chrono::high_resolution_clock::time_point _Stop;
};


