#include <nds.h>
#include "sddelta.h"
#include "sdmath.h"
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>

LARGE_INTEGER prevDelta;
LARGE_INTEGER deltaFrequency;
float deltaFrequencyFloat;
#endif

f32 deltaTime;
bool deltaTimeEngine;

#ifndef _NOTDS
void InitDeltaTime() {
	timerStart(2, ClockDivider_1024, 0, NULL);
	timerElapsed(2);
}

void UpdateDeltaTime() {
	deltaTime = timerElapsed(2);
	deltaTime = deltaTime / 8;
	deltaTime = Min(deltaTime, 409);
}

void StartBenchmark() {
	timerStart(1, ClockDivider_64, 0, NULL);
}

int StopBenchmark() {
	int retValue = timerElapsed(1);
	timerStop(1);
	return retValue * 128;
}
#endif
#ifdef _WIN32

unsigned long long benchmarkTime;

void InitDeltaTime() {
	QueryPerformanceCounter(&prevDelta);
	QueryPerformanceFrequency(&deltaFrequency);
	deltaFrequencyFloat = deltaFrequency.QuadPart;
}

void UpdateDeltaTime() {
	LARGE_INTEGER delta;
	QueryPerformanceCounter(&delta);
	unsigned long long newDelta = delta.QuadPart - prevDelta.QuadPart;
	prevDelta = delta;
	deltaTime = newDelta / deltaFrequencyFloat;
	deltaTime = Min(deltaTime, 0.1f);
}

void StartBenchmark() {
	QueryThreadCycleTime(GetCurrentThread(), &benchmarkTime);
}

int StopBenchmark() {
	long long bTime2;
	QueryThreadCycleTime(GetCurrentThread(), &bTime2);
	return bTime2 - benchmarkTime;
}

#endif