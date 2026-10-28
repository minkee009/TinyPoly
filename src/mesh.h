#ifndef MESH_H
#define MESH_H

#include <raylib.h>
#include <stdbool.h>

#define MAX_FACE_VERTS 16
#define MAX_OBJECTS    64
#define MAX_PALETTE    256        /* 팔레트는 자주 쓰는 색 모음 (색 자체는 자유) */
#define MIN_TEX_SIZE   16
#define MAX_TEX_SIZE   1024

typedef struct {
    Vector3 p;
    bool sel;
} Vert;

enum {
    FACE_DOUBLE  = 1 << 0,   /* 양면 */
    FACE_NOSHADE = 1 << 1,   /* 음영 없음 */
    FACE_NOTEX   = 1 << 2,   /* 텍스처 대신 단색 */
    FACE_BEHIND  = 1 << 3,   /* 다른 면 뒤에 먼저 그림 */
};

/* 정점 인덱스는 바깥에서 봤을 때 반시계 방향. uv는 텍스처 픽셀 단위 */
typedef struct {
    int n;
    int v[MAX_FACE_VERTS];
    Vector2 uv[MAX_FACE_VERTS];
    Color color;             /* 텍스처를 안 쓸 때의 색 */
    int flags;
    bool sel;
} Face;

typedef struct {
    char name[32];
    Vert *verts; int nverts, cverts;
    Face *faces; int nfaces, cfaces;
    Vector3 origin;          /* 회전/크기 조절 중심 */
    bool hidden, locked;
} Object;

/* RGBA 텍스처. 되돌리기 기록끼리 공유하고, 고칠 때만 복사(copy-on-write) */
typedef struct {
    int refs;
    int w, h;
    Color px[];
} TexBuf;

typedef struct {
    Object objs[MAX_OBJECTS];
    int count;
    int active;              /* 선택된 메쉬, 없으면 -1 */

    TexBuf *tex;
    Color palette[MAX_PALETTE];
    int npalette;
    Color bg;                /* 뷰 배경색 */
} Scene;

extern const Color DEFAULT_PALETTE[16];

TexBuf *texbuf_new(int w, int h);           /* 투명으로 채움 */
void    texbuf_release(TexBuf *t);

int     obj_add_vert(Object *o, Vector3 p);
int     obj_add_face(Object *o, const int *idx, int n, Color color);
void    obj_free(Object *o);
void    obj_copy(Object *dst, const Object *src);
Vector3 obj_face_normal(const Object *o, const Face *f);
Vector3 obj_face_center(const Object *o, const Face *f);
void    obj_translate(Object *o, Vector3 d);
void    obj_project_uv(Object *o, Face *f, float uv_scale);   /* 법선 주축 방향 평면 투영 */
void    obj_clear_selection(Object *o);

void obj_remove_unused_verts(Object *o);
void obj_delete_selected_faces(Object *o);
void obj_delete_selected_verts(Object *o);
bool obj_extrude_selected_faces(Object *o, float dist, float uv_scale);
bool obj_merge_selected_verts(Object *o);
void obj_snap_selected_verts(Object *o, float grid);
void obj_mirror(Object *o, int axis);
void obj_select_face_verts(Object *o);

enum { PRIM_PLANE, PRIM_CUBE, PRIM_PRISM, PRIM_PYRAMID, PRIM_CYLINDER, PRIM_CONE, PRIM_SPHERE, PRIM_COUNT };
extern const char *PRIM_NAMES[PRIM_COUNT];
/* 중심이 원점, +Y가 up 방향을 향하도록 만든 뒤 at으로 옮기고, 텍스처 안에 UV를 펼쳐 놓음 */
void prim_build(Object *o, int kind, Color color, Vector3 up, Vector3 at, const TexBuf *tex);

void    scene_init(Scene *s);                 /* 빈 씬 + 기본 팔레트/텍스처 (s는 비어 있어야 함) */
void    scene_reset_palette(Scene *s);
Object *scene_add(Scene *s, const char *name);
void    scene_delete(Scene *s, int i);
void    scene_clear(Scene *s);                /* 오브젝트만 해제 */
void    scene_release(Scene *s);              /* 오브젝트 + 텍스처 해제 */
void    scene_copy(Scene *dst, const Scene *src);
float   scene_uv_scale(const Scene *s);       /* 월드 1단위당 텍스처 픽셀 (기본 투영용) */
Color  *scene_tex_mut(Scene *s);              /* 텍스처를 고치기 전에: 공유 중이면 복사 */
void    scene_resize_tex(Scene *s, int w, int h);   /* 왼쪽 위 기준으로 자르거나 늘림 */

void undo_push(const Scene *s);
bool undo_undo(Scene *s);
bool undo_redo(Scene *s);
void undo_reset(void);

#endif
