#include "sdfile.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <nds.h>
#include "sdsound.h"

#ifndef _NOTDS
char* DirToNative(char* input) {
	char* retValue = (char*)malloc(strlen(input) + 1);
	strcpy(retValue, input);
	return retValue;
}

void fread_MusicYielding(void* buffer, int size, int count, FILE* f) {
	size *= count;
	while (size > 0x10000) {
		fread(buffer, size, 1, f);
		buffer = (uint)buffer + 0x10000;
		size -= 0x10000;
		UpdateMusicBuffer();
	}
	fread(buffer, size, 1, f);
	UpdateMusicBuffer();
}
#else
char* DirToNative(char* input) {
	if (memcmp(input, "sd:/", sizeof(char) * 4) == 0) {
		char* retValue = (char*)malloc(strlen(input) + 3);
		sprintf(retValue, "%s%s", "addons/", input + 4);
		return retValue;
	}
	if (memcmp(input, "fat:/", sizeof(char) * 5) == 0) {
		char* retValue = (char*)malloc(strlen(input) + 2);
		sprintf(retValue, "%s%s", "addons/", input + 5);
		return retValue;
	}
	if (memcmp(input, "nitro:/", sizeof(char) * 4) == 0) {
		char* retValue = (char*)malloc(strlen(input));
		sprintf(retValue, "%s%s", "data/", input + 7);
		return retValue;
	}
	// fallback; already native
	char* retValue = (char*)malloc(strlen(input) + 1);
	strcpy(retValue, input);
	return retValue;
}

void fread_MusicYielding(void* buffer, int size, int count, FILE* f) {
	fread(buffer, size, count, f);
}
#endif