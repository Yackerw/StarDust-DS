#ifndef SDDELTA
#define SDDELTA
#include "sdmath.h"
extern f32 deltaTime;
extern bool deltaTimeEngine;
void InitDeltaTime();
void UpdateDeltaTime();
void StartBenchmark();
int StopBenchmark();
#endif