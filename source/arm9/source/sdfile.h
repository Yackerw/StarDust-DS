#ifndef __SDFILE
#define __SDFILE
#include <stdbool.h>
#include <stdio.h>

char* DirToNative(char* input);

void fread_MusicYielding(void* buffer, int size, int count, FILE* f);

#endif