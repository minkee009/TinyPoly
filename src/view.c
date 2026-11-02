#include "view.h"
#include <raymath.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
    a->tex_dirty = true;            /* 첫 view_upload_texture에서 만든다 */

    static const Vector3 centers[VIEW_COUNT] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0.5f, 0 }, { 0, 0.5f, 0 } };
    for (int i = 0; i < VIEW_COUNT; i++) {
        a->views[i].kind = i;
        a->views[i].center = centers[i];
        a->views[i].zoom = 6.0f;
    }
}

void view_tex_dirty(App *a, int x0, int y0, int x1, int y1) {
    if (!a->tex_rect_dirty) {
        a->dx0 = x0; a->dy0 = y0; a->dx1 = x1; a->dy1 = y1;
        a->tex_rect_dirty = true;
        return;
    }
    if (x0 < a->dx0) a->dx0 = x0;
    if (y0 < a->dy0) a->dy0 = y0;
    if (x1 > a->dx1) a->dx1 = x1;
    if (y1 > a->dy1) a->dy1 = y1;
}

/* 크기가 바뀌었거나 다른 버퍼(되돌리기 등)면 새로 올리고, 아니면 바뀐 영역만 올린다 */
void view_upload_texture(App *a) {
    const TexBuf *t = a->scene.tex;
    if (!IsTextureValid(a->gpu_tex) || a->gpu_w != t->w || a->gpu_h != t->h) {
        if (IsTextureValid(a->gpu_tex)) UnloadTexture(a->gpu_tex);
        Image img = { (void *)t->px, t->w, t->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        a->gpu_tex = LoadTextureFromImage(img);
        SetTextureFilter(a->gpu_tex, TEXTURE_FILTER_POINT);
        a->gpu_w = t->w;
        a->gpu_h = t->h;
    } else if (a->tex_dirty || a->gpu_src != t) {
        UpdateTexture(a->gpu_tex, t->px);
    } else if (a->tex_rect_dirty) {
        int x0 = a->dx0 < 0 ? 0 : a->dx0, y0 = a->dy0 < 0 ? 0 : a->dy0;
        int x1 = a->dx1 >= t->w ? t->w - 1 : a->dx1, y1 = a->dy1 >= t->h ? t->h - 1 : a->dy1;
        if (x1 >= x0 && y1 >= y0) {
            int w = x1 - x0 + 1, h = y1 - y0 + 1;
            Color *tmp = malloc(sizeof(Color) * (size_t)w * (size_t)h);
            for (int y = 0; y < h; y++) memcpy(&tmp[y * w], &t->px[(y0 + y) * t->w + x0], sizeof(Color) * (size_t)w);
            UpdateTextureRec(a->gpu_tex, (Rectangle){ (float)x0, (float)y0, (float)w, (float)h }, tmp);
            free(tmp);
        }
    }
    a->gpu_src = t;
    a->tex_dirty = false;
    a->tex_rect_dirty = false;
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
    Rectangle area = { 0, TOPBAR_H, W, H - TOPBAR_H };
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
