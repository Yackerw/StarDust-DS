#include "Shader.h"
#include "OpenGL/OpenGLRenderer.h"

char* (*shaderIncludeCallback)(char*);

// literally just loads entire file
char* OpenShader(const char* path) {
	FILE* f = fopen(path, "rb");
	if (f == NULL) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	int vSize = ftell(f);
	char* fData = (char*)malloc(vSize + 1);
	fData[vSize] = 0;
	fseek(f, 0, SEEK_SET);
	fread(fData, vSize, 1, f);
	fclose(f);
	return fData;
}

// finds semi colon
int FindSemi(int start, char* input) {
	int len = strlen(input);
	while (start < len) {
		if (input[start] == ";"[0]) {
			return start;
		}
		++start;
	}
	return start;
}

// returns first character after white space
int FindAfterEmpty(int start, char* input) {
	int len = strlen(input);
	while (start < len) {
		if (input[start] > 0x20) {
			return start;
		}
		++start;
	}
	return start;
}

// returns first white space character
int FindEmpty(int start, char* input) {
	int len = strlen(input);
	while (start < len) {
		if (input[start] <= 0x20) {
			return start;
		}
		++start;
	}
	return start;
}

// returns first white space character/bracket (add 1 for bracket)
int FindEmptyAndBracket(int start, char* input) {
	int len = strlen(input);
	while (start < len) {
		if (input[start] <= 0x20) {
			return start;
		}
		if (input[start] == "{"[0] || input[start] == "}"[0]) {
			return start + 1;
		}
		++start;
	}
	return start;
}

// finds last character before new line
int FindBeforeNewLine(int start, char* input) {
	int len = strlen(input);
	while (start < len) {
		if (input[start] < 0x20) {
			return start - 1;
		}
		++start;
	}
	return start;
}

// returns new line
int FindNewLine(int start, char* input) {
	int len = strlen(input);
	bool whiteFound = false;
	while (start < len) {
		if (input[start] < 0x20) {
			whiteFound = true;
		}
		else if (whiteFound) {
			return start;
		}
		++start;
	}
	return start;
}

// finds the end of a /* style comment
int FindCommentEnd(int start, char* input) {
	int len = strlen(input);
	while (start < len) {
		if (input[start] == "*"[0] && start < len - 1 && input[start + 1] == "/"[0]) {
			return start;
		}
		++start;
	}
	return start;
}

// extracts string from between two indexes
char* ExtractString(int start, int end, char* input) {
	char* retval = (char*)malloc((end - start) + 1);
	memcpy(retval, input + start, end - start);
	retval[end - start] = 0;
	return retval;
}

int CheckStringExists(PsuedoVector *vec, char* str) {
	for (int i = 0; i < vec->count; ++i) {
		if (strcmp(((char**)vec->data)[i], str) == 0) {
			return i;
		}
	}
	return -1;
}

int ScrubComments(int i, char* v) {
	// handle comments
	if (v[i] == "/"[0]) {
		if (i < strlen(v) - 1) {
			switch (v[i + 1]) {
			case 0x2F:
				i = FindNewLine(i, v);
				break;
			case 0x2A:
				i = FindCommentEnd(i, v);
				break;
			}
		}
	}
	return i;
}

// just removes characters between start and end in a string
char* RemoveBetween(int start, int end, char* v) {
	int newSize = strlen(v) - (end - start);
	memmove(v + start, v + end, strlen(v) - end);
	v[newSize] = 0;
	return (char*)realloc(v, newSize + 1);
}

// replaces characters between start and end in a string
char* ReplaceBetween(int start, int end, char* v, char* r) {
	int len = strlen(v);
	int newSize = len - ((end - start) - strlen(r));
	if (newSize > len) {
		v = (char*)realloc(v, newSize + 1);
	}
	memmove(v + start + strlen(r), v + end, len - end);
	if (newSize < len) {
		v = (char*)realloc(v, newSize + 1);
	}
	v[newSize] = 0;
	memcpy(v + start, r, strlen(r));
	return v;
}

// does as it says, returns index
int FindStringInVec(PsuedoVector *v, char* s) {
	for (int i = 0; i < v->count; ++i) {
		if (strcmp(s, ((char**)v->data)[i]) == 0) {
			return i;
		}
	}
	return -1;
}

char* Insert(int start, char* v, char* input) {
	int inputLen = strlen(input);
	int baseLen = strlen(v);
	v = (char*)realloc(v, baseLen + inputLen + 1);
	memmove(v + start + inputLen, v + start, baseLen - start);
	memcpy(v + start, input, inputLen);
	v[inputLen + baseLen] = 0;
	return v;
}

// scans for # directives, including #include and #define
// char *defNames, char *defs
char* PreProcessShader(char* v, PsuedoVector* defNames, PsuedoVector* defs) {
	int ifLayer = 0;
	//std::vector<bool> ifDeletes;
	PsuedoVector ifDeletes;
	//std::vector<int> ifDeleteStarts;
	PsuedoVector ifDeleteStarts;
	memset(&ifDeletes, 0, sizeof(PsuedoVector));
	memset(&ifDeleteStarts, 0, sizeof(PsuedoVector));
	ifDeletes.itemSize = sizeof(bool);
	ifDeleteStarts.itemSize = sizeof(int);
	// if we're inside an ifdef that is going to be removed, don't waste time executing things (or accidentally defining things that shouldn't be!)
	bool dontExec = false;
	for (int i = 0; i < strlen(v);) {
		i = FindAfterEmpty(i, v);
		i = ScrubComments(i, v);
		dontExec = false;
		if (ifLayer > 0) {
			dontExec = ((bool*)ifDeletes.data)[ifLayer - 1];
		}
		if (v[i] == "#"[0]) {
			int start = i;
			int end = FindEmpty(i, v);
			char* directive = ExtractString(i, end, v);
			i = FindAfterEmpty(end, v);
			// there are pre-processor statements we don't handle here, and we don't want to accidentally remove them (notably, #version!)
			bool removeNewLine = false;
			// switch statements don't exist so
			if (strcmp(directive, "#define") == 0 && !dontExec) {
				// defines are formatted in the form of (name) [define] with name ending at a space and define ending at a new line
				// so we find these and store them to vectors for later use
				end = FindEmpty(i, v);
				char* defName = ExtractString(i, end, v);
				int newLineTest = FindBeforeNewLine(end, v) + 1;
				i = FindAfterEmpty(end, v);
				// if there's a new line before the first character after empty, there is nothing associated with this define. it's just defined, likely for an ifdef
				if (newLineTest >= i) {
					end = FindBeforeNewLine(i, v) + 1;
					char* def = ExtractString(i, end, v);
					PVectorAppend(defs, &def);
				}
				else {
					char* def = (char*)malloc(1);
					def[0] = 0;
					PVectorAppend(defs, &def);
				}
				PVectorAppend(defNames, &defName);

				removeNewLine = true;
			}
			else if (strcmp(directive, "#undef") == 0 && !dontExec) {
				// remove defines; still legal if that define doesn't exist
				end = FindEmpty(i, v);
				char* defName = ExtractString(i, end, v);
				int defInd = FindStringInVec(defNames, defName);
				free(defName);
				if (defInd != -1) {
					PVectorDelete(defNames, defInd);
					PVectorDelete(defs, defInd);
				}

				removeNewLine = true;
			}
			else if (strcmp(directive, "#ifdef") == 0) {
				// if the value is defined, we will continue to check for corresponding endif to remove. otherwise, we remove all the text between
				// we also need to account for #else and #elif
				end = FindEmpty(i, v);
				char* defName = ExtractString(i, end, v);
				int defExists = FindStringInVec(defNames, defName);
				free(defName);
				++ifLayer;
				PVectorAppend(&ifDeleteStarts, &start);
				if (defExists != -1 && !dontExec) {
					// exists, let us remove the endif and elses
					bool tmp = false;
					PVectorAppend(&ifDeletes, &tmp);
				}
				else {
					// do remove between this and endif
					bool tmp = true;
					PVectorAppend(&ifDeletes, &tmp);
				}
				removeNewLine = true;
			}
			else if (strcmp(directive, "#endif") == 0) {
				if (ifLayer <= 0) {
					// TODO: error out
				}
				else {
					// if we're in a check that failed, remove it. otherwise, do nothing
					if (((bool*)ifDeletes.data)[ifLayer - 1]) {
						v = RemoveBetween(((int*)ifDeleteStarts.data)[ifLayer - 1], start, v);
						start = ((int*)ifDeleteStarts.data)[ifLayer - 1];
					}
					PVectorResize(&ifDeletes, ifDeletes.count - 1);
					PVectorResize(&ifDeleteStarts, ifDeleteStarts.count - 1);
					--ifLayer;
				}
				removeNewLine = true;
			}
			else if (strcmp(directive, "#else") == 0) {
				if (ifLayer <= 0) {
					// TODO: error out
				}
				else {
					// if we're in a check that failed, remove it. otherwise, do nothing. same as endif
					if (((bool*)ifDeletes.data)[ifLayer - 1]) {
						v = RemoveBetween(((int*)ifDeleteStarts.data)[ifLayer - 1], start, v);
						start = ((int*)ifDeleteStarts.data)[ifLayer - 1];
					}
					// invert what the deletes should be, but only if we aren't in something that should also be deleted
					if (ifLayer <= 1 || (!((bool*)ifDeletes.data)[ifLayer - 2])) {
						((bool*)ifDeletes.data)[ifLayer - 1] = !((bool*)ifDeletes.data)[ifLayer - 1];
						((int*)ifDeleteStarts.data)[ifLayer - 1] = start;
					}
				}
			}
			else if (strcmp(directive, "#elif") == 0) {
				if (ifLayer <= 0) {
					// TODO: error out
				}
				else {
					// if we're in a check that failed, remove it. otherwise, do nothing. same as endif
					if (((bool*)ifDeletes.data)[ifLayer - 1]) {
						v = RemoveBetween(((int*)ifDeleteStarts.data)[ifLayer - 1], start, v);
						start = ((int*)ifDeleteStarts.data)[ifLayer - 1];
						// now we have to re-locate where we were before removing that chunk because we removed it
						i = start;
						i = FindEmpty(i, v);
						i = FindAfterEmpty(i, v);
						end = FindEmpty(i, v);
						// now check if the define exists, if it does, then mark this to not be deleted
						char* defName = ExtractString(i, end, v);
						int defExists = FindStringInVec(defNames, defName);
						free(defName);
						// ...but first make sure we're not in a block that needs to be erased too
						if (ifLayer > 1 && (!((bool*)ifDeletes.data)[ifLayer - 2])) {
							defExists = -1;
						}
						if (defExists != -1) {
							((bool*)ifDeletes.data)[ifLayer - 1] = false;
						}
					}
					else {
						((bool*)ifDeletes.data)[ifLayer - 1] = true;
						((int*)ifDeleteStarts.data)[ifLayer - 1] = start;
					}
				}
				removeNewLine = true;
			}
			else if (strcmp(directive, "#include") == 0) {
				// do nothing if we're in a check that failed
				if (ifLayer <= 0 || ((bool*)ifDeletes.data)[ifLayer - 1]) {
					// get name
					i = start;
					i = FindEmpty(i, v);
					i = FindAfterEmpty(i, v);
					end = FindEmpty(i, v);
					char* fileName = ExtractString(i, end, v);

					v = RemoveBetween(start, FindNewLine(start, v), v);
					if (shaderIncludeCallback != NULL) {
						char* callBackRet = shaderIncludeCallback(fileName);
						if (callBackRet == NULL) {
							// TODO: error out
						}
						else {
							v = Insert(start, v, callBackRet);
							free(callBackRet);
						}
					}
					free(fileName);
				}
			}

			if (removeNewLine) {
				v = RemoveBetween(start, FindNewLine(start, v), v);
				i = start;
			}
			else {
				i = FindNewLine(start, v);
			}
			free(directive);
		}
		else {
			// check for defined values and replace them in the string
			int end = FindEmpty(i, v);
			char* defCheck = ExtractString(i, end, v);
			int start = i;
			i = end;
			if (!dontExec) {
				for (int i2 = 0; i2 < defNames->count; ++i2) {
					if (strcmp(defCheck, ((char**)defNames->data)[i2]) == 0) {
						// you can use defines inside of defines, so you have to pre-process a define as it's placed in as well
						char* processedDef = (char*)malloc(strlen(((char**)defs->data)[i2]) + 1);
						memcpy(processedDef, ((char**)defs->data)[i2], strlen(((char**)defs->data)[i2]) + 1);
						processedDef = PreProcessShader(processedDef, defNames, defs);
						v = ReplaceBetween(start, end, v, processedDef);
						i = i + strlen(processedDef);
						i2 = (*defNames).count;
						free(processedDef);
					}
				}
			}
			free(defCheck);
		}
	}
	FreePVector(&ifDeletes);
	FreePVector(&ifDeleteStarts);
	return v;
}

// finds and stores all the uniforms in the file
void ParseShaderUniforms(char* v, Shader* shader) {
	int i = 0;
	int len = strlen(v);
	while (i < len) {
		i = FindAfterEmpty(i, v);
		i = ScrubComments(i, v);
		if (v[i] == "u"[0]) {
			// wow, we found a potential uniform! let us read what it ACTUALLY is...
			int end = FindEmpty(i, v);
			// set up string to do so
			char* str = ExtractString(i, end, v);
			// it actually is a uniform! find typing
			if (strcmp(str, "uniform") == 0) {
				i = FindAfterEmpty(end, v);
				end = FindEmpty(i, v);
				free(str);
				// set up string to figure out type
				str = ExtractString(i, end, v);
				// int defaults to 0
				int uniType = 0;
				// no string switch in C, so...
				if (strcmp(str, "mat4") == 0) {
					// indicate typing
					uniType = 1;
				}
				else if (strcmp(str, "vec2") == 0) {
					uniType = 2;
				}
				else if (strcmp(str, "vec3") == 0) {
					uniType = 3;
				}
				else if (strcmp(str, "vec4") == 0) {
					uniType = 4;
				}
				else if (strcmp(str, "float") == 0) {
					uniType = 5;
				}
				else if (strcmp(str, "int") == 0) {
					uniType = 6;
				}
				// texture type!
				else if (strcmp(str, "sampler2D") == 0) {
					uniType = 100;
				}
				// and find name
				i = FindAfterEmpty(end, v);
				end = FindSemi(i, v);
				int length = 1;
				int endArray = end;
				// ensure it's not an array though
				if (v[end - 1] == "]"[0]) {
					int lengthEnd = end - 1;
					while (v[end] != "["[0] && end > i)
						--end;
					// get length now
					char* len = ExtractString(end + 1, lengthEnd, v);
					length = atoi(len);
					free(len);
				}
				free(str);
				str = ExtractString(i, end, v);
				end = endArray;
				// now, check if it already exists inside of uniformNames/texturenames
				// uniType will be >= 100 if it's a texture type
				int len2 = strlen(str);
				char* newRef = (char*)malloc(len2 + 1);
				newRef[len2] = 0;
				memcpy(newRef, str, len2);
				if (uniType < 100) {
					if (CheckStringExists(&shader->uniformNames, str) == -1) {
						PVectorAppend(&shader->uniformNames, &newRef);
						PVectorAppend(&shader->uniformTypes, &uniType);
						unsigned int tmp = GetUniformLocation(shader->pID, newRef);
						PVectorAppend(&shader->uniformIDs, &tmp);
						PVectorAppend(&shader->uniformLengths, &length);
					}
					else {
						free(newRef);
					}
				}
				else {
					if (CheckStringExists(&shader->textureNames, str) == -1) {
						PVectorAppend(&shader->textureNames, &newRef);
						PVectorAppend(&shader->textureTypes, &uniType);
					}
					else {
						free(newRef);
					}
				}
			}
			free(str);
		}
		else if (v[i] == "#"[0] || v[i] == "{"[0] || v[i] == "}"[0]) {
			i = FindEmptyAndBracket(i, v);
		}
		else {
			i = FindSemi(i, v) + 1;
		}
	}
}

void DeleteShader(Shader* shader) {
	DeleteProgram(shader->pID);
	free(shader->origVert);
	free(shader->origFrag);
	for (int i = 0; i < shader->textureNames.count; ++i) {
		free(((char**)shader->textureNames.data)[i]);
	}
	for (int i = 0; i < shader->uniformNames.count; ++i) {
		free(((void**)shader->uniformNames.data)[i]);
	}
	FreePVector(&shader->textureNames);
	FreePVector(&shader->textureTypes);
	FreePVector(&shader->uniformNames);
	FreePVector(&shader->uniformTypes);
	FreePVector(&shader->uniformIDs);
	FreePVector(&shader->uniformLengths);
}

Shader* ProcessShader(char** vert, char** frag, Shader* shader) {
	char* v = *vert;
	char* f = *frag;
	// process the pre-processor directives
	//std::vector<char*>* defNames = new std::vector<char*>();
	//std::vector<char*>* defs = new std::vector<char*>();
	PsuedoVector defNames;
	PsuedoVector defs;
	memset(&defNames, 0, sizeof(PsuedoVector));
	memset(&defs, 0, sizeof(PsuedoVector));
	defNames.itemSize = sizeof(char*);
	defs.itemSize = sizeof(char*);
	v = PreProcessShader(v, &defNames, &defs);
	for (int i = 0; i < defNames.count; ++i) {
		free(((void**)defNames.data)[i]);
		free(((void**)defs.data)[i]);
	}
	FreePVector(&defNames);
	FreePVector(&defs);
	memset(&defNames, 0, sizeof(PsuedoVector));
	memset(&defs, 0, sizeof(PsuedoVector));
	defNames.itemSize = sizeof(char*);
	defs.itemSize = sizeof(char*);
	f = PreProcessShader(f, &defNames, &defs);
	for (int i = 0; i < defNames.count; ++i) {
		free(((void**)defNames.data)[i]);
		free(((void**)defs.data)[i]);
	}
	FreePVector(&defNames);
	FreePVector(&defs);
	// create shaders for us to use
	if (!CompileShader(v, f, &shader->pID)) {
		DeleteShader(shader);
		free(shader);
		return NULL;
	}

	// iterate through and find all the uniforms
	ParseShaderUniforms(v, shader);
	ParseShaderUniforms(f, shader);
	free(f);
	free(v);

	return shader;
}

void InitializeVectors(Shader* shader) {
	shader->textureNames.itemSize = sizeof(char*);
	shader->textureTypes.itemSize = sizeof(int);
	shader->uniformIDs.itemSize = sizeof(unsigned int);
	shader->uniformTypes.itemSize = sizeof(int);
	shader->uniformLengths.itemSize = sizeof(int);
	shader->uniformNames.itemSize = sizeof(char*);
}

Shader* LoadShader(const char* vertPath, const char* fragPath) {
	// open the two shaders and load them into memory
	char* vString = OpenShader(vertPath);
	char* fString = OpenShader(fragPath);
	if (vString == NULL || fString == NULL) {
		return NULL;
	}
	// preserve original files in case we need to re-process later
	Shader* retValue = (Shader*)calloc(1, sizeof(Shader));
	const int vSize = strlen(vString) + 1;
	const int fSize = strlen(fString) + 1;
	retValue->origVert = (char*)malloc(vSize);
	retValue->origFrag = (char*)malloc(fSize);
	memcpy(retValue->origVert, vString, vSize);
	memcpy(retValue->origFrag, fString, fSize);
	// set up the vectors first
	InitializeVectors(retValue);
	// and process
	return ProcessShader(&vString, &fString, retValue);
}