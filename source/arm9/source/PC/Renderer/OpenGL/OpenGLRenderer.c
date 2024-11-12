#include "OpenGLRenderer.h"
#include <glew.h>
#include <glfw3.h>

unsigned int GetUniformLocation(unsigned int pid, char* ref) {
	return glGetUniformLocation(pid, ref);
}

bool CompileIndividualShader(char* shader, int id) {
	// compile the shader itself
	glShaderSource(id, 1, &shader, NULL);
	glCompileShader(id);
	// did it succeed?
	GLint res;
	GLint infoLogLength;
	glGetShaderiv(id, GL_COMPILE_STATUS, &res);
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength > 0) {
		char* vertexShaderErrorMessage = (char*)malloc(sizeof(char) * infoLogLength + 1);
		glGetShaderInfoLog(id, infoLogLength, NULL, &vertexShaderErrorMessage[0]);
		printf("%s\n", &vertexShaderErrorMessage[0]);
		free(vertexShaderErrorMessage);
		return false;
	}
	return true;
}

bool CompileShader(char* vshader, char* fshader, unsigned int* pID) {
	unsigned int vID = glCreateShader(GL_VERTEX_SHADER);
	unsigned int fID = glCreateShader(GL_FRAGMENT_SHADER);
	if (!CompileIndividualShader(vshader, vID)) {
		glDeleteShader(vID);
		glDeleteShader(fID);
		return false;
	}
	if (!CompileIndividualShader(fshader, fID)) {
		glDeleteShader(vID);
		glDeleteShader(fID);
		return false;
	}
	// link
		// vertex and fragment shaders created, now we need to link the program itself, combining the two
	*pID = glCreateProgram();
	glAttachShader(*pID, vID);
	glAttachShader(*pID, fID);
	glLinkProgram(*pID);
	
	// free the vid and fid
	glDetachShader(*pID, vID);
	glDetachShader(*pID, fID);

	glDeleteShader(vID);
	glDeleteShader(fID);

	GLint res;
	GLint infoLogLength;
	// check the program
	glGetProgramiv(*pID, GL_LINK_STATUS, &res);
	glGetProgramiv(*pID, GL_INFO_LOG_LENGTH, &infoLogLength);
	if (infoLogLength > 0) {
		char* ProgramErrorMessage = (char*)malloc(sizeof(char) * infoLogLength);
		glGetProgramInfoLog(*pID, infoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
		free(ProgramErrorMessage);
		return false;
	}
	return true;
}

void DeleteProgram(unsigned int pID) {
	glDeleteProgram(pID);
}

void DeleteTextures(int count, unsigned int* ref) {
	glDeleteTextures(count, ref);
}

unsigned int GenerateTexture2D(unsigned int type, int width, int height, unsigned int dataFormat, void* color, int magFilter, int minFilter, bool mipmap) {
	unsigned int retValue;
	glGenTextures(1, &retValue);
	glBindTexture(GL_TEXTURE_2D, retValue);
	glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0, type, dataFormat, color);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	if (mipmap) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	return retValue;
}

void UploadBuffer(unsigned int* id, unsigned int size, void* data) {
	glGenBuffers(1, id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *id);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

void UniformMatrix4fv(unsigned int loc, unsigned int size, bool transpose, void *value) {
	glUniformMatrix4fv(loc, size, transpose, value);
}

void Uniform2fv(unsigned int loc, unsigned int size, void* value) {
	glUniform2fv(loc, size, value);
}

void Uniform3fv(unsigned int loc, unsigned int size, void* value) {
	glUniform3fv(loc, size, value);
}

void Uniform4fv(unsigned int loc, unsigned int size, void* value) {
	glUniform4fv(loc, size, value);
}

void Uniform1fv(unsigned int loc, unsigned int size, void* value) {
	glUniform1fv(loc, size, value);
}

void Uniform1iv(unsigned int loc, unsigned int size, void* value) {
	glUniform1iv(loc, size, value);
}

void AssignTexture(int slot, unsigned int texture, unsigned int textureUniform) {
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(texture, slot);
}

void VertexAttribPointer(unsigned int bufferId, int attribId, int size, int type, bool normalized, int stride, void *arrayBuffOffset) {
	glEnableVertexAttribArray(attribId);
	glBindBuffer(GL_ARRAY_BUFFER, bufferId);
	glVertexAttribPointer(
		attribId, // attrib 0
		size, // size()
		type, // type
		normalized, // normalized
		stride, // stride(??)
		arrayBuffOffset // array buffer offset(?)
	);
}

void VertexAttribIPointer(unsigned int bufferId, int attribId, int size, int type, bool normalized, int stride, void* arrayBuffOffset) {
	glEnableVertexAttribArray(attribId);
	glBindBuffer(GL_ARRAY_BUFFER, bufferId);
	glVertexAttribIPointer(
		attribId, // attrib 0
		size, // size()
		type, // type
		stride, // stride(??)
		arrayBuffOffset // array buffer offset(?)
	);
}

void DisableVertexAttribArray(int attribId) {
	glDisableVertexAttribArray(attribId);
}

void DrawTriangles(unsigned int tId, int count) {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tId);
	glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
}

void DeleteBuffers(int count, void* ref) {
	glDeleteBuffers(count, ref);
}

void UseProgram(unsigned int id) {
	glUseProgram(id);
}

void BindVertexArray(unsigned int VAO) {
	glBindVertexArray(VAO);
}

void GenVertexArrays(int count, void* ref) {
	glGenVertexArrays(count, ref);
}

void DeleteVertexArrays(int count, void* ref) {
	glDeleteVertexArrays(count, ref);
}

void EnableTransparency() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(false);
}

void DisableTransparency() {
	glDisable(GL_BLEND);
	glDepthMask(true);
}

void EnableBackfaceCulling() {
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

void EnableFrontfaceCulling() {
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
}

void DisableBackfaceCulling() {
	glDisable(GL_CULL_FACE);
}

void EnableDepthTest() {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	//glDisable(GL_DEPTH_TEST);
}

void SetTextureWrapClamp(int UorV, unsigned int texRef) {
	glBindTexture(GL_TEXTURE_2D, texRef);
	glTexParameteri(GL_TEXTURE_2D, UorV == 0 ? GL_TEXTURE_WRAP_S : GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void SetTextureWrapWrap(int UorV, unsigned int texRef) {
	glBindTexture(GL_TEXTURE_2D, texRef);
	glTexParameteri(GL_TEXTURE_2D, UorV == 0 ? GL_TEXTURE_WRAP_S : GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SetTextureWrapMirror(int UorV, unsigned int texRef) {
	glBindTexture(GL_TEXTURE_2D, texRef);
	glTexParameteri(GL_TEXTURE_2D, UorV == 0 ? GL_TEXTURE_WRAP_S : GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
}