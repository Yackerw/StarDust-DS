#include <stdio.h>
#include <nds.h>
#include <string.h>
#ifndef _NOTDS
#include <filesystem.h>
#endif
#include "sdcollision.h"
#include "sdrender.h"
#include "sdobject.h"
#include "sddelta.h"
#include "sdsound.h"
#include "sdfile.h"

#ifdef _WIN32

char* LoadShaderInclude(char* includePath) {
	char* path = (char*)malloc(strlen(includePath) + 1 + strlen("shaders/include/"));
	sprintf(path, "%s%s", "shaders/include/", includePath);
	FILE* f = fopen(path, "rb");
	free(path);
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* retValue = (char*)malloc(fsize + 1);
	retValue[fsize] = 0;
	fread(retValue, fsize, 1, f);
	fclose(f);
	return retValue;
}


u16 WindowsInputCallback() {
	u16 retValue = 0;
	retValue |= GetKey("Z"[0]);
	retValue |= GetKey("X"[0]) << 1;
	retValue |= GetKey(KEYB_ENTER) << 2;
	retValue |= GetKey(KEYB_RSHIFT) << 3;
	retValue |= GetKey(KEYB_RIGHT) << 4;
	retValue |= GetKey(KEYB_LEFT) << 5;
	retValue |= GetKey(KEYB_UP) << 6;
	retValue |= GetKey(KEYB_DOWN) << 7;
	retValue |= GetKey("W"[0]) << 8;
	retValue |= GetKey("Q"[0]) << 9;
	retValue |= GetKey("C"[0]) << 10;
	retValue |= GetKey("V"[0]) << 11;
	return retValue;
}

void WindowsInitialization() {
	shaderIncludeCallback = LoadShaderInclude;
	InitializeGraphics();
	InitializeWindow(640, 480, 4, true, false);
	defaultShader = LoadShader("shaders/default.vert", "shaders/default.frag");
	defaultRiggedShader = LoadShader("shaders/defaultRigged.vert", "shaders/default.frag");
	defaultSpriteShader = LoadShader("shaders/defaultSprite.vert", "shaders/defaultSprite.frag");
	keysCallback = WindowsInputCallback;
}

#endif

int main() {
	InitSound();
	// set top screen for 3d
#ifndef _NOTDS
	videoSetMode(MODE_0_3D);
	videoSetModeSub(MODE_0_2D);
	// initialize gl engine
	glInit();
	
	// AA because why not
	glEnable(GL_ANTIALIAS);
	
	glEnable(GL_TEXTURE_2D);
	
	glEnable(GL_BLEND);
	
	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_TEXTURE_SLOT2);
	vramSetBankE(VRAM_E_MAIN_SPRITE);
	vramSetBankF(VRAM_F_TEX_PALETTE_SLOT0);
	vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5);
	//vramSetBankH(VRAM_H_SUB_BG);
	vramSetBankI(VRAM_I_SUB_SPRITE);
	glMaterialShinyness();
	oamInit(&oamMain, SpriteMapping_Bmp_1D_128, false);
	oamInit(&oamSub, SpriteMapping_Bmp_1D_128, false);
	
	defaultExceptionHandler();
	if (!nitroFSInit(NULL)) {
		printf("NitroFSInit failure");
	}
	
	// set viewport to be size of screen
	glViewport(0,0,255,191);

#endif

#ifdef _WIN32
	WindowsInitialization();
#endif
	InitializeAsyncFiles();

	InitializeSubBG();
	
	InitDeltaTime();
	deltaTimeEngine = true;

	UpdateDeltaTime();

	while (1) {
		UpdateDeltaTime();
		scanKeys();
		
		ProcessObjects();

		FinalizeSprites();

		// flush screen
#ifndef _NOTDS
		// this handles glFlush(0) and swiWaitForVBlank() for us!
		AsyncFileHandler();
		glClearDepth(GL_MAX_DEPTH); // reset depth buffer, good idea to set to GL_MAX_DEPTH
		UpdateMusicBuffer();
		oamUpdate(&oamMain);
		oamUpdate(&oamSub);
		bgUpdate();
#endif
#ifdef _WIN32
		PollWindowEvents();
		if (GetWindowClosing()) {
			break;
		}
		SwapWindowBuffers();
		UpdateViewport(GetWindowWidth(), GetWindowHeight());
#endif
	}
#ifdef _WIN32
	DestroyGameWindow();
	DestroyGraphics();
#endif
	UninitializeAudio();
	return 0;
}