#include "PsuedoVector.h"

void PVectorAppend(PsuedoVector* pv, void* item) {
	if (pv->count >= pv->allocSize) {
		pv->allocSize = pv->count + 32;
		if (pv->data == NULL) {
			pv->data = malloc(pv->itemSize * pv->allocSize);
		}
		else {
			pv->data = realloc(pv->data, pv->itemSize * pv->allocSize);
		}
	}
	// doesn't accept void* for some reason
	memcpy((void*)((long)pv->data + pv->count * pv->itemSize), item, pv->itemSize);
	++pv->count;
}

void PVectorResize(PsuedoVector* pv, int size) {
	if (pv->data == NULL) {
		pv->data = malloc(size * pv->itemSize);
	}
	else if (size > pv->allocSize) {
		pv->data = realloc(pv->data, size * pv->itemSize);
		pv->allocSize = size;
	}
	pv->count = size;
}

void FreePVector(PsuedoVector* pv) {
	if (pv->data != NULL) {
		free(pv->data);
	}
}

void PVectorDelete(PsuedoVector* pv, int id) {
	if (id > pv->count) {
		return;
	}
	if (id < 0) {
		return;
	}
	if (pv->count != id) {
		memmove((void*)(((long)pv->data) + id * pv->allocSize), (void*)(((long)pv->data) + (id + 1) * pv->allocSize), pv->allocSize * (pv->count - id));
	}
	--pv->count;
}