#ifndef MESH_H
#define MESH_H

#include <raylib.h>
#include <stdbool.h>

#define MAX_FACE_VERTS 16
#define MAX_OBJECTS    64
#define PALETTE_SIZE   16
#define TEX_W          128
#define TEX_H          128
#define UV_SCALE       16.0f      /* 월드 1단위 = 텍스처 16픽셀 */

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
    int color;
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

typedef struct {
    Object objs[MAX_OBJECTS];
    int count;
    int active;              /* 선택된 메쉬, 없으면 -1 */

    unsigned char tex[TEX_W * TEX_H];   /* 팔레트 인덱스 */
    Color palette[PALETTE_SIZE];
    unsigned char ramp[PALETTE_SIZE];   /* 음영 시 한 단계 어두운 색 */
    int bg, alpha;                      /* 배경색, 투명색 인덱스 */
} Scene;

extern const Color DEFAULT_PALETTE[PALETTE_SIZE];

int     obj_add_vert(Object *o, Vector3 p);
int     obj_add_face(Object *o, const int *idx, int n, int color);
void    obj_free(Object *o);
void    obj_copy(Object *dst, const Object *src);
Vector3 obj_face_normal(const Object *o, const Face *f);
Vector3 obj_face_center(const Object *o, const Face *f);
void    obj_translate(Object *o, Vector3 d);
void    obj_project_uv(Object *o, Face *f);      /* 법선 주축 방향 평면 투영 */
void    obj_clear_selection(Object *o);

void obj_remove_unused_verts(Object *o);
void obj_delete_selected_faces(Object *o);
void obj_delete_selected_verts(Object *o);
bool obj_extrude_selected_faces(Object *o, float dist);
bool obj_merge_selected_verts(Object *o);
void obj_snap_selected_verts(Object *o, float grid);
void obj_mirror(Object *o, int axis);
void obj_select_face_verts(Object *o);

enum { PRIM_PLANE, PRIM_CUBE, PRIM_PRISM, PRIM_PYRAMID, PRIM_CYLINDER, PRIM_CONE, PRIM_SPHERE, PRIM_COUNT };
extern const char *PRIM_NAMES[PRIM_COUNT];
/* 중심이 원점, +Y가 up 방향을 향하도록 만든 뒤 at으로 옮김 */
void prim_build(Object *o, int kind, int color, Vector3 up, Vector3 at);

void    scene_init(Scene *s);                 /* 빈 씬 + 기본 팔레트/텍스처 */
void    scene_reset_palette(Scene *s);
Object *scene_add(Scene *s, const char *name);
void    scene_delete(Scene *s, int i);
void    scene_clear(Scene *s);                /* 오브젝트만 해제 */
void    scene_copy(Scene *dst, const Scene *src);

void undo_push(const Scene *s);
bool undo_undo(Scene *s);
bool undo_redo(Scene *s);
void undo_reset(void);

#endif
