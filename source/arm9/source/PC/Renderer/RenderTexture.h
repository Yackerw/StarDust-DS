#ifndef __OGLRENDERTEXTURE
#define __OGLRENDERTEXTURE
#include <stdbool.h>
#include "Texture.h"
#include "Mesh.h"
#define RENDERTEXTURE_TYPE_BYTE 0x1401
#define RENDERTEXTURE_TYPE_FLOAT 0x1406
typedef struct {
	NativeTexture texture[10];
	int format;
	int textureCount;
	unsigned int frameBuffer;
	unsigned int depthBuffer;
} RenderTexture;

extern RenderTexture* currRenderTexture;

void UseRenderTexture(RenderTexture *rt);

void DestroyRenderTexture(RenderTexture* rt);

RenderTexture* CreateRenderTexture(int width, int height, int type, bool cube, int MSAA, int count);

#endif