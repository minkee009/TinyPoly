#include "app.h"
#include "texture.h"
#include <raymath.h>
#include <rlgl.h>
#include <float.h>
#include <math.h>
#include <stddef.h>

Rectangle g_box;

static const Color AXIS_COLOR[3] = {
    { 230, 70, 80, 255 }, { 90, 210, 90, 255 }, { 70, 140, 250, 255 },
};
static const Color SEL_COLOR = { 255, 200, 60, 255 };
static const char *VIEW_NAMES[VIEW_COUNT] = { "top", "3d", "front", "side" };

/* 원근 보정 텍스처 + 투명색 픽셀은 버림 */
static const char *VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor;\n"
    "uniform mat4 mvp;\n"
    "out vec2 fragTexCoord; out vec4 fragColor;\n"
    "void main() { fragTexCoord = vertexTexCoord; fragColor = vertexColor;\n"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
static const char *FS =
    "#version 330\n"
    "in vec2 fragTexCoord; in vec4 fragColor;\n"
    "uniform sampler2D texture0; uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() { vec4 t = texture(texture0, fragTexCoord); if (t.a < 0.5) discard;\n"
    "  finalColor = t * fragColor * colDiffuse; }\n";

void view_init(App *a) {
    a->shader = LoadShaderFromMemory(VS, FS);
    Image img = GenImageColor(TEX_W, TEX_H, WHITE);
    for (int k = 0; k < 3; k++) {
        a->gpu_tex[k] = LoadTextureFromImage(img);
        SetTextureFilter(a->gpu_tex[k], TEXTURE_FILTER_POINT);
    }
    UnloadImage(img);
    a->tex_dirty = true;

    static const Vector3 centers[VIEW_COUNT] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0.5f, 0 }, { 0, 0.5f, 0 } };
    for (int i = 0; i < VIEW_COUNT; i++) {
        a->views[i].kind = i;
        a->views[i].center = centers[i];
        a->views[i].zoom = 6.0f;
    }
}

void view_upload_texture(App *a) {
    if (!a->tex_dirty) return;
    static Color px[TEX_W * TEX_H];
    for (int k = 0; k < 3; k++) {
        tex_build_rgba(&a->scene, k, px);
        UpdateTexture(a->gpu_tex[k], px);
    }
    a->tex_dirty = false;
}

static void set_rect(App *a, Viewport *vp, Rectangle r) {
    int rtw = (int)r.width / a->pixel, rth = (int)r.height / a->pixel;
    if (rtw < 1) rtw = 1;
    if (rth < 1) rth = 1;
    vp->rect = (Rectangle){ r.x, r.y, (float)(rtw * a->pixel), (float)(rth * a->pixel) };
    if (rtw != vp->rtw || rth != vp->rth || !IsRenderTextureValid(vp->rt)) {
        if (IsRenderTextureValid(vp->rt)) UnloadRenderTexture(vp->rt);
        vp->rt = LoadRenderTexture(rtw, rth);
        SetTextureFilter(vp->rt.texture, TEXTURE_FILTER_POINT);
        vp->rtw = rtw;
        vp->rth = rth;
    }
}

void view_layout(App *a) {
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    Rectangle area = { 0, TOPBAR_H, W, H - TOPBAR_H - STATUS_H };
    for (int i = 0; i < VIEW_COUNT; i++) a->views[i].rect.width = 0;

    if (a->mode == MODE_TEXTURE) {
        float cw = floorf(area.width * 0.55f);
        a->canvas_rect = (Rectangle){ 0, area.y, cw, area.height };
        set_rect(a, &a->views[VIEW_3D], (Rectangle){ cw + 2, area.y, area.width - cw - 2, area.height });
    } else if (a->full >= 0) {
        set_rect(a, &a->views[a->full], area);
    } else {
        float hw = floorf((area.width - 2) / 2), hh = floorf((area.height - 2) / 2);
        set_rect(a, &a->views[VIEW_TOP],   (Rectangle){ 0, area.y, hw, hh });
        set_rect(a, &a->views[VIEW_3D],    (Rectangle){ hw + 2, area.y, area.width - hw - 2, hh });
        set_rect(a, &a->views[VIEW_FRONT], (Rectangle){ 0, area.y + hh + 2, hw, area.height - hh - 2 });
        set_rect(a, &a->views[VIEW_SIDE],  (Rectangle){ hw + 2, area.y + hh + 2, area.width - hw - 2, area.height - hh - 2 });
    }
}

void view_axes(const Viewport *vp, Vector3 *right, Vector3 *up, Vector3 *normal) {
    switch (vp->kind) {
    case VIEW_TOP:   *right = (Vector3){ 1, 0, 0 };  *up = (Vector3){ 0, 0, -1 }; *normal = (Vector3){ 0, 1, 0 }; return;
    case VIEW_FRONT: *right = (Vector3){ 1, 0, 0 };  *up = (Vector3){ 0, 1, 0 };  *normal = (Vector3){ 0, 0, 1 }; return;
    case VIEW_SIDE:  *right = (Vector3){ 0, 0, -1 }; *up = (Vector3){ 0, 1, 0 };  *normal = (Vector3){ 1, 0, 0 }; return;
    default: {
        Vector3 fwd = Vector3Normalize(Vector3Subtract(vp->cam.target, vp->cam.position));
        *right = Vector3Normalize(Vector3CrossProduct(fwd, (Vector3){ 0, 1, 0 }));
        *up = Vector3CrossProduct(*right, fwd);
        *normal = Vector3Negate(fwd);
    }
    }
}

void view_update_cameras(App *a) {
    for (int i = 0; i < VIEW_COUNT; i++) {
        Viewport *vp = &a->views[i];
        Camera3D *c = &vp->cam;
        if (i == VIEW_3D) {
            float cp = cosf(a->pitch);
            Vector3 off = { cp * sinf(a->yaw), sinf(a->pitch), cp * cosf(a->yaw) };
            c->target = a->target;
            c->position = Vector3Add(a->target, Vector3Scale(off, a->dist));
            c->up = (Vector3){ 0, 1, 0 };
            c->fovy = 45.0f;
            c->projection = CAMERA_PERSPECTIVE;
        } else {
            Vector3 r, u, n;
            view_axes(vp, &r, &u, &n);
            c->target = vp->center;
            c->position = Vector3Add(vp->center, Vector3Scale(n, 100.0f));
            c->up = u;
            c->fovy = vp->zoom;
            c->projection = CAMERA_ORTHOGRAPHIC;
        }
    }
}

int view_at(const App *a, Vector2 m) {
    for (int i = 0; i < VIEW_COUNT; i++)
        if (a->views[i].rect.width > 0 && CheckCollisionPointRec(m, a->views[i].rect)) return i;
    return -1;
}

Ray view_ray(const Viewport *vp, Vector2 m) {
    Vector2 local = { m.x - vp->rect.x, m.y - vp->rect.y };
    return GetScreenToWorldRayEx(local, vp->cam, (int)vp->rect.width, (int)vp->rect.height);
}

bool view_project(const Viewport *vp, Vector3 p, Vector2 *out) {
    if (vp->kind == VIEW_3D) {
        Vector3 fwd = Vector3Subtract(vp->cam.target, vp->cam.position);
        if (Vector3DotProduct(Vector3Subtract(p, vp->cam.position), fwd) <= 0.01f) return false;
    }
    Vector2 s = GetWorldToScreenEx(p, vp->cam, (int)vp->rect.width, (int)vp->rect.height);
    *out = (Vector2){ s.x + vp->rect.x, s.y + vp->rect.y };
    return true;
}

float view_units_per_pixel(const App *a, const Viewport *vp) {
    float h = vp->kind == VIEW_3D ? 2.0f * a->dist * tanf(22.5f * DEG2RAD) : vp->zoom;
    return h / vp->rect.height;
}

bool view_pick_face(App *a, const Viewport *vp, Vector2 m, int only, int *out_obj, int *out_face) {
    Ray ray = view_ray(vp, m);
    float best = FLT_MAX;
    bool found = false;
    for (int i = 0; i < a->scene.count; i++) {
        const Object *o = &a->scene.objs[i];
        /* 잠긴 메쉬는 뷰에서 클릭으로 고를 수 없음 */
        if ((only >= 0 && i != only) || o->hidden || (only < 0 && o->locked)) continue;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            if (!(f->flags & FACE_DOUBLE) && Vector3DotProduct(obj_face_normal(o, f), ray.direction) >= 0) continue;
            Vector3 p0 = o->verts[f->v[0]].p;
            for (int j = 1; j + 1 < f->n; j++) {
                RayCollision c = GetRayCollisionTriangle(ray, p0, o->verts[f->v[j]].p, o->verts[f->v[j + 1]].p);
                if (c.hit && c.distance < best) {
                    best = c.distance;
                    *out_obj = i;
                    *out_face = k;
                    found = true;
                }
            }
        }
    }
    return found;
}

int view_pick_vert(App *a, const Viewport *vp, Vector2 m) {
    Object *o = app_active(a);
    if (!o) return -1;
    float best = 6.0f;
    int found = -1;
    for (int i = 0; i < o->nverts; i++) {
        Vector2 sp;
        if (!view_project(vp, o->verts[i].p, &sp)) continue;
        float d = Vector2Distance(sp, m) - (o->verts[i].sel ? 0.5f : 0.0f);   /* 겹치면 선택된 쪽 */
        if (d < best) { best = d; found = i; }
    }
    return found;
}

/* ---- 그리기 ---- */

static int light_level(Vector3 n) {
    const Vector3 L = { 0.43f, 0.86f, 0.26f };
    float d = Vector3DotProduct(n, L);
    return d > 0.55f ? 0 : d > -0.1f ? 1 : 2;
}

static void draw_meshes(App *a) {
    const Scene *s = &a->scene;
    bool blink = fmod(GetTime(), 0.6) < 0.3;

    BeginShaderMode(a->shader);
    for (int behind = 1; behind >= 0; behind--) {
        for (int dbl = 0; dbl < 2; dbl++) {
            rlDrawRenderBatchActive();
            if (behind) rlDisableDepthMask(); else rlEnableDepthMask();
            if (dbl) rlDisableBackfaceCulling(); else rlEnableBackfaceCulling();

            for (int slot = -1; slot < 3; slot++) {
                /* rlBegin은 모드가 바뀌면 텍스처를 기본값으로 되돌리므로,
                   배치를 비운 뒤 rlBegin 다음에 텍스처를 지정한다 */
                rlDrawRenderBatchActive();
                rlBegin(RL_TRIANGLES);
                rlSetTexture(slot < 0 ? rlGetTextureIdDefault() : a->gpu_tex[slot].id);
                for (int i = 0; i < s->count; i++) {
                    const Object *o = &s->objs[i];
                    if (o->hidden) continue;
                    for (int k = 0; k < o->nfaces; k++) {
                        const Face *f = &o->faces[k];
                        if (((f->flags & FACE_BEHIND) != 0) != behind || ((f->flags & FACE_DOUBLE) != 0) != dbl) continue;
                        Vector3 n = obj_face_normal(o, f);
                        int level = (a->shading && !(f->flags & FACE_NOSHADE)) ? light_level(n) : 0;
                        bool textured = a->render_mode != RENDER_COLOR && !(f->flags & FACE_NOTEX);
                        if ((textured ? level : -1) != slot) continue;

                        Color c = textured ? WHITE : tex_shade_color(s, f->color, level);
                        if (f->sel && i == s->active && blink) c = ColorLerp(c, SEL_COLOR, 0.6f);
                        rlColor4ub(c.r, c.g, c.b, 255);
                        for (int j = 1; j + 1 < f->n; j++) {
                            int idx[3] = { 0, j, j + 1 };
                            for (int m = 0; m < 3; m++) {
                                Vector3 p = o->verts[f->v[idx[m]]].p;
                                rlTexCoord2f(f->uv[idx[m]].x / TEX_W, f->uv[idx[m]].y / TEX_H);
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
    return a->scene.palette[a->scene.bg];
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
        else if (o == act && a->mode == MODE_MODEL) draw_edges(vp, o, (Color){ 255, 255, 255, 200 }, false);
    }
    if (act) draw_edges(vp, act, SEL_COLOR, true);

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
    EndTextureMode();

    DrawTexturePro(vp->rt.texture, (Rectangle){ 0, 0, (float)vp->rtw, (float)-vp->rth }, vp->rect,
                   (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawTextEx(GetFontDefault(), VIEW_NAMES[vp->kind], (Vector2){ vp->rect.x + 6, vp->rect.y + 4 }, 20, 2,
               ColorAlpha(WHITE, 0.7f));
    bool hover = CheckCollisionPointRec(GetMousePosition(), vp->rect);
    DrawRectangleLinesEx(vp->rect, 1, hover ? (Color){ 120, 116, 150, 255 } : (Color){ 50, 48, 66, 255 });

    if (g_box.width > 0 && CheckCollisionRecs(g_box, vp->rect)) {
        BeginScissorMode((int)vp->rect.x, (int)vp->rect.y, (int)vp->rect.width, (int)vp->rect.height);
        DrawRectangleLinesEx(g_box, 1, SEL_COLOR);
        EndScissorMode();
    }
}
