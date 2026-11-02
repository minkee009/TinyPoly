#include "view_draw.h"
#include "view.h"
#include "texture.h"
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <stddef.h>

static const Color AXIS_COLOR[3] = {
    { 230, 70, 80, 255 }, { 90, 210, 90, 255 }, { 70, 140, 250, 255 },
};
static const Color SEL_COLOR = { 255, 200, 60, 255 };
static const Color ACTIVE_EDGE_COLOR = { 255, 255, 255, 200 };   /* 활성 메쉬 모서리 */
static const Color HOVER_COLOR = { 120, 230, 255, 255 };   /* 누르면 고를 대상 */
static const char *VIEW_NAMES[VIEW_COUNT] = { "top", "3d", "front", "side" };

static int light_level(Vector3 n) {
    const Vector3 L = { 0.43f, 0.86f, 0.26f };
    float d = Vector3DotProduct(n, L);
    return d > 0.55f ? 0 : d > -0.1f ? 1 : 2;
}

static void draw_meshes(App *a) {
    const Scene *s = &a->scene;

    BeginShaderMode(a->shader);
    for (int behind = 1; behind >= 0; behind--) {
        for (int dbl = 0; dbl < 2; dbl++) {
            rlDrawRenderBatchActive();
            if (behind) rlDisableDepthMask(); else rlEnableDepthMask();
            if (dbl) rlDisableBackfaceCulling(); else rlEnableBackfaceCulling();

            /* 0: 단색 면 (기본 흰 텍스처), 1: 텍스처 면 */
            for (int pass = 0; pass < 2; pass++) {
                /* rlBegin은 모드가 바뀌면 텍스처를 기본값으로 되돌리므로,
                   배치를 비운 뒤 rlBegin 다음에 텍스처를 지정한다 */
                rlDrawRenderBatchActive();
                rlBegin(RL_TRIANGLES);
                rlSetTexture(pass == 0 ? rlGetTextureIdDefault() : a->gpu_tex.id);
                for (int i = 0; i < s->count; i++) {
                    const Object *o = &s->objs[i];
                    if (o->hidden) continue;
                    for (int k = 0; k < o->nfaces; k++) {
                        const Face *f = &o->faces[k];
                        if (((f->flags & FACE_BEHIND) != 0) != behind || ((f->flags & FACE_DOUBLE) != 0) != dbl) continue;
                        Vector3 n = obj_face_normal(o, f);
                        int level = (a->shading && !(f->flags & FACE_NOSHADE)) ? light_level(n) : 0;
                        bool textured = a->render_mode != RENDER_COLOR && !(f->flags & FACE_NOTEX);
                        if (textured != (pass == 1)) continue;

                        /* 텍스처 면은 흰색에 음영만 곱함 */
                        Color c = shade_color(textured ? WHITE : f->color, level);
                        /* 텍스처 모드는 UV 도구일 때만 옅게, 그리기 도구는 칠한 색이 그대로 보이게 테두리만 */
                        if (f->sel && i == s->active) {
                            if (a->mode == MODE_MODEL) c = ColorLerp(c, SEL_COLOR, 0.3f);
                            else if (a->tool == TOOL_UV) c = ColorLerp(c, SEL_COLOR, 0.15f);
                        }
                        if (i == a->hover_obj && k == a->hover_face) c = ColorLerp(c, HOVER_COLOR, 0.4f);
                        rlColor4ub(c.r, c.g, c.b, 255);
                        for (int j = 1; j + 1 < f->n; j++) {
                            int idx[3] = { 0, j, j + 1 };
                            for (int m = 0; m < 3; m++) {
                                Vector3 p = o->verts[f->v[idx[m]]].p;
                                rlTexCoord2f(f->uv[idx[m]].x / a->gpu_w, f->uv[idx[m]].y / a->gpu_h);
                                rlVertex3f(p.x, p.y, p.z);
                            }
                        }
                    }
                }
                rlEnd();
            }
        }
    }
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndShaderMode();
}

static Vector3 lift(const Viewport *vp, Vector3 p) {
    if (vp->kind != VIEW_3D) return Vector3Add(p, Vector3Scale(Vector3Normalize(Vector3Subtract(vp->cam.position, vp->cam.target)), 1.0f));
    return Vector3Lerp(p, vp->cam.position, 0.004f);
}

static void draw_edges(const Viewport *vp, const Object *o, Color c, bool only_sel) {
    for (int i = 0; i < o->nfaces; i++) {
        const Face *f = &o->faces[i];
        if (only_sel && !f->sel) continue;
        for (int k = 0; k < f->n; k++)
            DrawLine3D(lift(vp, o->verts[f->v[k]].p), lift(vp, o->verts[f->v[(k + 1) % f->n]].p), c);
    }
}

static void draw_face_edges(const Viewport *vp, const Object *o, const Face *f, Color c) {
    for (int k = 0; k < f->n; k++)
        DrawLine3D(lift(vp, o->verts[f->v[k]].p), lift(vp, o->verts[f->v[(k + 1) % f->n]].p), c);
}

static bool faces_camera(const Viewport *vp, const Object *o, const Face *f) {
    if (f->flags & FACE_DOUBLE) return true;
    return Vector3DotProduct(obj_face_normal(o, f), Vector3Subtract(vp->cam.position, o->verts[f->v[0]].p)) > 0;
}

static bool same_point(Vector3 p, Vector3 q) {
    return Vector3DistanceSqr(p, q) < 1e-8f;
}

/* 윤곽선: 카메라를 향한 면의 모서리 중 맞닿은 면이 없거나 그 면이 등을 돌린 것.
   정점을 공유하지 않는 면도 있으므로 위치로 비교 */
static void draw_outline(const Viewport *vp, const Object *o, Color c) {
    for (int i = 0; i < o->nfaces; i++) {
        const Face *f = &o->faces[i];
        if (!faces_camera(vp, o, f)) continue;
        for (int k = 0; k < f->n; k++) {
            Vector3 p = o->verts[f->v[k]].p, q = o->verts[f->v[(k + 1) % f->n]].p;
            bool inner = false;
            for (int j = 0; j < o->nfaces && !inner; j++) {
                const Face *g = &o->faces[j];
                if (j == i || !faces_camera(vp, o, g)) continue;
                for (int m = 0; m < g->n && !inner; m++) {
                    Vector3 r = o->verts[g->v[m]].p, s = o->verts[g->v[(m + 1) % g->n]].p;
                    inner = (same_point(p, s) && same_point(q, r)) || (same_point(p, r) && same_point(q, s));
                }
            }
            if (!inner) DrawLine3D(lift(vp, p), lift(vp, q), c);
        }
    }
}

/* 도트 원 테두리 (중점 원 알고리즘) */
static void draw_pixel_ring(int cx, int cy, int r, Color c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        int pts[8][2] = { { x, y }, { y, x }, { -y, x }, { -x, y }, { -x, -y }, { -y, -x }, { y, -x }, { x, -y } };
        for (int i = 0; i < 8; i++) DrawPixel(cx + pts[i][0], cy + pts[i][1], c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

static void draw_grid(const App *a, const Viewport *vp, Color c) {
    if (vp->kind == VIEW_3D) {
        for (int i = -8; i <= 8; i++) {
            DrawLine3D((Vector3){ (float)i, 0, -8 }, (Vector3){ (float)i, 0, 8 }, i ? c : ColorAlpha(AXIS_COLOR[2], 0.6f));
            DrawLine3D((Vector3){ -8, 0, (float)i }, (Vector3){ 8, 0, (float)i }, i ? c : ColorAlpha(AXIS_COLOR[0], 0.6f));
        }
        return;
    }
    /* 직교 뷰: 보이는 범위만, 그리드 4칸마다 진하게 */
    Vector3 r, u, n;
    view_axes(vp, &r, &u, &n);
    float half_h = vp->zoom / 2, half_w = half_h * vp->rect.width / vp->rect.height;
    float cr = Vector3DotProduct(vp->center, r), cu = Vector3DotProduct(vp->center, u);
    float step = a->grid;
    if (half_h / step > 60) step = 1.0f;
    Vector3 back = Vector3Scale(n, -50.0f);
    for (int axis = 0; axis < 2; axis++) {
        Vector3 along = axis ? u : r, across = axis ? r : u;
        float c0 = axis ? cr : cu, a0 = axis ? cu : cr;
        float ha = axis ? half_h : half_w, hc = axis ? half_w : half_h;
        for (float t = floorf((c0 - hc) / step) * step; t <= c0 + hc; t += step) {
            Vector3 p0 = Vector3Add(Vector3Add(Vector3Scale(across, t), Vector3Scale(along, a0 - ha)), back);
            Vector3 p1 = Vector3Add(Vector3Add(Vector3Scale(across, t), Vector3Scale(along, a0 + ha)), back);
            bool major = fabsf(fmodf(t, 1.0f)) < 1e-4f || fabsf(fabsf(fmodf(t, 1.0f)) - 1.0f) < 1e-4f;
            Color col = c;
            if (fabsf(t) < 1e-4f) {
                /* 원점을 지나는 선은 그 선이 뻗은 축의 색 */
                col = fabsf(along.x) > 0.5f ? AXIS_COLOR[0] : fabsf(along.y) > 0.5f ? AXIS_COLOR[1] : AXIS_COLOR[2];
            } else if (!major) {
                col = ColorAlpha(c, 0.45f);
            }
            DrawLine3D(p0, p1, col);
        }
    }
}

static Color bg_color(const App *a) {
    return a->scene.bg;
}

static Color grid_color(const App *a) {
    Color b = bg_color(a);
    return ColorBrightness(b, b.r + b.g + b.b > 380 ? -0.25f : 0.25f);
}

void view_draw(App *a, Viewport *vp) {
    const Scene *s = &a->scene;
    const Object *act = s->active >= 0 ? &s->objs[s->active] : NULL;
    bool ortho = vp->kind != VIEW_3D;

    BeginTextureMode(vp->rt);
    ClearBackground(bg_color(a));
    BeginMode3D(vp->cam);
    draw_grid(a, vp, grid_color(a));
    if (a->render_mode != RENDER_WIRE) draw_meshes(a);

    for (int i = 0; i < s->count; i++) {
        const Object *o = &s->objs[i];
        if (o->hidden) continue;
        if (a->render_mode == RENDER_WIRE) draw_edges(vp, o, o == act ? SEL_COLOR : WHITE, false);
        else if (o == act && a->mode == MODE_MODEL) draw_edges(vp, o, ACTIVE_EDGE_COLOR, false);
        /* 텍스처 모드는 칠한 면을 가리지 않게 윤곽만 */
        else if (o == act) draw_outline(vp, o, ACTIVE_EDGE_COLOR);
    }
    if (act) draw_edges(vp, act, SEL_COLOR, true);

    /* 누르기 전 강조: 면은 그 면의 테두리, 다른 메쉬는 메쉬 전체 테두리 */
    const Object *hov = a->hover_obj >= 0 && a->hover_obj < s->count ? &s->objs[a->hover_obj] : NULL;
    if (hov && a->hover_face >= 0 && a->hover_face < hov->nfaces) draw_face_edges(vp, hov, &hov->faces[a->hover_face], HOVER_COLOR);
    else if (hov && a->hover_face < 0 && a->hover_vert < 0) draw_edges(vp, hov, HOVER_COLOR, false);

    if (act && ortho && a->mode == MODE_MODEL) {
        Vector3 r, u, n;
        view_axes(vp, &r, &u, &n);
        float k = vp->zoom * 0.025f;
        Vector3 o = lift(vp, act->origin);
        DrawLine3D(Vector3Subtract(o, Vector3Scale(r, k)), Vector3Add(o, Vector3Scale(r, k)), (Color){ 255, 80, 200, 255 });
        DrawLine3D(Vector3Subtract(o, Vector3Scale(u, k)), Vector3Add(o, Vector3Scale(u, k)), (Color){ 255, 80, 200, 255 });
    }
    EndMode3D();

    /* 정점: 직교 뷰는 전부, 3D 뷰는 선택된 것만 */
    if (act && a->mode == MODE_MODEL) {
        for (int i = 0; i < act->nverts; i++) {
            const Vert *v = &act->verts[i];
            if (!ortho && !v->sel) continue;
            Vector3 fwd = Vector3Subtract(vp->cam.target, vp->cam.position);
            if (!ortho && Vector3DotProduct(Vector3Subtract(v->p, vp->cam.position), fwd) <= 0) continue;
            Vector2 sp = GetWorldToScreenEx(v->p, vp->cam, vp->rtw, vp->rth);
            int z = v->sel ? 3 : 2;
            DrawRectangle((int)sp.x - z / 2, (int)sp.y - z / 2, z, z, v->sel ? SEL_COLOR : (Color){ 20, 20, 30, 255 });
            if (!v->sel) DrawPixel((int)sp.x, (int)sp.y, WHITE);
        }
    }
    /* 누르기 전 강조: 정점은 둘레에 빈 원 (밝은 면 위에서도 보이게 바깥에 어두운 테두리) */
    if (hov && hov == act && a->hover_vert >= 0 && a->hover_vert < act->nverts) {
        Vector2 sp;
        Vector3 p = act->verts[a->hover_vert].p;
        Vector3 fwd = Vector3Subtract(vp->cam.target, vp->cam.position);
        if (ortho || Vector3DotProduct(Vector3Subtract(p, vp->cam.position), fwd) > 0) {
            sp = GetWorldToScreenEx(p, vp->cam, vp->rtw, vp->rth);
            /* 반지름은 커서의 이동 화살표보다 커서 가려지지 않게 */
            draw_pixel_ring((int)sp.x, (int)sp.y, VERT_RING_R + 1, (Color){ 20, 20, 30, 255 });
            draw_pixel_ring((int)sp.x, (int)sp.y, VERT_RING_R, HOVER_COLOR);
        }
    }
    EndTextureMode();

    DrawTexturePro(vp->rt.texture, (Rectangle){ 0, 0, (float)vp->rtw, (float)-vp->rth }, vp->rect,
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawTextEx(GetFontDefault(), VIEW_NAMES[vp->kind], (Vector2){ vp->rect.x + 6, vp->rect.y + 4 }, 20, 2,
               ColorAlpha(WHITE, 0.7f));
    bool hover = CheckCollisionPointRec(GetMousePosition(), vp->rect);
    DrawRectangleLinesEx(vp->rect, 1, hover ? (Color){ 120, 116, 150, 255 } : (Color){ 50, 48, 66, 255 });

    if (a->model.box.width > 0 && CheckCollisionRecs(a->model.box, vp->rect)) {
        BeginScissorMode((int)vp->rect.x, (int)vp->rect.y, (int)vp->rect.width, (int)vp->rect.height);
        DrawRectangleLinesEx(a->model.box, 1, SEL_COLOR);
        EndScissorMode();
    }
}
