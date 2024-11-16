#include "sdinput.h"
#include <nds.h>
#include "sdrender.h"
#ifdef _WIN32
#include "PC/Renderer/Window.h"
#endif

unsigned int currKeys;
unsigned int prevKeys;

int touchX;
int touchY;

#ifdef _WIN32

unsigned int WindowsInputCallback() {
	unsigned int retValue = 0;
	retValue |= GetKeyPC('Z') << INPUT_A;
	retValue |= GetKeyPC('X') << INPUT_B;
	retValue |= GetKeyPC(KEYB_ENTER) << INPUT_RIGHT;
	retValue |= GetKeyPC(KEYB_RSHIFT) << INPUT_SELECT;
	retValue |= GetKeyPC(KEYB_RIGHT) << INPUT_RIGHT;
	retValue |= GetKeyPC(KEYB_LEFT) << INPUT_LEFT;
	retValue |= GetKeyPC(KEYB_UP) << INPUT_UP;
	retValue |= GetKeyPC(KEYB_DOWN) << INPUT_DOWN;
	retValue |= GetKeyPC('W') << INPUT_R;
	retValue |= GetKeyPC('Q') << INPUT_L;
	retValue |= GetKeyPC('C') << INPUT_X;
	retValue |= GetKeyPC('V') << INPUT_Y;
	retValue |= GetMouseButtonPC(0) << INPUT_TOUCH;
	return retValue;
}
#endif

void UpdateInput() {
	prevKeys = currKeys;
#ifndef _NOTDS
	scanKeys();
	currKeys = keysCurrent();
	touchPosition touchPos;
	touchRead(&touchPos);
	touchX = touchPos.px;
	touchY = touchPos.py;
#endif
#ifdef _WIN32
	currKeys = WindowsInputCallback();
	// we just need the touch relative to the entire window
	touchX = GetMouseXPC();
	touchY = GetMouseYPC();
#endif
}

bool GetKeyDown(int key) {
	return ((currKeys & (~prevKeys)) & (1 << key)) != 0;
}

bool GetKey(int key) {
	return (currKeys & (1 << key)) != 0;
}

bool GetKeyUp(int key) {
	return (((~currKeys) & prevKeys) & (1 << key)) != 0;
}

unsigned int GetCurrentKeyState() {
	return currKeys;
}

int GetTouchScreenX(int origin) {
#ifndef _NOTDS
	switch (origin) {
	case TOUCH_ORIGIN_LEFT:
		return touchX;
		break;
	case TOUCH_ORIGIN_MIDDLE:
		// ds resolution of 256x192; divide by half for middle
		return touchX - 128;
		break;
	case TOUCH_ORIGIN_RIGHT:
		return touchX - 255;
		break;
	}
#endif
#ifdef _WIN32
	float divisor = (((float)GetWindowHeight() / (float)GetWindowWidth()) / (3.0f / 4.0f));
	if (touch3D) {
		switch (origin) {
		case TOUCH_ORIGIN_LEFT:
			return ((float)touchX / (float)GetWindowWidth()) * 256.0f / divisor;
			break;
		case TOUCH_ORIGIN_MIDDLE:
			// ds resolution of 256x192; divide by half for middle
			return (((float)touchX / (float)GetWindowWidth()) - 0.5f) * 256.0f / divisor;
			break;
		case TOUCH_ORIGIN_RIGHT:
			return (((float)touchX / (float)GetWindowWidth()) - 1.0f) * 256.0f / divisor;
			break;
		}
	}
	else {
		float originX = ((-64.0f / 256.0f) * divisor) + 1.0f;
		float width = 1.0f - originX;
		switch (origin) {
		case TOUCH_ORIGIN_LEFT:
			return ((((float)touchX / (float)GetWindowWidth()) - originX) / width) * 256.0f;
			break;
		case TOUCH_ORIGIN_MIDDLE:
			// ds resolution of 256x192; divide by half for middle
			return (((((float)touchX / (float)GetWindowWidth()) - originX) / width) * 256.0f) - 128;
			break;
		case TOUCH_ORIGIN_RIGHT:
			return (((((float)touchX / (float)GetWindowWidth()) - originX) / width) * 256.0f) - 255;
			break;
		}
	}
#endif
	// invalid input
	return 0xDEADBEEF;
}

int GetTouchScreenY(int origin) {
#ifndef _NOTDS
	switch (origin) {
	case TOUCH_ORIGIN_TOP:
		return touchY;
		break;
	case TOUCH_ORIGIN_MIDDLE:
		// ds resolution of 256x192; divide by half for middle
		return touchY - 96;
		break;
	case TOUCH_ORIGIN_BOTTOM:
		return touchY - 191;
		break;
	}
#endif
#ifdef _WIN32
	if (touch3D) {
		switch (origin) {
		case TOUCH_ORIGIN_TOP:
			return ((float)touchY / (float)GetWindowHeight()) * 192.0f;
			break;
		case TOUCH_ORIGIN_MIDDLE:
			// ds resolution of 256x192; divide by half for middle
			return (((float)touchY / (float)GetWindowHeight()) - 0.5f) * 192.0f;
			break;
		case TOUCH_ORIGIN_BOTTOM:
			return (((float)touchY / (float)GetWindowHeight()) - 1.0f) * 192.0f;
			break;
		}
	}
	else {
		float originY = 0.75f;
		float height = 1.0f - originY;
		switch (origin) {
		case TOUCH_ORIGIN_LEFT:
			return ((((float)touchY / (float)GetWindowHeight()) - originY) / height) * 192.0f;
			break;
		case TOUCH_ORIGIN_MIDDLE:
			// ds resolution of 256x192; divide by half for middle
			return (((((float)touchY / (float)GetWindowHeight()) - originY) / height) * 192.0f) - 96;
			break;
		case TOUCH_ORIGIN_RIGHT:
			return (((((float)touchY / (float)GetWindowHeight()) - originY) / height) * 192.0f) - 192;
			break;
		}
	}
#endif
	// invalid input
	return 0xDEADBEEF;
}