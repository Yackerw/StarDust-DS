#ifndef __PCAUDIO
#define __PCAUDIO
#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	extern HANDLE audioMutex;
	extern void (*audioCallback)(void*, int);
	bool InitializeXAudio2();
	void UninitializeXAudio2();
#ifdef __cplusplus
}
#endif
#endif