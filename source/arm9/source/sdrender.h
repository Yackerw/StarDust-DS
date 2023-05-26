#ifndef SDRENDER
#define SDRENDER
#include <nds.h>
#include "sdmath.h"
#ifdef _WIN32
#include "PC/Renderer/Shader.h"
#include "PC/Renderer/Mesh.h"
#include "PC/Renderer/Texture.h"
#include "PC/Renderer/Window.h"
#include "PC/Renderer/RenderTexture.h"

extern Shader *defaultShader;
extern Shader *defaultRiggedShader;
extern Shader* defaultSpriteShader;
#endif

extern Vec3 cameraPosition;
extern Quaternion cameraRotation;
extern f32 cameraFOV;
extern f32 cameraNear;
extern f32 cameraFar;
extern m4x4 cameraMatrix;

extern Vec3 lightNormal;
extern int lightColor;
extern int ambientColor;

enum SpriteRenderPositionX {SpriteAlignLeft, SpriteAlignCenter, SpriteAlignRight};
enum SpriteRenderPositionY {SpriteAlignTop, SpriteAlignBottom = 2};

enum VertexHeaderBitflags {VTX_MATERIAL_CHANGE = 1, VTX_STRIPS = 2, VTX_QUAD = 4};

typedef struct {
	v16 x;
	v16 y;
	v16 z;
	char boneID;
	char padding;
	int normal;
	t16 u;
	t16 v;
} Vertex;

typedef struct {
	short count;
	unsigned char bitFlags;
	unsigned char material;
	Vertex vertices;
} VertexHeader;

typedef struct {
	m4x4 inverseMatrix;
	Vec3 position;
	Vec3 scale;
	Quaternion rotation;
	int parent;
} Bone;

typedef struct {
	int FIFOCount;
	unsigned int** FIFOBatches;
} DSNativeModel;

typedef struct SDMaterial SDMaterial;

typedef struct {
	unsigned int version;
	int vertexGroupCount;
	VertexHeader *vertexGroups;
	int materialCount;
	SDMaterial *defaultMats;
	char *materialTextureNames;
	int skeletonCount;
	Bone *skeleton;
	Vec3 defaultOffset;
	f32 defaultScale;
	Vec3 boundsMin;
	Vec3 boundsMax;
	void* NativeModel;
} Model;

union KeyframeData {
	Vec3 position;
	Vec3 scale;
	Quaternion rotation;
};

typedef struct {
	f32 frame;
	union KeyframeData data;
} Keyframe;

typedef struct {
	int target;
	int type;
	int keyframeCount;
	Keyframe keyframes[256];
} KeyframeSet;

typedef struct {
	int version;
	int keyframeSetCount;
	f32 lastFrame;
	KeyframeSet *sets[256];
} Animation;

typedef struct {
	Vec3 currPosition;
	Vec3 prevPosition;
	Vec3 currScale;
	Vec3 prevScale;
	Quaternion currRotation;
	Quaternion prevRotation;
	m4x4 matrix;
} AnimatorItem;

typedef struct {
	f32 currFrame;
	f32 lerpPrevTime;
	f32 lerpPrevTimeTarget;
	f32 speed;
	Animation *currAnimation;
	Animation* queuedAnims[8];
	f32 queuedLerpTimes[8];
	int queuedAnimCount;
	int itemCount;
	AnimatorItem *items;
	bool loop;
	bool paused;
} Animator;

typedef struct Texture Texture;

struct Texture {
	char type;
	char width;
	char height;
	bool releaseFromRAM;
	unsigned int numReferences;
	unsigned char mapTypeU;
	unsigned char mapTypeV;
	bool uploaded;
	unsigned char padding2;
	unsigned short *palette;
	unsigned char *image;
	char *name;
	Texture *prev;
	Texture *next;
	int textureId;
	int paletteId;
	void* nativeTexture;
	int padding4;
};

struct SDMaterial {
	Vec3i color;
	int alpha;
	Texture *texture;
	bool backFaceCulling;
	bool lit;
	char specular;
	char padding;
	f32 texOffsX;
	f32 texOffsY;
	f32 texScaleX;
	f32 texScaleY;
	Vec3i emission;
};

typedef struct {
	char resolution;
	bool uploaded;
	bool sub;
	char padding;
	int width;
	int height;
	char format;
	char padding2;
	short paletteOffset;
	unsigned char* image;
	unsigned short* palette;
	char* gfx;
	void* nativeSprite;
} Sprite;

extern Texture startTexture;

void SetupModelFromMemory(Model* model, char* textureDir, bool asyncTextures, void (*asyncCallback)(void* data), void* asyncCallbackData);

Model *LoadModel(char *input);

int LoadModelAsync(char* input, void (*callBack)(void* data, Model* model), void* callBackData);

void CacheModel(Model* reference);

// does not work with code generated models
Model* FreeModelKeepCache(Model* model);

void UpdateModel(Model* model);

void RenderModel(Model *model, m4x4 *matrix, SDMaterial *mats);

void UploadTexture(Texture* input);

void LoadTextureFromRAM(Texture* input, bool upload, char* name);

Texture *LoadTexture(char *input, bool upload);

void LoadTextureAsync(char* input, bool upload, void (*callBack)(void* data, Texture* texture), void* callBackData);

void UnloadTexture(Texture* tex);

void SetLightDir(int x, int y, int z);

void SetLightColor(int color);

void SetAmbientColor(int color);

void RenderModelRigged(Model *model, m4x4 *matrix, SDMaterial *mats, Animator *animator);

void LoadAnimationFromRAM(Animation* anim);

Animation *LoadAnimation(char *input);

int LoadAnimationAsync(char* input, void (*callBack)(void* data, Animation* anim), void* callBackData);

Animator *CreateAnimator(Model *referenceModel);

void UpdateAnimator(Animator *animator, Model *referenceModel);

void PlayAnimation(Animator *animator, Animation *animation, f32 lerpTime);

void SetSDMaterialTexture(SDMaterial *mat, Texture *texture);

void DestroySDMaterial(SDMaterial *mat);

void DestroyModel(Model *m);

void DestroyGeneratedModel(Model* m);

void SetupCameraMatrix();

bool AABBInCamera(Vec3* min, Vec3* max, m4x4* transform);

bool QueueAnimation(Animator* animator, Animation* animation, f32 lerpTime);

void DestroyAnimator(Animator* animator);

void LoadBGTexture(Sprite* input);

void InitializeSubBG();

void UploadSprite(Sprite* sprite, bool sub, bool BG);

Sprite* LoadSprite(char* input, bool sub, bool upload);

void UnloadSprite(Sprite* sprite);

void RenderSprite(Sprite* sprite, int x, int y, bool flipX, bool flipY, int xAlign, int yAlign);

void RenderSpriteScaled(Sprite* sprite, int x, int y, bool flipX, bool flipY, f32 xScale, f32 yScale, int xAlign, int yAlign);

void FinalizeSprites();

void SetBackgroundTile(int x, int y, int tileId);

void RenderTransparentModels();

#endif