#include "sdfile.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <nds.h>
#include "sdsound.h"
#include <stdint.h>

typedef struct AsyncReadData AsyncReadData;

struct AsyncReadData {
	void* buffer;
	int size;
	FILE* f;
	void (*callBack)(void* data, bool success);
	void* callBackData;
	int id;
	AsyncReadData* prev;
	AsyncReadData* next;
};

AsyncReadData firstAsyncRead;
int currArdId;

volatile bool vBlanked;

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
		buffer = (uint32_t)buffer + 0x10000;
		size -= 0x10000;
		UpdateMusicBuffer();
	}
	fread(buffer, size, 1, f);
	UpdateMusicBuffer();
}

int fread_Async(void* buffer, int size, int count, FILE* f, void (*callBack)(void* data, bool success), void* callBackArgument) {
	AsyncReadData* ard = (AsyncReadData*)malloc(sizeof(AsyncReadData));
	ard->buffer = buffer;
	ard->size = size * count;
	ard->f = f;
	ard->callBack = callBack;
	ard->callBackData = callBackArgument;
	ard->next = NULL;
	AsyncReadData* linkedArd = &firstAsyncRead;
	while (linkedArd->next != NULL) {
		linkedArd = linkedArd->next;
	}
	ard->prev = linkedArd;
	linkedArd->next = ard;
	ard->id = currArdId;
	++currArdId;
	return ard->id;
}

void AsyncFileHandler() {
	vBlanked = false;
	bool vBlankedThisFrame = false;
	irqEnable(IRQ_VCOUNT);
	// add some leeway...
	while (!vBlanked) {
		AsyncReadData* ard = firstAsyncRead.next;
		if (ard != NULL) {
			if (ard->size > 0x400) {
				fread(ard->buffer, 0x400, 1, ard->f);
				ard->buffer = (void*)((uint32_t)ard->buffer + 0x400);
				ard->size -= 0x400;
			}
			else {
				fread(ard->buffer, ard->size, 1, ard->f);
				if (ard->callBack != NULL) {
					ard->callBack(ard->callBackData, true);
				}
				firstAsyncRead.next = ard->next;
				if (ard->next != NULL)
					ard->next->prev = &firstAsyncRead;
				free(ard);
			}
		}
		else {
			glFlush(0);
			swiWaitForVBlank();
			vBlankedThisFrame = true;
			vBlanked = true;
		}
	}
	if (!vBlankedThisFrame) {
		glFlush(0);
		swiWaitForVBlank();
	}
	irqDisable(IRQ_VCOUNT);
}

void AsyncFileHandlerFlush() {
	vBlanked = true;
}

void InitializeAsyncFiles() {
	irqSet(IRQ_VCOUNT, AsyncFileHandlerFlush);
	REG_DISPSTAT |= 172 << 8;
	irqDisable(IRQ_VCOUNT);
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

// not truly async on PC since PCs should be able to load DS size files instantly...!
int fread_Async(void* buffer, int size, int count, FILE* f, void (*callBack)(void* data, bool success), void* callBackArgument) {
	fread(buffer, size, count, f);
	if (callBack != NULL)
		callBack(callBackArgument, true);
	return -1;
}

void AsyncFileHandler() {

}

void InitializeAsyncFiles() {

}
#endif

void CancelAsyncRead(int id) {
	AsyncReadData* ard = firstAsyncRead.next;
	while (ard != NULL) {
		if (ard->id == id) {
			if (ard->callBack != NULL) {
				ard->callBack(ard->callBackData, false);
			}
			ard->prev->next = ard->next;
			if (ard->next != NULL) {
				ard->next->prev = ard->prev;
			}
			free(ard);
			return;
		}
		ard = ard->next;
	}
}

bool CheckAsyncReadRunning(int id) {
	AsyncReadData* ard = firstAsyncRead.next;
	while (ard != NULL) {
		if (ard->id == id) {
			return true;
		}
	}
	return false;
}