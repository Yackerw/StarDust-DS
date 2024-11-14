#ifndef __SDFILE
#define __SDFILE
#include <stdbool.h>
#include <stdio.h>

extern volatile bool ioThreadRunning;

char* DirToNative(char* input);

void fread_MusicYielding(void* buffer, int size, int count, FILE* f);

int fread_Async(void* buffer, int size, int count, FILE* f, int priority, void (*callBack)(void* data, bool success), void* callBackArgument);

// if returns true, then the read was cancelled successfully. if it returns false, then it failed to cancel it; either because it doesn't exist, or because it's actively being read on another thread
// and can no longer be cancelled. to determine which, you could use CheckAsyncReadRunning, or your callback
bool CancelAsyncRead(int id);

bool CheckAsyncReadRunning(int id);

#endif