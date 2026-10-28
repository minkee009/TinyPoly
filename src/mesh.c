#include "mesh.h"
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *PRIM_NAMES[PRIM_COUNT] = { "plane", "cube", "prism", "pyramid", "cylinder", "cone", "sphere" };

static void *grow(void *p, int *cap, int need, size_t elem) {
    if (need <= *cap) return p;
    int nc = *cap ? *cap * 2 : 16;
    while (nc < need) nc *= 2;
    p = realloc(p, elem * (size_t)nc);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    *cap = nc;
    return p;
}

int obj_add_vert(Object *o, Vector3 p) {
    o->verts = grow(o->verts, &o->cverts, o->nverts + 1, sizeof(Vert));
    o->verts[o->nverts] = (Vert){ p, false };
    return o->nverts++;
}

int obj_add_face(Object *o, const int *idx, int n, int color) {
    if (n < 3 || n > MAX_FACE_VERTS) return -1;
    o->faces = grow(o->faces, &o->cfaces, o->nfaces + 1, sizeof(Face));
    Face *f = &o->faces[o->nfaces];
    memset(f, 0, sizeof *f);
    f->n = n;
    f->color = color;
    memcpy(f->v, idx, sizeof(int) * (size_t)n);
    return o->nfaces++;
}

void obj_free(Object *o) {
    free(o->verts);
    free(o->faces);
    memset(o, 0, sizeof *o);
}

void obj_copy(Object *dst, const Object *src) {
    *dst = *src;
    dst->verts = NULL; dst->cverts = 0;
    dst->faces = NULL; dst->cfaces = 0;
    if (src->nverts) {
        dst->verts = grow(NULL, &dst->cverts, src->nverts, sizeof(Vert));
        memcpy(dst->verts, src->verts, sizeof(Vert) * (size_t)src->nverts);
    }
    if (src->nfaces) {
        dst->faces = grow(NULL, &dst->cfaces, src->nfaces, sizeof(Face));
        memcpy(dst->faces, src->faces, sizeof(Face) * (size_t)src->nfaces);
    }
}

/* Newell 방식: 오목하거나 살짝 휜 면에서도 안정적 */
Vector3 obj_face_normal(const Object *o, const Face *f) {
    Vector3 n = { 0 };
    for (int i = 0; i < f->n; i++) {
        Vector3 a = o->verts[f->v[i]].p;
        Vector3 b = o->verts[f->v[(i + 1) % f->n]].p;
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return Vector3Normalize(n);
}

Vector3 obj_face_center(const Object *o, const Face *f) {
    Vector3 c = { 0 };
    for (int i = 0; i < f->n; i++) c = Vector3Add(c, o->verts[f->v[i]].p);
    return Vector3Scale(c, 1.0f / (float)f->n);
}

void obj_translate(Object *o, Vector3 d) {
    for (int i = 0; i < o->nverts; i++) o->verts[i].p = Vector3Add(o->verts[i].p, d);
    o->origin = Vector3Add(o->origin, d);
}

void obj_project_uv(Object *o, Face *f) {
    Vector3 n = obj_face_normal(o, f);
    float ax = fabsf(n.x), ay = fabsf(n.y), az = fabsf(n.z);
    float umin = 1e9f, vmin = 1e9f;
    for (int k = 0; k < f->n; k++) {
        Vector3 p = o->verts[f->v[k]].p;
        float u, v;
        if (ay >= ax && ay >= az) { u = n.y > 0 ? p.x : -p.x; v = p.z; }
        else if (ax >= az)        { u = n.x > 0 ? -p.z : p.z; v = -p.y; }
        else                      { u = n.z > 0 ? p.x : -p.x; v = -p.y; }
        f->uv[k] = (Vector2){ u * UV_SCALE, v * UV_SCALE };
        if (f->uv[k].x < umin) umin = f->uv[k].x;
        if (f->uv[k].y < vmin) vmin = f->uv[k].y;
    }
    for (int k = 0; k < f->n; k++)
        f->uv[k] = (Vector2){ roundf(f->uv[k].x - umin), roundf(f->uv[k].y - vmin) };
}

void obj_clear_selection(Object *o) {
    for (int i = 0; i < o->nverts; i++) o->verts[i].sel = false;
    for (int i = 0; i < o->nfaces; i++) o->faces[i].sel = false;
}

void obj_remove_unused_verts(Object *o) {
    int *remap = calloc((size_t)o->nverts + 1, sizeof(int));
    for (int i = 0; i < o->nfaces; i++)
        for (int k = 0; k < o->faces[i].n; k++) remap[o->faces[i].v[k]] = 1;

    int n = 0;
    for (int i = 0; i < o->nverts; i++) {
        if (remap[i]) {
            o->verts[n] = o->verts[i];
            remap[i] = n++;
        } else {
            remap[i] = -1;
        }
    }
    o->nverts = n;
    for (int i = 0; i < o->nfaces; i++)
        for (int k = 0; k < o->faces[i].n; k++) o->faces[i].v[k] = remap[o->faces[i].v[k]];
    free(remap);
}

void obj_delete_selected_faces(Object *o) {
    int n = 0;
    for (int i = 0; i < o->nfaces; i++)
        if (!o->faces[i].sel) o->faces[n++] = o->faces[i];
    o->nfaces = n;
    obj_remove_unused_verts(o);
}

void obj_delete_selected_verts(Object *o) {
    for (int i = 0; i < o->nfaces; i++) {
        Face *f = &o->faces[i];
        f->sel = false;
        for (int k = 0; k < f->n; k++)
            if (o->verts[f->v[k]].sel) { f->sel = true; break; }
    }
    obj_delete_selected_faces(o);
    for (int i = 0; i < o->nverts; i++) o->verts[i].sel = false;
}

static bool face_has_edge(const Face *f, int a, int b) {
    for (int k = 0; k < f->n; k++) {
        int x = f->v[k], y = f->v[(k + 1) % f->n];
        if ((x == a && y == b) || (x == b && y == a)) return true;
    }
    return false;
}

static void flip_face(Face *f) {
    for (int a = 0, b = f->n - 1; a < b; a++, b--) {
        int t = f->v[a]; f->v[a] = f->v[b]; f->v[b] = t;
        Vector2 u = f->uv[a]; f->uv[a] = f->uv[b]; f->uv[b] = u;
    }
}

/* 선택된 면들을 한 덩어리로 돌출: 선택 영역의 바깥 경계에만 옆면을 만든다 */
bool obj_extrude_selected_faces(Object *o, float dist) {
    int nv = o->nverts, nf = o->nfaces;
    int *map = malloc(sizeof(int) * (size_t)(nv + 1));
    Vector3 acc = { 0 };
    bool any = false;
    for (int i = 0; i < nv; i++) map[i] = -1;

    for (int i = 0; i < nf; i++) {
        Face *f = &o->faces[i];
        if (!f->sel) continue;
        any = true;
        acc = Vector3Add(acc, obj_face_normal(o, f));
        for (int k = 0; k < f->n; k++) map[f->v[k]] = 0;
    }
    if (!any) { free(map); return false; }

    /* 영역 전체를 평균 법선 한 방향으로 밀어 면들이 평평하게 유지되도록 */
    Vector3 off = Vector3Scale(Vector3Normalize(acc), dist);
    for (int i = 0; i < nv; i++) {
        if (map[i] < 0) continue;
        Vector3 p = Vector3Add(o->verts[i].p, off);
        map[i] = obj_add_vert(o, p);
    }

    for (int i = 0; i < nf; i++) {
        if (!o->faces[i].sel) continue;
        Face f = o->faces[i];   /* obj_add_face가 배열을 옮길 수 있으므로 복사 */
        for (int k = 0; k < f.n; k++) {
            int a = f.v[k], b = f.v[(k + 1) % f.n];
            bool shared = false;
            for (int j = 0; j < nf && !shared; j++)
                if (j != i && o->faces[j].sel && face_has_edge(&o->faces[j], a, b)) shared = true;
            if (shared) continue;
            int fi = obj_add_face(o, (int[]){ a, b, map[b], map[a] }, 4, f.color);
            o->faces[fi].flags = f.flags;
        }
    }

    for (int i = 0; i < nf; i++) {
        Face *f = &o->faces[i];
        if (!f->sel) continue;
        for (int k = 0; k < f->n; k++) f->v[k] = map[f->v[k]];
    }
    /* 옆면 uv는 새 위치 기준으로 투영 */
    for (int i = nf; i < o->nfaces; i++) obj_project_uv(o, &o->faces[i]);
    for (int i = 0; i < o->nverts; i++) o->verts[i].sel = false;
    obj_select_face_verts(o);
    free(map);
    return true;
}

bool obj_merge_selected_verts(Object *o) {
    Vector3 c = { 0 };
    int keep = -1, count = 0;
    for (int i = 0; i < o->nverts; i++) {
        if (!o->verts[i].sel) continue;
        if (keep < 0) keep = i;
        c = Vector3Add(c, o->verts[i].p);
        count++;
    }
    if (count < 2) return false;
    o->verts[keep].p = Vector3Scale(c, 1.0f / (float)count);

    int nf = 0;
    for (int i = 0; i < o->nfaces; i++) {
        Face f = o->faces[i];
        int n = 0;
        for (int k = 0; k < f.n; k++) {
            int v = o->verts[f.v[k]].sel ? keep : f.v[k];
            if (n == 0 || f.v[n - 1] != v) {
                f.uv[n] = f.uv[k];
                f.v[n++] = v;
            }
        }
        while (n > 1 && f.v[n - 1] == f.v[0]) n--;
        f.n = n;
        if (n >= 3) o->faces[nf++] = f;
    }
    o->nfaces = nf;
    for (int i = 0; i < o->nverts; i++) o->verts[i].sel = false;
    o->verts[keep].sel = true;
    obj_remove_unused_verts(o);
    return true;
}

void obj_snap_selected_verts(Object *o, float g) {
    for (int i = 0; i < o->nverts; i++) {
        if (!o->verts[i].sel) continue;
        Vector3 *p = &o->verts[i].p;
        *p = (Vector3){ roundf(p->x / g) * g, roundf(p->y / g) * g, roundf(p->z / g) * g };
    }
}

static float *comp(Vector3 *v, int axis) {
    return axis == 0 ? &v->x : axis == 1 ? &v->y : &v->z;
}

void obj_mirror(Object *o, int axis) {
    float c = *comp(&o->origin, axis);
    for (int i = 0; i < o->nverts; i++) {
        float *p = comp(&o->verts[i].p, axis);
        *p = 2.0f * c - *p;
    }
    for (int i = 0; i < o->nfaces; i++) flip_face(&o->faces[i]);
}

void obj_select_face_verts(Object *o) {
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) o->verts[o->faces[i].v[k]].sel = true;
}

/* ---- 기본 도형 ---- */

/* 볼록 도형은 중심에서 바깥을 향하도록 면 방향을 맞춘다 */
static void orient_outward(Object *o) {
    Vector3 c = { 0 };
    for (int i = 0; i < o->nverts; i++) c = Vector3Add(c, o->verts[i].p);
    c = Vector3Scale(c, 1.0f / (float)o->nverts);
    for (int i = 0; i < o->nfaces; i++) {
        Face *f = &o->faces[i];
        Vector3 d = Vector3Subtract(obj_face_center(o, f), c);
        if (Vector3DotProduct(obj_face_normal(o, f), d) < 0) flip_face(f);
    }
}

static void add_ring(Object *o, int segs, float y, float r) {
    for (int i = 0; i < segs; i++) {
        float a = 2.0f * PI * (float)i / (float)segs;
        obj_add_vert(o, (Vector3){ cosf(a) * r, y, sinf(a) * r });
    }
}

static void build_shape(Object *o, int kind, int color) {
    const float h = 0.5f;
    const int segs = 8;
    int ring[MAX_FACE_VERTS];
    switch (kind) {
    case PRIM_PLANE:
        obj_add_vert(o, (Vector3){ -h, 0, -h });
        obj_add_vert(o, (Vector3){  h, 0, -h });
        obj_add_vert(o, (Vector3){  h, 0,  h });
        obj_add_vert(o, (Vector3){ -h, 0,  h });
        obj_add_face(o, (int[]){ 3, 2, 1, 0 }, 4, color);
        o->faces[0].flags |= FACE_DOUBLE;
        return;
    case PRIM_CUBE: {
        for (int y = 0; y < 2; y++) {
            float yy = y ? h : -h;
            obj_add_vert(o, (Vector3){ -h, yy, -h });
            obj_add_vert(o, (Vector3){  h, yy, -h });
            obj_add_vert(o, (Vector3){  h, yy,  h });
            obj_add_vert(o, (Vector3){ -h, yy,  h });
        }
        static const int f[6][4] = {
            { 7, 6, 5, 4 }, { 0, 1, 2, 3 }, { 3, 2, 6, 7 },
            { 1, 0, 4, 5 }, { 2, 1, 5, 6 }, { 0, 3, 7, 4 },
        };
        for (int i = 0; i < 6; i++) obj_add_face(o, f[i], 4, color);
        return;
    }
    case PRIM_PRISM:
        for (int z = 0; z < 2; z++) {
            float zz = z ? h : -h;
            obj_add_vert(o, (Vector3){ -h, -h, zz });
            obj_add_vert(o, (Vector3){  h, -h, zz });
            obj_add_vert(o, (Vector3){  0,  h, zz });
        }
        obj_add_face(o, (int[]){ 0, 1, 2 }, 3, color);
        obj_add_face(o, (int[]){ 3, 4, 5 }, 3, color);
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            obj_add_face(o, (int[]){ i, j, j + 3, i + 3 }, 4, color);
        }
        break;
    case PRIM_PYRAMID:
        obj_add_vert(o, (Vector3){ -h, -h, -h });
        obj_add_vert(o, (Vector3){  h, -h, -h });
        obj_add_vert(o, (Vector3){  h, -h,  h });
        obj_add_vert(o, (Vector3){ -h, -h,  h });
        obj_add_vert(o, (Vector3){  0,  h,  0 });
        obj_add_face(o, (int[]){ 0, 1, 2, 3 }, 4, color);
        for (int i = 0; i < 4; i++) obj_add_face(o, (int[]){ i, 4, (i + 1) % 4 }, 3, color);
        break;
    case PRIM_CYLINDER:
        add_ring(o, segs, -h, h);
        add_ring(o, segs, h, h);
        for (int i = 0; i < segs; i++) {
            int j = (i + 1) % segs;
            obj_add_face(o, (int[]){ i, j, segs + j, segs + i }, 4, color);
            ring[i] = i;
        }
        obj_add_face(o, ring, segs, color);
        for (int i = 0; i < segs; i++) ring[i] = segs + i;
        obj_add_face(o, ring, segs, color);
        break;
    case PRIM_CONE: {
        add_ring(o, segs, -h, h);
        int apex = obj_add_vert(o, (Vector3){ 0, h, 0 });
        for (int i = 0; i < segs; i++) {
            obj_add_face(o, (int[]){ i, (i + 1) % segs, apex }, 3, color);
            ring[i] = i;
        }
        obj_add_face(o, ring, segs, color);
        break;
    }
    case PRIM_SPHERE: {
        const int rings = 6;
        int top = obj_add_vert(o, (Vector3){ 0, h, 0 });
        for (int r = 1; r < rings; r++) {
            float phi = PI * (float)r / (float)rings;
            add_ring(o, segs, cosf(phi) * h, sinf(phi) * h);
        }
        int bot = obj_add_vert(o, (Vector3){ 0, -h, 0 });
        #define RING(r, i) (1 + ((r) - 1) * segs + ((i) % segs))
        for (int i = 0; i < segs; i++) {
            obj_add_face(o, (int[]){ top, RING(1, i + 1), RING(1, i) }, 3, color);
            for (int r = 1; r < rings - 1; r++)
                obj_add_face(o, (int[]){ RING(r, i), RING(r, i + 1), RING(r + 1, i + 1), RING(r + 1, i) }, 4, color);
            obj_add_face(o, (int[]){ bot, RING(rings - 1, i), RING(rings - 1, i + 1) }, 3, color);
        }
        #undef RING
        break;
    }
    }
    orient_outward(o);
}

void prim_build(Object *o, int kind, int color, Vector3 up, Vector3 at) {
    build_shape(o, kind, color);

    up = Vector3Normalize(up);
    Vector3 axis = Vector3CrossProduct((Vector3){ 0, 1, 0 }, up);
    float ang = acosf(Clamp(up.y, -1.0f, 1.0f));
    if (Vector3Length(axis) < 1e-5f) axis = (Vector3){ 1, 0, 0 };
    for (int i = 0; i < o->nverts; i++) {
        Vector3 p = o->verts[i].p;
        if (ang > 1e-5f) p = Vector3RotateByAxisAngle(p, Vector3Normalize(axis), ang);
        p = (Vector3){ roundf(p.x * 1e4f) / 1e4f, roundf(p.y * 1e4f) / 1e4f, roundf(p.z * 1e4f) / 1e4f };
        o->verts[i].p = Vector3Add(p, at);
    }
    o->origin = at;

    /* 면마다 투영한 UV를 텍스처 위에 줄지어 배치(선반 방식)해서 겹치지 않게 */
    float x = 0, y = 0, row = 0;
    for (int i = 0; i < o->nfaces; i++) {
        Face *f = &o->faces[i];
        obj_project_uv(o, f);
        float w = 0, h = 0;
        for (int k = 0; k < f->n; k++) { w = fmaxf(w, f->uv[k].x); h = fmaxf(h, f->uv[k].y); }
        if (x + w > TEX_W) { x = 0; y += row; row = 0; }
        if (y + h > TEX_H) y = 0;
        for (int k = 0; k < f->n; k++) f->uv[k] = (Vector2){ f->uv[k].x + x, f->uv[k].y + y };
        x += w;
        row = fmaxf(row, h);
    }
}

/* ---- 씬 ---- */

void scene_init(Scene *s) {
    memset(s, 0, sizeof *s);
    s->active = -1;
    scene_reset_palette(s);
    /* 8픽셀 체크무늬: UV 위치가 잘 보이도록 */
    for (int y = 0; y < TEX_H; y++)
        for (int x = 0; x < TEX_W; x++)
            s->tex[y * TEX_W + x] = ((x / 8 + y / 8) & 1) ? 6 : 7;
}

Object *scene_add(Scene *s, const char *name) {
    if (s->count >= MAX_OBJECTS) return NULL;
    Object *o = &s->objs[s->count++];
    memset(o, 0, sizeof *o);
    snprintf(o->name, sizeof o->name, "%s", name);
    return o;
}

void scene_delete(Scene *s, int i) {
    obj_free(&s->objs[i]);
    memmove(&s->objs[i], &s->objs[i + 1], sizeof(Object) * (size_t)(s->count - i - 1));
    s->count--;
    if (s->active == i) s->active = -1;
    else if (s->active > i) s->active--;
}

void scene_clear(Scene *s) {
    for (int i = 0; i < s->count; i++) obj_free(&s->objs[i]);
    s->count = 0;
    s->active = -1;
}

void scene_copy(Scene *dst, const Scene *src) {
    memcpy(dst, src, sizeof *dst);
    for (int i = 0; i < src->count; i++) obj_copy(&dst->objs[i], &src->objs[i]);
}

/* ---- 되돌리기: 씬 전체 스냅샷. past + future 개수는 항상 UNDO_MAX 이하 ---- */

#define UNDO_MAX 64
static Scene g_past[UNDO_MAX], g_future[UNDO_MAX];
static int g_npast, g_nfuture;

void undo_push(const Scene *s) {
    if (g_npast == UNDO_MAX) {
        scene_clear(&g_past[0]);
        memmove(&g_past[0], &g_past[1], sizeof(Scene) * (UNDO_MAX - 1));
        g_npast--;
    }
    scene_copy(&g_past[g_npast++], s);
    while (g_nfuture) scene_clear(&g_future[--g_nfuture]);
}

bool undo_undo(Scene *s) {
    if (!g_npast) return false;
    g_future[g_nfuture++] = *s;
    *s = g_past[--g_npast];
    return true;
}

bool undo_redo(Scene *s) {
    if (!g_nfuture) return false;
    g_past[g_npast++] = *s;
    *s = g_future[--g_nfuture];
    return true;
}

void undo_reset(void) {
    while (g_npast) scene_clear(&g_past[--g_npast]);
    while (g_nfuture) scene_clear(&g_future[--g_nfuture]);
}
