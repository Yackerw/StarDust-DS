#ifndef __OGLTEXTURE
#define __OGLTEXTURE
#include "PsuedoVector.h"
#include <stdbool.h>

enum TextureWrapType {TexWrapClamp, TexWrapWrap, TexWrapMirror};

// >:(
typedef struct {
	char r;
	char g;
	char b;
	char a;
} TextureRGBA;

typedef struct {
	float r;
	float g;
	float b;
	float a;
} TextureHDRRGBA;;

typedef struct {
	unsigned int texRef;
	int width;
	int height;
	union {
		TextureRGBA* color;
		TextureHDRRGBA* HDRColor;
	};
	//std::vector<Material*> materialsIn;
	enum TextureWrapType WrapU;
	enum TextureWrapType WrapV;
	PsuedoVector materialsIn;
	bool HDR;
} NativeTexture;

#define MIN_LINEAR 0x2601
#define MAG_MIPMAP 0x2703
#define MAG_LINEAR 0x2601
#define MIN_NEAREST 0x2600
#define MAG_NEAREST 0x2600


void DestroyTexture(NativeTexture* tex);

void DeleteTexture(NativeTexture* tex);

void UpdateTexture(NativeTexture* tex, bool mipmap, int magFilter, int minFilter);

void InitializeTexture(NativeTexture* tex);
#endif