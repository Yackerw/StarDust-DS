#ifndef __OPENGL_RENDERER
#define __OPENGL_RENDERER
#include <stdio.h>
#include <stdbool.h>

#define TEX_RGBA 0x1908
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_INT 0x1404

unsigned int GetUniformLocation(unsigned int pid, char* ref);

bool CompileShader(char* vshader, char* fshader, unsigned int* pid);

void DeleteProgram(unsigned int pID);

void DeleteTextures(int count, unsigned int* ref);

unsigned int GenerateTexture2D(unsigned int type, int width, int height, unsigned int dataFormat, void* color, int magFilter, int minFilter, bool mipmap);

void UploadBuffer(unsigned int* id, unsigned int size, void* data);

void UniformMatrix4fv(unsigned int loc, unsigned int size, bool transpose, void* value);

void Uniform2fv(unsigned int loc, unsigned int size, void* value);

void Uniform3fv(unsigned int loc, unsigned int size, void* value);

void Uniform4fv(unsigned int loc, unsigned int size, void* value);

void Uniform1fv(unsigned int loc, unsigned int size, void* value);

void Uniform1iv(unsigned int loc, unsigned int size, void* value);

void AssignTexture(int slot, unsigned int texture, unsigned int textureUniform);

void VertexAttribPointer(unsigned int bufferId, int attribId, int size, int type, bool normalized, int stride, void* arrayBuffOffset);

void VertexAttribIPointer(unsigned int bufferId, int attribId, int size, int type, bool normalized, int stride, void* arrayBuffOffset);

void DisableVertexAttribArray(int attribId);

void DeleteBuffers(int count, void* ref);

void UseProgram(unsigned int id);

void BindVertexArray(unsigned int VAO);

void GenVertexArrays(int count, void* ref);

void DeleteVertexArrays(int count, void* ref);

void EnableDepthTest();

void SetTextureWrapClamp(int UorV, unsigned int texRef);

void SetTextureWrapWrap(int UorV, unsigned int texRef);

void SetTextureWrapMirror(int UorV, unsigned int texRef);

void EnableBackfaceCulling();

void DisableBackfaceCulling();

void EnableFrontfaceCulling();

void DisableTransparency();

void EnableTransparency();

#endif