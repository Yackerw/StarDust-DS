#ifndef __OGLMESH
#define __OGLMESH
#include "PsuedoVector.h"
#include <stdbool.h>
#include "../../sdmath.h"
// >:(
typedef struct {
	void* shader;
	char* pass;
	//Texture **currTexs;
	PsuedoVector currTexs;
	//void **currUniforms;
	PsuedoVector currUniforms;
} MaterialShaderReference;
typedef struct {
	//Shader* shader;
	MaterialShaderReference mainShader;
	MaterialShaderReference directionalShadowShader;
	MaterialShaderReference pointShadowShader;
	//char **textures;
	PsuedoVector textures;
	//Texture **texRefs;
	PsuedoVector texRefs;
	//UniformValue *uniformValues;
	PsuedoVector uniformValues;
	// MaterialShaderReference *passShaders;
	PsuedoVector passShaders;
	bool transparent;
	bool backFaceCulling;
	bool frontFaceCulling;
} Material;
#include "Texture.h"
#include "Shader.h"

typedef struct {
	char* name;
	void* value;
} UniformValue;

typedef struct {
	bool valid;
	int triangleCount;
	unsigned int triangleBuffer;
	bool tUsed;
	int* triangles;
} SubMesh;

typedef struct {
	int vertexCount;
	int uvCount;
	int normalCount;
	int colorCount;
	int weightCount;
	int boneIndexCount;
	unsigned int vertexBuffer;
	bool vUsed;
	unsigned int vertexUVs;
	bool uUsed;
	unsigned int vertexNorms;
	bool nUsed;
	unsigned int vertexColors;
	bool cUsed;
	unsigned int vertexWeights;
	bool wUsed;
	unsigned int vertexBoneIndex;
	bool bUsed;

	int subMeshCount;
	SubMesh* subMeshes;
	Vec3* color;
	Vec2* UV;
	Vec3* normals;
	float* weights;
	int* boneIndex;
	Vec3* vert;
	unsigned int VAO;
} Mesh;

void SetMaterialNativeTexture(Material* mat, char* name, NativeTexture* tex);

void SetMaterialShader(Material* mat, Shader* shad);

void SetMaterialDirectionalShader(Material* mat, Shader* shad);

void SetupMaterialPointShader(Material* mat, Shader* shad);

void SetMaterialPassShader(Material* mat, Shader* shad, char* pass);

void SetMaterialUniform(Material* mat, char* uniformName, void* uniformValue);

void DeleteMaterial(Material* mat);

void InitMaterial(Material* mat);

void RenderSubMesh(Mesh* mesh, Material* material, SubMesh subMesh, MaterialShaderReference* pass);

void RenderMesh(Mesh* mesh, Material** materials, int materialCount);

void DestroyMesh(Mesh* mesh);

void UpdateMesh(Mesh* mesh);

void DeleteMesh(Mesh* mesh);

void SetMeshVertices(Mesh* mesh, Vec3* verts, int vertCount);

void SetMeshUVs(Mesh* mesh, Vec2* uvs, int uvCount);

void SetSubmeshCount(Mesh* mesh, int subMeshCount);

void SetSubmeshTriangles(Mesh* mesh, int subMeshId, int* tris, int triCount);
#endif