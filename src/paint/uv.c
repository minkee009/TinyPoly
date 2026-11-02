#include "paint/uv.h"
#include "ui/cursor.h"
#include "paint/texview.h"
#include <raymath.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool KeyShift(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool KeyCtrl(void)  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

static bool PointInUV(const Face *f, Vector2 p) {
    for (int j = 1; j + 1 < f->n; j++)
        if (CheckCollisionPointTriangle(p, f->uv[0], f->uv[j], f->uv[j + 1])) return true;
    return false;
}

static bool UVBounds(Object *o, Vector2 *mn, Vector2 *mx) {
    bool any = false;
    *mn = (Vector2){ 1e9f, 1e9f };
    *mx = (Vector2){ -1e9f, -1e9f };
    for (int i = 0; i < o->nfaces; i++) {
        if (!o->faces[i].sel) continue;
        for (int k = 0; k < o->faces[i].n; k++) {
            *mn = Vector2Min(*mn, o->faces[i].uv[k]);
            *mx = Vector2Max(*mx, o->faces[i].uv[k]);
        }
        any = true;
    }
    return any;
}

static Object *UVTarget(App *a) {
    Object *o = AppActive(a);
    if (!o) return NULL;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) return o;
    return NULL;
}

bool UVHasTarget(App *a) {
    return UVTarget(a) != NULL;
}

void UVRotate(App *a) {
    Object *o = UVTarget(a);
    Vector2 mn, mx;
    if (!o || !UVBounds(o, &mn, &mx)) return;
    AppMark(a);
    Vector2 c = { floorf((mn.x + mx.x) / 2), floorf((mn.y + mx.y) / 2) };
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) {
                Vector2 d = Vector2Subtract(o->faces[i].uv[k], c);
                o->faces[i].uv[k] = (Vector2){ c.x - d.y, c.y + d.x };
            }
}

void UVFlip(App *a, bool vertical) {
    Object *o = UVTarget(a);
    Vector2 mn, mx;
    if (!o || !UVBounds(o, &mn, &mx)) return;
    AppMark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) {
                Vector2 *p = &o->faces[i].uv[k];
                if (vertical) p->y = mn.y + mx.y - p->y;
                else p->x = mn.x + mx.x - p->x;
            }
}

void UVReset(App *a) {
    Object *o = UVTarget(a);
    if (!o) return;
    AppMark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) ObjProjectUV(o, &o->faces[i], SceneUVScale(&a->scene));
}

void UVCopy(App *a) {
    Object *o = UVTarget(a);
    if (!o) return;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) {
            a->uv_clip_n = o->faces[i].n;
            memcpy(a->uv_clip, o->faces[i].uv, sizeof(Vector2) * (size_t)a->uv_clip_n);
            AppMsg(a, "copied uv");
            return;
        }
}

void UVPaste(App *a) {
    Object *o = UVTarget(a);
    if (!o || !a->uv_clip_n) return;
    AppMark(a);
    int n = 0;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel && o->faces[i].n == a->uv_clip_n) {
            memcpy(o->faces[i].uv, a->uv_clip, sizeof(Vector2) * (size_t)a->uv_clip_n);
            n++;
        }
    if (!n) AppMsg(a, "uv paste: face has a different vertex count");
}

void UVNudge(App *a, Vector2 d) {
    Object *o = UVTarget(a);
    if (!o) return;
    AppMark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) o->faces[i].uv[k] = Vector2Add(o->faces[i].uv[k], d);
}

static void UVBeginDrag(App *a, PaintDragKind kind) {
    Object *o = AppActive(a);
    free(a->paint.uv0);
    a->paint.uv0 = malloc(sizeof *a->paint.uv0 * (size_t)(o->nfaces + 1));
    for (int i = 0; i < o->nfaces; i++) memcpy(a->paint.uv0[i], o->faces[i].uv, sizeof a->paint.uv0[i]);
    a->paint.drag = kind;
    a->paint.marked = false;
}

void UVApplyDrag(App *a, Vector2 m) {
    Object *o = AppActive(a);
    if (!o || !a->paint.moved) return;
    for (int i = 0; i < o->nfaces; i++) memcpy(o->faces[i].uv, a->paint.uv0[i], sizeof a->paint.uv0[i]);
    if (!a->paint.marked) { AppMark(a); a->paint.marked = true; }

    float sc = TexViewScale(a);
    Vector2 d = { (m.x - a->paint.press.x) / sc, (m.y - a->paint.press.y) / sc };
    if (!KeyCtrl()) d = (Vector2){ roundf(d.x), roundf(d.y) };

    if (a->paint.drag == PDRAG_UV_VERT) {
        Vector2 *p = &o->faces[a->paint.uv_face].uv[a->paint.uv_vert];
        *p = Vector2Add(*p, d);
    } else if (a->paint.drag == PDRAG_UV_SCALE) {
        Vector2 mn, mx;
        UVBounds(o, &mn, &mx);
        Vector2 c = { (mn.x + mx.x) / 2, (mn.y + mx.y) / 2 };
        float f = fmaxf(0.0f, 1.0f + (m.x - a->paint.press.x - (m.y - a->paint.press.y)) * 0.01f);
        for (int i = 0; i < o->nfaces; i++)
            if (o->faces[i].sel)
                for (int k = 0; k < o->faces[i].n; k++) {
                    Vector2 p = Vector2Add(c, Vector2Scale(Vector2Subtract(o->faces[i].uv[k], c), f));
                    o->faces[i].uv[k] = KeyCtrl() ? p : (Vector2){ roundf(p.x), roundf(p.y) };
                }
    } else {
        for (int i = 0; i < o->nfaces; i++)
            if (o->faces[i].sel)
                for (int k = 0; k < o->faces[i].n; k++) o->faces[i].uv[k] = Vector2Add(o->faces[i].uv[k], d);
    }
}

void UVPress(App *a, Vector2 m) {
    Object *o = AppActive(a);
    if (!o) return;
    Vector2 t = TexViewToTex(a, m);

    if (IsKeyDown(KEY_T) && UVTarget(a)) {
        UVBeginDrag(a, PDRAG_UV_SCALE);
        return;
    }
    /* 선택된 면의 UV 꼭짓점 → 그 점만 */
    for (int i = 0; i < o->nfaces; i++) {
        if (!o->faces[i].sel) continue;
        for (int k = 0; k < o->faces[i].n; k++)
            if (Vector2Distance(TexViewToScreen(a, o->faces[i].uv[k]), m) < 6.0f) {
                a->paint.uv_face = i;
                a->paint.uv_vert = k;
                UVBeginDrag(a, PDRAG_UV_VERT);
                return;
            }
    }
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel && PointInUV(&o->faces[i], t)) {
            if (KeyShift()) { o->faces[i].sel = false; return; }
            UVBeginDrag(a, PDRAG_UV_MOVE);
            return;
        }
    /* 선택 안 된 면의 UV를 누르면 그 면을 선택 */
    for (int i = 0; i < o->nfaces; i++)
        if (PointInUV(&o->faces[i], t)) {
            if (!KeyShift()) ObjClearSelection(o);
            o->faces[i].sel = true;
            ObjSelectFaceVerts(o);
            UVBeginDrag(a, PDRAG_UV_MOVE);
            return;
        }
    if (!KeyShift()) ObjClearSelection(o);
}

int UVCursor(App *a, Vector2 m) {
    Vector2 t = TexViewToTex(a, m);
    Object *o = AppActive(a);
    if (!o) return CURSOR_ARROW;
    if (IsKeyDown(KEY_T) && UVTarget(a)) return CURSOR_SCALE;
    /* UV 위면 끌 수 있다는 표시 */
    for (int i = 0; i < o->nfaces; i++)
        if (!(o->faces[i].flags & FACE_NOTEX) && PointInUV(&o->faces[i], t)) return CURSOR_MOVE;
    return CURSOR_ARROW;
}

void UVDraw(App *a) {
    /* UV: 어떤 텍스처 위에서도 보이게 어두운 테두리 + 밝은 선 (테두리를 먼저 다 그려야 이음새가 안 덮임).
       선택된 면은 노란 선, UV 도구면 영역을 옅게 칠하고 꼭짓점도 */
    Object *ob = AppActive(a);
    if (ob) {
        const Color dark = { 16, 14, 24, 220 };
        for (int pass = 0; pass < 2; pass++) {
            Color c = pass ? (Color){ 255, 200, 60, 255 } : (Color){ 240, 236, 228, 230 };
            float w = 2.0f;
            for (int layer = 0; layer < 3; layer++)
                for (int i = 0; i < ob->nfaces; i++) {
                    const Face *f = &ob->faces[i];
                    if (f->sel != (pass == 1) || (f->flags & FACE_NOTEX)) continue;
                    if (layer == 0) {
                        if (!pass || a->tool != TOOL_UV) continue;
                        /* 볼록 다각형 부채꼴. 감김 방향을 몰라서 양쪽 다 (뒷면은 걸러짐) */
                        Vector2 p0 = TexViewToScreen(a, f->uv[0]);
                        for (int k = 1; k + 1 < f->n; k++) {
                            Vector2 p1 = TexViewToScreen(a, f->uv[k]), p2 = TexViewToScreen(a, f->uv[k + 1]);
                            DrawTriangle(p0, p1, p2, (Color){ 255, 200, 60, 50 });
                            DrawTriangle(p0, p2, p1, (Color){ 255, 200, 60, 50 });
                        }
                        continue;
                    }
                    for (int k = 0; k < f->n; k++) {
                        Vector2 p = TexViewToScreen(a, f->uv[k]), q = TexViewToScreen(a, f->uv[(k + 1) % f->n]);
                        if (layer == 1) {
                            DrawLineEx(p, q, w + 2, dark);
                            DrawRectangleV((Vector2){ p.x - (w + 2) / 2, p.y - (w + 2) / 2 }, (Vector2){ w + 2, w + 2 }, dark);
                        } else {
                            DrawLineEx(p, q, w, c);
                        }
                    }
                }
            if (pass && a->tool == TOOL_UV)
                for (int i = 0; i < ob->nfaces; i++) {
                    const Face *f = &ob->faces[i];
                    if (!f->sel || (f->flags & FACE_NOTEX)) continue;
                    for (int k = 0; k < f->n; k++) {
                        Vector2 p = TexViewToScreen(a, f->uv[k]);
                        DrawRectangle((int)p.x - 4, (int)p.y - 4, 8, 8, dark);
                        DrawRectangle((int)p.x - 3, (int)p.y - 3, 6, 6, c);
                    }
                }
        }
    }
}
