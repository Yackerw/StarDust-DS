#include <nds.h>
#include <stdio.h>
#include "sdrender.h"
#include "sdmath.h"
#include <stdbool.h>
#include "sdfile.h"
#include <stdlib.h>
//#define FLIP_X

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
Quaternion cameraRotation;
f32 cameraFOV = Angle16ToNative(8192);
f32 cameraNear = Fixed32ToNative(900);
f32 cameraFar = Fixed32ToNative(1409600);
m4x4 cameraMatrix;
ViewFrustum frustum;

Vec3 lightNormal;
Vec3 nativeLightNormal;
int lightColor;
int ambientColor;

int bgID;

Texture startTexture;

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
	retValue->vertexGroups = (VertexHeader*)((unsigned int)retValue->vertexGroups + (unsigned int)retValue);
	retValue->defaultMats = (SDMaterial*)((uint)retValue + (uint)retValue->defaultMats);
	retValue->materialTextureNames = (char*)((uint)retValue + (uint)retValue->materialTextureNames);
	if (retValue->skeleton != NULL)
	{
		retValue->skeleton = (Bone*)((uint)retValue + (uint)retValue->skeleton);
		for (int i = 0; i < retValue->skeletonCount; ++i) {
			m4x4 tempMtx;
			MatrixToDSMatrix(&retValue->skeleton[i].inverseMatrix, &tempMtx);
			memcpy(&retValue->skeleton[i].inverseMatrix, &tempMtx, sizeof(m4x4));
		}
	}
	char *currString = retValue->materialTextureNames;
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
		retValue->defaultMats[i].texture = LoadTexture(tmpString, true);
		if (retValue->defaultMats[i].texture != NULL) {
			retValue->defaultMats[i].texture->numReferences += 1;
		}
	}

	CacheModel(retValue);

#ifdef _NOTDS
	// convert as much as possible to floats
	for (int i = 0; i < retValue->skeletonCount; ++i) {
		for (int j = 0; j < 16; ++j) {
			retValue->skeleton[i].inverseMatrix.m[j] = Fixed32ToNative(*(int*)&retValue->skeleton[i].inverseMatrix.m[j]);
		}
		retValue->skeleton[i].position.x = Fixed32ToNative(*(int*)&retValue->skeleton[i].position.x);
		retValue->skeleton[i].position.y = Fixed32ToNative(*(int*)&retValue->skeleton[i].position.y);
		retValue->skeleton[i].position.z = Fixed32ToNative(*(int*)&retValue->skeleton[i].position.z);
		retValue->skeleton[i].scale.x = Fixed32ToNative(*(int*)&retValue->skeleton[i].scale.x);
		retValue->skeleton[i].scale.y = Fixed32ToNative(*(int*)&retValue->skeleton[i].scale.y);
		retValue->skeleton[i].scale.z = Fixed32ToNative(*(int*)&retValue->skeleton[i].scale.z);
		retValue->skeleton[i].rotation.x = Fixed32ToNative(*(int*)&retValue->skeleton[i].rotation.x);
		retValue->skeleton[i].rotation.y = Fixed32ToNative(*(int*)&retValue->skeleton[i].rotation.y);
		retValue->skeleton[i].rotation.z = Fixed32ToNative(*(int*)&retValue->skeleton[i].rotation.z);
		retValue->skeleton[i].rotation.w = Fixed32ToNative(*(int*)&retValue->skeleton[i].rotation.w);
	}
	Mesh* nativeModel = (Mesh*)calloc(sizeof(Mesh), 1);
	int vertCount = 0;
	VertexHeader* currHeader = retValue->vertexGroups;
	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		vertCount += currHeader->count;
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	Vec3* nativeVerts = (Vec3*)malloc(sizeof(Vec3) * vertCount);
	int currVert = 0;
	currHeader = retValue->vertexGroups;
	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			nativeVerts[currVert].x = Fixed32ToNative(verts[j].x);
			nativeVerts[currVert].y = Fixed32ToNative(verts[j].y);
			nativeVerts[currVert].z = Fixed32ToNative(verts[j].z);
			++currVert;
		}
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	SetMeshVertices(nativeModel, nativeVerts, vertCount);
	// UVs
	Vec2* nativeUVs = (Vec2*)malloc(sizeof(Vec2) * vertCount);
	currVert = 0;
	currHeader = retValue->vertexGroups;
	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		float UDivider = 1.0f;
		float VDivider = 1.0f;
		// adjust UVs from pixel scale to 0-1
		if (retValue->defaultMats[currHeader->material].texture != NULL) {
			UDivider = Pow(2, retValue->defaultMats[currHeader->material].texture->width + 3);
			VDivider = Pow(2, retValue->defaultMats[currHeader->material].texture->height + 3);
		}
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			nativeUVs[currVert].x = (verts[j].u / 16.0f) / UDivider;
			nativeUVs[currVert].y = (verts[j].v / 16.0f) / VDivider;
			++currVert;
		}
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	SetMeshUVs(nativeModel, nativeUVs, vertCount);
	free(nativeVerts);
	free(nativeUVs);
	// normals...this one's a bit trickier
	Vec3* nativeNormals = (Vec3*)malloc(sizeof(Vec3) * vertCount);
	currVert = 0;
	currHeader = retValue->vertexGroups;
	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			uint norm = verts[j].normal;
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
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	nativeModel->normals = nativeNormals;
	nativeModel->normalCount = vertCount;


	currVert = 0;
	currHeader = retValue->vertexGroups;

	int* boneIDs = (int*)calloc(sizeof(int) * vertCount * 4, 1);
	float* boneWeights = (float*)calloc(sizeof(float) * vertCount * 4, 1);

	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		// assign weights
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			boneIDs[currVert * 4] = verts[j].boneID;
			boneWeights[currVert * 4] = 1.0f;
			++currVert;
		}
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	nativeModel->boneIndex = boneIDs;
	nativeModel->weights = boneWeights;
	nativeModel->boneIndexCount = vertCount;
	nativeModel->weightCount = vertCount;


	SetSubmeshCount(nativeModel, retValue->vertexGroupCount);
	currVert = 0;
	currHeader = retValue->vertexGroups;
	for (int i = 0; i < retValue->vertexGroupCount; ++i) {
		int* tris = (int*)malloc(sizeof(int) * currHeader->count);
		for (int j = 0; j < currHeader->count; ++j) {
			tris[j] = currVert;
			++currVert;
		}
		SetSubmeshTriangles(nativeModel, i, tris, currHeader->count);
		free(tris);
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	UpdateMesh(nativeModel);
	retValue->NativeModel = nativeModel;

	// adjust default scale/offset
	retValue->defaultOffset.x = Fixed32ToNative(*(int*)&retValue->defaultOffset.x);
	retValue->defaultOffset.y = Fixed32ToNative(*(int*)&retValue->defaultOffset.y);
	retValue->defaultOffset.z = Fixed32ToNative(*(int*)&retValue->defaultOffset.z);
	retValue->defaultScale = Fixed32ToNative(*(int*)&retValue->defaultScale);

	retValue->boundsMax.x = Fixed32ToNative(*(int*)&retValue->boundsMax.x);
	retValue->boundsMax.y = Fixed32ToNative(*(int*)&retValue->boundsMax.y);
	retValue->boundsMax.z = Fixed32ToNative(*(int*)&retValue->boundsMax.z);
	retValue->boundsMin.x = Fixed32ToNative(*(int*)&retValue->boundsMin.x);
	retValue->boundsMin.y = Fixed32ToNative(*(int*)&retValue->boundsMin.y);
	retValue->boundsMin.z = Fixed32ToNative(*(int*)&retValue->boundsMin.z);

#endif
	return retValue;
}

#ifndef _NOTDS
unsigned int FIFOLookup[] = { FIFO_COMMAND_PACK(FIFO_NORMAL, FIFO_TEX_COORD, FIFO_VERTEX16, FIFO_NORMAL), FIFO_COMMAND_PACK(FIFO_TEX_COORD, FIFO_VERTEX16, FIFO_NORMAL, FIFO_TEX_COORD), FIFO_COMMAND_PACK(FIFO_VERTEX16, FIFO_NORMAL, FIFO_TEX_COORD, FIFO_VERTEX16) };
#endif

void CacheModel(Model* reference) {
#ifdef _NOTDS
	return;
#else
	DSNativeModel dsnm;
	dsnm.FIFOCount = reference->vertexGroupCount;
	dsnm.FIFOBatches = (unsigned int**)malloc(sizeof(unsigned int*) * reference->vertexGroupCount);
	// get vertex count to try and calculate FIFO count
	VertexHeader* currHeader = &reference->vertexGroups[0];
	for (int i = 0; i < reference->vertexGroupCount; ++i) {
		int vertCount = currHeader->count;
		if (vertCount == 0) {
			dsnm.FIFOBatches[i] = NULL;
			uint toAdd = (sizeof(Vertex) * (currHeader->count));
			currHeader = (VertexHeader*)(((uint)(&(currHeader->vertices))) + toAdd);
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
		FIFOBatch[2] = GL_TRIANGLE;
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
		uint toAdd = (sizeof(Vertex) * (currHeader->count));
		currHeader = (VertexHeader*)(((uint)(&(currHeader->vertices))) + toAdd);
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
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}

	// copy verts
	Vec3* nativeVerts = malloc(sizeof(Vec3) * vertCount);
	int currVert = 0;
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			nativeVerts[currVert].x = Fixed32ToNative(verts[j].x);
			nativeVerts[currVert].y = Fixed32ToNative(verts[j].y);
			nativeVerts[currVert].z = Fixed32ToNative(verts[j].z);
			++currVert;
		}
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	SetMeshVertices(nativeModel, nativeVerts, vertCount);
	free(nativeVerts);

	currVert = 0;
	// this is where it gets tricky...UVs...
	Vec2* nativeUVs = malloc(sizeof(Vec2) * vertCount);
	// let's just assume, for now, that UVs are typically stored relative to the texture as expected
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		float UDivider = 1.0f;
		float VDivider = 1.0f;
		// adjust UVs from pixel scale to 0-1
		if (model->defaultMats[currHeader->material].texture != NULL) {
			UDivider = Pow(2, model->defaultMats[currHeader->material].texture->width + 3);
			VDivider = Pow(2, model->defaultMats[currHeader->material].texture->height + 3);
		}
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			nativeUVs[currVert].x = (verts[j].u / 16.0f) / UDivider;
			nativeUVs[currVert].y = (verts[j].v / 16.0f) / VDivider;
			++currVert;
		}
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	SetMeshUVs(nativeModel, nativeUVs, vertCount);
	free(nativeUVs);

	// normals
	Vec3* nativeNormals = (Vec3*)malloc(sizeof(Vec3) * vertCount);
	currVert = 0;
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		Vertex* verts = (Vertex*)&currHeader->vertices;
		for (int j = 0; j < currHeader->count; ++j) {
			uint norm = verts[j].normal;
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
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
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
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
	}
	nativeModel->boneIndex = boneIDs;
	nativeModel->weights = boneWeights;
	nativeModel->boneIndexCount = vertCount;
	nativeModel->weightCount = vertCount;


	SetSubmeshCount(nativeModel, model->vertexGroupCount);
	currVert = 0;
	currHeader = model->vertexGroups;
	for (int i = 0; i < model->vertexGroupCount; ++i) {
		int* tris = (int*)malloc(sizeof(int) * currHeader->count);
		for (int j = 0; j < currHeader->count; ++j) {
			tris[j] = currVert;
			++currVert;
		}
		SetSubmeshTriangles(nativeModel, i, tris, currHeader->count);
		free(tris);
		currHeader = (VertexHeader*)((uint)(&(currHeader->vertices)) + (uint)(sizeof(Vertex) * (currHeader->count)));
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
void SetupMaterial(SDMaterial *mat, bool rigged) {
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
	uint flags = POLY_ALPHA(mat->alpha) | POLY_ID(1);
	if (mat->backFaceCulling) {
		#ifdef FLIP_X
		flags |= POLY_CULL_FRONT;
		#else
		flags |= POLY_CULL_BACK;
		#endif
	} else {
		flags |= POLY_CULL_NONE;
	}
	if (mat->lit) {
		flags |= POLY_FORMAT_LIGHT0;
	} else {
		flags |= POLY_FORMAT_LIGHT0;
		glLight(0, RGB15(mat->color.x,
		mat->color.y,
		mat->color.z), 0, 0, 0);
	}
	glMaterialf(GL_SPECULAR, RGB15(((lightColor & 0x1F) * mat->specular) >> 8, (((lightColor >> 5) & 0x1F) * mat->specular) >> 8, (((lightColor >> 10) & 0x1F) * mat->specular) >> 8));
	glMaterialf(GL_EMISSION, RGB15(mat->emission.x, mat->emission.y, mat->emission.z));
	glPolyFmt(flags);
	Texture *currTex = mat->texture;
	glBindTexture(0, currTex->textureId);
	glAssignColorTable(0, currTex->paletteId);
}

void NormalizeMatrix(m4x4 *input, m4x4 *output) {
	f32 matLength;
	matLength = mulf32(input->m[0], input->m[0]);
	matLength += mulf32(input->m[4], input->m[4]);
	matLength += mulf32(input->m[8], input->m[8]);
	matLength = sqrtf32(matLength);
	output->m[0] = divf32(input->m[0], matLength);
	output->m[4] = divf32(input->m[4], matLength);
	output->m[8] = divf32(input->m[8], matLength);

	matLength = mulf32(input->m[1], input->m[1]);
	matLength += mulf32(input->m[5], input->m[5]);
	matLength += mulf32(input->m[9], input->m[9]);
	matLength = sqrtf32(matLength);
	output->m[1] = divf32(input->m[1], matLength);
	output->m[5] = divf32(input->m[5], matLength);
	output->m[9] = divf32(input->m[9], matLength);

	matLength = mulf32(input->m[2], input->m[2]);
	matLength += mulf32(input->m[6], input->m[6]);
	matLength += mulf32(input->m[10], input->m[10]);
	matLength = sqrtf32(matLength);
	output->m[2] = divf32(input->m[2], matLength);
	output->m[6] = divf32(input->m[6], matLength);
	output->m[10] = divf32(input->m[10], matLength);
}

void RenderModelRigged(Model *model, m4x4 *matrix, SDMaterial *mats, Animator *animator) {
	m4x4 tmpMatrix;
	tmpMatrix.m[12] = 0;
	tmpMatrix.m[13] = 0;
	tmpMatrix.m[14] = 0;
	tmpMatrix.m[3] = 0;
	tmpMatrix.m[7] = 0;
	tmpMatrix.m[11] = 0;
	tmpMatrix.m[15] = 0;
	NormalizeMatrix(matrix, &tmpMatrix);
	// set current matrix to be model matrix
	glMatrixMode(GL_MODELVIEW);
	VertexHeader *currVertexGroup = model->vertexGroups;
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	// cache up to 30 bones
	for (int i = 0; i < 31 && i < model->skeletonCount; ++i) {
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrix4x4(&tmpMatrix);
		glMatrixMode(GL_POSITION);
		glLoadMatrix4x4(matrix);
		glMatrixMode(GL_MODELVIEW);
		// get all parents
		int parentQueue[128];
		int parentQueueSlot = 0;
		for (int parent = model->skeleton[i].parent; parent != -1; parent = model->skeleton[parent].parent) {
			parentQueue[parentQueueSlot] = parent;
			++parentQueueSlot;
		}
		for (int parent = parentQueueSlot - 1; parent >= 0; --parent) {
			glMultMatrix4x4(&animator->items[parentQueue[parent]].matrix);
		}
		glMultMatrix4x4(&animator->items[i].matrix);
		glMultMatrix4x4(&model->skeleton[i].inverseMatrix);
		glPushMatrix();
	}
	glMatrixMode(GL_MODELVIEW);
	int currBone = -1;
	const int vertGroupCount = model->vertexGroupCount;
	for (int i = 0; i < vertGroupCount; ++i) {
		if (currVertexGroup->materialChange) {
			SetupMaterial(&mats[currVertexGroup->material], true);
			currBone = -1;
		}
		glBegin(GL_TRIANGLE);
		const int vertCount = currVertexGroup->count;
		for (int i2 = 0; i2 < vertCount; ++i2) {
			const Vertex *currVert = &((&(currVertexGroup->vertices))[i2]);
			if (currVert->boneID != currBone) {
				currBone = currVert->boneID;
				if (currBone > 30) {
					glLoadMatrix4x4(matrix);
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
				} else {
					glRestoreMatrix(currBone);
				}
			}
			glNormal(currVert->normal);
			glTexCoord2t16(currVert->u, currVert->v);
			glVertex3v16(currVert->x, currVert->y, currVert->z);
		}
		currVertexGroup = (VertexHeader*)((uint)(&(currVertexGroup->vertices)) + (uint)(sizeof(Vertex)*(currVertexGroup->count)));
		//glEnd();
	}
	glPopMatrix(1);
	if (31 > model->skeletonCount) {
		glPopMatrix(model->skeletonCount - 1);
	} else {
		glPopMatrix(30);
	}
}

void RenderModel(Model *model, m4x4 *matrix, SDMaterial *mats) {
	// have to work around the DS' jank by omitting scale from the MODELVIEW matrix for normals, but not the POSITION matrix
	m4x4 tmpMatrix;
	tmpMatrix.m[12] = 0;
	tmpMatrix.m[13] = 0;
	tmpMatrix.m[14] = 0;
	tmpMatrix.m[3] = 0;
	tmpMatrix.m[7] = 0;
	tmpMatrix.m[11] = 0;
	tmpMatrix.m[15] = 4096;
	NormalizeMatrix(matrix, &tmpMatrix);
	// set current matrix to be model matrix
	glMatrixMode(GL_MODELVIEW);
	//glPushMatrix();
	glLoadMatrix4x4(&tmpMatrix);
	glMatrixMode(GL_POSITION);
	glLoadMatrix4x4(matrix);
	VertexHeader *currVertexGroup = model->vertexGroups;
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	if (model->NativeModel == NULL) {
		const int vertGroupCount = model->vertexGroupCount;
		for (int i = 0; i < vertGroupCount; ++i) {
			if (currVertexGroup->materialChange) {
				// update our material
				SetupMaterial(&mats[currVertexGroup->material], false);
			}
			glBegin(GL_TRIANGLE);
			const int vertCount = currVertexGroup->count;
			for (int i2 = 0; i2 < vertCount; ++i2) {
				Vertex* currVert = &((&(currVertexGroup->vertices))[i2]);
				glNormal(currVert->normal);
				glTexCoord2t16(currVert->u, currVert->v);
				glVertex3v16(currVert->x, currVert->y, currVert->z);
			}
			currVertexGroup = (VertexHeader*)((uint)(&(currVertexGroup->vertices)) + (uint)(sizeof(Vertex) * (currVertexGroup->count)));
			//glEnd();
		}
	 }
	else {
		DSNativeModel* dsnm = model->NativeModel;
		for (int i = 0; i < dsnm->FIFOCount; ++i) {
			if (currVertexGroup->materialChange) {
				// update our material
				SetupMaterial(&mats[currVertexGroup->material], false);
			}
			if (dsnm->FIFOBatches[i] != NULL) {
				glCallList((u32*)dsnm->FIFOBatches[i]);
			}
			currVertexGroup = (VertexHeader*)((uint)(&(currVertexGroup->vertices)) + (uint)(sizeof(Vertex) * (currVertexGroup->count)));
		}
	}
	//glPopMatrix(1);
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
	DC_FlushRange(newTex, fsize);
	newTex->palette = (unsigned short*)((uint)newTex->palette + (uint)newTex);
	newTex->image = (char*)((uint)newTex->image + (uint)newTex);
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

Convert32Palette(Texture *newTex, TextureRGBA *nativeColors, int width, int height) {
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

Convert256Palette(unsigned char* image, u16* palette, TextureRGBA* nativeColors, int width, int height) {
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

Convert16Palette(Texture* newTex, TextureRGBA* nativeColors, int width, int height) {
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
	int width = Pow(2, input->width + 3);
	int height = Pow(2, input->height + 3);
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
	UpdateTexture(nativeTexture, false, MIN_LINEAR, MAG_LINEAR);
	input->nativeTexture = nativeTexture;
	input->uploaded = true;
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
	newTex->palette = (unsigned short*)((uint)newTex->palette + (uint)newTex);
	newTex->image = (char*)((uint)newTex->image + (uint)newTex);

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

	return newTex;
}

void RenderModel(Model* model, m4x4* matrix, SDMaterial* mats) {
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	m4x4 MVP;
	CombineMatricesFull(&cameraMatrix, matrix, &MVP);
	// really big TODO: re-use materials for efficiency
	Material renderMat;
	InitMaterial(&renderMat);
	SetMaterialShader(&renderMat, defaultShader);
	m4x4* matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &MVP, sizeof(m4x4));
	SetMaterialUniform(&renderMat, "MVP", matMatrix);


	// also account for M
	matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, matrix, sizeof(m4x4));
	SetMaterialUniform(&renderMat, "M", matMatrix);


	// lighting
	Vec3 *lightDir = (Vec3*)malloc(sizeof(Vec3));
	lightDir->x = -nativeLightNormal.x;
	lightDir->y = -nativeLightNormal.y;
	lightDir->z = -nativeLightNormal.z;
	SetMaterialUniform(&renderMat, "lightDirection", lightDir);
	Vec3 *lightCol = (Vec3*)malloc(sizeof(Vec3));
	lightCol->x = (lightColor & 0x1F) / 31.0f;
	lightCol->y = ((lightColor >> 5) & 0x1F) / 31.0f;
	lightCol->z = ((lightColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "lightColor", lightCol);

	Normalize(lightDir, lightDir);


	// ambient
	Vec3* ambient = (Vec3*)malloc(sizeof(Vec3));
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
			modelDrawCalls[modelDrawCallCount].matrix = *matrix;
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
			Vec4* diffColor = (Vec4*)malloc(sizeof(Vec4));
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

void RenderModelRigged(Model* model, m4x4* matrix, SDMaterial* mats, Animator* animator) {
	if (mats == NULL) {
		mats = model->defaultMats;
	}
	m4x4 MVP;
	CombineMatricesFull(&cameraMatrix, matrix, &MVP);
	// really big TODO: re-use materials for efficiency
	Material renderMat;
	InitMaterial(&renderMat);
	SetMaterialShader(&renderMat, defaultRiggedShader);
	m4x4* matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &MVP, sizeof(m4x4));
	SetMaterialUniform(&renderMat, "MVP", matMatrix);


	// also account for M
	matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, matrix, sizeof(m4x4));
	SetMaterialUniform(&renderMat, "M", matMatrix);


	// lighting
	Vec3* lightDir = (Vec3*)malloc(sizeof(Vec3));
	lightDir->x = -nativeLightNormal.x;
	lightDir->y = -nativeLightNormal.y;
	lightDir->z = -nativeLightNormal.z;
	SetMaterialUniform(&renderMat, "lightDirection", lightDir);
	Vec3* lightCol = (Vec3*)malloc(sizeof(Vec3));
	lightCol->x = (lightColor & 0x1F) / 31.0f;
	lightCol->y = ((lightColor >> 5) & 0x1F) / 31.0f;
	lightCol->z = ((lightColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "lightColor", lightCol);

	Normalize(lightDir, lightDir);


	// ambient
	Vec3* ambient = (Vec3*)malloc(sizeof(Vec3));
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
					workMatrix.m[j] = 1.0f;
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
			modelDrawCalls[modelDrawCallCount].matrix = *matrix;
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
			Vec4* diffColor = (Vec4*)malloc(sizeof(Vec4));
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
	SetMaterialUniform(&renderMat, "MVP", matMatrix);


	// also account for M
	matMatrix = (m4x4*)malloc(sizeof(m4x4));
	memcpy(matMatrix, &call->matrix, sizeof(m4x4));
	SetMaterialUniform(&renderMat, "M", matMatrix);


	// lighting
	Vec3* lightDir = (Vec3*)malloc(sizeof(Vec3));
	lightDir->x = -nativeLightNormal.x;
	lightDir->y = -nativeLightNormal.y;
	lightDir->z = -nativeLightNormal.z;
	SetMaterialUniform(&renderMat, "lightDirection", lightDir);
	Vec3* lightCol = (Vec3*)malloc(sizeof(Vec3));
	lightCol->x = (lightColor & 0x1F) / 31.0f;
	lightCol->y = ((lightColor >> 5) & 0x1F) / 31.0f;
	lightCol->z = ((lightColor >> 10) & 0x1F) / 31.0f;
	SetMaterialUniform(&renderMat, "lightColor", lightCol);

	Normalize(lightDir, lightDir);


	// ambient
	Vec3* ambient = (Vec3*)malloc(sizeof(Vec3));
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
						workMatrix.m[j] = 1.0f;
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
		SetMaterialUniform(&renderMat, "boneMatrices", boneMatrices);
	}

	if (call->subMat.texture != NULL) {
		SetMaterialNativeTexture(&renderMat, "mainTexture", call->subMat.texture->nativeTexture);
	}
	// set up diffuse color
	Vec4* diffColor = (Vec4*)malloc(sizeof(Vec4));
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
	for (int i = 0; i < retValue->keyframeSetCount; ++i) {
		retValue->sets[i] = (KeyframeSet*)((uint)retValue->sets[i] + (uint)retValue);
	}
#ifdef _NOTDS
	for (int i = 0; i < retValue->keyframeSetCount; ++i) {
		for (int j = 0; j < retValue->sets[i]->keyframeCount; ++j) {
			retValue->sets[i]->keyframes[j].data.rotation.x = Fixed32ToNative(*(int*)&retValue->sets[i]->keyframes[j].data.rotation.x);
			retValue->sets[i]->keyframes[j].data.rotation.y = Fixed32ToNative(*(int*)&retValue->sets[i]->keyframes[j].data.rotation.y);
			retValue->sets[i]->keyframes[j].data.rotation.z = Fixed32ToNative(*(int*)&retValue->sets[i]->keyframes[j].data.rotation.z);
			retValue->sets[i]->keyframes[j].data.rotation.w = Fixed32ToNative(*(int*)&retValue->sets[i]->keyframes[j].data.rotation.w);
			retValue->sets[i]->keyframes[j].frame = Fixed32ToNative(*(int*)&retValue->sets[i]->keyframes[j].frame);
		}
	}
	retValue->lastFrame = Fixed32ToNative(*(int*)&retValue->lastFrame);
#endif
	return retValue;
}

Animator *CreateAnimator(Model *referenceModel) {
	Animator *retValue = malloc(sizeof(Animator));
	retValue->speed = Fixed32ToNative(4096);
	retValue->items = malloc(sizeof(AnimatorItem)*referenceModel->skeletonCount);
	retValue->itemCount = referenceModel->skeletonCount;
	retValue->currFrame = 0;
	retValue->lerpPrevTime = 0;
	retValue->lerpPrevTimeTarget = 0;
	retValue->currAnimation = NULL;
	retValue->queuedAnimCount = 0;
	retValue->loop = true;
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
	if (animator->currAnimation == NULL) {
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
	f32 prevLerpAmnt = divf32(animator->lerpPrevTime, animator->lerpPrevTimeTarget);
	if (animator->lerpPrevTimeTarget == 0) {
		prevLerpAmnt = Fixed32ToNative(4096);
	}
	if (prevLerpAmnt < 0) {
		prevLerpAmnt = Fixed32ToNative(4096);
	}
	if (prevLerpAmnt >= Fixed32ToNative(4096)) {
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

void DestroyModel(Model *m) {
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
		free(m->NativeModel);
	}
#endif
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
	input->gfx = oamAllocateGfx(sub ? &oamSub : &oamMain, input->resolution, input->format);
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

Sprite* LoadSprite(char* input, bool sub, bool upload) {
	char* newInput = DirToNative(input);
	FILE* f = fopen(newInput, "rb");
	free(newInput);
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	Sprite* newSprite = malloc(fsize);
	fread_MusicYielding(newSprite, fsize, 1, f);
	fclose(f);
	newSprite->image = (char*)((uint)newSprite->image + (uint)newSprite);
	newSprite->palette = (unsigned short*)((uint)newSprite->palette + (uint)newSprite);
	// that's it really, not much setup to be done here
	if (upload) {
		UploadSprite(newSprite, sub, false);
	}
	return newSprite;
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
	MakePerspectiveMatrix(cameraFOV, divf32(Fixed32ToNative(GetWindowWidth() * 4096), Fixed32ToNative(GetWindowHeight() * 4096)), cameraNear, cameraFar, &perspectiveMatrix);
#ifdef FLIP_X
	m4x4 tmpMat;
	m4x4 workMat;
	MakeScaleMatrix(Fixed32ToNative(-4096), Fixed32ToNative(4096), Fixed32ToNative(4096), &tmpMat);
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
	return (left->x * right->x) + (left->y * right->y) + (left->z * right->z) + (left->w * right->w);
}
#endif

bool AABBInCamera(Vec3* min, Vec3* max, m4x4* transform) {
#ifdef _NOTDS
	// TODO
	Vec3 newMin, newMax;
	MatrixTimesVec3(transform, min, &newMin);
	MatrixTimesVec3(transform, max, &newMax);
	const Vec4* planes = frustum.planes;
	Vec4 v1 = { newMin.x, newMin.y, newMin.z, 1.0f };
	Vec4 v2 = { newMax.x,  newMin.y, newMin.z, 1.0f };
	Vec4 v3 = { newMin.x,  newMax.y, newMin.z, 1.0f };
	Vec4 v4 = { newMax.x,  newMax.y, newMin.z, 1.0f };
	Vec4 v5 = { newMin.x,  newMin.y, newMax.z, 1.0f };
	Vec4 v6 = { newMax.x,  newMin.y, newMax.z, 1.0f };
	Vec4 v7 = { newMin.x,  newMax.y, newMax.z, 1.0f };
	Vec4 v8 = { newMax.x,  newMax.y, newMax.z, 1.0f };

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