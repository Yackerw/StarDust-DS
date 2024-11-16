#ifndef SDINPUT
#define SDINPUT
#include "sdmath.h"
#include <stdbool.h>

enum INPUT_KEY {INPUT_A, INPUT_B, INPUT_SELECT, INPUT_START, INPUT_RIGHT, INPUT_LEFT, INPUT_UP, INPUT_DOWN, INPUT_R, INPUT_L, INPUT_X, INPUT_Y, INPUT_TOUCH};

enum TOUCH_ORIGIN {TOUCH_ORIGIN_LEFT, TOUCH_ORIGIN_MIDDLE, TOUCH_ORIGIN_RIGHT, TOUCH_ORIGIN_TOP = 0, TOUCH_ORIGIN_BOTTOM = 2};

void UpdateInput();
bool GetKeyDown(int key);
bool GetKey(int key);
bool GetKeyUp(int key);
unsigned int GetCurrentKeyState();
int GetTouchScreenX(int origin);
int GetTouchScreenY(int origin);

#endif