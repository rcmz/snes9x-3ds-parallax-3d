
#ifndef _3DSIMPL_GPU_H_
#define _3DSIMPL_GPU_H_

#include "3dsgpu.h"
#include "3dssettings.h"

#define COMPOSE_HASH(vramAddr, pal)   ((vramAddr) << 4) + ((pal) & 0xf)

#define MAX_VERTICES                65534

// backdrop * 2, window_lr, brightness, color math
#define MAX_VERTICES_RECT           (241 * 2 + 241 + 241 + 241)
#define MAX_VERTICES_MODE7_LINE     8192
#define MAX_VERTICES_MODE7_TILE     16388

#define MAX_VERTICES_QUAD           256

#define LAYERS_COUNT 9

#define MAX_TEXTURE_POSITIONS		16383
#define MAX_HASH					(65536 * 16 / 8)

#define HDMA_PALETTE_VARIANT_CACHE_SIZE 4096

// Boundary between normal tile cache and HDMA variant pool.
// Variant pool size must be >= hash size to avoid collision.
// Boundary must be even (normal cache pairs slot p with alt-frame p^1).
#define MAX_TEXTURE_HASH_POSITIONS ((MAX_TEXTURE_POSITIONS - HDMA_PALETTE_VARIANT_CACHE_SIZE) & ~1)

typedef struct {
    s16 x, y;
} SVector2i;

typedef struct {
    s16 x, y, z;
} SVector3i;

typedef struct {
    s16 x, y, z, w;
} SVector4i;

typedef struct {
	s16 u, v;
} STexCoord2i;

typedef struct {
    float u, v;
} STexCoord2f;

typedef struct {
    float x, y, z, w;
} SVector4f;

typedef struct {
    SVector4f    Position;
	STexCoord2f  TexCoord;
	u32          Color;
	u32          Flags;
} SQuadVertex;

typedef struct {
    SVector3i    Position;
	STexCoord2i  TexCoord;
} STileVertex;

typedef struct {
    SVector2i   Position;
	u32         Color;
} SRectVertex;

typedef struct {
    SVector2i    Position;
	STexCoord2f  TexCoord;
} SMode7LineVertex;

typedef struct {
    SVector4i	Position;
} SMode7TileVertex;

typedef enum 
{
    LAYER_BG0,
    LAYER_BG1,
    LAYER_BG2,
    LAYER_BG3,
    LAYER_OBJ,
    LAYER_BACKDROP,
    LAYER_COLOR_MATH, // main target
    LAYER_BRIGHTNESS, // main target
    LAYER_WINDOW_LR, // depth target
} LAYER_ID; // keep this order!

typedef enum
{
    VS_BACKDROP_SUB,    
	VS_BACKDROP_MAIN,
	VS_CLIP_TO_BLACK,
	VS_COLOR_MATH,
} VERTICAL_SECTION_ID;

typedef union {
    u64 packed;
    struct {
        u32 color;
        u32 v2; // depth for backdrop, stencil for color math
    };
} DrawableSectionValue;

typedef union {
    u16 packed;
    struct {
        u8 alphaBlending;
        u8 textureEnv;
    };
} DrawableSectionRenderState;

typedef struct 
{
	DrawableSectionValue value;
	DrawableSectionRenderState state;

    u16 	startY;
    u16 	endY;
} DrawableVerticalSection;


typedef struct 
{
    SGPURenderState     state;

    u16                 from;
    u16                 count;

    SGPU_VBO_ID         vboId;
    bool                onSub;
} SLayerSection;

typedef struct 
{
    u32             bufferOffset;

    u16             sectionsByTarget[2];
    u16             verticesByTarget[2];
    u16             sectionsTotal;
    u16             sectionsMax;
    u16             sectionsOffset;
    u16             sectionsSkipped;

    LAYER_ID        id;
    bool            m7Tile0;
} SLayer;

typedef struct
{
    SLayer          layers[LAYERS_COUNT];
    LAYER_ID        layersByTarget[2][LAYERS_COUNT];

    SLayerSection   *sections;
    void            *ibo;

    u32             sizeInBytes;

    u16             verticesTotal;
    u16             sectionsSizeInBytes;
    u16             sectionsMax;

    u8              layersTotalByTarget[2];

    bool            anythingOnSub;
    bool            hasSkippedSections;

    // true when obj and bg0-bg3 can skip indexed batching in the tiled pass
    bool            useDrawArraysForTiledLayers;
} SLayerList;

typedef struct
{
    bool            enabled;
    bool            dirty;
} SRender2xState;

//---------------------------------------------------------
// Per-layer stereoscopic parallax.
//
// The SNES composites a fixed set of hardware planes, so each
// plane can be pushed to its own depth by giving it a horizontal
// shift that is mirrored between the two eyes. The frame's vertex
// data is built once and replayed per eye with a different shift
// per layer, which keeps the SNES compositing rules (priority,
// windows, colour math) exactly as they are in 2D.
//---------------------------------------------------------
// Number of depth slots the renderer can assign. Mode 1 uses 1..13, and slot 0
// is where the backdrop, brightness and colour-math fills land -- those cover
// the whole screen, so shifting them would only open a gap and they stay put.
#define SNES_DEPTH_SLOTS    17

typedef struct
{
    // Shift in SNES pixels for the right eye, per configurable slot; the left
    // eye uses the negated value. Index = DEPTH3D_SLOT.
    s8              slotShift[DEPTH3D_SLOT_COUNT];

    // Which configurable slot each hardware depth slot draws its shift from,
    // or -1 for the slots nothing configurable lands on. Sprites are fixed at
    // slots 3, 6, 9 and 12; a background's two priorities land on slots that
    // depend on the background mode, so those are recorded as the modes assign
    // them. Keeping the mapping rather than the resolved shifts lets the paused
    // preview follow the menu without re-rendering the frame.
    s8              depthSlotSource[SNES_DEPTH_SLOTS];

    // Sign of the shift for the eye currently being drawn:
    // -1 = left, +1 = right, 0 = flat (no per-layer shift)
    s8              eye;

    // Largest and smallest shift in force this frame. A slot displaced towards
    // one screen edge shows, at the opposite edge, whatever the tilemap holds
    // outside the visible area -- which the game is free to have reused for
    // somewhere else. These bound the strip each eye has to leave undrawn.
    s8              shiftMax;
    s8              shiftMin;

    // true when at least one layer has a non-zero shift this frame
    bool            active;

    // false when the right eye's SNES screen could not be allocated
    bool            supported;

    // the draw lists and index buffer are built once per frame and replayed
    // for the second eye
    bool            listsBuilt;
} SStereoLayerState;

typedef struct
{
    u16             vramCacheHashToTexturePosition[MAX_HASH + 1]; // 262146 bytes
    int             vramCacheTexturePositionToHash[MAX_TEXTURE_HASH_POSITIONS]; // 4*MAX_TEXTURE_HASH_POSITIONS bytes

    SLayerList      layerList;

    u32             newCacheTexturePosition;

    u16             mode7FrameCount;

    GPU_TEXCOLOR    mode7TextureFormat;
    bool            mode7SectionsModified[4];
    bool            mode7TilesModified;

    SRender2xState  render2x;       // 512px internal SNES render path:
                                    // true hires for Mode 5, finer Mode 7 sampling, doubled geometry elsewhere
    int             renderWidth;    // 512 when render2x.enabled, else 256

    SStereoLayerState stereo;       // per-layer stereoscopic parallax
} SGPU3DSExtended;

extern SGPU3DSExtended GPU3DSExt;

void gpu3dsDeallocLayers();
void gpu3dsResetLayerSectionLimits(SLayerList *list);
void gpu3dsInitLayers();
void gpu3dsPrepareSnesScreenForNextFrame();
void gpu3dsDrawSnesScreen();
void gpu3dsUpdateStereoLayerShifts();
void gpu3dsUpdateStereoLayerShiftsForPreview();

//---------------------------------------------------------
// Width, in SNES pixels, that an eye must leave undrawn at each
// screen edge. A shifted slot pulls content in from outside the
// visible area, and the game only keeps the tilemap valid where
// it means to draw, so that strip is not shown at all rather
// than shown wrong. This is the stereo window, and putting it in
// front of everything is what makes the edges read cleanly.
//---------------------------------------------------------
static inline void gpu3dsGetStereoEdgeMask(gfx3dSide_t side, int *left, int *right)
{
    const SStereoLayerState *stereo = &GPU3DSExt.stereo;
    int eye = side == GFX_RIGHT ? 1 : -1;

    int pulledFromLeft = eye > 0 ? stereo->shiftMax : -stereo->shiftMin;
    int pulledFromRight = eye > 0 ? -stereo->shiftMin : stereo->shiftMax;

    *left = pulledFromLeft > 0 ? pulledFromLeft : 0;
    *right = pulledFromRight > 0 ? pulledFromRight : 0;
}
void gpu3dsDrawSnesScreenForEye(gfx3dSide_t side);

//---------------------------------------------------------
// How far a background's two tile priorities pull apart, in
// pixels, when the low priority is the one sitting further back.
//
// Only the priority that is further back can sensibly continue
// into the gap between them, and extending the low priority is
// safe because it also draws behind the high one, so the fill
// stays hidden wherever the high priority is opaque. The
// opposite arrangement -- the high priority further back -- would
// have to paint over the low priority to fill anything, which
// would cost more than the gap does, so it is left alone.
//---------------------------------------------------------
static inline int gpu3dsGetPriorityFillWidth(int bg)
{
    const SStereoLayerState *stereo = &GPU3DSExt.stereo;

    if (!stereo->active)
        return 0;

    int lowShift = stereo->slotShift[DEPTH3D_BG_SLOT(bg, 0)];
    int highShift = stereo->slotShift[DEPTH3D_BG_SLOT(bg, 1)];

    return lowShift > highShift ? lowShift - highShift : 0;
}

//---------------------------------------------------------
// Records which depth slots a background's two tile priorities
// land on. Which slot that is depends on the background mode,
// so it is recorded where the modes assign it rather than
// duplicated as a table.
//---------------------------------------------------------
static inline void gpu3dsMapPlaneDepthSlots(int bg, int slot0, int slot1)
{
    SStereoLayerState *stereo = &GPU3DSExt.stereo;

    if (!stereo->active)
        return;

    if (slot0 >= 0 && slot0 < SNES_DEPTH_SLOTS)
        stereo->depthSlotSource[slot0] = (s8)DEPTH3D_BG_SLOT(bg, 0);

    if (slot1 >= 0 && slot1 < SNES_DEPTH_SLOTS)
        stereo->depthSlotSource[slot1] = (s8)DEPTH3D_BG_SLOT(bg, 1);
}
void gpu3dsCommitLayerSection(SGPU_VBO_ID vboId, LAYER_ID id, SGPURenderState *state, bool sub = false, bool reuseVertices = false);

void gpu3dsSetMode7TexturesPixelFormat(GPU_TEXCOLOR fmt);

void gpu3dsInitializeMode7Vertexes();

void gpu3dsAddQuadRect(float x0, float y0, float x1, float y1, u16 wx, u16 wy, int z, u32 fillColor, u32 borderColor = 0, u8 borderSize = 0);

inline u16 __attribute__((always_inline)) gpu3dsGetValueWithinLimit(u16 value, u32 from, u32 max) {
    return (from + value > max) ? (max - from) : value;
}

inline void __attribute__((always_inline)) gpu3dsAddQuadVertexes(
    float x0, float y0, float x1, float y1,
    STexCoord2f tl, STexCoord2f tr, STexCoord2f bl, STexCoord2f br,
    float z, int color = 0)
{
    SVertexList *list = &GPU3DS.vertices[VBO_SCREEN];
    SQuadVertex *vertices = &((SQuadVertex *) list->data)[list->from + list->count];

	vertices[0].Position = {x0, y0, z, 1};
	vertices[1].Position = {x1, y0, z, 1};
	vertices[2].Position = {x0, y1, z, 1};

	vertices[3].Position = {x1, y1, z, 1};
	vertices[4].Position = {x0, y1, z, 1};
	vertices[5].Position = {x1, y0, z, 1};

	vertices[0].TexCoord = tl;
	vertices[1].TexCoord = tr;
	vertices[2].TexCoord = bl;

	vertices[3].TexCoord = br;
	vertices[4].TexCoord = bl;
	vertices[5].TexCoord = tr;

	u32 colorSwapped = __builtin_bswap32(color);

    vertices[0].Color = colorSwapped;
    vertices[1].Color = colorSwapped;
    vertices[2].Color = colorSwapped;

    vertices[3].Color = colorSwapped;
    vertices[4].Color = colorSwapped;
    vertices[5].Color = colorSwapped;

    list->count += 6;
}

inline void __attribute__((always_inline)) gpu3dAddSubTextureQuadVertexes(
    float x0, float y0, float x1, float y1,
    const Tex3DS_SubTexture* subTex, int origWidth, int origHeight, int textureWidth, int textureHeight,
    float z, u32 color)
{
    if (!Tex3DS_SubTextureRotated(subTex)) {
        float top_u, top_v, bot_u, bot_v;
        Tex3DS_SubTextureTopLeft(subTex, &top_u, &top_v);
        Tex3DS_SubTextureBottomLeft(subTex, &bot_u, &bot_v);

        float tx0 = top_u * textureWidth;
        float ty0 = top_v * textureHeight;

        // If Top > Bottom, we are traversing memory backwards (common in 3DS)
        float dirY = (bot_v * textureHeight > ty0) ? 1.0f : -1.0f;
        float tx1 = tx0 + origWidth;
        float ty1 = ty0 + (origHeight * dirY);

        STexCoord2f tl = { tx0, ty0 };
        STexCoord2f tr = { tx1, ty0 };
        STexCoord2f bl = { tx0, ty1 };
        STexCoord2f br = { tx1, ty1 };

        gpu3dsAddQuadVertexes(x0, y0, x1, y1, tl, tr, bl, br, z, color);
    }
    else {
        float tl_u, tl_v, tr_u, tr_v, bl_u, bl_v, br_u, br_v;
        Tex3DS_SubTextureTopLeft(subTex, &tl_u, &tl_v);
        Tex3DS_SubTextureTopRight(subTex, &tr_u, &tr_v);
        Tex3DS_SubTextureBottomLeft(subTex, &bl_u, &bl_v);
        Tex3DS_SubTextureBottomRight(subTex, &br_u, &br_v);

        STexCoord2f tl = { tl_u * textureWidth, tl_v * textureHeight };
        STexCoord2f tr = { tr_u * textureWidth, tr_v * textureHeight };
        STexCoord2f bl = { bl_u * textureWidth, bl_v * textureHeight };
        STexCoord2f br = { br_u * textureWidth, br_v * textureHeight };

        gpu3dsAddQuadVertexes(x0, y0, x1, y1, tl, tr, bl, br, z, color);
    }
}

inline void __attribute__((always_inline)) gpu3dsAddSimpleQuadVertexes(
    float x0, float y0, float x1, float y1,
    float tx0, float ty0, float tx1, float ty1,
    float z, int color = 0)
{
    STexCoord2f tl = {tx0, ty0};
    STexCoord2f tr = {tx1, ty0};
    STexCoord2f bl = {tx0, ty1};
    STexCoord2f br = {tx1, ty1};

    gpu3dsAddQuadVertexes(x0, y0, x1, y1, tl, tr, bl, br, z, color);
}

inline void __attribute__((always_inline)) gpu3dsAddRectangleVertexes(s16 x0, s16 y0, s16 x1, s16 y1, u32 color)
{
    SVertexList *list = &GPU3DS.vertices[VBO_SCENE_RECT];
    SRectVertex *vertices = &((SRectVertex *) list->data)[list->from + list->count];

    // using -1 for non-tile detection in shader
    vertices[0].Position = (SVector2i){x0, y0};
    vertices[1].Position = (SVector2i){x1, y1};

    u32 swappedColor = __builtin_bswap32(color);
    vertices[0].Color = swappedColor;
    vertices[1].Color = swappedColor;

    list->count += 2;
}


inline void __attribute__((always_inline)) gpu3dsAddTileVertexes(
    s16 x0, s16 y0, s16 x1, s16 y1,
    s16 tx0, s16 ty0, s16 tx1, s16 ty1,
    s16 z)
{
    SVertexList *list = &GPU3DS.vertices[VBO_SCENE_TILE];
    STileVertex *vertices = &((STileVertex *) list->data)[list->from + list->count];

    vertices[0].Position = (SVector3i){x0, y0, z};
    vertices[1].Position = (SVector3i){x1, y1, z};

    vertices[0].TexCoord = (STexCoord2i){tx0, ty0};
    vertices[1].TexCoord = (STexCoord2i){tx1, ty1};

    list->count += 2;
}

inline void __attribute__((always_inline)) gpu3dsAddMode7LineVertexes(
    s16 x0, s16 y0, s16 x1, s16 y1,
    float tx0, float ty0, float tx1, float ty1)
{
    SVertexList *list = &GPU3DS.vertices[VBO_SCENE_MODE7_LINE];
    SMode7LineVertex *vertices = (SMode7LineVertex *) list->data + list->from + list->count;

    vertices[0].Position = (SVector2i){x0, y0};
    vertices[1].Position = (SVector2i){x1, y1};

    vertices[0].TexCoord = {tx0, ty0};
    vertices[1].TexCoord = {tx1, ty1};

    list->count += 2;
}


inline void __attribute__((always_inline)) gpu3dsSetMode7TileModified(int idx, u8 data)
{
    SMode7TileVertex *m7vertices = &((SMode7TileVertex *)GPU3DS.vertices[VBO_MODE7_TILE].data) [idx];

    m7vertices[0].Position.w = GPU3DSExt.mode7FrameCount;
    m7vertices[0].Position.z = data;

    GPU3DSExt.mode7TilesModified = true;
    GPU3DSExt.mode7SectionsModified[idx >> 12] = true;
}

#endif
