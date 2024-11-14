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
	timerStart(1, ClockDivider_1024, 0, NULL);
	timerElapsed(1);
}

void UpdateDeltaTime() {
	deltaTime = timerElapsed(1);
	deltaTime = deltaTime / 8;
	deltaTime = Min(deltaTime, 409);
}

// TODO: needs to be updated to get calico's timer!
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
	deltaTime = (newDelta / deltaFrequencyFloat) * 4096;
	deltaTime = Min(deltaTime, 490);
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