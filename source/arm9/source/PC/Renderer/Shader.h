#ifndef __OGLSHADER
#define __OGLSHADER
#include <string.h>
#include "PsuedoVector.h"
#include <stdbool.h>

typedef struct {
	char* origVert;
	char* origFrag;
	unsigned int vID;
	unsigned int fID;
	unsigned int pID;
	//std::vector<char*> textureNames;
	PsuedoVector textureNames;
	//std::vector<int> textureTypes;
	PsuedoVector textureTypes;
	//std::vector<GLuint> uniformIDs;
	PsuedoVector uniformIDs;
	//std::vector<int> uniformTypes;
	PsuedoVector uniformTypes;
	//std::vector<int> uniformLengths;
	PsuedoVector uniformLengths;
	//std::vector<char*> uniformNames;
	PsuedoVector uniformNames;
} Shader;
extern char* (*shaderIncludeCallback)(char*);

void DeleteShader(Shader* shader);

Shader* LoadShader(const char* vertPath, const char* fragPath);

#endif