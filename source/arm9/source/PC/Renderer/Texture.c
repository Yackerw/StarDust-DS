#include "Texture.h"
#include "Mesh.h"
#include "OpenGL/OpenGLRenderer.h"

void DestroyTexture(NativeTexture* tex) {
	DeleteTextures(1, &tex->texRef);
}

void DeleteTexture(NativeTexture* tex) {
	DestroyTexture(tex);
	if (tex->color != NULL) {
		free(tex->color);
	}
	// TODO: remove self from materials
	FreePVector(&tex->materialsIn);
}

// GL_LINEAR, GL_NEAREST for magFilter, GL_LINEAR_MIPMAP_LINEAR, etc for minFilter
void UpdateTexture(NativeTexture* tex, bool mipmap, int magFilter, int minFilter) {
	if (tex->texRef != NULL) {
		DeleteTextures(1, &tex->texRef);
	}
	tex->texRef = GenerateTexture2D(TEX_RGBA, tex->width, tex->height, tex->HDR ? GL_FLOAT : GL_UNSIGNED_BYTE, tex->color, magFilter, minFilter, mipmap);
	switch (tex->WrapU) {
	case TexWrapClamp:
		SetTextureWrapClamp(0, tex->texRef);
		break;
	case TexWrapWrap:
		SetTextureWrapWrap(0, tex->texRef);
		break;
	case TexWrapMirror:
		SetTextureWrapMirror(0, tex->texRef);
	}
	switch (tex->WrapV) {
	case TexWrapClamp:
		SetTextureWrapClamp(1, tex->texRef);
		break;
	case TexWrapWrap:
		SetTextureWrapWrap(1, tex->texRef);
		break;
	case TexWrapMirror:
		SetTextureWrapMirror(1, tex->texRef);
	}
}

void InitializeTexture(NativeTexture* tex) {
	memset(&tex->materialsIn, 0, sizeof(PsuedoVector));
	tex->materialsIn.itemSize = sizeof(Material*);
	tex->WrapU = TexWrapWrap;
	tex->WrapV = TexWrapWrap;
	tex->HDR = false;
}