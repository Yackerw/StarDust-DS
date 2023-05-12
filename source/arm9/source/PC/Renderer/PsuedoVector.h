#ifndef __OGLPVECTOR
#define __OGLPVECTOR
#include <stdio.h>
#include <malloc.h>
#include <string.h>
typedef struct {
	int count;
	int allocSize;
	void* data;
	int itemSize;
} PsuedoVector;

void PVectorAppend(PsuedoVector* pv, void* item);

void PVectorResize(PsuedoVector* pv, int size);

void FreePVector(PsuedoVector* pv);

void PVectorDelete(PsuedoVector* pv, int id);

#endif