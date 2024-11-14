#include "sdfile.h"
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <nds.h>
#include "sdsound.h"
#include <stdint.h>
#ifndef _NOTDS
#include <calico.h>
#endif

typedef struct AsyncReadData AsyncReadData;

struct AsyncReadData {
	void* buffer;
	int size;
	FILE* f;
	void (*callBack)(void* data, bool success);
	void* callBackData;
	int id;
	int priority;
	bool running;
	AsyncReadData* prev;
	AsyncReadData* next;
};

AsyncReadData firstAsyncRead;
int currArdId;

volatile bool vBlanked;

#ifndef _NOTDS

// realistically, shouldn't need THAT much thread stack
alignas(8) unsigned char ioThreadStack[2048];
Thread ioThread;
RMutex ioMutex;
volatile bool ioThreadRunning;
unsigned char* ioThreadLocalStorage;

int AsyncIOHandler(void* notUsed) {
	while (true) {
		// claim mutex to ensure we have no thread conflicts
		rmutexLock(&ioMutex);

		AsyncReadData* currArd = firstAsyncRead.next;
		AsyncReadData* dataToRead = NULL;
		int currPrio = 100;
		while (currArd != NULL) {
			if (currArd->priority < currPrio) {
				currPrio = currArd->priority;
				dataToRead = currArd;
			}
			currArd = currArd->next;
		}

		if (dataToRead == NULL) {
			// failed to find any data to read! we're done here!
			ioThreadRunning = false;
			// we need to keep the mutex locked until here so it doesn't mistakenly think we're still running the thread
			rmutexUnlock(&ioMutex);
			return 1;
		}

		dataToRead->running = true;

		rmutexUnlock(&ioMutex);

		// okay, read!
		fread_MusicYielding(dataToRead->buffer, dataToRead->size, 1, dataToRead->f);
		rmutexLock(&ioMutex);
		if (dataToRead->callBack != NULL) {
			dataToRead->callBack(dataToRead->callBackData, true);
		}
		if (dataToRead->next != NULL) {
			dataToRead->next->prev = dataToRead->prev;
		}
		dataToRead->prev->next = dataToRead->next;

		free(dataToRead);

		rmutexUnlock(&ioMutex);
	}
	ioThreadRunning = false;
	return 1;
}

char* DirToNative(char* input) {
	char* retValue = (char*)malloc(strlen(input) + 1);
	strcpy(retValue, input);
	return retValue;
}

void fread_MusicYielding(void* buffer, int size, int count, FILE* f) {
	size *= count;
	while (size > 0x1000) {
		fread(buffer, 0x1000, 1, f);
		buffer = (void*)((uint32_t)buffer + 0x1000);
		size -= 0x1000;
		UpdateMusicBuffer();
	}
	fread(buffer, size, 1, f);
	UpdateMusicBuffer();
}

int fread_Async(void* buffer, int size, int count, FILE* f, int priority, void (*callBack)(void* data, bool success), void* callBackArgument) {
	AsyncReadData* ard = (AsyncReadData*)malloc(sizeof(AsyncReadData));
	ard->buffer = buffer;
	ard->size = size * count;
	ard->f = f;
	ard->callBack = callBack;
	ard->callBackData = callBackArgument;
	ard->next = NULL;
	ard->running = false;

	// set up thread data
	rmutexLock(&ioMutex);
	AsyncReadData* linkedArd = &firstAsyncRead;
	while (linkedArd->next != NULL) {
		linkedArd = linkedArd->next;
	}
	ard->prev = linkedArd;
	linkedArd->next = ard;
	ard->id = currArdId;
	ard->priority = priority;
	++currArdId;

	// if thread isn't active, start it
	if (!ioThreadRunning) {
		ioThreadRunning = true;
		threadPrepare(&ioThread, AsyncIOHandler, NULL, &ioThreadStack[2048], 0x2C);
		if (ioThreadLocalStorage == NULL) {
			ioThreadLocalStorage = (unsigned char*)malloc(threadGetLocalStorageSize());
		}
		memset(ioThreadLocalStorage, 0, threadGetLocalStorageSize());
		threadAttachLocalStorage(&ioThread, ioThreadLocalStorage);
		threadStart(&ioThread);
	}

	rmutexUnlock(&ioMutex);

	return ard->id;
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
int fread_Async(void* buffer, int size, int count, FILE* f, int priority, void (*callBack)(void* data, bool success), void* callBackArgument) {
	fread(buffer, size, count, f);
	if (callBack != NULL)
		callBack(callBackArgument, true);
	return -1;
}
#endif

bool CancelAsyncRead(int id) {
#ifndef _NOTDS
	rmutexLock(&ioMutex);
#endif
	AsyncReadData* ard = firstAsyncRead.next;
	while (ard != NULL) {
		if (ard->id == id) {
			if (ard->running) {
#ifndef _NOTDS
				rmutexUnlock(&ioMutex);
#endif
				return false;
			}
			if (ard->callBack != NULL) {
				ard->callBack(ard->callBackData, false);
			}
			ard->prev->next = ard->next;
			if (ard->next != NULL) {
				ard->next->prev = ard->prev;
			}
			free(ard);
#ifndef _NOTDS
			rmutexUnlock(&ioMutex);
#endif
			return true;
		}
		ard = ard->next;
	}
#ifndef _NOTDS
	rmutexUnlock(&ioMutex);
#endif
	return false;
}

bool CheckAsyncReadRunning(int id) {
#ifndef _NOTDS
	rmutexLock(&ioMutex);
#endif
	AsyncReadData* ard = firstAsyncRead.next;
	while (ard != NULL) {
		if (ard->id == id) {
#ifndef _NOTDS
			rmutexUnlock(&ioMutex);
#endif
			return true;
		}
	}
#ifndef _NOTDS
	rmutexUnlock(&ioMutex);
#endif
	return false;
}