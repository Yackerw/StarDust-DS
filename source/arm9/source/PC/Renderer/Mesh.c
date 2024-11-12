#include "Mesh.h"
#include "OpenGL/OpenGLRenderer.h"
void SetMaterialReferenceTexture(char* name, NativeTexture* tex, MaterialShaderReference* ref) {
	// if no texture is located, then there's nothing we can even do here anyways
	if (ref->shader == NULL) {
		return;
	}
	// find texture reference in shader
	for (int i = 0; i < ((Shader*)ref->shader)->textureNames.count; ++i) {
		if (strcmp(name, ((char**)((Shader*)ref->shader)->textureNames.data)[i]) == 0) {
			((NativeTexture**)ref->currTexs.data)[i] = tex;
			i = ((Shader*)ref->shader)->textureNames.count;
		}
	}
}

void SetMaterialNativeTexture(Material* mat, char* name, NativeTexture* tex) {
	// copy the name so we can recall it later
	int len = strlen(name) + 1;
	char* newName = (char*)malloc(len);
	memcpy(newName, name, len);
	// find if texture already exists
	NativeTexture* prevTex = NULL;
	bool found = false;
	for (int i = 0; i < mat->textures.count; ++i) {
		if (strcmp(name, ((char**)mat->textures.data)[i]) == 0) {
			prevTex = ((NativeTexture**)mat->texRefs.data)[i];
			free(((void**)mat->textures.data)[i]);
			((char**)mat->textures.data)[i] = newName;
			((NativeTexture**)mat->texRefs.data)[i] = tex;
			found = true;
			i = mat->textures.count;
		}
	}
	// else, push it back
	if (!found) {
		PVectorAppend(&mat->textures, &newName);
		PVectorAppend(&mat->texRefs, &tex);
	}
	found = false;
	// if we've replaced a texture, ensure we haven't set it anywhere else in the material, otherwise remove our reference from it
	if (prevTex != NULL) {
		for (int i = 0; i < mat->texRefs.count; ++i) {
			if (((NativeTexture**)mat->texRefs.data)[i] == prevTex) {
				found = true;
				i = mat->texRefs.count;
			}
		}
	}
	if (!found && prevTex != NULL) {
		// remove this material from the now dereferenced texture
		for (int i = 0; i < prevTex->materialsIn.count; ++i) {
			if (mat == ((Material**)prevTex->materialsIn.data)[i]) {
				PVectorDelete(&prevTex->materialsIn, i);
			}
		}
	}
	// set the material reference in the texture so it can be cleared correctly later
	found = false;
	if (tex->materialsIn.data != NULL) {
		for (int i = 0; i < tex->materialsIn.count; ++i) {
			if (((Material**)tex->materialsIn.data)[i] == mat) {
				found = true;
				i = tex->materialsIn.count;
			}
		}
		if (!found) {
			PVectorAppend(&tex->materialsIn, &mat);
		}
	}
	SetMaterialReferenceTexture(name, tex, &mat->mainShader);
	SetMaterialReferenceTexture(name, tex, &mat->directionalShadowShader);
	SetMaterialReferenceTexture(name, tex, &mat->pointShadowShader);
	for (int i = 0; i < mat->passShaders.count; ++i) {
		SetMaterialReferenceTexture(name, tex, &((MaterialShaderReference*)mat->passShaders.data)[i]);
	}
}

void InitializeTexturesAndUniforms(Material* mat, Shader* shad, MaterialShaderReference* ref) {
	// copy saved textures here, if applicable
	for (int i = 0; i < mat->textures.count; ++i) {
		for (int i2 = 0; i2 < shad->textureNames.count; ++i2) {
			if (strcmp(((char**)mat->textures.data)[i], ((char**)shad->textureNames.data)[i2]) == 0) {
				((NativeTexture**)ref->currTexs.data)[i2] = ((NativeTexture**)mat->texRefs.data)[i];
				i2 = shad->textureNames.count;
			}
		}
	}
	// same, but for uniform variables
	for (int i = 0; i < mat->uniformValues.count; ++i) {
		for (int i2 = 0; i2 < shad->uniformNames.count; ++i2) {
			if (strcmp(((UniformValue*)mat->uniformValues.data)[i].name, ((char**)shad->uniformNames.data)[i2]) == 0) {
				((void**)ref->currUniforms.data)[i2] = ((UniformValue*)mat->uniformValues.data)[i].value;
				i2 = shad->uniformNames.count;
			}
		}
	}
}

void SetupMaterialShaderPass(Material* mat, Shader* shad, MaterialShaderReference* ref) {
	ref->currTexs.count = 0;
	ref->currUniforms.count = 0;
	PVectorResize(&ref->currTexs, shad->textureNames.count);
	PVectorResize(&ref->currUniforms, shad->uniformIDs.count);
	memset(ref->currTexs.data, 0, sizeof(NativeTexture*) * shad->textureNames.count);
	memset(ref->currUniforms.data, 0, sizeof(void*) * shad->uniformIDs.count);
	ref->shader = shad;
	InitializeTexturesAndUniforms(mat, shad, ref);
}

void SetMaterialShader(Material* mat, Shader* shad) {
	SetupMaterialShaderPass(mat, shad, &mat->mainShader);
}

void SetMaterialDirectionalShader(Material* mat, Shader* shad) {
	SetupMaterialShaderPass(mat, shad, &mat->directionalShadowShader);
}

void SetupMaterialPointShader(Material* mat, Shader* shad) {
	SetupMaterialShaderPass(mat, shad, &mat->pointShadowShader);
}

void SetMaterialPassShader(Material* mat, Shader* shad, char* pass) {
	// check if we already have a shader for this pass
	for (int i = 0; i < mat->passShaders.count; ++i) {
		if (strcmp(pass, ((MaterialShaderReference*)mat->passShaders.data)[i].pass) == 0) {
			SetupMaterialShaderPass(mat, shad, &((MaterialShaderReference*)mat->passShaders.data)[i]);
			return;
		}
	}
	// ok, set up a new pass then
	MaterialShaderReference ref;
	memset(&ref, 0, sizeof(ref));
	ref.currTexs.itemSize = sizeof(NativeTexture*);
	ref.currUniforms.itemSize = sizeof(void*);
	int passLength = strlen(pass) + 1;
	ref.pass = (char*)malloc(sizeof(char) * passLength);
	memcpy(ref.pass, pass, sizeof(char) * passLength);
	SetupMaterialShaderPass(mat, shad, &ref);
	PVectorAppend(&mat->passShaders, &ref);
}

void SetMaterialReferenceUniform(MaterialShaderReference* ref, char* uniformName, void* uniformValue) {
	if (ref->shader == NULL) {
		return;
	}
	for (int i = 0; i < ((Shader*)ref->shader)->uniformNames.count; ++i) {
		if (strcmp(((char**)((Shader*)ref->shader)->uniformNames.data)[i], uniformName) == 0) {
			((void**)ref->currUniforms.data)[i] = uniformValue;
			break;
		}
	}
}

void SetMaterialUniform(Material* mat, char* uniformName, void* uniformValue) {
	int uniform;
	bool uniformFound = false;
	// replace if already exists...
	for (int i = 0; i < mat->uniformValues.count; ++i) {
		if (strcmp(((UniformValue*)mat->uniformValues.data)[i].name, uniformName) == 0) {
			uniformFound = true;
			uniform = i;
			break;
		}
	}
	// if uniform not found, we must add a new one
	if (!uniformFound) {
		// copy the name so we can recall it later
		int len = strlen(uniformName) + 1;
		char* newName = (char*)malloc(len);
		memcpy(newName, uniformName, len);
		UniformValue uv;
		uv.name = newName;
		uv.value = uniformValue;
		PVectorAppend(&mat->uniformValues, &uv);
		uniform = mat->uniformValues.count - 1;
	}
	else {
		free(((UniformValue*)mat->uniformValues.data)[uniform].value);
		((UniformValue*)mat->uniformValues.data)[uniform].value = uniformValue;
	}
	SetMaterialReferenceUniform(&mat->mainShader, uniformName, uniformValue);
	SetMaterialReferenceUniform(&mat->directionalShadowShader, uniformName, uniformValue);
	SetMaterialReferenceUniform(&mat->pointShadowShader, uniformName, uniformValue);
	for (int i = 0; i < mat->passShaders.count; ++i) {
		SetMaterialReferenceUniform(&((MaterialShaderReference*)mat->passShaders.data)[i], uniformName, uniformValue);
	}
}

void DeleteMaterial(Material* mat) {
	// remove ourselves from all the textures
	for (int i = 0; i < mat->texRefs.count; ++i) {
		if (((NativeTexture**)mat->texRefs.data)[i]->materialsIn.data != NULL) {
			for (int i2 = 0; i2 < ((NativeTexture**)mat->texRefs.data)[i]->materialsIn.count; ++i2) {
				if (((Material**)((NativeTexture**)mat->texRefs.data)[i]->materialsIn.data)[i2] == mat) {
					PVectorDelete(&((NativeTexture**)mat->texRefs.data)[i]->materialsIn, i2);
					i2 = ((NativeTexture**)mat->texRefs.data)[i]->materialsIn.count;
				}
			}
		}
	}
	// free various names and such
	for (int i = 0; i < mat->textures.count; ++i) {
		free(((char**)mat->textures.data)[i]);
	}
	for (int i = 0; i < mat->uniformValues.count; ++i) {
		free(((UniformValue*)mat->uniformValues.data)[i].name);
		free(((UniformValue*)mat->uniformValues.data)[i].value);
	}
	FreePVector(&mat->textures);
	FreePVector(&mat->uniformValues);
	FreePVector(&mat->texRefs);
	FreePVector(&mat->mainShader.currTexs);
	FreePVector(&mat->mainShader.currUniforms);
	FreePVector(&mat->directionalShadowShader.currTexs);
	FreePVector(&mat->directionalShadowShader.currUniforms);
	FreePVector(&mat->pointShadowShader.currTexs);
	FreePVector(&mat->pointShadowShader.currUniforms);
	for (int i = 0; i < mat->passShaders.count; ++i) {
		MaterialShaderReference* ref = &((MaterialShaderReference*)mat->passShaders.data)[i];
		free(ref->pass);
		FreePVector(&ref->currTexs);
		FreePVector(&ref->currUniforms);
	}
	FreePVector(&mat->passShaders);
}

void InitMaterial(Material* mat) {
	mat->textures.count = 0;
	mat->texRefs.count = 0;
	mat->textures.allocSize = 0;
	mat->texRefs.allocSize = 0;
	mat->textures.data = NULL;
	mat->texRefs.data = NULL;
	mat->textures.itemSize = sizeof(char*);
	mat->texRefs.itemSize = sizeof(NativeTexture*);

	mat->uniformValues.count = 0;
	mat->uniformValues.allocSize = 0;
	mat->uniformValues.data = NULL;
	mat->uniformValues.itemSize = sizeof(UniformValue);

	mat->mainShader.currTexs.count = 0;
	mat->mainShader.currTexs.allocSize = 0;
	mat->mainShader.currTexs.data = NULL;
	mat->mainShader.currTexs.itemSize = sizeof(NativeTexture*);

	mat->mainShader.currUniforms.count = 0;
	mat->mainShader.currUniforms.allocSize = 0;
	mat->mainShader.currUniforms.data = NULL;
	mat->mainShader.currUniforms.itemSize = sizeof(void*);
	mat->mainShader.shader = NULL;

	mat->directionalShadowShader.currTexs.count = 0;
	mat->directionalShadowShader.currTexs.allocSize = 0;
	mat->directionalShadowShader.currTexs.data = NULL;
	mat->directionalShadowShader.currTexs.itemSize = sizeof(NativeTexture*);

	mat->directionalShadowShader.currUniforms.count = 0;
	mat->directionalShadowShader.currUniforms.allocSize = 0;
	mat->directionalShadowShader.currUniforms.data = NULL;
	mat->directionalShadowShader.currUniforms.itemSize = sizeof(void*);
	mat->directionalShadowShader.shader = NULL;

	mat->pointShadowShader.currTexs.count = 0;
	mat->pointShadowShader.currTexs.allocSize = 0;
	mat->pointShadowShader.currTexs.data = NULL;
	mat->pointShadowShader.currTexs.itemSize = sizeof(NativeTexture*);

	mat->pointShadowShader.currUniforms.count = 0;
	mat->pointShadowShader.currUniforms.allocSize = 0;
	mat->pointShadowShader.currUniforms.data = NULL;
	mat->pointShadowShader.currUniforms.itemSize = sizeof(void*);
	mat->pointShadowShader.shader = NULL;

	mat->passShaders.count = 0;
	mat->passShaders.allocSize = 0;
	mat->passShaders.data = NULL;
	mat->passShaders.itemSize = sizeof(MaterialShaderReference);
	mat->transparent = false;
	mat->frontFaceCulling = false;
	mat->backFaceCulling = false;
}

void UpdateSubMesh(SubMesh* subMesh) {

	// send all the vertex information to the gpu
	if (subMesh->triangleCount > 0) {
		UploadBuffer(&subMesh->triangleBuffer, sizeof(int) * subMesh->triangleCount, &subMesh->triangles[0]);
		subMesh->tUsed = true;
	}
	subMesh->valid = true;
}

void FreeSubMeshBuffers(SubMesh* subMesh) {
	if (subMesh->tUsed) {
		DeleteBuffers(1, &subMesh->triangleBuffer);
	}
	subMesh->tUsed = false;
}

void DestroySubMesh(SubMesh* subMesh) {
	FreeSubMeshBuffers(subMesh);
}

void DeleteSubMesh(SubMesh* subMesh) {
	DestroySubMesh(subMesh);
	if (subMesh->triangles != NULL) {
		free(subMesh->triangles);
	}
}

void RenderSubMesh(Mesh* mesh, Material* material, SubMesh subMesh, MaterialShaderReference* pass) {
	//glFrontFace(GL_CW);
//glCullFace(GL_FRONT);
	BindVertexArray(mesh->VAO);
	UseProgram(((Shader*)pass->shader)->pID);
	// set all of our uniforms
	for (int i = 0; i < ((Shader*)pass->shader)->uniformIDs.count && i < pass->currUniforms.count; ++i) {
		if ((((void**)pass->currUniforms.data)[i]) != NULL) {
			switch (((int*)((Shader*)pass->shader)->uniformTypes.data)[i]) {
			case 1:
				UniformMatrix4fv(((unsigned int*)((Shader*)pass->shader)->uniformIDs.data)[i], ((int*)((Shader*)pass->shader)->uniformLengths.data)[i], true, (((void**)pass->currUniforms.data)[i]));
				break;
			case 2:
				Uniform2fv(((unsigned int*)((Shader*)pass->shader)->uniformIDs.data)[i], ((int*)((Shader*)pass->shader)->uniformLengths.data)[i], (((void**)pass->currUniforms.data)[i]));
				break;
			case 3:
				Uniform3fv(((unsigned int*)((Shader*)pass->shader)->uniformIDs.data)[i], ((int*)((Shader*)pass->shader)->uniformLengths.data)[i], (((void**)pass->currUniforms.data)[i]));
				break;
			case 4:
				Uniform4fv(((unsigned int*)((Shader*)pass->shader)->uniformIDs.data)[i], ((int*)((Shader*)pass->shader)->uniformLengths.data)[i], (((void**)pass->currUniforms.data)[i]));
				break;
			case 5:
				Uniform1fv(((unsigned int*)((Shader*)pass->shader)->uniformIDs.data)[i], ((int*)((Shader*)pass->shader)->uniformLengths.data)[i], (((void**)pass->currUniforms.data)[i]));
				break;
			case 6:
				Uniform1iv(((unsigned int*)((Shader*)pass->shader)->uniformIDs.data)[i], ((int*)((Shader*)pass->shader)->uniformLengths.data)[i], (((void**)pass->currUniforms.data)[i]));
				break;
			}
		}
	}
	// iterate over our textures and set them all in the shader
	for (int i = 0; i < pass->currTexs.count; ++i) {
		if (((NativeTexture**)pass->currTexs.data)[i] != NULL) {
			AssignTexture(i, ((NativeTexture**)pass->currTexs.data)[i]->texRef, ((unsigned int*)(((Shader*)pass->shader)->textureUniforms.data))[i]);
		}
	}
	VertexAttribPointer(mesh->vertexBuffer, 0, 3, GL_FLOAT, false, 0, (void*)0);
	if (mesh->uUsed) {
		VertexAttribPointer(mesh->vertexUVs, 1, 2, GL_FLOAT, false, 0, (void*)0);
	}
	if (mesh->nUsed) {
		VertexAttribPointer(mesh->vertexNorms, 2, 3, GL_FLOAT, false, 0, (void*)0);
	}
	if (mesh->bUsed) {
		VertexAttribIPointer(mesh->vertexBoneIndex, 3, 4, GL_INT, false, 0, (void*)0);
	}
	if (mesh->wUsed) {
		VertexAttribPointer(mesh->vertexWeights, 4, 4, GL_FLOAT, false, 0, (void*)0);
	}
	// ehh change this later maybe
	EnableDepthTest();
	// transparency, backface culling, etc
	if (material->transparent) {
		EnableTransparency();
	}
	else {
		DisableTransparency();
	}
	if (material->backFaceCulling) {
		EnableBackfaceCulling();
	}
	else if (material->frontFaceCulling) {
		EnableFrontfaceCulling();
	} else {
		DisableBackfaceCulling();
	}
	// first array position, length
	DrawTriangles(subMesh.triangleBuffer, subMesh.triangleCount);
	// TODO: when we get around to model rendering, optimize this?
	DisableVertexAttribArray(0);
	if (mesh->uUsed) {
		DisableVertexAttribArray(1);
	}
	if (mesh->nUsed) {
		DisableVertexAttribArray(2);
	}
	if (mesh->bUsed) {
		DisableVertexAttribArray(3);
	}
	if (mesh->wUsed) {
		DisableVertexAttribArray(4);
	}
}

void RenderMesh(Mesh* mesh, Material** material, int materialCount) {
	if (materialCount < mesh->subMeshCount) {
		printf("Insufficient materials for mesh!");
		return;
	}
	for (int i = 0; i < mesh->subMeshCount; ++i) {
		//subMeshes[i].Render(material);
		RenderSubMesh(mesh, material[i], mesh->subMeshes[i], &material[i]->mainShader);
	}
}

void FreeMeshBuffers(Mesh* mesh) {
	if (mesh->uUsed) {
		DeleteBuffers(1, &mesh->vertexUVs);
	}
	if (mesh->nUsed) {
		DeleteBuffers(1, &mesh->vertexNorms);
	}
	if (mesh->vUsed) {
		DeleteBuffers(1, &mesh->vertexBuffer);
	}
	if (mesh->bUsed) {
		DeleteBuffers(1, &mesh->vertexBoneIndex);
	}
	if (mesh->wUsed) {
		DeleteBuffers(1, &mesh->vertexWeights);
	}
	mesh->uUsed = false;
	mesh->nUsed = false;
	mesh->vUsed = false;
	mesh->bUsed = false;
	mesh->wUsed = false;
}

void DestroyMesh(Mesh* mesh) {
	FreeMeshBuffers(mesh);
	for (int i = 0; i < mesh->subMeshCount; ++i) {
		DestroySubMesh(&mesh->subMeshes[i]);
	}
	DeleteVertexArrays(1, &mesh->VAO);
}

void UpdateMesh(Mesh* mesh) {
	DestroyMesh(mesh);

	GenVertexArrays(1, &mesh->VAO);
	BindVertexArray(mesh->VAO);

	for (int i = 0; i < mesh->subMeshCount; ++i) {
		DestroySubMesh(&mesh->subMeshes[i]);
		UpdateSubMesh(&mesh->subMeshes[i]);
	}

	if (mesh->vertexCount > 0) {
		UploadBuffer(&mesh->vertexBuffer, sizeof(Vec3) * mesh->vertexCount, &mesh->vert[0]);
		/*glGenBuffers(1, &mesh->vertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexBuffer);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3) * mesh->vertexCount, &mesh->vert[0], GL_STATIC_DRAW);*/
		mesh->vUsed = true;
	}

	if (mesh->uvCount > 0) {
		UploadBuffer(&mesh->vertexUVs, sizeof(Vec2) * mesh->uvCount, &mesh->UV[0]);
		/*glGenBuffers(1, &mesh->vertexUVs);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexUVs);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vec2) * mesh->uvCount, &mesh->UV[0], GL_STATIC_DRAW);*/
		mesh->uUsed = true;
	}
	if (mesh->normalCount > 0) {
		UploadBuffer(&mesh->vertexNorms, sizeof(Vec3) * mesh->normalCount, &mesh->normals[0]);
		/*glGenBuffers(1, &mesh->vertexNorms);
		glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexNorms);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vec3) * mesh->normalCount, &mesh->normals[0], GL_STATIC_DRAW);*/
		mesh->nUsed = true;
	}
	if (mesh->boneIndexCount > 0) {
		UploadBuffer(&mesh->vertexBoneIndex, sizeof(float) * mesh->boneIndexCount * 4, &mesh->boneIndex[0]);
		mesh->bUsed = true;
	}
	if (mesh->weightCount > 0) {
		UploadBuffer(&mesh->vertexWeights, sizeof(float) * mesh->weightCount * 4, &mesh->weights[0]);
		mesh->wUsed = true;
	}
}

void DeleteMesh(Mesh* mesh) {
	DestroyMesh(mesh);
	if (mesh->vert != NULL) {
		free(mesh->vert);
		mesh->vert = NULL;
	}
	if (mesh->UV != NULL) {
		free(mesh->UV);
		mesh->UV = NULL;
	}
	if (mesh->color != NULL) {
		free(mesh->color);
		mesh->color = NULL;
	}
	if (mesh->normals != NULL) {
		free(mesh->normals);
		mesh->normals = NULL;
	}
	if (mesh->weights != NULL) {
		free(mesh->weights);
		mesh->weights = NULL;
	}
	if (mesh->boneIndex != NULL) {
		free(mesh->boneIndex);
		mesh->boneIndex = NULL;
	}
	for (int i = 0; i < mesh->subMeshCount; ++i) {
		DeleteSubMesh(&mesh->subMeshes[i]);
	}
	free(mesh->subMeshes);
}

void SetMeshVertices(Mesh* mesh, Vec3* verts, int vertCount) {
	if (verts == NULL) {
		mesh->vertexCount = 0;
		return;
	}
	// free old data
	if (mesh->vert != NULL) {
		free(mesh->vert);
	}
	mesh->vertexCount = vertCount;
	mesh->vert = (Vec3*)malloc(vertCount * sizeof(Vec3));
	memcpy(mesh->vert, verts, vertCount * sizeof(Vec3));
}

void SetMeshUVs(Mesh* mesh, Vec2* uvs, int uvCount) {
	if (uvs == NULL) {
		mesh->uUsed = false;
		return;
	}
	// free old data
	if (mesh->UV != NULL) {
		free(mesh->UV);
	}
	mesh->uvCount = uvCount;
	mesh->UV = (Vec2*)malloc(uvCount * sizeof(Vec2));
	memcpy(mesh->UV, uvs, uvCount * sizeof(Vec2));
}

void SetSubmeshCount(Mesh* mesh, int subMeshCount) {
	if (mesh->subMeshes != NULL) {
		for (int i = 0; i < mesh->subMeshCount; ++i) {
			DeleteSubMesh(&mesh->subMeshes[i]);
		}
		free(mesh->subMeshes);
	}
	mesh->subMeshCount = subMeshCount;
	mesh->subMeshes = (SubMesh*)calloc(1, sizeof(SubMesh) * subMeshCount);
}

void SetSubmeshTriangles(Mesh* mesh, int subMeshId, int* tris, int triCount) {
	if (subMeshId > mesh->subMeshCount) {
		printf("Tried setting vertices for submesh greater than count of submeshes in model!");
		return;
	}
	if (tris == NULL) {
		mesh->subMeshes[subMeshId].triangleCount = 0;
		return;
	}
	if (mesh->subMeshes[subMeshId].triangles != NULL) {
		free(mesh->subMeshes[subMeshId].triangles);
	}
	mesh->subMeshes[subMeshId].triangleCount = triCount;
	mesh->subMeshes[subMeshId].triangles = (int*)malloc(sizeof(int) * triCount);
	memcpy(mesh->subMeshes[subMeshId].triangles, tris, triCount * sizeof(int));
}