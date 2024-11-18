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
#include "sdinput.h"

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

void WindowsInitialization() {
	shaderIncludeCallback = LoadShaderInclude;
	InitializeGraphics();
	InitializeWindow(640, 480, 4, true, false);
	defaultShader = LoadShader("shaders/default.vert", "shaders/default.frag");
	defaultRiggedShader = LoadShader("shaders/defaultRigged.vert", "shaders/default.frag");
	defaultSpriteShader = LoadShader("shaders/defaultSprite.vert", "shaders/defaultSprite.frag");
}

#endif

int main() {
	InitSound();
#ifndef _NOTDS
	
	defaultExceptionHandler();
	if (!nitroFSInit(NULL)) {
		printf("NitroFSInit failure");
	}

#endif

#ifdef _WIN32
	WindowsInitialization();
#endif

	Initialize3D(false, true);
	Set3DOnTop();
	
	// be sure to set up lighting
	SetLightColor(RGB15(16, 16, 16));
	SetAmbientColor(RGB15(8, 8, 8));
	SetLightDir(0, -4096, 0);

	InitializeNetworking(1, 1);

	InitializeSubBG();
	
	InitDeltaTime();
	deltaTimeEngine = true;

	UpdateDeltaTime();

	while (1) {
		UpdateDeltaTime();
		UpdateInput();
		
		ProcessObjects();
#ifdef _WIN32
		if (GetWindowClosing()) {
			break;
		}
#endif
	}
#ifdef _WIN32
	DestroyGameWindow();
	DestroyGraphics();
#endif
	UninitializeAudio();
	return 0;
}