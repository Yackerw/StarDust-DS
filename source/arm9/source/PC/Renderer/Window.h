#ifndef WINDOW
#define WINDOW
#include <stdio.h>
#include <stdbool.h>

#define KEYB_RIGHT 262
#define KEYB_LEFT 263
#define KEYB_DOWN 264
#define KEYB_UP 265
#define KEYB_ENTER 257
#define KEYB_RSHIFT 344

int InitializeGraphics();

int InitializeWindow(int width, int height, int MSAA, bool resizable, bool fullScreen);

void DestroyGraphics();

void DestroyGameWindow();

int GetWindowHeight();

int GetWindowWidth();

bool GetWindowClosing();

void PollWindowEvents();

void SwapWindowBuffers();

bool GetKeyPC(int key);

bool GetMouseButtonPC(int button);

int GetMouseXPC();

int GetMouseYPC();

void UpdateViewport(int width, int height);

void ClearDepth();

void ClearColor();
#endif