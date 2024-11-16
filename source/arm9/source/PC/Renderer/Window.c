#include "Window.h"
#include <glew.h>
#include <glfw3.h>

GLFWwindow* mWindow;

void GLFWErrorHandler(int error, const char* errstring) {
	printf(errstring);
	printf("\n");
}

int InitializeGraphics() {
	glewExperimental = true;
	glfwSetErrorCallback(GLFWErrorHandler);
	if (!glfwInit()) {
		printf("Failed to init glfw\n");
		return -1;
	}
}

int InitializeWindow(int width, int height, int MSAA, bool resizable, bool fullScreen) {
	glfwWindowHint(GLFW_SAMPLES, MSAA);
	// TODO: get version of opengl supported by the graphics card
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // forward rendering
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // new opengl
	//glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_FALSE);
	glfwWindowHint(GLFW_RESIZABLE, resizable);

	mWindow = glfwCreateWindow(width, height, "OpenGL", fullScreen ? glfwGetPrimaryMonitor() : NULL, NULL);
	// TODO: make a cleanup function
	if (mWindow == NULL) {
		printf("Failed to create window\n");
		glfwTerminate();
		return -1;
	}
	// TODO: rendering should definitely take place on a separate thread, see: description for glfwMakeContextCurrent
	glfwMakeContextCurrent(mWindow);
	glfwSwapInterval(1);
	if (glewInit() != GLEW_OK) {
		printf("Failed to initialize glew\n");
		glfwTerminate();
		return -1;
	}

	glfwSetInputMode(mWindow, GLFW_STICKY_KEYS, GL_TRUE);

	return 1;
}

void DestroyGraphics() {
	glfwTerminate();
}

void DestroyGameWindow() {
	glfwDestroyWindow(mWindow);
}

int GetWindowHeight() {
	int retValue, dummy;
	glfwGetWindowSize(mWindow, &dummy, &retValue);
	return retValue;
}

int GetWindowWidth() {
	int retValue, dummy;
	glfwGetWindowSize(mWindow, &retValue, &dummy);
	return retValue;
}

bool GetWindowClosing() {
	return glfwWindowShouldClose(mWindow) != 0;
}

void PollWindowEvents() {
	glfwPollEvents();
}

void SwapWindowBuffers() {
	glfwSwapBuffers(mWindow);
	ClearDepth();
	ClearColor();
}

bool GetKeyPC(int key) {
	return glfwGetKey(mWindow, key);
}

bool GetMouseButtonPC(int button) {
	return glfwGetMouseButton(mWindow, button);
}

int GetMouseXPC() {
	double mouseX, mouseY;
	glfwGetCursorPos(mWindow, &mouseX, &mouseY);
	return mouseX;
}

int GetMouseYPC() {
	double mouseX, mouseY;
	glfwGetCursorPos(mWindow, &mouseX, &mouseY);
	return mouseY;
}

void UpdateViewport(int width, int height) {
	glViewport(0, 0, width, height);
}

void ClearDepth() {
	glDepthMask(true);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void ClearColor() {
	glClear(GL_COLOR_BUFFER_BIT);
}