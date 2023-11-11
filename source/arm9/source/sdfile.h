#ifndef __SDFILE
#define __SDFILE
#include <stdbool.h>
#include <stdio.h>

char* DirToNative(char* input);

void fread_MusicYielding(void* buffer, int size, int count, FILE* f);

int fread_Async(void* buffer, int size, int count, FILE* f, void (*callBack)(void* data, bool success), void* callBackArgument);

void AsyncFileHandler();

void CancelAsyncRead(int id);

bool CheckAsyncReadRunning(int id);

void InitializeAsyncFiles();

#endif