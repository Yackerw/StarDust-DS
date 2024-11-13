#include <nds.h>
#include <stdio.h>
#include "sdrender.h"
#include "sdmath.h"
#include <stdbool.h>
#include "sdfile.h"
#include <stdlib.h>
#include <stdint.h>

typedef struct {
	Vec3 position;
	m4x4 matrix;
	int materialId;
	SDMaterial subMat;
	Animator* animator;
	Model* model;
} ModelDrawCall;

typedef struct {
	Sprite* sprite;
	int x;
	int y;
	int spriteAlignX;
	int spriteAlignY;
	bool flipX;
	bool flipY;
	bool scaled;
	f32 xScale;
	f32 yScale;
} SpriteDrawCall;

SpriteDrawCall mainSpriteCalls[128];
SpriteDrawCall subSpriteCalls[128];
int mainSpriteCallCount;
int subSpriteCallCount;
int spriteMatrixId;

ModelDrawCall* modelDrawCalls;
int modelDrawCallCount;
int modelDrawCallAllocated;

#ifdef _WIN32
Shader *defaultShader;
Shader *defaultRiggedShader;
Shader *defaultSpriteShader;
Sprite* BGTexture;
RenderTexture* subScreenTexture;
unsigned short subBackground[32 * 32];
#endif

Vec3 cameraPosition;
Quaternion cameraRotation = {0, 0, 0, 4096};
f32 cameraFOV = 8192;
f32 cameraNear = 900;
f32 cameraFar = 1409600;
m4x4 cameraMatrix;
ViewFrustum frustum;

Vec3 lightNormal;
Vec3 nativeLightNormal;
int lightColor;
int ambientColor;

int bgID;

Texture startTexture;

typedef struct {
	FILE* f;
	Model* model;
	void (*callBack)(void* data, Model* model);
	void* callBackData;
	char* texDir;
} ModelCallbackData;

typedef struct {
	FILE* f;
	Texture* texture;
	void (*callBack)(void* data, Texture* model);
	void* callBackData;
} TextureCallbackData;

typedef struct {
	void (*callBack)(void* data);
	void* callBackData;
	Model* modelToTexture;
	int matId;
}ModelTexturesCallbackData;

typedef struct TextureQueue TextureQueue;

struct TextureQueue {
	char* textureToLoad;
	bool upload;
	void (*callBack)(void* data, Texture* texture);
	void* callBackData;
	FILE* f;
	Texture* tex;
	TextureQueue* prev;
	TextureQueue* next;
};

typedef struct {
	FILE* f;
	Sprite* sprite;
	bool sub;
	bool upload;
	void (*callBack)(void* data, Sprite* sprite);
	void* callBackData;
} SpriteCallbackData;

typedef struct {
	FILE* f;
	Animation* anim;
	void (*callBack)(void* data, Animation* anim);
	void* callBackData;
} AnimationCallbackData;

TextureQueue* firstTextureQueue;

void LoadModelTexturesCallback(void* data, Texture* texture) {
	ModelTexturesCallbackData* mtcbd = (ModelTexturesCallbackData*)data;

	if (texture != NULL) {
		mtcbd->modelToTexture->defaultMats[mtcbd->matId].texture = texture;
		++texture->numReferences;
	}

	if (mtcbd->callBack != NULL) {
		mtcbd->callBack(mtcbd->callBackData);
	}
	free(data);
}

void SetupModelFromMemory(Model* model, char* textureDir, bool asyncTextures, void (*asyncCallback)(void* data), void* asyncCallbackData) {
	Model* retValue = model;
	char* input = textureDir;
	retValue->vertexGroups = (VertexHeader*)((unsigned int)retValue->vertexGroups + (unsigned int)retValue);
	retValue->defaultMats = (SDMaterial*)((uint32_t)retValue + (uint32_t)retValue->defaultMats);
	retValue->materialTextureNames = (char*)((uint32_t)retValue + (uint32_t)retValue->materialTextureNames);
	if (retValue->skeleton != NULL)
	{
		retValue->skeleton = (Bone*)((uint32_t)retValue + (uint32_t)retValue->skeleton);
		for (int i = 0; i < retValue->skeletonCount; ++i) {
			m4x4 tempMtx;
			MatrixToDSMatrix(&retValue->skeleton[i].inverseMatrix, &tempMtx);
			memcpy(&retValue->skeleton[i].inverseMatrix, &tempMtx, sizeof(m4x4));
		}
	}
	char* currString = retValue->materialTextureNames;
	// do this before the loop for optimization
	int inputLen = strlen(input);
	// cut off everything after the last / or backslash
	for (; inputLen > 0; --inputLen) {
		if (input[inputLen] == "/"[0]) {
			inputLen++;
			break;
		}
	}
	char tmpString[512];
	memcpy(tmpString, input, inputLen);
	for (int i = 0; i < retValue->materialCount; ++i) {
		strcpy(tmpString + inputLen, currString);
		currString += strlen(currString);
		currString += 1;
		if (!asyncTextures) {
			retValue->defaultMats[i].texture = LoadTexture(tmpString, true);
			if (retValue->defaultMats[i].texture != NULL) {
				retValue->defaultMats[i].texture->numReferences += 1;
			}
		}
		else {
			retValue->defaultMats[i].texture = NULL;

			ModelTexturesCallbackData* callbackData = (ModelTexturesCallbackData*)malloc(sizeof(ModelTexturesCallbackData));

			if (i == retValue->materialCount - 1) {
				callbackData->callBack = asyncCallback;
				callbackData->callBackData = asyncCallbackData;
			}
			else {
				callbackData->callBack = NULL;
			}
			callbackData->matId = i;
			callbackData->modelToTexture = retValue;

			LoadTextureAsync(tmpString, true, LoadModelTexturesCallback, callbackData);
		}
	}

	CacheModel(retValue);

#ifdef _NOTDS
	UpdateModel(model);

#endif
}

Model *LoadModel(char *input) {
	char* nativeDir = DirToNative(input);
	FILE *f = fopen(nativeDir, "rb");
	free(nativeDir);
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Model *retValue = (Model*)malloc(fsize);
	fread_MusicYielding(retValue, fsize, 1, f);
	fclose(f);
	SetupModelFromMemory(retValue, input, false, NULL, NULL);
	return retValue;
}

Model* FreeModelKeepCache(Model* model) {
	if (model->NativeModel == NULL) {
		// no cache...
		return model;
	}
	// duplicate materials
	SDMaterial* mats = (SDMaterial*)malloc(sizeof(SDMaterial) * model->materialCount);
	memcpy(mats, model->defaultMats, sizeof(SDMaterial) * model->materialCount);
	Model* retValue = (Model*)calloc(sizeof(Model), 1);
	retValue->defaultMats = mats;
	retValue->materialCount = model->materialCount;
	retValue->NativeModel = model->NativeModel;
	retValue->boundsMin = model->boundsMin;
	retValue->boundsMax = model->boundsMax;
	retValue->defaultOffset = model->defaultOffset;
	retValue->defaultScale = model->defaultScale;
	retValue->skeletonCount = model->skeletonCount;
	if (retValue->skeletonCount != 0) {
		retValue->skeleton = (Bone*)malloc(sizeof(Bone) * retValue->skeletonCount);
		memcpy(retValue->skeleton, model->skeleton, sizeof(Bone) * retValue->skeletonCount);
	}
	// set up vertex headers
	retValue->vertexGroupCount = model->vertexGroupCount;
	retValue->vertexGroups = (VertexHeader*)malloc((sizeof(VertexHeader) * model->vertexGroupCount) - (sizeof(Vertex) * model->vertexGroupCount));
	VertexHeader* currHeader = retValue->vertexGroups;
	VertexHeader* modelHeader = model->vertexGroups;
	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		currHeader->material = modelHeader->material;
		currHeader->bitFlags = modelHeader->bitFlags;
		currHeader->count = 0;
		currHeader = (VertexHeader*)&currHeader->vertices;
		modelHeader = (VertexHeader*)((uint32_t)(&(modelHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (modelHeader->count)));
	}
	retValue->version = 0x80000000 | model->version;
	free(model);
	return retValue;
}

void LoadModelAsyncInitCallback(void* data) {
	ModelCallbackData* cbd = (ModelCallbackData*)data;
	cbd->callBack(cbd->callBackData, cbd->model);
	free(cbd);
}

void LoadModelAsyncCallback(void* data, bool success) {
	ModelCallbackData* cbd = (ModelCallbackData*)data;
	fclose(cbd->f);
	if (success) {
		SetupModelFromMemory(cbd->model, cbd->texDir, true, LoadModelAsyncInitCallback, cbd);
	}
	else {
		cbd->callBack(cbd->callBackData, NULL);
		free(cbd);
	}
}

int LoadModelAsync(char* input, void (*callBack)(void* data, Model* model), void* callBackData) {
	if (callBack == NULL) {
		// ?
		return -1;
	}
	char* nativeDir = DirToNative(input);
	FILE* f = fopen(nativeDir, "rb");
	free(nativeDir);
	if (f == NULL) {
		callBack(callBackData, NULL);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Model* retValue = (Model*)malloc(fsize);

	ModelCallbackData* cbd = (ModelCallbackData*)malloc(sizeof(ModelCallbackData));
	cbd->f = f;
	cbd->model = retValue;
	cbd->callBack = callBack;
	cbd->callBackData = callBackData;
	cbd->texDir = malloc(strlen(input) + 1);
	strcpy(cbd->texDir, input);

	return fread_Async((void*)retValue, fsize, 1, f, LoadModelAsyncCallback, cbd);
}

void LoadTextureFromQueue();

void UpdateTextureQueue() {
	TextureQueue* tmp = firstTextureQueue;
	firstTextureQueue = firstTextureQueue->next;
	if (firstTextureQueue != NULL)
		firstTextureQueue->prev = NULL;
	free(tmp->textureToLoad);
	free(tmp);
	if (firstTextureQueue != NULL)
		LoadTextureFromQueue();
}

void TextureAsyncCallback(void* data, bool success) {
	TextureQueue* tq = (TextureQueue*)data;
	fclose(tq->f);
	// once more...!
	Texture* tex = startTexture.next;
	while (tex != NULL) {
		if (strcmp(tq->textureToLoad, tex->name) == 0) {
			tq->callBack(tq->callBackData, tex);
			free(tq->tex);
			UpdateTextureQueue();
			return;
		}
		tex = tex->next;
	}

	// okay, we're good, initialize the texture properly
	LoadTextureFromRAM(tq->tex, tq->upload, tq->textureToLoad);

	tq->callBack(tq->callBackData, tq->tex);
	UpdateTextureQueue();
}

void LoadTextureFromQueue() {
	Texture* tex = startTexture.next;
	while (tex != NULL) {
		if (strcmp(firstTextureQueue->textureToLoad, tex->name) == 0) {
			firstTextureQueue->callBack(firstTextureQueue->callBackData, tex);
			UpdateTextureQueue();
			return;
		}
		tex = tex->next;
	}
	FILE* f = fopen(firstTextureQueue->textureToLoad, "rb");
	if (f == NULL) {
		firstTextureQueue->callBack(firstTextureQueue->callBackData, NULL);
		UpdateTextureQueue();
		return;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Texture* newTex = (Texture*)malloc(fsize);
	firstTextureQueue->f = f;
	firstTextureQueue->tex = newTex;
	fread_Async(newTex, fsize, 1, f, TextureAsyncCallback, firstTextureQueue);
}

void LoadTextureAsync(char* input, bool upload, void (*callBack)(void* data, Texture* texture), void* callBackData) {
	if (callBack == NULL) {
		// ?
		return;
	}
	Texture* tex = startTexture.next;
	while (tex != NULL) {
		if (strcmp(input, tex->name) == 0) {
			callBack(callBackData, tex);
			return;
		}
		tex = tex->next;
	}

	// okay, so this is a little dumb, but we don't want to repeatedly load textures we already have loaded. so, we set up a new queue here.
	// if there's no textures currently loading, we go ahead. otherwise, we add to the queue.
	// we have this in a custom queue rather than in the texture itself so that we can mix and match async texture loading and synchronous texture loading
	TextureQueue* newQueue = (TextureQueue*)malloc(sizeof(TextureQueue));
	newQueue->textureToLoad = DirToNative(input);
	newQueue->callBack = callBack;
	newQueue->upload = upload;
	newQueue->callBackData = callBackData;
	newQueue->next = NULL;
	newQueue->prev = NULL;
	if (firstTextureQueue != NULL) {
		TextureQueue* tq = firstTextureQueue;
		while (tq->next != NULL) {
			tq = tq->next;
		}
		tq->next = newQueue;
		newQueue->prev = tq;
	}
	else {
		firstTextureQueue = newQueue;
		LoadTextureFromQueue();
	}

}

#ifndef _NOTDS
unsigned int FIFOLookup[] = { FIFO_COMMAND_PACK(FIFO_NORMAL, FIFO_TEX_COORD, FIFO_VERTEX16, FIFO_NORMAL), FIFO_COMMAND_PACK(FIFO_TEX_COORD, FIFO_VERTEX16, FIFO_NORMAL, FIFO_TEX_COORD), FIFO_COMMAND_PACK(FIFO_VERTEX16, FIFO_NORMAL, FIFO_TEX_COORD, FIFO_VERTEX16) };
#endif

#define FIFO_MTX_RESTORE 0x50 >> 2

#ifndef _NOTDS
void CacheRiggedModel(Model* reference) {
	if (reference->skeletonCount > 30) {
		return;
	}
	DSNativeModel dsnm;
	dsnm.FIFOCount = reference->vertexGroupCount;
	dsnm.FIFOBatches = (unsigned int**)malloc(sizeof(unsigned int*) * reference->vertexGroupCount);
	VertexHeader* currHeader = &reference->vertexGroups[0];
	for (int i = 0; i < reference->vertexGroupCount; ++i) {
		int vertCount = currHeader->count;
		if (vertCount == 0) {
			dsnm.FIFOBatches[i] = NULL;
			uint32_t toAdd = (sizeof(Vertex) * (currHeader->count));
			currHeader = (VertexHeader*)(((uint32_t)(&(currHeader->vertices))) + toAdd);
			continue;
		}
		// ack...
		int FIFOCount;
		int NOPCount;
		FIFOCount = vertCount * 3 + 1;
		// iterate over verts as well and add 1 for each bone change
		Vertex* vertices = &currHeader->vertices;
		int currBone = -1;
		int FIFOCountLaterAddition = 0;
		for (int j = 0; j < vertCount; ++j) {
			if (vertices[j].boneID != currBone) {
				++FIFOCount;
				++FIFOCountLaterAddition;
				currBone = vertices[j].boneID;
			}
		}
		NOPCount = 4 - (FIFOCount % 4);
		if (NOPCount == 4) {
			NOPCount = 0;
		}
		FIFOCount /= 4;
		if (NOPCount != 0) {
			FIFOCount += 1;
		}
		int FIFOIterator = FIFOCount;
		// agghh
		FIFOCount += FIFOCountLaterAddition + vertCount * 4 + 1;
		// now generate FIFO batch
		unsigned int* FIFOBatch = (unsigned int*)malloc(sizeof(unsigned int) * (FIFOCount + 1));
		int currVert = 0;
		int FIFOBatchPosition = 6;
		// FIFO size
		FIFOBatch[0] = FIFOCount;
		// initial one with GFX BEGIN and such
		FIFOBatch[1] = FIFO_COMMAND_PACK(FIFO_BEGIN, FIFO_MTX_RESTORE, FIFO_NORMAL, FIFO_TEX_COORD);
		// pack the data
		if (currHeader->bitFlags & VTX_QUAD) {
			if (currHeader->bitFlags & VTX_STRIPS) {
				FIFOBatch[2] = GL_QUAD_STRIP;
			}
			else {
				FIFOBatch[2] = GL_QUAD;
			}
		}
		else {
			if (currHeader->bitFlags & VTX_STRIPS) {
				FIFOBatch[2] = GL_TRIANGLE_STRIP;
			}
			else {
				FIFOBatch[2] = GL_TRIANGLE;
			}
		}
		FIFOBatch[3] = vertices[0].boneID;
		FIFOBatch[4] = vertices[0].normal;
		FIFOBatch[5] = TEXTURE_PACK(vertices[0].u, vertices[0].v);

		int subVert = 3;
		currBone = vertices[0].boneID;
		while (currVert < vertCount) {
			unsigned char toPack[4];
			int storedBatchPosition = FIFOBatchPosition;
			++FIFOBatchPosition;
			for (int k = 0; k < 4; ++k) {
				switch (subVert) {
				case 1:
					if (currVert < vertCount) {
						toPack[k] = FIFO_NORMAL;
						FIFOBatch[FIFOBatchPosition] = vertices[currVert].normal;
					}
					else {
						toPack[k] = FIFO_NOP;
					}
					break;
				case 2:
					if (currVert < vertCount) {
						toPack[k] = FIFO_TEX_COORD;
						FIFOBatch[FIFOBatchPosition] = TEXTURE_PACK(vertices[currVert].u, vertices[currVert].v);
					}
					else {
						toPack[k] = FIFO_NOP;
					}
					break;
				case 3:
					if (currVert < vertCount) {
						toPack[k] = FIFO_VERTEX16;
						FIFOBatch[FIFOBatchPosition] = VERTEX_PACK(vertices[currVert].x, vertices[currVert].y);
						++FIFOBatchPosition;
						FIFOBatch[FIFOBatchPosition] = vertices[currVert].z;
						++currVert;
						subVert = -1;
					}
					else {
						toPack[k] = FIFO_NOP;
					}
					break;
				case 0:
					if (currVert < vertCount) {
						if (vertices[currVert].boneID != currBone) {
							toPack[k] = FIFO_MTX_RESTORE;
							FIFOBatch[FIFOBatchPosition] = vertices[currVert].boneID;
							currBone = vertices[currVert].boneID;
						}
						else {
							--k;
							++subVert;
							continue;
						}
					}
					else {
						toPack[k] = FIFO_NOP;
					}
					break;
				}
				++subVert;
				++FIFOBatchPosition;
			}
			FIFOBatch[storedBatchPosition] = FIFO_COMMAND_PACK(toPack[0], toPack[1], toPack[2], toPack[3]);
		}
		dsnm.FIFOBatches[i] = FIFOBatch;
		uint32_t toAdd = (sizeof(Vertex) * (currHeader->count));
		currHeader = (VertexHeader*)(((uint32_t)(&(currHeader->vertices))) + toAdd);
	}
	reference->NativeModel = malloc(sizeof(DSNativeModel));
	DSNativeModel* dsnmptr = (DSNativeModel*)reference->NativeModel;
	dsnmptr[0] = dsnm;
}
#endif

void CacheModel(Model* reference) {
#ifdef _NOTDS
	return;
#else
	if (reference->skeletonCount > 0) {
		CacheRiggedModel(reference);
		return;
	}
	DSNativeModel dsnm;
	dsnm.FIFOCount = reference->vertexGroupCount;
	dsnm.FIFOBatches = (unsigned int**)malloc(sizeof(unsigned int*) * reference->vertexGroupCount);
	// get vertex count to try and calculate FIFO count
	VertexHeader* currHeader = &reference->vertexGroups[0];
	for (int i = 0; i < reference->vertexGroupCount; ++i) {
		int vertCount = currHeader->count;
		if (vertCount == 0) {
			dsnm.FIFOBatches[i] = NULL;
			uint32_t toAdd = (sizeof(Vertex) * (currHeader->count));
			currHeader = (VertexHeader*)(((uint32_t)(&(currHeader->vertices))) + toAdd);
			continue;
		}
		// each vertex makes 3/4ths of a FIFOLookup, so multiply by 3, then modulo by 4 to get the remaining number of NOPs we need
		int FIFOCount;
		int NOPCount;
		// plus one for FIFO_BEGIN
		FIFOCount = vertCount * 3 + 1;
		NOPCount = 4 - (FIFOCount % 4);
		if (NOPCount == 4) {
			NOPCount = 0;
		}
		FIFOCount /= 4;
		if (NOPCount != 0) {
			FIFOCount += 1;
		}
		// 1 int for normal, 1 int for UV, 2 ints for position...so in other words: make space for arguments
		int FIFOIterator = FIFOCount;
		// plus one for FIFO_BEGIN argument
		FIFOCount += vertCount * 4 + 1;

		// now generate the FIFO batch
		unsigned int* FIFOBatch = (unsigned int*)malloc(sizeof(unsigned int) * (FIFOCount + 1));
		int currVert = 1;
		int FIFOBatchPosition = 7;
		int FIFOLookupId = 0;
		Vertex* currVerts = &currHeader->vertices;
		// FIFO size
		FIFOBatch[0] = FIFOCount;
		// initial one with GFX BEGIN and such
		FIFOBatch[1] = FIFO_COMMAND_PACK(FIFO_BEGIN, FIFO_NORMAL, FIFO_TEX_COORD, FIFO_VERTEX16);
		// pack the data
		if (currHeader->bitFlags & VTX_QUAD) {
			if (currHeader->bitFlags & VTX_STRIPS) {
				FIFOBatch[2] = GL_QUAD_STRIP;
			}
			else {
				FIFOBatch[2] = GL_QUAD;
			}
		}
		else {
			if (currHeader->bitFlags & VTX_STRIPS) {
				FIFOBatch[2] = GL_TRIANGLE_STRIP;
			}
			else {
				FIFOBatch[2] = GL_TRIANGLE;
			}
		}
		FIFOBatch[3] = currVerts[0].normal;
		FIFOBatch[4] = TEXTURE_PACK(currVerts[0].u, currVerts[0].v);
		FIFOBatch[5] = VERTEX_PACK(currVerts[0].x, currVerts[0].y);
		FIFOBatch[6] = currVerts[0].z;
		for (int j = 1; j < FIFOIterator; ++j) {
			FIFOBatch[FIFOBatchPosition] = FIFOLookup[FIFOLookupId];
			int FIFOCommandPos = FIFOBatchPosition;
			++FIFOBatchPosition;
			switch (FIFOLookupId) {
			case 0:
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].normal;
				FIFOBatch[FIFOBatchPosition++] = TEXTURE_PACK(currVerts[currVert].u, currVerts[currVert].v);
				FIFOBatch[FIFOBatchPosition++] = VERTEX_PACK(currVerts[currVert].x, currVerts[currVert].y);
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].z;
				++currVert;
				if (currVert >= currHeader->count) {
					FIFOBatch[FIFOCommandPos] = (FIFO_NOP << 24) | (FIFOBatch[FIFOCommandPos] & 0x00FFFFFF);
					break;
				}
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].normal;
				break;
			case 1:
				FIFOBatch[FIFOBatchPosition++] = TEXTURE_PACK(currVerts[currVert].u, currVerts[currVert].v);
				FIFOBatch[FIFOBatchPosition++] = VERTEX_PACK(currVerts[currVert].x, currVerts[currVert].y);
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].z;
				++currVert;
				if (currVert >= currHeader->count) {
					FIFOBatch[FIFOCommandPos] = (FIFO_NOP << 24) | (FIFO_NOP << 16) | (FIFOBatch[FIFOCommandPos] & 0x0000FFFF);
					break;
				}
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].normal;
				FIFOBatch[FIFOBatchPosition++] = TEXTURE_PACK(currVerts[currVert].u, currVerts[currVert].v);
				break;
			case 2:
				FIFOBatch[FIFOBatchPosition++] = VERTEX_PACK(currVerts[currVert].x, currVerts[currVert].y);
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].z;
				++currVert;
				if (currVert >= currHeader->count) {
					FIFOBatch[FIFOCommandPos] = (FIFO_NOP << 24) | (FIFO_NOP << 16) | (FIFO_NOP << 8) | (FIFOBatch[FIFOCommandPos] & 0x000000FF);
					break;
				}
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].normal;
				FIFOBatch[FIFOBatchPosition++] = TEXTURE_PACK(currVerts[currVert].u, currVerts[currVert].v);
				FIFOBatch[FIFOBatchPosition++] = VERTEX_PACK(currVerts[currVert].x, currVerts[currVert].y);
				FIFOBatch[FIFOBatchPosition++] = currVerts[currVert].z;
				++currVert;
				break;
			}
			++FIFOLookupId;
			FIFOLookupId %= 3;
		}
		dsnm.FIFOBatches[i] = FIFOBatch;
		uint32_t toAdd = (sizeof(Vertex) * (currHeader->count));
		currHeader = (VertexHeader*)(((uint32_t)(&(currHeader->vertices))) + toAdd);
	}
	reference->NativeModel = malloc(sizeof(DSNativeModel));
	DSNativeModel* dsnmptr = (DSNativeModel*)reference->NativeModel;
	dsnmptr[0] = dsnm;
#endif
}

void UpdateModel(Model* model) {
#ifdef _NOTDS
	Mesh* nativeModel = (Mesh*)calloc(sizeof(Mesh), 1);
	int vertCount = 0;
	VertexHeader* currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		vertCount += currHeader->count;
		currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
	}

	// copy verts
	Vec3f* nativeVerts = (Vec3f*)malloc(sizeof(Vec3f) * vertCount);
	int currVert = 0;
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			nativeVerts[currVert].x = f32tofloat(verts[j].x);
			nativeVerts[currVert].y = f32tofloat(verts[j].y);
			nativeVerts[currVert].z = f32tofloat(verts[j].z);
			++currVert;
		}
		currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
	}
	SetMeshVertices(nativeModel, nativeVerts, vertCount);
	free(nativeVerts);

	currVert = 0;
	// this is where it gets tricky...UVs...
	Vec2f* nativeUVs = (Vec2f*)malloc(sizeof(Vec2f) * vertCount);
	// let's just assume, for now, that UVs are typically stored relative to the texture as expected
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			nativeUVs[currVert].x = verts[j].u / 16.0f;
			nativeUVs[currVert].y = verts[j].v / 16.0f;
			++currVert;
		}
		currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
	}
	SetMeshUVs(nativeModel, nativeUVs, vertCount);
	free(nativeUVs);

	// normals
	Vec3f* nativeNormals = (Vec3f*)malloc(sizeof(Vec3f) * vertCount);
	currVert = 0;
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			uint32_t norm = verts[j].normal;
			int x = norm & 0x3FF;
			if (x & 0x200) {
				x = 0x1FF - (x & 0x1FF);
				x = -x;
			}
			int y = (norm >> 10) & 0x3FF;
			if (y & 0x200) {
				y = 0x1FF - (y & 0x1FF);
				y = -y;
			}
			int z = (norm >> 20) & 0x3FF;
			if (z & 0x200) {
				z = 0x1FF - (z & 0x1FF);
				z = -z;
			}
			nativeNormals[currVert].x = x / 511.0f;
			nativeNormals[currVert].y = y / 511.0f;
			nativeNormals[currVert].z = z / 511.0f;
			++currVert;
		}
		currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
	}
	nativeModel->normals = nativeNormals;
	nativeModel->normalCount = vertCount;

	// weights
	currVert = 0;
	currHeader = model->vertexGroups;

	int* boneIDs = (int*)calloc(sizeof(int) * vertCount * 4, 1);
	float* boneWeights = (float*)calloc(sizeof(float) * vertCount * 4, 1);

	for (int i = 0; i < model->vertexGroupCount; ++i) {
		// assign weights
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			boneIDs[currVert * 4] = verts[j].boneID;
			boneWeights[currVert * 4] = 1.0f;
			++currVert;
		}
		currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
	}
	nativeModel->boneIndex = boneIDs;
	nativeModel->weights = boneWeights;
	nativeModel->boneIndexCount = vertCount;
	nativeModel->weightCount = vertCount;

	// only create a new submesh for each material change
	int materialChangeCount = 0;
	currHeader = model->vertexGroups;
	int prevQuad = 0;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		if (currHeader->bitFlags & VTX_MATERIAL_CHANGE || (currHeader->bitFlags & VTX_QUAD) != prevQuad) {
			++materialChangeCount;
			prevQuad = currHeader->bitFlags & VTX_QUAD;
		}
		currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
	}

	prevQuad = 0;
	SetSubmeshCount(nativeModel, materialChangeCount);
	currVert = 0;
	currHeader = model->vertexGroups;
	int subMeshId = 0;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		// allocate enough memory for the whole thing until next incompatible material
		int triCount = 0;
		VertexHeader* headerIterator = currHeader;
		prevQuad = currHeader->bitFlags & VTX_QUAD;
		for (int j = i; j < model->vertexGroupCount; ++j) {
			if ((headerIterator->bitFlags & VTX_MATERIAL_CHANGE || (headerIterator->bitFlags & VTX_QUAD) != prevQuad) && j != i) {
				break;
			}
			if (!(headerIterator->bitFlags & VTX_QUAD)) {
				if (headerIterator->bitFlags & VTX_STRIPS) {
					triCount += 3 + (headerIterator->count - 3) * 3;
				}
				else {
					triCount += headerIterator->count;
				}
			}
			else {
				if (headerIterator->bitFlags & VTX_STRIPS) {
					// 2 verts for 2 triangles, times 3
					triCount += 6 + (headerIterator->count - 4) * 3;
				}
				else {
					triCount += (headerIterator->count / 4) * 6;
				}
			}
			prevQuad = currHeader->bitFlags & VTX_QUAD;
			headerIterator = (VertexHeader*)((uint32_t)(&(headerIterator->vertices)) + (uint32_t)(sizeof(Vertex) * headerIterator->count));
		}
		int* tris = (int*)calloc(sizeof(int) * triCount, 1);
		int triPos = 0;
		while (currHeader != headerIterator) {
			if (!(currHeader->bitFlags & VTX_QUAD)) {
				if (!(currHeader->bitFlags & VTX_STRIPS)) {
					for (int j = 0; j < currHeader->count; ++j) {
						tris[triPos] = currVert;
						++currVert;
						++triPos;
					}
				}
				else {
					// add first triangle
					tris[triPos] = currVert;
					tris[triPos + 1] = currVert + 1;
					tris[triPos + 2] = currVert + 2;
					currVert += 3;
					triPos += 3;
					int stripIterator = 0;
					for (int j = 3; j < currHeader->count; ++j) {
						if ((stripIterator & 1) == 0) {
							tris[triPos] = currVert - 2;
							tris[triPos + 2] = currVert - 1;
							tris[triPos + 1] = currVert;
						}
						else {
							tris[triPos + 2] = currVert;
							tris[triPos + 1] = currVert - 1;
							tris[triPos] = currVert - 2;
						}

						triPos += 3;
						++currVert;
						++stripIterator;
					}
				}
			}
			else {
				if (!(currHeader->bitFlags & VTX_STRIPS)) {
					int quadTracker = 0;
					for (int j = 0; j < currHeader->count; ++j) {
						tris[triPos] = currVert;
						++currVert;
						++triPos;
						++quadTracker;
						if (quadTracker % 4 == 0) {
							tris[triPos] = currVert - 4;
							++triPos;
							tris[triPos] = currVert - 2;
							++triPos;
						}
					}
				}
				else {
					// create initial quad
					tris[triPos] = currVert;
					tris[triPos + 1] = currVert + 1;
					tris[triPos + 2] = currVert + 2;
					triPos += 3;
					currVert += 3;
					tris[triPos] = currVert - 2;
					tris[triPos + 1] = currVert;
					tris[triPos + 2] = currVert - 1;
					triPos += 3;
					++currVert;
					// now, un-stripify
					for (int j = 4; j < currHeader->count; j += 2) {
						// create virtual quad
						int quad[4];
						quad[0] = currVert - 1;
						quad[1] = currVert - 2;
						quad[2] = currVert + 1;
						quad[3] = currVert;
						currVert += 2;
						// create two triangles from quads
						tris[triPos] = quad[2];
						tris[triPos + 1] = quad[1];
						tris[triPos + 2] = quad[0];
						triPos += 3;
						tris[triPos] = quad[1];
						tris[triPos + 1] = quad[2];
						tris[triPos + 2] = quad[3];
						triPos += 3;
					}
				}
			}
			currHeader = (VertexHeader*)((uint32_t)(&(currHeader->vertices)) + (uint32_t)(sizeof(Vertex) * (currHeader->count)));
			++i;
		}
		SetSubmeshTriangles(nativeModel, subMeshId, tris, triCount);
		free(tris);
		++subMeshId;
		--i;
	}
	UpdateMesh(nativeModel);
	
	// clean up old nativemodel
	if (model->NativeModel != NULL) {
		DeleteMesh(model->NativeModel);
		free(model->NativeModel);
	}
	model->NativeModel = nativeModel;
#endif
}

#ifndef _NOTDS
void SetupMaterial(SDMaterial* mat, bool rigged) {
	uint32_t flags = POLY_ALPHA(mat->alpha) | POLY_ID(1);
	if (mat->backFaceCulling) {
#ifdef FLIP_X
		flags |= POLY_CULL_FRONT;
#else
		flags |= POLY_CULL_BACK;
#endif
	}
	else {
		flags |= POLY_CULL_NONE;
	}
	if (mat->lit) {
		// reset matrix because for SOME REASON the light is rotated by that when set up
		glMatrixMode(GL_MODELVIEW);
		if (rigged) {
			glLoadIdentity();
		}
		else {
			glPushMatrix();
			glLoadIdentity();
		}
		// set our color to the light color, because DS handles color very weirdly
		glLight(0, RGB15(mat->color.x,
			mat->color.y,
			mat->color.z), lightNormal.x, lightNormal.y, lightNormal.z);
		if (!rigged) {
			glPopMatrix(1);
		}
		flags |= POLY_FORMAT_LIGHT0;
		glMaterialf(GL_EMISSION, RGB15(mat->emission.x, mat->emission.y, mat->emission.z));
	}
	else {
		glMaterialf(GL_EMISSION, RGB15(mat->color.x, mat->color.y, mat->color.z));
	}
	// specular is OFFICIALLY un-supported. sorry. DS GPU sucks.
	//glMaterialf(GL_SPECULAR, RGB15(((lightColor & 0x1F) * mat->specular) >> 8, (((lightColor >> 5) & 0x1F) * mat->specular) >> 8, (((lightColor >> 10) & 0x1F) * mat->specular) >> 8));
	glPolyFmt(flags);
	Texture* currTex = mat->texture;
	//glBindTexture(0, currTex->textureId);
	//glAssignColorTable(0, currTex->paletteId);
	GFX_TEX_FORMAT = currTex->textureWrite; // this is a minor optimization, but glBindTexture and glAssignColorTable accounted for about half the call time for material setup, and material setup needs to be called a lot.
	GFX_PAL_FORMAT = currTex->paletteWrite;
}

void GetMatrixLengths(m4x4* input, Vec3* output) {
	f32 matLength;
	long long tmpMat = (long long)input->m[0] * (long long)input->m[0];
	tmpMat += (long long)input->m[4] * (long long)input->m[4];
	tmpMat += (long long)input->m[8] * (long long)input->m[8];
	matLength = tmpMat >> 12;
	matLength = sqrtf32(matLength);
	output->x = divf32(4096, matLength);

	tmpMat = (long long)input->m[1] * (long long)input->m[1];
	tmpMat += (long long)input->m[5] * (long long)input->m[5];
	tmpMat += (long long)input->m[9] * (long long)input->m[9];
	matLength = tmpMat >> 12;
	matLength = sqrtf32(matLength);
	output->y = divf32(4096, matLength);

	tmpMat = (long long)input->m[2] * (long long)input->m[2];
	tmpMat += (long long)input->m[6] * (long long)input->m[6];
	tmpMat += (long long)input->m[10] * (long long)input->m[10];
	matLength = tmpMat >> 12;
	matLength = sqrtf32(matLength);
	output->z = divf32(4096, matLength);
}

void RenderModelRigged(Model *model, Vec3 *position, Vec3 *scale, Quaternion *rotation, SDMaterial *mats, Animator *animator) {
	// set current matrix to be model matrix
	glMatrixMode(GL_MODELVIEW);
	// ensure materials are valid...
	VertexHeader *currVertexGroup = model->vertexGroups;
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	// a little silly, but lets push until the end of the matrix, then store our base matrix in the last bone we would use.
	const int lastBone = Min(31, model->skeletonCount) - 1;
	for (int i = 0; i <= lastBone; ++i) {
		glPushMatrix();
	}
	// set up base object matrix
	m4x4 rotationMatrix;
	m4x4 rotationMatrixPrepared;
	MakeRotationMatrix(rotation, &rotationMatrix);
	rotationMatrix.m[3] = position->x;
	rotationMatrix.m[7] = position->y;
	rotationMatrix.m[11] = position->z;
	MatrixToDSMatrix(&rotationMatrix, &rotationMatrixPrepared);
	glLoadMatrix4x4(&rotationMatrixPrepared);
	glScalef32(scale->x, scale->y, scale->z);
	glStoreMatrix(lastBone);

	// hardware AABB test
	if (!BoxTest(model->boundsMin.x, model->boundsMin.y, model->boundsMin.z, model->boundsMax.x - model->boundsMin.x, model->boundsMax.y - model->boundsMin.y, model->boundsMax.z - model->boundsMin.z)) {
		return;
	}

	// cache up to 31 bones.
	for (int i = 0; i <= lastBone; ++i) {
		//glRestoreMatrix(lastBone);
		// get all parents
		// if parent is within stack already, then don't recalculate it!
		if (model->skeleton[i].parent < i) {
			// no parent! use model transform!
			if (model->skeleton[i].parent < 0) {
				glRestoreMatrix(lastBone);
			} else {
				glRestoreMatrix(model->skeleton[i].parent);
			}
		} else {
			glRestoreMatrix(lastBone);
			int parentQueue[128];
			int parentQueueSlot = 0;
			for (int parent = model->skeleton[i].parent; parent != -1; parent = model->skeleton[parent].parent) {
				parentQueue[parentQueueSlot] = parent;
				++parentQueueSlot;
			}
			for (int parent = parentQueueSlot - 1; parent >= 0; --parent) {
				glMultMatrix4x4(&animator->items[parentQueue[parent]].matrix);
			}
		}
		glMultMatrix4x4(&animator->items[i].matrix);
		glStoreMatrix(i);
	}
	for (int i = 0; i < lastBone + 1; ++i) {
		// apply inverse matrices now
		glRestoreMatrix(i);
		glMultMatrix4x4(&model->skeleton[i].inverseMatrix);
		glStoreMatrix(i);
	}
	// if a mesh has > 31 bones, then we need to set up the basic matrix too
	m4x4 matrix;
	if (model->skeletonCount > 31) {
		m4x4 scaleMatrix;
		m4x4 workMatrix;
		MakeScaleMatrix(scale->x, scale->y, scale->z, &scaleMatrix);
		CombineMatrices(&scaleMatrix, &rotationMatrix, &workMatrix);
		MatrixToDSMatrix(&workMatrix, &matrix);
		matrix.m[3] = position->x;
		matrix.m[7] = position->y;
		matrix.m[11] = position->z;
	}
	if (model->NativeModel == NULL) {
		int currBone = -1;
		const int vertGroupCount = model->vertexGroupCount;
		for (int i = 0; i < vertGroupCount; ++i) {
			if (currVertexGroup->bitFlags & VTX_MATERIAL_CHANGE) {
				SetupMaterial(&mats[currVertexGroup->material], true);
				currBone = -1;
			}
			if (currVertexGroup->bitFlags & VTX_QUAD) {
				if (currVertexGroup->bitFlags & VTX_STRIPS) {
					glBegin(GL_QUAD_STRIP);
				}
				else {
					glBegin(GL_QUAD);
				}
			}
			else {
				if (currVertexGroup->bitFlags & VTX_STRIPS) {
					glBegin(GL_TRIANGLE_STRIP);
				}
				else {
					glBegin(GL_TRIANGLE);
				}
			}
			const int vertCount = currVertexGroup->count;
			for (int i2 = 0; i2 < vertCount; ++i2) {
				const Vertex* currVert = &((&(currVertexGroup->vertices))[i2]);
				if (currVert->boneID != currBone) {
					currBone = currVert->boneID;
					if (currBone > 31) {
						glLoadMatrix4x4(&matrix);
						// get all parents
						int parentQueue[128];
						int parentQueueSlot = 0;
						for (int parent = model->skeleton[currBone].parent; parent != -1; parent = model->skeleton[parent].parent) {
							parentQueue[parentQueueSlot] = parent;
							++parentQueueSlot;
						}
						for (int parent = parentQueueSlot - 1; parent >= 0; --parent) {
							glMultMatrix4x4(&animator->items[parentQueue[parent]].matrix);
						}
						glMultMatrix4x4(&animator->items[currBone].matrix);
						glMultMatrix4x4(&model->skeleton[currBone].inverseMatrix);
					}
					else {
						glRestoreMatrix(currBone);
					}
				}
				glNormal(currVert->normal);
				glTexCoord2t16(currVert->u, currVert->v);
				glVertex3v16(currVert->x, currVert->y, currVert->z);
			}
			currVertexGroup = (VertexHeader*)((uint32_t)(&(currVertexGroup->vertices)) + (uint32_t)(sizeof(Vertex) * (currVertexGroup->count)));
			//glEnd();
		}
	}
	else {
		DSNativeModel* dsnm = model->NativeModel;
		for (int i = 0; i < dsnm->FIFOCount; ++i) {
			// uh oh!! we can free except the cache!! assume in order then!!
			if (currVertexGroup == NULL) {
				SetupMaterial(&mats[i], true);
			}
			else {
				if (currVertexGroup->bitFlags & VTX_MATERIAL_CHANGE)
					SetupMaterial(&mats[currVertexGroup->material], true);
			}
			if (dsnm->FIFOBatches[i] != NULL) {
				glCallList((u32*)dsnm->FIFOBatches[i]);
			}
			if (currVertexGroup != NULL) {
				currVertexGroup = (VertexHeader*)((uint32_t)(&(currVertexGroup->vertices)) + (uint32_t)(sizeof(Vertex) * (currVertexGroup->count)));
			}
		}
	}
	glPopMatrix(1);
	if (31 > model->skeletonCount) {
		glPopMatrix(model->skeletonCount);
	} else {
		glPopMatrix(31);
	}
}

void RenderModel(Model *model, Vec3 *position, Vec3 *scale, Quaternion *rotation, SDMaterial *mats) {
	// have to work around the DS' jank by omitting scale from the MODELVIEW matrix for normals, but not the POSITION matrix
	//Vec3 matrixSize;
	//GetMatrixLengths(matrix, &matrixSize);
	// set current matrix to be model matrix
	glMatrixMode(GL_MODELVIEW);
	//glPushMatrix();
	m4x4 rotationMatrix;
	m4x4 rotationMatrixPrepared;
	MakeRotationMatrix(rotation, &rotationMatrix);
	rotationMatrix.m[3] = position->x;
	rotationMatrix.m[7] = position->y;
	rotationMatrix.m[11] = position->z;
	MatrixToDSMatrix(&rotationMatrix, &rotationMatrixPrepared);
	glLoadMatrix4x4(&rotationMatrixPrepared);
	glScalef32(scale->x, scale->y, scale->z);

	// hardware AABB test
	if (!BoxTest(model->boundsMin.x, model->boundsMin.y, model->boundsMin.z, model->boundsMax.x - model->boundsMin.x, model->boundsMax.y - model->boundsMin.y, model->boundsMax.z - model->boundsMin.z)) {
		return;
	}

	VertexHeader *currVertexGroup = model->vertexGroups;
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	if (model->NativeModel == NULL) {
		const int vertGroupCount = model->vertexGroupCount;
		for (int i = 0; i < vertGroupCount; ++i) {
			if (currVertexGroup->bitFlags & VTX_MATERIAL_CHANGE) {
				// update our material
				SetupMaterial(&mats[currVertexGroup->material], false);
			}
			if (!(currVertexGroup->bitFlags & VTX_QUAD)) {
				if (currVertexGroup->bitFlags & VTX_STRIPS) {
					glBegin(GL_TRIANGLE_STRIP);
				}
				else {
					glBegin(GL_TRIANGLE);
				}
			}
			else {
				if (currVertexGroup->bitFlags & VTX_STRIPS) {
					glBegin(GL_QUAD_STRIP);
				}
				else {
					glBegin(GL_QUAD);
				}
			}
			const int vertCount = currVertexGroup->count;
			for (int i2 = 0; i2 < vertCount; ++i2) {
				Vertex* currVert = &((&(currVertexGroup->vertices))[i2]);
				glNormal(currVert->normal);
				glTexCoord2t16(currVert->u, currVert->v);
				glVertex3v16(currVert->x, currVert->y, currVert->z);
			}

			currVertexGroup = (VertexHeader*)((uint32_t)(&(currVertexGroup->vertices)) + (uint32_t)(sizeof(Vertex) * (currVertexGroup->count)));
			//glEnd();
		}
	 }
	else {
		for (int i = 0; i < model->skeletonCount; ++i) {
			glPushMatrix();
			glRestoreMatrix(0);
		}
		DSNativeModel* dsnm = model->NativeModel;
		for (int i = 0; i < dsnm->FIFOCount; ++i) {
			// uh oh!! we can free except the cache!! assume in order then!!
			if (currVertexGroup == NULL) {
				SetupMaterial(&mats[i], false);
			}
			else {
				if (currVertexGroup->bitFlags & VTX_MATERIAL_CHANGE)
					SetupMaterial(&mats[currVertexGroup->material], false);
			}
			if (dsnm->FIFOBatches[i] != NULL) {
				glCallList((u32*)dsnm->FIFOBatches[i]);
			}
			if (currVertexGroup != NULL) {
				currVertexGroup = (VertexHeader*)((uint32_t)(&(currVertexGroup->vertices)) + (uint32_t)(sizeof(Vertex) * (currVertexGroup->count)));
			}
		}
		glPopMatrix(model->skeletonCount);
	}
}

void UploadTexture(Texture* input) {
	// get palette size
	int paletteSize = 0;
	switch (input->type) {
	case 1:
		paletteSize = 32;
		break;
	case 2:
		paletteSize = 4;
		break;
	case 3:
		paletteSize = 16;
		break;
	case 4:
		paletteSize = 256;
		break;
	case 6:
		paletteSize = 8;
		break;
	case 7:
	case 8:
		paletteSize = 0;
		break;
	}
	// send the palette to VRAM if it exists
	if (paletteSize != 0) {
		glGenTextures(1, &input->paletteId);
		glBindTexture(0, input->paletteId);
		glColorTableEXT(0, 0, paletteSize, 0, 0, input->palette);
	}
	// now send the texture data to vram
	glGenTextures(1, &input->textureId);
	glBindTexture(0, input->textureId);
	unsigned int flagValue = TEXGEN_TEXCOORD;
	switch (input->mapTypeU) {
	case 1:
		flagValue |= GL_TEXTURE_FLIP_S;
	case 0:
		flagValue |= GL_TEXTURE_WRAP_S;
		break;
	}
	switch (input->mapTypeV) {
	case 1:
		flagValue |= GL_TEXTURE_FLIP_T;
	case 0:
		flagValue |= GL_TEXTURE_WRAP_T;
		break;
	}
	if (!(input->palette[0] & 0x8000)) {
		flagValue |= GL_TEXTURE_COLOR0_TRANSPARENT;
	}
	glTexImage2D(0, 0, input->type, input->width, input->height, 0, flagValue, input->image);
	input->uploaded = true;
	// re-assign so we can get the palette format here...
	if (paletteSize != 0) {
		glAssignColorTable(0, input->paletteId);
	}
	// ugh, derive the data from libnds now
	gl_texture_data *tex = (gl_texture_data*)DynamicArrayGet(&glGlob->texturePtrs, input->textureId);
	input->textureWrite = tex->texFormat;
	input->paletteWrite = ((gl_palette_data*)DynamicArrayGet( &glGlob->palettePtrs, tex->palIndex ))->addr;
}

void LoadTextureFromRAM(Texture* newTex, bool upload, char* name) {
	char* input = name;

	int flushRange = 0x30;

	int texMultiplier = 1 * 4096;

	switch (newTex->type) {
	case 1:
		texMultiplier = 1 * 4096;
		flushRange += 32 * 2;
		break;
	case 2:
		texMultiplier = 1 * 4096 / 4;
		flushRange += 4 * 2;
		break;
	case 3:
		texMultiplier = 1 * 4096 / 2;
		flushRange += 16 * 2;
		break;
	case 4:
		texMultiplier = 1 * 4096;
		flushRange += 256 * 2;
		break;
	case 6:
		texMultiplier = 1 * 4096;
		flushRange += 8 * 2;
		break;
	case 7:
	case 8:
		texMultiplier = 2 * 4096;
		break;
	}

	int width = Pow(2 * 4096, (newTex->width + 3) * 4096);
	int height = Pow(2 * 4096, (newTex->height + 3) * 4096);

	flushRange += mulf32(width, mulf32(height, texMultiplier)) / 4096;

	DC_FlushRange(newTex, flushRange);
	newTex->palette = (unsigned short*)((uint32_t)newTex->palette + (uint32_t)newTex);
	newTex->image = (char*)((uint32_t)newTex->image + (uint32_t)newTex);
	if (upload) {
		UploadTexture(newTex);
	}
	// save the name
	newTex->name = malloc(strlen(input) + 1);
	strcpy(newTex->name, input);
	// place in linked list
	if (startTexture.next != NULL) {
		startTexture.next->prev = newTex;
	}
	newTex->next = startTexture.next;
	startTexture.next = newTex;
	newTex->prev = &startTexture;
}

Texture *LoadTexture(char *input, bool upload) {
	// first check if we have the texture cached
	Texture *tex = startTexture.next;
	while (tex != NULL) {
		if (strcmp(input, tex->name) == 0) {
			return tex;
		}
		tex = tex->next;
	}
	// otherwise try opening it
	FILE *f = fopen(input, "rb");
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Texture *newTex = malloc(fsize);
	fread_MusicYielding(newTex, fsize, 1, f);
	fclose(f);
	LoadTextureFromRAM(newTex, upload, input);
	return newTex;
}

void UnloadTexture(Texture *tex) {
	// very simple, just remove it from the linked list
	tex->prev->next = tex->next;
	tex->next->prev = tex->prev;
	free(tex->name);
	if (tex->uploaded) {
		glDeleteTextures(1, &tex->paletteId);
		glDeleteTextures(1, &tex->textureId);
	}
	free(tex);
}
#endif

#ifdef _WIN32
void UnloadTexture(Texture* tex) {
	tex->prev->next = tex->next;
	tex->next->prev = tex->prev;
	free(tex->name);
	if (tex->uploaded) {
		DestroyTexture((NativeTexture*)tex->nativeTexture);
	}
	free(tex);
}

TextureRGBA DecodeColor(u16 color) {
	TextureRGBA retValue;
	int alpha = color & 0x8000;
	if (alpha != 0) {
		retValue.a = 255;
	}
	else {
		retValue.a = 0;
	}
	retValue.r = (color & 0x1F) << 3;
	retValue.g = ((color >> 5) & 0x1F) << 3;
	retValue.b = ((color >> 10) & 0x1F) << 3;
	return retValue;
}

void Convert32Palette(Texture *newTex, TextureRGBA *nativeColors, int width, int height) {
	TextureRGBA convertedPalette[32];
	for (int i = 0; i < 32; ++i) {
		convertedPalette[i] = DecodeColor(newTex->palette[i]);
	}
	// now convert the palette indices; recall, the upper 3 bits are used for alpha in this format
	int currPixel = 0;
	for (int i = 0; i < width; ++i) {
		for (int j = 0; j < height; ++j) {
			nativeColors[currPixel] = convertedPalette[newTex->image[currPixel] & 0x1F];
			nativeColors[currPixel].a = ((newTex->image[currPixel] & 0xE0) / (float)0xE0) * 255;
			++currPixel;
		}
	}
}

void Convert256Palette(unsigned char* image, u16* palette, TextureRGBA* nativeColors, int width, int height) {
	TextureRGBA convertedPalette[256];
	for (int i = 0; i < 256; ++i) {
		convertedPalette[i] = DecodeColor(palette[i]);
		if (i > 0)
			convertedPalette[i].a = 255;
	}
	// now convert the palette indices
	int currPixel = 0;
	for (int i = 0; i < width; ++i) {
		for (int j = 0; j < height; ++j) {
			nativeColors[currPixel] = convertedPalette[image[currPixel]];
			++currPixel;
		}
	}
}

void Convert16Palette(Texture* newTex, TextureRGBA* nativeColors, int width, int height) {
	TextureRGBA convertedPalette[16];
	for (int i = 0; i < 16; ++i) {
		convertedPalette[i] = DecodeColor(newTex->palette[i]);
		if (i > 0)
			convertedPalette[i].a = 255;
	}
	// now convert the palette indices
	int currPixel = 0;
	int targetPixel = 0;
	for (int i = 0; i < width; ++i) {
		for (int j = 0; j < height; j += 2) {
			nativeColors[targetPixel] = convertedPalette[newTex->image[currPixel] & 0x0F];
			nativeColors[targetPixel + 1] = convertedPalette[(newTex->image[currPixel] & 0xF0) >> 4];
			++currPixel;
			targetPixel += 2;
		}
	}
}

void ConvertPaletteless(u16* colors, TextureRGBA* nativeColors, int width, int height) {
	// no palette in this format, just convert direct colors
	int currPixel = 0;
	for (int i = 0; i < width; ++i) {
		for (int j = 0; j < height; ++j) {
			nativeColors[currPixel] = DecodeColor(colors[currPixel]);
			++currPixel;
		}
	}
}

void UploadTexture(Texture* input) {
	// get palette size
	int paletteSize = 0;
	switch (input->type) {
	case 1:
		paletteSize = 32;
		break;
	case 2:
		paletteSize = 4;
		break;
	case 3:
		paletteSize = 16;
		break;
	case 4:
		paletteSize = 256;
		break;
	case 6:
		paletteSize = 8;
		break;
	case 7:
	case 8:
		paletteSize = 0;
		break;
	}
	int width = 1 << (input->width + 3);
	int height = 1 << (input->height + 3);
	NativeTexture* nativeTexture = (NativeTexture*)malloc(sizeof(NativeTexture));
	InitializeTexture(nativeTexture);
	// convert paletttes to RGBA
	TextureRGBA* nativeColors = (TextureRGBA*)malloc(sizeof(TextureRGBA) * width * height);
	switch (paletteSize) {
	case 32:
		Convert32Palette(input, nativeColors, width, height);
		break;
	case 4:
		//Convert4Palette(input, nativeColors);
		break;
	case 16:
		Convert16Palette(input, nativeColors, width, height);
		break;
	case 256:
		Convert256Palette(input->image, input->palette, nativeColors, width, height);
		break;
	case 8:
		//Convert8Palette(input, nativeColors);
		break;
	case 0:
		ConvertPaletteless(input->image, nativeColors, width, height);
	}

	// set up wrap flags
	nativeTexture->WrapU = TexWrapClamp;
	nativeTexture->WrapV = TexWrapClamp;
	switch (input->mapTypeU) {
	case 1:
		nativeTexture->WrapU = TexWrapMirror;
		break;
	case 0:
		nativeTexture->WrapU = TexWrapWrap;
		break;
	}
	switch (input->mapTypeV) {
	case 1:
		nativeTexture->WrapV = TexWrapMirror;
	case 0:
		nativeTexture->WrapV = TexWrapWrap;
		break;
	}

	nativeTexture->color = nativeColors;
	nativeTexture->width = width;
	nativeTexture->height = height;
	UpdateTexture(nativeTexture, false, MIN_NEAREST, MAG_NEAREST);
	input->nativeTexture = nativeTexture;
	input->uploaded = true;
}

void LoadTextureFromRAM(Texture* newTex, bool upload, char* name) {
	char* input = name;
	newTex->palette = (unsigned short*)((uint32_t)newTex->palette + (uint32_t)newTex);
	newTex->image = (char*)((uint32_t)newTex->image + (uint32_t)newTex);

	// save the name
	newTex->name = malloc(strlen(input) + 1);
	strcpy(newTex->name, input);
	// place in linked list
	if (startTexture.next != NULL) {
		startTexture.next->prev = newTex;
	}
	newTex->next = startTexture.next;
	startTexture.next = newTex;
	newTex->prev = &startTexture;

	if (upload) {
		UploadTexture(newTex);
	}
}

Texture* LoadTexture(char* input, bool upload) {
	// "upload" not used on PC sowwy
	// first check if we have the texture cached
	Texture* tex = startTexture.next;
	while (tex != NULL) {
		if (strcmp(input, tex->name) == 0) {
			return tex;
		}
		tex = tex->next;
	}
	// otherwise try opening it
	char* fileDir = DirToNative(input);
	FILE* f = fopen(fileDir, "rb");
	free(fileDir);
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Texture* newTex = malloc(fsize);
	fread_MusicYielding(newTex, fsize, 1, f);
	fclose(f);
	
	LoadTextureFromRAM(newTex, upload, input);

	return newTex;
}

void PCRenderNormalize(Vec3f* in, Vec3f* out) {
	float magnitude = sqrtf((in->x * in->x) + (in->y * in->y) + (in->z * in->z));
	out->x = in->x / magnitude;
	out->y = in->y / magnitude;
	out->z = in->z / magnitude;
}

void RenderModel(Model* model, Vec3* position, Vec3* scale, Quaternion* rotation, SDMaterial* mats) {
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	m4x4 matrix;
	m4x4 scaleMatrix;
	m4x4 rotationMatrix;
	MakeScaleMatrix(scale->x, scale->y, scale->z, &scaleMatrix);
	MakeRotationMatrix(rotation, &rotationMatrix);
	scaleMatrix.m[3] = position->x;
	scaleMatrix.m[7] = position->y;
	scaleMatrix.m[11] = position->z;
	CombineMatrices(&scaleMatrix, &rotationMatrix, &matrix);
	m4x4 MVP;
	CombineMatricesFull(&cameraMatrix, &matrix, &MVP);

	// check to ensure it's in the camera!
	if (!AABBInCamera(&model->boundsMin, &model->boundsMax, &matrix)) {
		return;
	}

	// really big TODO: re-use materials for efficiency
	Material renderMat;
	InitMaterial(&renderMat);
	SetMaterialShader(&renderMat, defaultShader);
	m4x4* matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &MVP, sizeof(m4x4));
	// convert to float...
	for (int i = 0; i < 16; ++i) {
		matMatrix->mf[i] = f32tofloat(matMatrix->m[i]);
	}
	SetMaterialUniform(&renderMat, "MVP", matMatrix);


	// also account for M
	matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &matrix, sizeof(m4x4));
	// convert to float...
	for (int i = 0; i < 16; ++i) {
		matMatrix->mf[i] = f32tofloat(matMatrix->m[i]);
	}
	SetMaterialUniform(&renderMat, "M", matMatrix);


	// lighting
	Vec3f* lightDir = (Vec3f*)malloc(sizeof(Vec3f));
	lightDir->x = -f32tofloat(nativeLightNormal.x);
	lightDir->y = -f32tofloat(nativeLightNormal.y);
	lightDir->z = -f32tofloat(nativeLightNormal.z);
	PCRenderNormalize(lightDir, lightDir);
	SetMaterialUniform(&renderMat, "lightDirection", lightDir);
	Vec3f* lightCol = (Vec3f*)malloc(sizeof(Vec3f));
	lightCol->x = (lightColor & 0x1F) / 31.0f;
	lightCol->y = ((lightColor >> 5) & 0x1F) / 31.0f;
	lightCol->z = ((lightColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "lightColor", lightCol);


	// ambient
	Vec3f* ambient = (Vec3f*)malloc(sizeof(Vec3f));
	ambient->x = (ambientColor & 0x1F) / 31.0f;
	ambient->y = ((ambientColor >> 5) & 0x1F) / 31.0f;
	ambient->z = ((ambientColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "ambient", ambient);

	for (int i = 0; i < model->materialCount; ++i) {
		// queue up transparencies...
		if ((mats[i].texture != NULL && (mats[i].texture->type == 1 || mats[i].texture->type == 6)) || mats[i].alpha < 31) {
			if (modelDrawCallAllocated <= modelDrawCallCount) {
				modelDrawCallAllocated += 128;
				modelDrawCalls = realloc(modelDrawCalls, modelDrawCallAllocated * sizeof(ModelDrawCall));
			}
			modelDrawCalls[modelDrawCallCount].animator = NULL;
			modelDrawCalls[modelDrawCallCount].materialId = i;
			memcpy(&modelDrawCalls[modelDrawCallCount].subMat, &mats[i], sizeof(SDMaterial));
			modelDrawCalls[modelDrawCallCount].matrix = matrix;
			modelDrawCalls[modelDrawCallCount].model = model;
			Vec3 zero = { 0, 0, 0 };
			MatrixTimesVec3(&MVP, &zero, &modelDrawCalls[modelDrawCallCount].position);
			++modelDrawCallCount;
		}
		else {
			// some per-material data...
			if (mats[i].texture != NULL) {
				SetMaterialNativeTexture(&renderMat, "mainTexture", mats[i].texture->nativeTexture);
			}
			// set up diffuse color
			Vec4f* diffColor = (Vec4f*)malloc(sizeof(Vec4f));
			diffColor->x = (*(int*)&mats[i].color.x) / 31.0f;
			diffColor->y = (*(int*)&mats[i].color.y) / 31.0f;
			diffColor->z = (*(int*)&mats[i].color.z) / 31.0f;
			diffColor->w = (*(int*)&mats[i].alpha) / 31.0f;
			SetMaterialUniform(&renderMat, "diffColor", diffColor);
			renderMat.backFaceCulling = mats[i].backFaceCulling;
			int* unlit = malloc(sizeof(int));
			unlit[0] = 0;
			if (!mats[i].lit) {
				unlit[0] = 1;
			}
			SetMaterialUniform(&renderMat, "unlit", unlit);
			// render the submesh now
			RenderSubMesh(model->NativeModel, &renderMat, ((Mesh*)model->NativeModel)->subMeshes[i], &renderMat.mainShader);
		}
	}
	DeleteMaterial(&renderMat);
}

void RenderModelRigged(Model* model, Vec3* position, Vec3* scale, Quaternion* rotation, SDMaterial* mats, Animator* animator) {
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	m4x4 matrix;
	m4x4 scaleMatrix;
	m4x4 rotationMatrix;
	MakeScaleMatrix(scale->x, scale->y, scale->z, &scaleMatrix);
	MakeRotationMatrix(rotation, &rotationMatrix);
	scaleMatrix.m[3] = position->x;
	scaleMatrix.m[7] = position->y;
	scaleMatrix.m[11] = position->z;
	CombineMatrices(&scaleMatrix, &rotationMatrix, &matrix);
	m4x4 MVP;
	CombineMatricesFull(&cameraMatrix, &matrix, &MVP);

	// check to ensure it's in the camera!
	if (!AABBInCamera(&model->boundsMin, &model->boundsMax, &matrix)) {
		return;
	}

	// really big TODO: re-use materials for efficiency
	Material renderMat;
	InitMaterial(&renderMat);
	SetMaterialShader(&renderMat, defaultRiggedShader);
	m4x4* matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &MVP, sizeof(m4x4));
	// convert to float...
	for (int i = 0; i < 16; ++i) {
		matMatrix->mf[i] = f32tofloat(matMatrix->m[i]);
	}
	SetMaterialUniform(&renderMat, "MVP", matMatrix);


	// also account for M
	matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &matrix, sizeof(m4x4));
	// convert to float...
	for (int i = 0; i < 16; ++i) {
		matMatrix->mf[i] = f32tofloat(matMatrix->m[i]);
	}
	SetMaterialUniform(&renderMat, "M", matMatrix);


	// lighting
	Vec3f* lightDir = (Vec3f*)malloc(sizeof(Vec3f));
	lightDir->x = -f32tofloat(nativeLightNormal.x);
	lightDir->y = -f32tofloat(nativeLightNormal.y);
	lightDir->z = -f32tofloat(nativeLightNormal.z);
	PCRenderNormalize(lightDir, lightDir);
	SetMaterialUniform(&renderMat, "lightDirection", lightDir);
	Vec3f* lightCol = (Vec3f*)malloc(sizeof(Vec3f));
	lightCol->x = (lightColor & 0x1F) / 31.0f;
	lightCol->y = ((lightColor >> 5) & 0x1F) / 31.0f;
	lightCol->z = ((lightColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "lightColor", lightCol);


	// ambient
	Vec3f* ambient = (Vec3f*)malloc(sizeof(Vec3f));
	ambient->x = (ambientColor & 0x1F) / 31.0f;
	ambient->y = ((ambientColor >> 5) & 0x1F) / 31.0f;
	ambient->z = ((ambientColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "ambient", ambient);


	// bone matrices!
	m4x4* boneMatrices = (m4x4*)malloc(sizeof(m4x4) * 128);
	for (int i = 0; i < animator->itemCount; ++i) {
		m4x4 tmpMatrix;
		m4x4 workMatrix;
		if (model->skeleton[i].parent == -1) {
			CombineMatrices(&animator->items[i].matrix, &model->skeleton[i].inverseMatrix, &boneMatrices[i]);
		}
		else {
			// set up identity matrix
			for (int j = 0; j < 16; ++j) {
				workMatrix.m[j] = 0;
				if (j % 5 == 0) {
					workMatrix.m[j] = 4096;
				}
			}
			// set up parent stack
			m4x4 *parentStack[128];
			int parentStackSize = 0;
			Bone* currBone = &model->skeleton[i];
			while (currBone->parent != -1) {
				parentStack[parentStackSize] = &animator->items[currBone->parent].matrix;
				currBone = &model->skeleton[currBone->parent];
				++parentStackSize;
			}
			for (int j = parentStackSize - 1; j >= 0; --j) {
				CombineMatrices(&workMatrix, parentStack[j], &tmpMatrix);
				memcpy(&workMatrix, &tmpMatrix, sizeof(m4x4));
			}
			CombineMatrices(&workMatrix, &animator->items[i].matrix, &tmpMatrix);
			CombineMatrices(&tmpMatrix, &model->skeleton[i].inverseMatrix, &boneMatrices[i]);
		}
		//memcpy(&boneMatrices[i], &model->skeleton[i].inverseMatrix, sizeof(m4x4));
		//CombineMatrices(&animator->items[i].matrix, &model->skeleton[i].inverseMatrix, &boneMatrices[i]);
	}
	for (int i = 0; i < animator->itemCount; ++i) {
		// convert to float...
		for (int j = 0; j < 16; ++j) {
			boneMatrices[i].mf[j] = f32tofloat(boneMatrices[i].m[j]);
		}
	}
	SetMaterialUniform(&renderMat, "boneMatrices", boneMatrices);

	for (int i = 0; i < model->materialCount; ++i) {
		if ((mats[i].texture != NULL && (mats[i].texture->type == 1 || mats[i].texture->type == 6)) || mats[i].alpha < 31) {
			if (modelDrawCallAllocated <= modelDrawCallCount) {
				modelDrawCallAllocated += 128;
				modelDrawCalls = realloc(modelDrawCalls, modelDrawCallAllocated * sizeof(ModelDrawCall));
			}
			modelDrawCalls[modelDrawCallCount].animator = animator;
			modelDrawCalls[modelDrawCallCount].materialId = i;
			memcpy(&modelDrawCalls[modelDrawCallCount].subMat, &mats[i], sizeof(Material));
			modelDrawCalls[modelDrawCallCount].matrix = matrix;
			modelDrawCalls[modelDrawCallCount].model = model;
			Vec3 zero = { 0, 0, 0 };
			MatrixTimesVec3(&MVP, &zero, &modelDrawCalls[modelDrawCallCount].position);
			++modelDrawCallCount;
		}
		else {
			// some per-material data...
			if (mats[i].texture != NULL) {
				SetMaterialNativeTexture(&renderMat, "mainTexture", mats[i].texture->nativeTexture);
			}
			// set up diffuse color
			Vec4f* diffColor = (Vec4f*)malloc(sizeof(Vec4f));
			diffColor->x = (*(int*)&mats[i].color.x) / 31.0f;
			diffColor->y = (*(int*)&mats[i].color.y) / 31.0f;
			diffColor->z = (*(int*)&mats[i].color.z) / 31.0f;
			diffColor->w = (*(int*)&mats[i].alpha) / 31.0f;
			SetMaterialUniform(&renderMat, "diffColor", diffColor);
			renderMat.backFaceCulling = mats[i].backFaceCulling;
			// render the submesh now
			RenderSubMesh(model->NativeModel, &renderMat, ((Mesh*)model->NativeModel)->subMeshes[i], &renderMat.mainShader);
		}
	}
	DeleteMaterial(&renderMat);
}

void RenderModelTransparent(ModelDrawCall* call) {
	m4x4 MVP;
	CombineMatricesFull(&cameraMatrix, &call->matrix, &MVP);
	// really big TODO: re-use materials for efficiency
	Material renderMat;
	InitMaterial(&renderMat);
	if (call->animator != NULL) {
		SetMaterialShader(&renderMat, defaultRiggedShader);
	}
	else {
		SetMaterialShader(&renderMat, defaultShader);
	}
	m4x4* matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &MVP, sizeof(m4x4));
	// convert to float...
	for (int i = 0; i < 16; ++i) {
		matMatrix->mf[i] = f32tofloat(matMatrix->m[i]);
	}
	SetMaterialUniform(&renderMat, "MVP", matMatrix);


	// also account for M
	matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &call->matrix, sizeof(m4x4));
	// convert to float...
	for (int i = 0; i < 16; ++i) {
		matMatrix->mf[i] = f32tofloat(matMatrix->m[i]);
	}
	SetMaterialUniform(&renderMat, "M", matMatrix);


	// lighting
	Vec3f* lightDir = (Vec3f*)malloc(sizeof(Vec3f));
	lightDir->x = -f32tofloat(nativeLightNormal.x);
	lightDir->y = -f32tofloat(nativeLightNormal.y);
	lightDir->z = -f32tofloat(nativeLightNormal.z);
	PCRenderNormalize(lightDir, lightDir);
	SetMaterialUniform(&renderMat, "lightDirection", lightDir);
	Vec3f* lightCol = (Vec3f*)malloc(sizeof(Vec3f));
	lightCol->x = (lightColor & 0x1F) / 31.0f;
	lightCol->y = ((lightColor >> 5) & 0x1F) / 31.0f;
	lightCol->z = ((lightColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "lightColor", lightCol);


	// ambient
	Vec3f* ambient = (Vec3f*)malloc(sizeof(Vec3f));
	ambient->x = (ambientColor & 0x1F) / 31.0f;
	ambient->y = ((ambientColor >> 5) & 0x1F) / 31.0f;
	ambient->z = ((ambientColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "ambient", ambient);


	if (call->animator != NULL) {
		// bone matrices!
		m4x4* boneMatrices = (m4x4*)malloc(sizeof(m4x4) * 128);
		for (int i = 0; i < call->animator->itemCount; ++i) {
			m4x4 tmpMatrix;
			m4x4 workMatrix;
			if (call->model->skeleton[i].parent == -1) {
				CombineMatrices(&call->animator->items[i].matrix, &call->model->skeleton[i].inverseMatrix, &boneMatrices[i]);
			}
			else {
				// set up identity matrix
				for (int j = 0; j < 16; ++j) {
					workMatrix.m[j] = 0;
					if (j % 5 == 0) {
						workMatrix.m[j] = 4096;
					}
				}
				// set up parent stack
				m4x4* parentStack[128];
				int parentStackSize = 0;
				Bone* currBone = &call->model->skeleton[i];
				while (currBone->parent != -1) {
					parentStack[parentStackSize] = &call->animator->items[currBone->parent].matrix;
					currBone = &call->model->skeleton[currBone->parent];
					++parentStackSize;
				}
				for (int j = parentStackSize - 1; j >= 0; --j) {
					CombineMatrices(&workMatrix, parentStack[j], &tmpMatrix);
					memcpy(&workMatrix, &tmpMatrix, sizeof(m4x4));
				}
				CombineMatrices(&workMatrix, &call->animator->items[i].matrix, &tmpMatrix);
				CombineMatrices(&tmpMatrix, &call->model->skeleton[i].inverseMatrix, &boneMatrices[i]);
			}
			//memcpy(&boneMatrices[i], &model->skeleton[i].inverseMatrix, sizeof(m4x4));
			//CombineMatrices(&animator->items[i].matrix, &model->skeleton[i].inverseMatrix, &boneMatrices[i]);
		}
		for (int i = 0; i < call->animator->itemCount; ++i) {
			// convert to float...
			for (int j = 0; j < 16; ++j) {
				boneMatrices[i].mf[j] = f32tofloat(boneMatrices[i].m[j]);
			}
		}
		SetMaterialUniform(&renderMat, "boneMatrices", boneMatrices);
	}

	if (call->subMat.texture != NULL) {
		SetMaterialNativeTexture(&renderMat, "mainTexture", call->subMat.texture->nativeTexture);
	}
	// set up diffuse color
	Vec4f* diffColor = (Vec4f*)malloc(sizeof(Vec4f));
	diffColor->x = (*(int*)&call->subMat.color.x) / 31.0f;
	diffColor->y = (*(int*)&call->subMat.color.y) / 31.0f;
	diffColor->z = (*(int*)&call->subMat.color.z) / 31.0f;
	diffColor->w = (*(int*)&call->subMat.alpha) / 31.0f;
	SetMaterialUniform(&renderMat, "diffColor", diffColor);
	renderMat.backFaceCulling = call->subMat.backFaceCulling;
	renderMat.transparent = true;
	// render the submesh now
	RenderSubMesh(call->model->NativeModel, &renderMat, ((Mesh*)call->model->NativeModel)->subMeshes[call->materialId], &renderMat.mainShader);

	DeleteMaterial(&renderMat);
}

int TransparentSortFunction(ModelDrawCall* a, ModelDrawCall* b) {
	return a->position.z - b->position.z > 0 ? -1 : 1;
}

void RenderTransparentModels() {
	qsort(modelDrawCalls, modelDrawCallCount, sizeof(ModelDrawCall), TransparentSortFunction);
	for (int i = 0; i < modelDrawCallCount; ++i) {
		RenderModelTransparent(&modelDrawCalls[i]);
	}
	modelDrawCallCount = 0;
}
#endif

void SetLightDir(int x, int y, int z) {
	nativeLightNormal.x = x;
	nativeLightNormal.y = y;
	nativeLightNormal.z = z;
	x /= 8;
	y /= 8;
	z /= 8;
	if (x >= 512) {
		x = 511;
	}
	if (y >= 512) {
		y = 511;
	}
	if (z >= 512) {
		z = 511;
	}
	lightNormal.x = x;
	lightNormal.y = y;
	lightNormal.z = z;
}

void SetLightColor(int color) {
	lightColor = color;
#ifndef _NOTDS
	glMaterialf(GL_DIFFUSE, color);
#endif
}

void SetAmbientColor(int color) {
	ambientColor = color;
#ifndef _NOTDS
	glMaterialf(GL_AMBIENT, color);
#endif
}

void LoadAnimationFromRAM(Animation* anim) {
	for (int i = 0; i < anim->keyframeSetCount; ++i) {
		anim->sets[i] = (KeyframeSet*)((uint32_t)anim->sets[i] + (uint32_t)anim);
	}
}

Animation *LoadAnimation(char *input) {
	char* fileDir = DirToNative(input);
	FILE *f = fopen(fileDir, "rb");
	free(fileDir);
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Animation *retValue = malloc(fsize);
	fread_MusicYielding(retValue, fsize, 1, f);
	fclose(f);
	LoadAnimationFromRAM(retValue);
	return retValue;
}

void LoadAnimationAsyncCallback(void* data, bool success) {
	AnimationCallbackData* acd = (AnimationCallbackData*)data;
	fclose(acd->f);
	if (!success) {
		acd->callBack(acd->callBackData, NULL);
		free(acd->anim);
	}
	else {
		acd->callBack(acd->callBackData, acd->anim);
	}
	free(acd);
}

int LoadAnimationAsync(char* input, void (*callBack)(void* data, Animation* anim), void* callBackData) {
	if (callBack == NULL) {
		// ?
		return -1;
	}
	char* fileDir = DirToNative(input);
	FILE* f = fopen(fileDir, "rb");
	free(fileDir);
	if (f == NULL) {
		callBack(callBackData, NULL);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Animation* retValue = malloc(fsize);
	AnimationCallbackData* acd = malloc(sizeof(AnimationCallbackData));
	acd->f = f;
	acd->anim = retValue;
	acd->callBack = callBack;
	acd->callBackData = callBackData;
	return fread_Async(retValue, fsize, 1, f, LoadAnimationAsyncCallback, acd);
}

Animator *CreateAnimator(Model *referenceModel) {
	Animator *retValue = malloc(sizeof(Animator));
	retValue->speed = 4096;
	retValue->items = malloc(sizeof(AnimatorItem)*referenceModel->skeletonCount);
	retValue->itemCount = referenceModel->skeletonCount;
	retValue->currFrame = 0;
	retValue->lerpPrevTime = 0;
	retValue->lerpPrevTimeTarget = 0;
	retValue->currAnimation = NULL;
	retValue->queuedAnimCount = 0;
	retValue->loop = true;
	retValue->paused = false;
	// set up default animation values...
	for (int i = 0; i < referenceModel->skeletonCount; ++i) {
		memcpy(&retValue->items[i].currRotation, &referenceModel->skeleton[i].rotation, sizeof(Quaternion));
		memcpy(&retValue->items[i].currPosition, &referenceModel->skeleton[i].position, sizeof(Vec3));
		memcpy(&retValue->items[i].currScale, &referenceModel->skeleton[i].scale, sizeof(Vec3));
		AnimatorItem *currItem = &retValue->items[i];
		// and also matrix
		m4x4 w1;
		m4x4 w2;
		m4x4 w3;
		MakeScaleMatrix(currItem->currScale.x, currItem->currScale.y, currItem->currScale.z, &w1);
		MakeRotationMatrix(&currItem->currRotation, &w2);
		Combine3x3Matrices(&w1, &w2, &w3);
		w3.m[3] = currItem->currPosition.x;
		w3.m[7] = currItem->currPosition.y;
		w3.m[11] = currItem->currPosition.z;
		MatrixToDSMatrix(&w3, &retValue->items[i].matrix);
	}
	return retValue;
}

void UpdateAnimator(Animator *animator, Model *referenceModel) {
	if (animator->currAnimation == NULL || animator->paused) {
		return;
	}
	animator->currFrame += animator->speed;
	animator->lerpPrevTime += animator->speed;
	if (animator->lerpPrevTime > animator->lerpPrevTimeTarget) {
		animator->lerpPrevTime = animator->lerpPrevTimeTarget;
	}
	if (animator->queuedAnimCount == 0) {
		// no queued animations, handle end of animation normally
		if (animator->loop) {
			animator->currFrame = f32Mod(animator->currFrame, animator->currAnimation->lastFrame);
		}
		else {
			animator->currFrame = Min(animator->currFrame, animator->currAnimation->lastFrame);
		}
	}
	else {
		// queued animation(s), switch to them
		if (animator->currFrame >= animator->currAnimation->lastFrame) {
			f32 currFrame = animator->currFrame - animator->currAnimation->lastFrame;
			PlayAnimation(animator, animator->queuedAnims[0], animator->queuedLerpTimes[0]);
			animator->currFrame = currFrame;
			for (int i = 0; i < animator->queuedAnimCount - 1; ++i) {
				animator->queuedAnims[i] = animator->queuedAnims[i + 1];
				animator->queuedLerpTimes[i] = animator->queuedLerpTimes[i + 1];
			}
			--animator->queuedAnimCount;
		}
	}
	int currKeyFrame = 0;
	for (int i = 0; i < animator->currAnimation->keyframeSetCount; ++i) {
		KeyframeSet *currSet = animator->currAnimation->sets[i];
		// catch animations wanting to animate more bones than we have
		if (currSet->target >= animator->itemCount) {
			continue;
		}

		// optimization: cache the current keyframe between sets. typically works, unless animation is code generated
		if (currKeyFrame >= currSet->keyframeCount) {
			currKeyFrame = 0;
		}
		Keyframe *leftKeyframe = &currSet->keyframes[currKeyFrame];
		Keyframe *rightKeyframe = &currSet->keyframes[currKeyFrame];
		// catch it out if it's wrong
		if (leftKeyframe->frame >= animator->currFrame) {
			currKeyFrame = 0;
		}
		for (int i2 = currKeyFrame; i2 < currSet->keyframeCount; ++i2) {
			if (animator->currFrame < currSet->keyframes[i2].frame) {
				rightKeyframe = &currSet->keyframes[i2];
				int maxKeyframe = i2 - 1;
				if (maxKeyframe < 0) {
					maxKeyframe = i2;
				}
				leftKeyframe = &currSet->keyframes[maxKeyframe];
				currKeyFrame = maxKeyframe;
				break;
			}
		}
		// previous and next key frame acquired, now simply lerp between them
		f32 lerpAmnt = divf32(animator->currFrame - leftKeyframe->frame, rightKeyframe->frame - leftKeyframe->frame);
		switch (currSet->type) {
			case 0: {
				QuatSlerp(&leftKeyframe->data.rotation, &rightKeyframe->data.rotation, &animator->items[currSet->target].currRotation, lerpAmnt);
				}
				break;
			case 1: {
				animator->items[currSet->target].currPosition.x = Lerp(leftKeyframe->data.position.x, rightKeyframe->data.position.x, lerpAmnt);
				animator->items[currSet->target].currPosition.y = Lerp(leftKeyframe->data.position.y, rightKeyframe->data.position.y, lerpAmnt);
				animator->items[currSet->target].currPosition.z = Lerp(leftKeyframe->data.position.z, rightKeyframe->data.position.z, lerpAmnt);
				}
				break;
			case 2: {
				animator->items[currSet->target].currScale.x = Lerp(leftKeyframe->data.scale.x, rightKeyframe->data.scale.x, lerpAmnt);
				animator->items[currSet->target].currScale.y = Lerp(leftKeyframe->data.scale.y, rightKeyframe->data.scale.y, lerpAmnt);
				animator->items[currSet->target].currScale.z = Lerp(leftKeyframe->data.scale.z, rightKeyframe->data.scale.z, lerpAmnt);
				}
				break;
		}
	}
	
	// matrix time
	f32 prevLerpAmnt;
	if (animator->lerpPrevTimeTarget == 0) {
		prevLerpAmnt = 4096;
	}
	else {
		prevLerpAmnt = divf32(animator->lerpPrevTime, animator->lerpPrevTimeTarget);
	}
	if (prevLerpAmnt < 0) {
		prevLerpAmnt = 4096;
	}
	if (prevLerpAmnt >= 4096) {
		for (int i = 0; i < animator->itemCount; ++i) {
			AnimatorItem *currItem = &animator->items[i];
			// i choose...optimization, here.
			m4x4 w1;
			m4x4 w2;
			m4x4 w3;
			MakeScaleMatrix(currItem->currScale.x, currItem->currScale.y, currItem->currScale.z, &w1);
			MakeRotationMatrix(&currItem->currRotation, &w2);
			Combine3x3Matrices(&w1, &w2, &w3);
			w3.m[3] = currItem->currPosition.x;
			w3.m[7] = currItem->currPosition.y;
			w3.m[11] = currItem->currPosition.z;
			MatrixToDSMatrix(&w3, &animator->items[i].matrix);
		}
	} else {
		for (int i = 0; i < animator->itemCount; ++i) {
			AnimatorItem *currItem = &animator->items[i];
			// i choose...optimization, here.
			m4x4 w1;
			m4x4 w2;
			m4x4 w3;
			Quaternion slerpedQuat;
			QuatSlerp(&currItem->prevRotation, &currItem->currRotation, &slerpedQuat, prevLerpAmnt);
			MakeScaleMatrix(Lerp(currItem->prevScale.x, currItem->currScale.x, prevLerpAmnt),
			Lerp(currItem->prevScale.y, currItem->currScale.y, prevLerpAmnt), 
			Lerp(currItem->prevScale.z, currItem->currScale.z, prevLerpAmnt), &w1);
			MakeRotationMatrix(&slerpedQuat, &w2);
			Combine3x3Matrices(&w1, &w2, &w3);
			w3.m[3] = Lerp(currItem->prevPosition.x, currItem->currPosition.x, prevLerpAmnt);
			w3.m[7] = Lerp(currItem->prevPosition.y, currItem->currPosition.y, prevLerpAmnt);
			w3.m[11] = Lerp(currItem->prevPosition.z, currItem->currPosition.z, prevLerpAmnt);
			MatrixToDSMatrix(&w3, &animator->items[i].matrix);
		}
	}
}

void PlayAnimation(Animator *animator, Animation *animation, f32 lerpTime) {
	if (animator == NULL) {
		return;
	}
	animator->currFrame = 0;
	animator->lerpPrevTime = 0;
	animator->lerpPrevTimeTarget = lerpTime;
	animator->currAnimation = animation;
	for (int i = 0; i < animator->itemCount; ++i) {
		memcpy(&animator->items[i].prevPosition, &animator->items[i].currPosition, sizeof(Vec3));
		memcpy(&animator->items[i].prevScale, &animator->items[i].currScale, sizeof(Vec3));
		memcpy(&animator->items[i].prevRotation, &animator->items[i].currRotation, sizeof(Quaternion));
	}
}

void SetSDMaterialTexture(SDMaterial *mat, Texture *texture) {
	if (mat->texture != NULL) {
		mat->texture->numReferences -= 1;
		if (mat->texture->numReferences == 0) {
			UnloadTexture(mat->texture);
		}
	}
	mat->texture = texture;
	if (texture != NULL)
		texture->numReferences += 1;
}

// NOTE: THIS FUNCTION DOES NOT FREE IT
void DestroySDMaterial(SDMaterial *mat) {
	if (mat->texture != NULL) {
		mat->texture->numReferences -= 1;
		if (mat->texture->numReferences == 0) {
			UnloadTexture(mat->texture);
		}
	}
}

void DestroyModel(Model* m) {
	for (int i = 0; i < m->materialCount; ++i) {
		DestroySDMaterial(&m->defaultMats[i]);
	}
#ifdef _WIN32
	if (m->NativeModel != NULL) {
		DeleteMesh(m->NativeModel);
		free(m->NativeModel);
	}
#endif
#ifndef _NOTDS
	if (m->NativeModel != NULL) {
		DSNativeModel* dsnm = (DSNativeModel*)m->NativeModel;
		for (int i = 0; i < dsnm->FIFOCount; ++i) {
			if (dsnm->FIFOBatches[i] != NULL) {
				free(dsnm->FIFOBatches[i]);
			}
		}
		free(dsnm->FIFOBatches);
		free(m->NativeModel);
	}
#endif
	// check for freed model
	if (m->version & 0x80000000) {
		free(m->defaultMats);
		free(m->vertexGroups);
		if (m->skeleton != NULL) {
			free(m->skeleton);
		}
	}
	free(m);
}

// UNLIKE DESTROYMODEL, THIS WILL NOT FREE THE MODEL ITSELF
void DestroyGeneratedModel(Model* m) {
	for (int i = 0; i < m->materialCount; ++i) {
		DestroySDMaterial(&m->defaultMats[i]);
	}
#ifdef _WIN32
	if (m->NativeModel != NULL) {
		DestroyMesh(m->NativeModel);
		free(m->NativeModel);
	}
#endif
#ifndef _NOTDS
	if (m->NativeModel != NULL) {
		DSNativeModel* dsnm = (DSNativeModel*)m->NativeModel;
		for (int i = 0; i < dsnm->FIFOCount; ++i) {
			if (dsnm->FIFOBatches[i] != NULL) {
				free(dsnm->FIFOBatches[i]);
			}
}
		free(dsnm->FIFOBatches);
		free(m->NativeModel);
	}
#endif
	free(m->vertexGroups);
	if (m->skeleton != NULL)
		free(m->skeleton);
}

void LoadBGTexture(Sprite* input) {
#ifdef _NOTDS
	UploadSprite(input, true, true);
	BGTexture = input;
#else
	float paletteMultiplier = 1;
	switch (input->format) {
	case 0:
		paletteMultiplier = 0.5f;
		break;
	case 2:
		paletteMultiplier = 2.0f;
		break;
	}
	int paletteSize = 0;
	switch (input->format) {
	case 0:
		paletteSize = 16;
		break;
	case 1:
		paletteSize = 256;
		break;
	case 2:
		paletteSize = 0;
		break;
	}
	int width = input->width;
	int height = input->height;
	dmaCopy(input->image, bgGetGfxPtr(bgID), width * height * paletteMultiplier);
	dmaCopy(input->palette, BG_PALETTE_SUB, paletteSize * 2);
#endif
}

void InitializeSubBG() {
#ifndef _NOTDS
	bgID = bgInitSub_call(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);
#else
	subScreenTexture = CreateRenderTexture(256, 192, RENDERTEXTURE_TYPE_BYTE, false, 0, 1);
#endif
}

void UploadSprite(Sprite* input, bool sub, bool BG) {
#ifndef _NOTDS
	input->gfx = (char*)oamAllocateGfx(sub ? &oamSub : &oamMain, input->resolution, input->format);
	float multiplier = input->format == 0 ? 0.5f : input->format == 1 ? 1.0f : 2.0f;
	dmaCopy(input->image, input->gfx, multiplier * (input->width * input->height));
	input->paletteOffset = 15;
	// TODO: 8 & 4 bit sprites
#else
	TextureRGBA* nativeColors = malloc(sizeof(TextureRGBA) * input->width * input->height);
	switch (input->format) {
	case 1:
		Convert256Palette(input->image, input->palette, nativeColors, input->width, input->height);
		break;
	case 2:
		ConvertPaletteless(input->image, nativeColors, input->width, input->height);
		break;
	}
	NativeTexture* texture = malloc(sizeof(NativeTexture));
	InitializeTexture(texture);
	texture->color = nativeColors;
	texture->width = input->width;
	texture->height = input->height;
	texture->texRef = NULL;
	texture->WrapU = TexWrapClamp;
	texture->WrapV = TexWrapClamp;

	// make first color transparent
	if (!BG) {
		for (int i = 0; i < input->width * input->height; ++i) {
			if (nativeColors[i].r == nativeColors[0].r && nativeColors[i].g == nativeColors[0].g && nativeColors[i].b == nativeColors[i].b) {
				nativeColors[i].a = 0;
			}
		}
	}
	else {
		// un-fuck the texture
		TextureRGBA* newColors = malloc(sizeof(TextureRGBA) * input->width * input->height);
		for (int i = 0; i < input->height; i += 8) {
			for (int j = 0; j < input->width; j += 8) {
				for (int k = 0; k < 8; ++k) {
					for (int l = 0; l < 8; ++l) {
						newColors[(i + k) * input->width + j + l] = nativeColors[i * input->width + ((j + k) * 8) + l];
					}
				}
			}
		}
		free(texture->color);
		texture->color = newColors;
	}

	UpdateTexture(texture, false, MAG_NEAREST, MIN_NEAREST);
	input->nativeSprite = texture;
#endif
	input->uploaded = true;
	input->sub = sub;
}

void LoadSpriteFromRAM(Sprite* sprite) {
	sprite->image = (char*)((uint32_t)sprite->image + (uint32_t)sprite);
	sprite->palette = (unsigned short*)((uint32_t)sprite->palette + (uint32_t)sprite);
}

Sprite* LoadSprite(char* input, bool sub, bool upload) {
	char* newInput = DirToNative(input);
	FILE* f = fopen(newInput, "rb");
	free(newInput);
	if (f == NULL) {;
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Sprite* newSprite = (Sprite*)malloc(fsize);
	fread_MusicYielding(newSprite, fsize, 1, f);
	fclose(f);
	LoadSpriteFromRAM(newSprite);
	// that's it really, not much setup to be done here
	if (upload) {
		UploadSprite(newSprite, sub, false);
	}
	return newSprite;
}

void LoadSpriteAsyncCallback(void* data, bool success) {
	SpriteCallbackData* scd = (SpriteCallbackData*)data;
	fclose(scd->f);
	if (!success) {
		free(scd->sprite);
		scd->callBack(scd->callBackData, NULL);
	}
	else {
		LoadSpriteFromRAM(scd->sprite);
		if (scd->upload) {
			UploadSprite(scd->sprite, scd->sub, false);
		}
		scd->callBack(scd->callBackData, scd->sprite);
	}
	free(scd);
}

int LoadSpriteAsync(char* input, bool sub, bool upload, void (*callBack)(void* data, Sprite* sprite), void* callBackData) {
	if (callBack == NULL) {
		// ?
		return -1;
	}
	char* newInput = DirToNative(input);
	FILE* f = fopen(newInput, "rb");
	free(newInput);
	if (f == NULL) {
		callBack(callBackData, NULL);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Sprite* newSprite = (Sprite*)malloc(fsize);

	SpriteCallbackData* scd = (SpriteCallbackData*)malloc(sizeof(SpriteCallbackData));
	scd->callBack = callBack;
	scd->callBackData = callBackData;
	scd->sprite = newSprite;
	scd->sub = sub;
	scd->upload = upload;
	scd->f = f;

	return fread_Async(newSprite, fsize, 1, f, LoadSpriteAsyncCallback, scd);
}

void UnloadSprite(Sprite* input) {
	if (input->uploaded) {
#ifndef _NOTDS
		oamFreeGfx(input->sub ? &oamSub : &oamMain, input->gfx);
#else
		if (input->uploaded) {
			DeleteTexture(input->nativeSprite);
			free(input->nativeSprite);
		}
#endif
	}
	free(input);
}

void RenderSprite(Sprite* sprite, int x, int y, bool flipX, bool flipY, int xAlign, int yAlign) {
	SpriteDrawCall* drawList;
	int drawListCount;
	if (sprite->sub) {
		if (subSpriteCallCount >= 128) {
			return;
		}
		drawList = subSpriteCalls;
		drawListCount = subSpriteCallCount;
		++subSpriteCallCount;
}
	else {
		if (mainSpriteCallCount >= 128) {
			return;
		}
		drawList = mainSpriteCalls;
		drawListCount = mainSpriteCallCount;
		++mainSpriteCallCount;
	}
	drawList[drawListCount].scaled = false;
	drawList[drawListCount].sprite = sprite;
	drawList[drawListCount].x = x;
	drawList[drawListCount].y = y;
	drawList[drawListCount].flipX = flipX;
	drawList[drawListCount].flipY = flipY;
	drawList[drawListCount].spriteAlignX = xAlign;
	drawList[drawListCount].spriteAlignY = yAlign;
}

void RenderSpriteScaled(Sprite* sprite, int x, int y, bool flipX, bool flipY, f32 xScale, f32 yScale, int xAlign, int yAlign) {
	SpriteDrawCall* drawList;
	int drawListCount;
	if (sprite->sub) {
		if (subSpriteCallCount >= 128) {
			return;
		}
		drawList = subSpriteCalls;
		drawListCount = subSpriteCallCount;
		++subSpriteCallCount;
	}
	else {
		if (mainSpriteCallCount >= 128) {
			return;
		}
		drawList = mainSpriteCalls;
		drawListCount = mainSpriteCallCount;
		++mainSpriteCallCount;
	}
	drawList[drawListCount].scaled = true;
	drawList[drawListCount].sprite = sprite;
	drawList[drawListCount].x = x;
	drawList[drawListCount].y = y;
	drawList[drawListCount].xScale = xScale;
	drawList[drawListCount].yScale = yScale;
	drawList[drawListCount].flipX = flipX;
	drawList[drawListCount].flipY = flipY;
	drawList[drawListCount].spriteAlignX = xAlign;
	drawList[drawListCount].spriteAlignY = yAlign;
}

#ifndef _NOTDS
void RenderSpriteInternal(SpriteDrawCall* sprite) {
	int realDrawPosX = 0;
	int realDrawPosY = 0;
	if (sprite->spriteAlignX == SpriteAlignLeft) {
		realDrawPosX = sprite->x;
	}
	else if (sprite->spriteAlignX == SpriteAlignCenter) {
		realDrawPosX = sprite->x + 127;
	}
	else if (sprite->spriteAlignX == SpriteAlignRight) {
		realDrawPosX = sprite->x + 255;
	}
	if (sprite->spriteAlignY == SpriteAlignTop) {
		realDrawPosY = sprite->y;
	}
	else if (sprite->spriteAlignY == SpriteAlignCenter) {
		realDrawPosY = sprite->y + 96;
	}
	else if (sprite->spriteAlignY == SpriteAlignBottom) {
		realDrawPosY = sprite->y + 192;
	}
	if (sprite->sprite->sub) {
		int affineId = 0;
		if (sprite->scaled && spriteMatrixId < 32) {
			oamAffineTransformation(&oamSub, spriteMatrixId, sprite->xScale, 0, 0, sprite->yScale);
			affineId = spriteMatrixId;
			++spriteMatrixId;
		}
		oamSet(&oamSub, 0, realDrawPosX, realDrawPosY, 0, sprite->sprite->paletteOffset, sprite->sprite->resolution, sprite->sprite->format, sprite->sprite->gfx, affineId, false, false, sprite->flipX, sprite->flipY, false);
	}
	else {
		int affineId = 0;
		if (sprite->scaled && spriteMatrixId < 32) {
			oamAffineTransformation(&oamSub, spriteMatrixId, sprite->xScale, 0, 0, sprite->yScale);
			affineId = spriteMatrixId;
			++spriteMatrixId;
		}
		oamSet(&oamMain, 0, realDrawPosX, realDrawPosY, 0, sprite->sprite->paletteOffset, sprite->sprite->resolution, sprite->sprite->format, sprite->sprite->gfx, affineId, false, false, sprite->flipX, sprite->flipY, false);
	}
}
#else
void RenderSpriteInternal(SpriteDrawCall* sprite) {
	float divisor;
	float divisorY = 2.0f;
	if (sprite->sprite->sub) {
		divisor = 2.0f;
		divisorY = 2.0f;
	}
	else {
		divisor = (((float)GetWindowHeight() / (float)GetWindowWidth()) / (3.0f / 4.0f)) * 2.0f;
	}
	float realDrawPosX = (sprite->x / 256.0f) * divisor;
	float realDrawPosY = (sprite->y / 192.0f) * divisorY;
	if (sprite->spriteAlignX == SpriteAlignLeft) {
		realDrawPosX -= 1.0f;
	}
	else if (sprite->spriteAlignX == SpriteAlignCenter) {
		realDrawPosX += 0;
	}
	else if (sprite->spriteAlignX == SpriteAlignRight) {
		realDrawPosX += 1.0f;
	}
	if (sprite->spriteAlignY == SpriteAlignTop) {
		realDrawPosY += 1.0f;
	}
	else if (sprite->spriteAlignY == SpriteAlignCenter) {
		realDrawPosY += 0.0f;
	}
	else if (sprite->spriteAlignY == SpriteAlignBottom) {
		realDrawPosY -= 1.0f;
	}

	Material drawMaterial;
	drawMaterial.backFaceCulling = false;
	InitMaterial(&drawMaterial);
	SetMaterialShader(&drawMaterial, defaultSpriteShader);
	SetMaterialNativeTexture(&drawMaterial, "mainTexture", sprite->sprite->nativeSprite);
	drawMaterial.transparent = true;
	Mesh *spriteModel = calloc(sizeof(Mesh), 1);
	SetSubmeshCount(spriteModel, 1);
	Vec2 UVs[] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
	SetMeshUVs(spriteModel, UVs, 4);
	int triangles[] = { 0, 1, 2, 2, 3, 0 };
	SetSubmeshTriangles(spriteModel, 0, triangles, 6);
	// finally, generate the freakin' positions
	float width = (sprite->sprite->width / 256.0f) * divisor;
	float height = (sprite->sprite->height / 192.0f) * divisorY;
	if (sprite->scaled) {
		width *= sprite->xScale;
		height *= sprite->yScale;
	}
	Vec3 positions[] = { {realDrawPosX, realDrawPosY, 0}, {realDrawPosX + width, realDrawPosY, 0}, {realDrawPosX + width, realDrawPosY-height, 0}, {realDrawPosX, realDrawPosY-height, 0} };
	if (sprite->flipY) {
		Vec3 tmp[4];
		for (int i = 0; i < 4; ++i) {
			tmp[i] = positions[i];
		}
		positions[0].y = tmp[2].y;
		positions[1].y = tmp[3].y;
		positions[2].y = tmp[0].y;
		positions[3].y = tmp[1].y;
	}
	if (sprite->flipX) {
		Vec3 tmp[4];
		for (int i = 0; i < 4; ++i) {
			tmp[i] = positions[i];
		}
		positions[0].x = tmp[1].x;
		positions[1].x = tmp[0].x;
		positions[2].x = tmp[3].x;
		positions[3].x = tmp[2].x;
	}
	SetMeshVertices(spriteModel, positions, 4);
	UpdateMesh(spriteModel);
	Material* matPtr = &drawMaterial;
	RenderMesh(spriteModel, &matPtr, 1);
	DeleteMaterial(&drawMaterial);
	DeleteMesh(spriteModel);
	free(spriteModel);
}
#endif

#ifdef _NOTDS
void RenderBackground() {
	Material drawMaterial;
	InitMaterial(&drawMaterial);
	SetMaterialShader(&drawMaterial, defaultSpriteShader);
	SetMaterialNativeTexture(&drawMaterial, "mainTexture", BGTexture->nativeSprite);
	drawMaterial.backFaceCulling = false;
	drawMaterial.transparent = true;

	Mesh* bgModel = calloc(sizeof(Mesh), 1);

	int *tris = malloc(sizeof(int) * 32 * 32 * 6);
	int triId = 0;
	for (int i = 0; i < 32; ++i) {
		for (int j = 0; j < 32; ++j) {
			tris[(i * 32 * 6) + (j * 6)] = triId;
			tris[(i * 32 * 6) + (j * 6) + 1] = triId + 1;
			tris[(i * 32 * 6) + (j * 6) + 2] = triId + 2;
			tris[(i * 32 * 6) + (j * 6) + 3] = triId;
			tris[(i * 32 * 6) + (j * 6) + 4] = triId + 2;
			tris[(i * 32 * 6) + (j * 6) + 5] = triId + 3;
			triId += 4;
		}
	}
	SetSubmeshCount(bgModel, 1);
	SetSubmeshTriangles(bgModel, 0, tris, 32 * 32 * 6);
	free(tris);

	Vec2 *UVs = malloc(sizeof(Vec2) * 32 * 32 * 4);
	float eightEquivX = 8.0f / BGTexture->width;
	float eightEquivY = 8.0f / BGTexture->height;
	int xWidth = BGTexture->width / 8;
	for (int i = 0; i < 32; ++i) {
		for (int j = 0; j < 32; ++j) {
			UVs[(i * 32 * 4) + (j * 4)].x = (subBackground[(i * 32) + j] % xWidth) * eightEquivX;
			UVs[(i * 32 * 4) + (j * 4)].y = floorf(subBackground[(i * 32) + j] / xWidth) * 8 * eightEquivY;

			UVs[(i * 32 * 4) + (j * 4) + 1].x = (subBackground[(i * 32) + j] % xWidth) * eightEquivX + eightEquivX;
			UVs[(i * 32 * 4) + (j * 4) + 1].y = floorf(subBackground[(i * 32) + j] / xWidth) * 8 * eightEquivY;

			UVs[(i * 32 * 4) + (j * 4) + 2].x = (subBackground[(i * 32) + j] % xWidth) * eightEquivX + eightEquivX;
			UVs[(i * 32 * 4) + (j * 4) + 2].y = floorf(subBackground[(i * 32) + j] / xWidth) * 8 * eightEquivY + eightEquivY;

			UVs[(i * 32 * 4) + (j * 4) + 3].x = (subBackground[(i * 32) + j] % xWidth) * eightEquivX;
			UVs[(i * 32 * 4) + (j * 4) + 3].y = floorf(subBackground[(i * 32) + j] / xWidth) * 8 * eightEquivY + eightEquivY;
		}
	}
	SetMeshUVs(bgModel, UVs, 32 * 32 * 4);
	free(UVs);

	Vec3 *verts = malloc(sizeof(Vec3) * 32 * 32 * 4);
	eightEquivX = 8.0f / 256.0f;
	eightEquivY = 8.0f / 192.0f;
	for (int i = 0; i < 32; ++i) {
		for (int j = 0; j < 32; ++j) {
			verts[(i * 32 * 4) + (j * 4)].x = (j * eightEquivX) * 2.0f - 1.0f;
			verts[(i * 32 * 4) + (j * 4)].y = (1.0f - (i * eightEquivY)) * 2.0f - 1.0f;

			verts[(i * 32 * 4) + (j * 4) + 1].x = (j * eightEquivX + eightEquivX) * 2.0f - 1.0f;
			verts[(i * 32 * 4) + (j * 4) + 1].y = (1.0f - (i * eightEquivY)) * 2.0f - 1.0f;

			verts[(i * 32 * 4) + (j * 4) + 2].x = (j * eightEquivX + eightEquivX) * 2.0f - 1.0f;
			verts[(i * 32 * 4) + (j * 4) + 2].y = (1.0f - (i * eightEquivY + eightEquivY)) * 2.0f - 1.0f;

			verts[(i * 32 * 4) + (j * 4) + 3].x = (j * eightEquivX) * 2.0f - 1.0f;
			verts[(i * 32 * 4) + (j * 4) + 3].y = (1.0f - (i * eightEquivY + eightEquivY)) * 2.0f - 1.0f;

			verts[(i * 32 * 4) + (j * 4)].z = 0;
			verts[(i * 32 * 4) + (j * 4) + 1].z = 0;
			verts[(i * 32 * 4) + (j * 4) + 2].z = 0;
			verts[(i * 32 * 4) + (j * 4) + 3].z = 0;
		}
	}

	SetMeshVertices(bgModel, verts, 32 * 32 * 4);
	UpdateMesh(bgModel);
	free(verts);
	
	Material* matPtr = &drawMaterial;
	RenderMesh(bgModel, &matPtr, 1);
	DeleteMaterial(&drawMaterial);
	DeleteMesh(bgModel);
	free(bgModel);
}

void RenderBottomScreen() {
	SpriteDrawCall tempDraw;
	Sprite tempSprite;
	tempSprite.nativeSprite = subScreenTexture->texture;
	tempDraw.x = -64.0f;
	tempDraw.y = 48.0f;
	tempDraw.scaled = true;
	tempDraw.xScale = 0.25f;
	tempDraw.yScale = 0.25f;
	tempDraw.spriteAlignX = SpriteAlignRight;
	tempDraw.spriteAlignY = SpriteAlignBottom;
	tempSprite.sub = false;
	tempSprite.height = 192;
	tempSprite.width = 256;
	tempDraw.sprite = &tempSprite;
	tempDraw.flipY = true;
	tempDraw.flipX = false;
	RenderSpriteInternal(&tempDraw);
}
#endif

void FinalizeSprites() {
#ifndef _NOTDS
	oamClear(&oamMain, 0, 0);
	oamClear(&oamSub, 0, 0);
#else
	ClearDepth();
#endif
	spriteMatrixId = 1;
	for (int i = 0; i < mainSpriteCallCount; ++i) {
		RenderSpriteInternal(&mainSpriteCalls[i]);
	}
	spriteMatrixId = 1;
#ifdef _NOTDS
	UseRenderTexture(subScreenTexture);
	if (BGTexture != NULL) {
		ClearColor();
		RenderBackground();
	}
	else {
		ClearColor();
	}
#endif
	for (int i = 0; i < subSpriteCallCount; ++i) {
		RenderSpriteInternal(&subSpriteCalls[i]);
	}
#ifdef _NOTDS
	UseRenderTexture(NULL);
	RenderBottomScreen();
#endif
	mainSpriteCallCount = 0;
	subSpriteCallCount = 0;
}

void SetBackgroundTile(int x, int y, int id) {
#ifndef _NOTDS
	bgGetMapPtr(bgID)[x + y * 32] = id;
#else
	subBackground[x + y * 32] = id;
#endif
}

#ifndef _NOTDS
void SetupCameraMatrix() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	#ifdef FLIP_X
	m4x4 tmpMat;
	MakeScaleMatrix(-4096, 4096, 4096, &tmpMat);
	glMultMatrix4x4(&tmpMat);
	#endif
	gluPerspectivef32(cameraFOV, 5461, cameraNear, cameraFar);
	Vec3 camPosInverse;
	camPosInverse.x = -cameraPosition.x;
	camPosInverse.y = -cameraPosition.y;
	camPosInverse.z = -cameraPosition.z;
	m4x4 camTransform;
	MakeTranslationMatrix(camPosInverse.x, camPosInverse.y, camPosInverse.z, &camTransform);
	// rotation
	Quaternion inverseCamRot;
	QuaternionInverse(&cameraRotation, &inverseCamRot);
	m4x4 camRotation;
	MakeRotationMatrix(&inverseCamRot, &camRotation);
	CombineMatrices(&camRotation, &camTransform, &cameraMatrix);
	m4x4 trueCameraMatrix;
	MatrixToDSMatrix(&cameraMatrix, &trueCameraMatrix);
	glMultMatrix4x4(&trueCameraMatrix);
}
#else

void SetupCameraMatrix() {
	m4x4 perspectiveMatrix;
	MakePerspectiveMatrix(cameraFOV, divf32(GetWindowWidth() * 4096, GetWindowHeight() * 4096), cameraNear, cameraFar, &perspectiveMatrix);
#ifdef FLIP_X
	m4x4 tmpMat;
	m4x4 workMat;
	MakeScaleMatrix(-4096, 4096, 4096, &tmpMat);
	CombineMatricesFull(&tmpMat, &perspectiveMatrix, &workMat);
	memcpy(&perspectiveMatrix, &workMat, sizeof(m4x4));
#endif
	Vec3 camPosInverse;
	camPosInverse.x = -cameraPosition.x;
	camPosInverse.y = -cameraPosition.y;
	camPosInverse.z = -cameraPosition.z;
	m4x4 camTranslation;
	MakeTranslationMatrix(camPosInverse.x, camPosInverse.y, camPosInverse.z, &camTranslation);
	// rotation
	Quaternion inverseCamRot;
	QuaternionInverse(&cameraRotation, &inverseCamRot);
	m4x4 camRotation;
	MakeRotationMatrix(&inverseCamRot, &camRotation);
	CombineMatrices(&camRotation, &camTranslation, &cameraMatrix);
	m4x4 trueCameraMatrix;
	CombineMatricesFull(&perspectiveMatrix, &cameraMatrix, &trueCameraMatrix);
	MatrixToDSMatrix(&trueCameraMatrix, &cameraMatrix);
	GenerateViewFrustum(&cameraMatrix, &frustum);
}
#endif

#ifdef _NOTDS
f32 DotProduct4(Vec4* left, Vec4* right) {
	return (f32tofloat(left->x) * f32tofloat(right->x)) + (f32tofloat(left->y) * f32tofloat(right->y)) + (f32tofloat(left->z) * f32tofloat(right->z)) + (f32tofloat(left->w) * f32tofloat(right->w));
}
#endif

bool AABBInCamera(Vec3* min, Vec3* max, m4x4* transform) {
#ifdef _NOTDS
	// TODO
	Vec3 newMin, newMax;
	MatrixTimesVec3(transform, min, &newMin);
	MatrixTimesVec3(transform, max, &newMax);
	const Vec4* planes = frustum.planes;
	Vec4 v1 = { newMin.x, newMin.y, newMin.z, 4096 };
	Vec4 v2 = { newMax.x,  newMin.y, newMin.z, 4096 };
	Vec4 v3 = { newMin.x,  newMax.y, newMin.z, 4096 };
	Vec4 v4 = { newMax.x,  newMax.y, newMin.z, 4096 };
	Vec4 v5 = { newMin.x,  newMin.y, newMax.z, 4096 };
	Vec4 v6 = { newMax.x,  newMin.y, newMax.z, 4096 };
	Vec4 v7 = { newMin.x,  newMax.y, newMax.z, 4096 };
	Vec4 v8 = { newMax.x,  newMax.y, newMax.z, 4096 };

	for (int i = 0; i < 6; ++i)
	{
		if ((DotProduct4(&planes[i], &v1) < 0.0f) &&
			(DotProduct4(&planes[i], &v2) < 0.0f) &&
			(DotProduct4(&planes[i], &v3) < 0.0f) &&
			(DotProduct4(&planes[i], &v4) < 0.0f) &&
			(DotProduct4(&planes[i], &v5) < 0.0f) &&
			(DotProduct4(&planes[i], &v6) < 0.0f) &&
			(DotProduct4(&planes[i], &v7) < 0.0f) &&
			(DotProduct4(&planes[i], &v8) < 0.0f))
		{
			return false;
		}
	}
	return true;
#else
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrix4x4(transform);
	return BoxTest(min->x, min->y, min->z, max->x - min->x, max->y - min->y, max->z - min->z);
#endif
}

bool QueueAnimation(Animator* animator, Animation* animation, f32 lerpTime) {
	if (animator->queuedAnimCount >= 8) return false;
	animator->queuedAnims[animator->queuedAnimCount] = animation;
	animator->queuedLerpTimes[animator->queuedAnimCount] = lerpTime;
	++animator->queuedAnimCount;
	return true;
}

void DestroyAnimator(Animator* animator) {
	free(animator->items);
	free(animator);
}