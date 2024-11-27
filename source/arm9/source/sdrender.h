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

enum SpriteRenderPositionX {SpriteAlignLeft, SpriteAlignCenter, SpriteAlignRight};
enum SpriteRenderPositionY {SpriteAlignTop, SpriteAlignBottom = 2};

enum VertexHeaderBitflags {VTX_MATERIAL_CHANGE = 1, VTX_STRIPS = 2, VTX_QUAD = 4};

enum StencilBitFlags {STENCIL_VALUE = 0x1F, STENCIL_SHADOW_COMPARE_WRITE = 0x20, STENCIL_FORCE_OPAQUE_ORDERING = 0x40};

enum GeneralMaterialFlags {NO_CULLING = 0x0, BACK_CULLING = 0x1, FRONT_CULLING = 0x2, CULLING_MASK = 0x3, TEXTURE_TRANSFORM = 0x4};

enum LightingFlags {LIGHT_ENABLE = 0x1, LIGHT_TOON_HIGHLIGHT = 0x2, LIGHT_DECAL_TEXTURE = 0x6, LIGHT_OVERRIDE0 = 0x8, LIGHT_OVERRIDE1 = 0x10, LIGHT_OVERRIDE2 = 0x20, LIGHT_OVERRIDE3 = 0x40};

#define RENDER_PRIO_RESERVED 0x80000000

typedef int i29d3;

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
	i29d3 frame;
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
	bool dontReleaseFromRAM;
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
	union {
		void* nativeTexture;
		unsigned int textureWrite;
	};
	unsigned int paletteWrite;
};

// this is very messy now, but it was done to maintain backwards compatibility with files. these will be fixed when the 2.0 file update is finalized...


struct SDMaterial {
	unsigned char colorR;
	unsigned char lightNormal2Pt0;
	unsigned short lightOverride0;
	unsigned char colorG;
	unsigned char lightNormal2Pt1;
	unsigned short lightOverride1;
	unsigned char colorB;
	unsigned char lightNormal3Pt0;
	unsigned short lightOverride2;
	unsigned char alpha;
	unsigned char lightNormal3Pt1;
	unsigned short lightOverride3;
	Texture *texture;
	unsigned char materialFlags0;
	unsigned char lightingFlags;
	unsigned char padding4;
	unsigned char stencilPack;
	f32 texOffsX;
	f32 texOffsY;
	f32 texScaleX;
	f32 texScaleY;
	unsigned char emissionR;
	unsigned char emissPadding0;
	unsigned short texRotation;
	unsigned char emissionG;
	unsigned char emissPadding2;
	unsigned short lightNormal0;
	unsigned char emissionB;
	unsigned char emissPadding4;
	unsigned short lightNormal1;
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
	union {
		void* nativeSprite;
		int DSResolution;
	};
} Sprite;

extern Texture startTexture;

extern bool touch3D;

extern bool multipassRendering;

void SetupModelFromMemory(Model* model, char* textureDir, bool asyncTextures, void (*asyncCallback)(void* data), void* asyncCallbackData);

Model *LoadModel(char *input);

int LoadModelAsync(char* input, void (*callBack)(void* data, Model* model), void* callBackData);

void CacheModel(Model* reference);

// does not work with code generated models
Model* FreeModelKeepCache(Model* model);

void UpdateModel(Model* model);

void RenderModel(Model *model, Vec3 *position, Vec3 *scale, Quaternion *rotation, SDMaterial *mats, int renderPriority);

void UploadTexture(Texture* input);

Texture *LoadTextureFromRAM(Texture* input, bool upload, char* name);

Texture *LoadTexture(char *input, bool upload);

void LoadTextureAsync(char* input, bool upload, void (*callBack)(void* data, Texture* texture), void* callBackData);

void UnloadTexture(Texture* tex);

void SetLightDir(int lightId, f32 x, f32 y, f32 z);

void SetLightColor(int lightId, char R, char G, char B);

void EnableLight(int lightId);

void DisableLight(int lightId);

void SetAmbientColor(char R, char G, char B);

void RenderModelRigged(Model *model, Vec3 *position, Vec3 *scale, Quaternion *rotation, SDMaterial *mats, Animator *animator, int renderPriority);

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

void SetupCameraMatrixPartial(int x, int y, int width, int height);

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

void Set3DOnTop();
void Set3DOnBottom();

void Initialize3D(bool multipass, bool subBG);

void SetMaterialLightOverride(SDMaterial *material, int id, char R, char G, char B, f32 normalX, f32 normalY, f32 normalZ);

//void SaveLCD();
//void RestoreLCD();

#endif