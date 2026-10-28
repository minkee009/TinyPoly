#include "app.h"
#include "texture.h"
#include "ui.h"
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOOLBAR_H 30
#define PALBAR_H  60
#define SWATCH    22

typedef enum { P_NONE, P_STROKE, P_SHAPE, P_PAN, P_UV_MOVE, P_UV_VERT, P_UV_SCALE, P_SEL, P_SEL_MOVE } PaintDrag;

static PaintDrag g_drag;
static int g_x0, g_y0, g_lx, g_ly;           /* 시작/직전 텍셀 */
static Color g_draw_color;
static Vector2 g_press;
static bool g_moved, g_marked;
static Color *g_backup;                       /* 도형 미리보기/선택 이동용 원본 (텍스처 크기) */
static int g_backup_n;

/* 선택 영역(마키): 텍셀 좌표. w == 0이면 없음 */
static int g_sx, g_sy, g_sw, g_sh;
static bool g_floating;                      /* g_backup 위에 g_float를 띄워 둔 상태 */
static Color *g_float;
static int g_fw, g_fh, g_fx, g_fy, g_fx0, g_fy0;
static Color *g_clip;
static int g_cw, g_ch;

/* UV 드래그: 드래그 시작 시 활성 메쉬 모든 면의 UV */
static Vector2 (*g_uv0)[MAX_FACE_VERTS];
static int g_uv_face, g_uv_vert;

static const float ZOOMS[] = { 0.125f, 0.25f, 0.5f, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
#define NZOOM ((int)(sizeof ZOOMS / sizeof ZOOMS[0]))

static bool key_shift(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool key_ctrl(void)  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

static int tex_w(const App *a) { return a->scene.tex->w; }
static int tex_h(const App *a) { return a->scene.tex->h; }

/* 고칠 수 있는 텍스처 (되돌리기 기록과 공유 중이면 이때 복사됨) */
static Canvas canvas(App *a) {
    return (Canvas){ scene_tex_mut(&a->scene), tex_w(a), tex_h(a) };
}

static void ensure_backup(App *a) {
    int n = tex_w(a) * tex_h(a);
    if (g_backup_n != n) {
        free(g_backup);
        g_backup = malloc(sizeof(Color) * (size_t)n);
        g_backup_n = n;
    }
}

/* ---- 캔버스 좌표 ---- */

static Rectangle area_rect(const App *a) {
    Rectangle c = a->canvas_rect;
    return (Rectangle){ c.x, c.y + TOOLBAR_H, c.width, c.height - TOOLBAR_H - PALBAR_H };
}

/* 창에 맞춤: 확대 단계 중 화면에 다 들어가는 가장 큰 값 */
static float fit_scale(const App *a) {
    Rectangle r = area_rect(a);
    float raw = fminf((r.width - 16) / tex_w(a), (r.height - 16) / tex_h(a));
    float best = ZOOMS[0];
    for (int i = 0; i < NZOOM; i++)
        if (ZOOMS[i] <= raw) best = ZOOMS[i];
    return best;
}

static float tex_scale(const App *a) {
    return a->tex_zoom > 0 ? a->tex_zoom : fit_scale(a);
}

static Vector2 tex_origin(const App *a) {
    Rectangle r = area_rect(a);
    float sc = tex_scale(a);
    return (Vector2){ floorf(r.x + r.width / 2 - tex_w(a) * sc / 2 + a->tex_pan.x),
                      floorf(r.y + r.height / 2 - tex_h(a) * sc / 2 + a->tex_pan.y) };
}

static Vector2 screen_to_tex(const App *a, Vector2 m) {
    Vector2 o = tex_origin(a);
    float sc = tex_scale(a);
    return (Vector2){ (m.x - o.x) / sc, (m.y - o.y) / sc };
}

static Vector2 tex_to_screen(const App *a, Vector2 t) {
    Vector2 o = tex_origin(a);
    float sc = tex_scale(a);
    return (Vector2){ o.x + t.x * sc, o.y + t.y * sc };
}

static bool texel_at(const App *a, int x, int y, Color *out) {
    if (x < 0 || y < 0 || x >= tex_w(a) || y >= tex_h(a)) return false;
    *out = a->scene.tex->px[y * tex_w(a) + x];
    return true;
}

/* 확대: 마우스 아래 텍셀이 그 자리에 머물도록 이동량을 맞춤 */
static void zoom_to(App *a, float z, Vector2 anchor) {
    Vector2 t = screen_to_tex(a, anchor);
    a->tex_zoom = z;
    Vector2 p = tex_to_screen(a, t);
    a->tex_pan = Vector2Add(a->tex_pan, Vector2Subtract(anchor, p));
}

static void zoom_step(App *a, int dir, Vector2 anchor) {
    float cur = tex_scale(a);
    int i = 0;
    while (i < NZOOM - 1 && ZOOMS[i] < cur) i++;
    if (dir > 0 && ZOOMS[i] <= cur && i < NZOOM - 1) i++;
    if (dir < 0 && i > 0) i--;
    zoom_to(a, ZOOMS[i], anchor);
}

static void zoom_fit(App *a) {
    a->tex_zoom = 0;
    a->tex_pan = (Vector2){ 0 };
}

/* ---- 마키 ---- */

static void stamp_float(App *a) {
    Canvas c = canvas(a);
    memcpy(c.px, g_backup, sizeof(Color) * (size_t)(c.w * c.h));
    for (int y = 0; y < g_fh; y++)
        for (int x = 0; x < g_fw; x++) tex_plot(c, g_fx + x, g_fy + y, g_float[y * g_fw + x]);
    g_sx = g_fx; g_sy = g_fy; g_sw = g_fw; g_sh = g_fh;
    a->tex_dirty = true;
}

static void commit_float(void) {
    g_floating = false;
}

static Color *copy_region(const App *a, int sx, int sy, int w, int h) {
    Color *dst = malloc(sizeof(Color) * (size_t)w * (size_t)h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            Color c = BLANK;
            texel_at(a, sx + x, sy + y, &c);
            dst[y * w + x] = c;
        }
    return dst;
}

static void lift_selection(App *a) {
    app_mark(a);
    g_fw = g_sw; g_fh = g_sh; g_fx = g_sx; g_fy = g_sy;
    free(g_float);
    g_float = copy_region(a, g_sx, g_sy, g_sw, g_sh);
    ensure_backup(a);
    memcpy(g_backup, a->scene.tex->px, sizeof(Color) * (size_t)g_backup_n);
    tex_rect((Canvas){ g_backup, tex_w(a), tex_h(a) }, g_sx, g_sy, g_sx + g_sw - 1, g_sy + g_sh - 1, BLANK, true);
    g_floating = true;
}

static void sel_copy(App *a) {
    if (g_sw <= 0) return;
    g_cw = g_sw; g_ch = g_sh;
    free(g_clip);
    g_clip = copy_region(a, g_sx, g_sy, g_sw, g_sh);
    app_msg(a, "copied %dx%d pixels", g_cw, g_ch);
}

static void sel_paste(App *a) {
    if (g_cw <= 0) return;
    commit_float();
    app_mark(a);
    ensure_backup(a);
    memcpy(g_backup, a->scene.tex->px, sizeof(Color) * (size_t)g_backup_n);
    free(g_float);
    g_float = malloc(sizeof(Color) * (size_t)g_cw * (size_t)g_ch);
    memcpy(g_float, g_clip, sizeof(Color) * (size_t)g_cw * (size_t)g_ch);
    g_fw = g_cw; g_fh = g_ch;
    g_fx = g_sw > 0 ? g_sx : 0;
    g_fy = g_sw > 0 ? g_sy : 0;
    g_floating = true;
    stamp_float(a);
    a->tool = TOOL_MARQUEE;
}

static void sel_delete(App *a) {
    if (g_sw <= 0) return;
    if (g_floating) {
        memcpy(canvas(a).px, g_backup, sizeof(Color) * (size_t)g_backup_n);
        commit_float();
    } else {
        app_mark(a);
        tex_rect(canvas(a), g_sx, g_sy, g_sx + g_sw - 1, g_sy + g_sh - 1, BLANK, true);
    }
    g_sw = 0;
    a->tex_dirty = true;
}

/* 텍스처 크기가 바뀌면 선택/떠 있는 조각은 버림 */
void paint_reset_selection(void) {
    g_floating = false;
    g_sw = 0;
    g_drag = P_NONE;
}

/* ---- UV ---- */

static bool point_in_uv(const Face *f, Vector2 p) {
    for (int j = 1; j + 1 < f->n; j++)
        if (CheckCollisionPointTriangle(p, f->uv[0], f->uv[j], f->uv[j + 1])) return true;
    return false;
}

static bool uv_bounds(Object *o, Vector2 *mn, Vector2 *mx) {
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

static Object *uv_target(App *a) {
    Object *o = app_active(a);
    if (!o) return NULL;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) return o;
    return NULL;
}

void uv_rotate(App *a) {
    Object *o = uv_target(a);
    Vector2 mn, mx;
    if (!o || !uv_bounds(o, &mn, &mx)) return;
    app_mark(a);
    Vector2 c = { floorf((mn.x + mx.x) / 2), floorf((mn.y + mx.y) / 2) };
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) {
                Vector2 d = Vector2Subtract(o->faces[i].uv[k], c);
                o->faces[i].uv[k] = (Vector2){ c.x - d.y, c.y + d.x };
            }
}

void uv_flip(App *a, bool vertical) {
    Object *o = uv_target(a);
    Vector2 mn, mx;
    if (!o || !uv_bounds(o, &mn, &mx)) return;
    app_mark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) {
                Vector2 *p = &o->faces[i].uv[k];
                if (vertical) p->y = mn.y + mx.y - p->y;
                else p->x = mn.x + mx.x - p->x;
            }
}

void uv_reset(App *a) {
    Object *o = uv_target(a);
    if (!o) return;
    app_mark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) obj_project_uv(o, &o->faces[i], scene_uv_scale(&a->scene));
}

void uv_copy(App *a) {
    Object *o = uv_target(a);
    if (!o) return;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) {
            a->uv_clip_n = o->faces[i].n;
            memcpy(a->uv_clip, o->faces[i].uv, sizeof(Vector2) * (size_t)a->uv_clip_n);
            app_msg(a, "copied uv");
            return;
        }
}

void uv_paste(App *a) {
    Object *o = uv_target(a);
    if (!o || !a->uv_clip_n) return;
    app_mark(a);
    int n = 0;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel && o->faces[i].n == a->uv_clip_n) {
            memcpy(o->faces[i].uv, a->uv_clip, sizeof(Vector2) * (size_t)a->uv_clip_n);
            n++;
        }
    if (!n) app_msg(a, "uv paste: face has a different vertex count");
}

static void uv_nudge(App *a, Vector2 d) {
    Object *o = uv_target(a);
    if (!o) return;
    app_mark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel)
            for (int k = 0; k < o->faces[i].n; k++) o->faces[i].uv[k] = Vector2Add(o->faces[i].uv[k], d);
}

static void uv_begin_drag(App *a, PaintDrag kind) {
    Object *o = app_active(a);
    free(g_uv0);
    g_uv0 = malloc(sizeof *g_uv0 * (size_t)(o->nfaces + 1));
    for (int i = 0; i < o->nfaces; i++) memcpy(g_uv0[i], o->faces[i].uv, sizeof g_uv0[i]);
    g_drag = kind;
    g_marked = false;
}

static void uv_apply_drag(App *a, Vector2 m) {
    Object *o = app_active(a);
    if (!o || !g_moved) return;
    for (int i = 0; i < o->nfaces; i++) memcpy(o->faces[i].uv, g_uv0[i], sizeof g_uv0[i]);
    if (!g_marked) { app_mark(a); g_marked = true; }

    float sc = tex_scale(a);
    Vector2 d = { (m.x - g_press.x) / sc, (m.y - g_press.y) / sc };
    if (!key_ctrl()) d = (Vector2){ roundf(d.x), roundf(d.y) };

    if (g_drag == P_UV_VERT) {
        Vector2 *p = &o->faces[g_uv_face].uv[g_uv_vert];
        *p = Vector2Add(*p, d);
    } else if (g_drag == P_UV_SCALE) {
        Vector2 mn, mx;
        uv_bounds(o, &mn, &mx);
        Vector2 c = { (mn.x + mx.x) / 2, (mn.y + mx.y) / 2 };
        float f = fmaxf(0.0f, 1.0f + (m.x - g_press.x - (m.y - g_press.y)) * 0.01f);
        for (int i = 0; i < o->nfaces; i++)
            if (o->faces[i].sel)
                for (int k = 0; k < o->faces[i].n; k++) {
                    Vector2 p = Vector2Add(c, Vector2Scale(Vector2Subtract(o->faces[i].uv[k], c), f));
                    o->faces[i].uv[k] = key_ctrl() ? p : (Vector2){ roundf(p.x), roundf(p.y) };
                }
    } else {
        for (int i = 0; i < o->nfaces; i++)
            if (o->faces[i].sel)
                for (int k = 0; k < o->faces[i].n; k++) o->faces[i].uv[k] = Vector2Add(o->faces[i].uv[k], d);
    }
}

static void uv_press(App *a, Vector2 m) {
    Object *o = app_active(a);
    if (!o) return;
    Vector2 t = screen_to_tex(a, m);

    if (IsKeyDown(KEY_T) && uv_target(a)) {
        uv_begin_drag(a, P_UV_SCALE);
        return;
    }
    /* 선택된 면의 UV 꼭짓점 → 그 점만 */
    for (int i = 0; i < o->nfaces; i++) {
        if (!o->faces[i].sel) continue;
        for (int k = 0; k < o->faces[i].n; k++)
            if (Vector2Distance(tex_to_screen(a, o->faces[i].uv[k]), m) < 6.0f) {
                g_uv_face = i;
                g_uv_vert = k;
                uv_begin_drag(a, P_UV_VERT);
                return;
            }
    }
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel && point_in_uv(&o->faces[i], t)) {
            if (key_shift()) { o->faces[i].sel = false; return; }
            uv_begin_drag(a, P_UV_MOVE);
            return;
        }
    /* 선택 안 된 면의 UV를 누르면 그 면을 선택 */
    for (int i = 0; i < o->nfaces; i++)
        if (point_in_uv(&o->faces[i], t)) {
            if (!key_shift()) obj_clear_selection(o);
            o->faces[i].sel = true;
            obj_select_face_verts(o);
            uv_begin_drag(a, P_UV_MOVE);
            return;
        }
    if (!key_shift()) obj_clear_selection(o);
}

/* ---- 그리기 도구 ---- */

static void draw_shape(App *a, int x1, int y1) {
    Canvas c = canvas(a);
    memcpy(c.px, g_backup, sizeof(Color) * (size_t)g_backup_n);
    if (a->tool == TOOL_LINE)   tex_line(c, g_x0, g_y0, x1, y1, g_draw_color);
    if (a->tool == TOOL_RECT)   tex_rect(c, g_x0, g_y0, x1, y1, g_draw_color, a->rect_filled);
    if (a->tool == TOOL_CIRCLE) tex_ellipse(c, g_x0, g_y0, x1, y1, g_draw_color, a->circle_filled);
    a->tex_dirty = true;
}

static void set_tool(App *a, int tool) {
    if (tool != TOOL_MARQUEE) { commit_float(); g_sw = 0; }
    /* 사각형/원 버튼을 다시 누르면 채우기 전환 */
    if (tool == a->tool && tool == TOOL_RECT) a->rect_filled = !a->rect_filled;
    if (tool == a->tool && tool == TOOL_CIRCLE) a->circle_filled = !a->circle_filled;
    a->tool = tool;
}

static void canvas_press(App *a, Vector2 m, bool right) {
    Vector2 t = screen_to_tex(a, m);
    int x = (int)floorf(t.x), y = (int)floorf(t.y);
    g_x0 = g_lx = x;
    g_y0 = g_ly = y;
    g_draw_color = right ? a->color2 : a->color;
    Color under;
    bool inside = texel_at(a, x, y, &under);

    if (key_ctrl() || a->tool == TOOL_PICK) {
        if (inside) { if (right) a->color2 = under; else a->color = under; }
        return;
    }
    switch (a->tool) {
    case TOOL_UV:
        if (!right) uv_press(a, m);
        break;
    case TOOL_PEN:
        app_mark(a);
        tex_plot(canvas(a), x, y, g_draw_color);
        view_tex_dirty(a, x, y, x, y);
        g_drag = P_STROKE;
        break;
    case TOOL_FILL:
        if (!inside) break;
        app_mark(a);
        tex_fill(canvas(a), x, y, g_draw_color);
        a->tex_dirty = true;
        break;
    case TOOL_LINE: case TOOL_RECT: case TOOL_CIRCLE:
        app_mark(a);
        ensure_backup(a);
        memcpy(g_backup, a->scene.tex->px, sizeof(Color) * (size_t)g_backup_n);
        draw_shape(a, x, y);
        g_drag = P_SHAPE;
        break;
    case TOOL_MARQUEE:
        if (g_sw > 0 && x >= g_sx && y >= g_sy && x < g_sx + g_sw && y < g_sy + g_sh) {
            if (!g_floating) lift_selection(a);
            g_fx0 = g_fx;
            g_fy0 = g_fy;
            g_drag = P_SEL_MOVE;
        } else {
            commit_float();
            g_sw = 0;
            g_drag = P_SEL;
        }
        break;
    }
}

static void canvas_drag(App *a, Vector2 m) {
    Vector2 t = screen_to_tex(a, m);
    int x = (int)floorf(t.x), y = (int)floorf(t.y);
    switch (g_drag) {
    case P_STROKE:
        if (key_shift()) {
            /* 시작점 기준 축 고정 */
            if (abs(x - g_x0) >= abs(y - g_y0)) y = g_y0; else x = g_x0;
        }
        tex_line(canvas(a), g_lx, g_ly, x, y, g_draw_color);
        view_tex_dirty(a, g_lx < x ? g_lx : x, g_ly < y ? g_ly : y, g_lx > x ? g_lx : x, g_ly > y ? g_ly : y);
        g_lx = x;
        g_ly = y;
        break;
    case P_SHAPE:
        draw_shape(a, x, y);
        break;
    case P_SEL: {
        int x0 = (int)Clamp((float)g_x0, 0, tex_w(a) - 1), y0 = (int)Clamp((float)g_y0, 0, tex_h(a) - 1);
        int x1 = (int)Clamp((float)x, 0, tex_w(a) - 1), y1 = (int)Clamp((float)y, 0, tex_h(a) - 1);
        g_sx = x0 < x1 ? x0 : x1;
        g_sy = y0 < y1 ? y0 : y1;
        g_sw = abs(x1 - x0) + 1;
        g_sh = abs(y1 - y0) + 1;
        break;
    }
    case P_SEL_MOVE:
        g_fx = g_fx0 + (x - g_x0);
        g_fy = g_fy0 + (y - g_y0);
        stamp_float(a);
        break;
    case P_UV_MOVE: case P_UV_VERT: case P_UV_SCALE:
        uv_apply_drag(a, m);
        break;
    case P_PAN:
        a->tex_pan = Vector2Add(a->tex_pan, GetMouseDelta());
        break;
    default:
        break;
    }
}

static bool any_button_down(void) {
    return IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
           IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
}

static void update_keys(App *a) {
    bool ctrl = key_ctrl();
    if (ctrl) {
        if (IsKeyPressed(KEY_C)) { if (a->tool == TOOL_UV) uv_copy(a); else sel_copy(a); }
        if (IsKeyPressed(KEY_V)) { if (a->tool == TOOL_UV) uv_paste(a); else sel_paste(a); }
        return;
    }
    if (IsKeyPressed(KEY_V)) set_tool(a, TOOL_UV);
    if (IsKeyPressed(KEY_B)) set_tool(a, TOOL_PEN);
    if (IsKeyPressed(KEY_I)) set_tool(a, TOOL_PICK);
    if (IsKeyPressed(KEY_G)) set_tool(a, TOOL_FILL);
    Rectangle ar = area_rect(a);
    Vector2 center = { ar.x + ar.width / 2, ar.y + ar.height / 2 };
    if (IsKeyPressed(KEY_ONE))   zoom_fit(a);
    if (IsKeyPressed(KEY_TWO))   zoom_to(a, 1, center);
    if (IsKeyPressed(KEY_THREE)) zoom_to(a, 2, center);
    if (IsKeyPressed(KEY_FOUR))  zoom_to(a, 4, center);
    if (IsKeyPressed(KEY_X)) { Color t = a->color; a->color = a->color2; a->color2 = t; }
    if (IsKeyPressed(KEY_R)) uv_rotate(a);
    if (IsKeyPressed(KEY_H)) uv_flip(a, key_shift());
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) sel_delete(a);
    if (IsKeyPressed(KEY_ESCAPE)) { commit_float(); g_sw = 0; }

    int dx = (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D)) -
             (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A));
    int dy = (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN) || IsKeyPressed(KEY_S)) -
             (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP) || IsKeyPressed(KEY_W));
    if (dx || dy) uv_nudge(a, (Vector2){ (float)dx, (float)dy });
}

/* ---- 커서 ---- */

bool paint_hover_texel(const App *a, Color *out) {
    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, area_rect(a))) return false;
    Vector2 t = screen_to_tex(a, m);
    return texel_at(a, (int)floorf(t.x), (int)floorf(t.y), out);
}

int paint_cursor(App *a) {
    /* 드래그 중이면 그 동작에 맞춰 */
    switch (g_drag) {
    case P_PAN:      return CURSOR_GRAB;
    case P_STROKE:   return CURSOR_PEN;
    case P_SHAPE:
    case P_SEL:      return CURSOR_CROSS;
    case P_UV_SCALE: return CURSOR_SCALE;
    case P_UV_MOVE:
    case P_UV_VERT:
    case P_SEL_MOVE: return CURSOR_MOVE;
    default: break;
    }

    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, area_rect(a))) return -1;
    if (IsKeyDown(KEY_SPACE)) return CURSOR_HAND;
    if (key_ctrl()) return CURSOR_PICKER;

    Vector2 t = screen_to_tex(a, m);
    switch (a->tool) {
    case TOOL_PEN:  return CURSOR_PEN;
    case TOOL_PICK: return CURSOR_PICKER;
    case TOOL_FILL: return CURSOR_BUCKET;
    case TOOL_MARQUEE: {
        int x = (int)floorf(t.x), y = (int)floorf(t.y);
        bool inside = g_sw > 0 && x >= g_sx && y >= g_sy && x < g_sx + g_sw && y < g_sy + g_sh;
        return inside ? CURSOR_MOVE : CURSOR_CROSS;
    }
    case TOOL_UV: {
        Object *o = app_active(a);
        if (!o) return CURSOR_ARROW;
        if (IsKeyDown(KEY_T) && uv_target(a)) return CURSOR_SCALE;
        /* UV 위면 끌 수 있다는 표시 */
        for (int i = 0; i < o->nfaces; i++)
            if (!(o->faces[i].flags & FACE_NOTEX) && point_in_uv(&o->faces[i], t)) return CURSOR_MOVE;
        return CURSOR_ARROW;
    }
    default:
        return CURSOR_CROSS;
    }
}

/* ---- 팔레트 줄 ---- */

static Rectangle palbar_rect(const App *a) {
    Rectangle c = a->canvas_rect;
    return (Rectangle){ c.x, c.y + c.height - PALBAR_H, c.width, PALBAR_H };
}

/* 팔레트 칸 배치: [투명][팔레트 0..n-1][+] 를 두 줄로, 넘치면 pal_scroll 줄부터 */
static int pal_cols(const App *a) {
    Rectangle r = palbar_rect(a);
    int cols = (int)((r.width - 80 - 110) / SWATCH);
    return cols < 4 ? 4 : cols;
}

static Rectangle pal_cell(const App *a, int slot) {
    Rectangle r = palbar_rect(a);
    int cols = pal_cols(a);
    int row = slot / cols - a->pal_scroll, col = slot % cols;
    return (Rectangle){ r.x + 80 + col * SWATCH, r.y + 6 + row * SWATCH, SWATCH - 2, SWATCH - 2 };
}

static void update_palette_bar(App *a) {
    Rectangle r = palbar_rect(a);
    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, r) || a->ui_hover) return;

    int cols = pal_cols(a), slots = a->scene.npalette + 2;
    int rows = (slots + cols - 1) / cols;
    float wheel = GetMouseWheelMove();
    if (wheel != 0) a->pal_scroll = (int)Clamp((float)a->pal_scroll - (wheel > 0 ? 1 : -1), 0, (float)(rows > 2 ? rows - 2 : 0));

    /* 왼쪽 큰 칸 두 개: 누르면 색 선택 창 */
    Rectangle c1 = { r.x + 8, r.y + 8, 36, 36 }, c2 = { r.x + 30, r.y + 22, 36, 36 };
    bool l = IsMouseButtonPressed(MOUSE_BUTTON_LEFT), rt = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (l && CheckCollisionPointRec(m, c1)) { a->pick_target = 0; a->show_color = 1; return; }
    if (l && CheckCollisionPointRec(m, c2)) { a->pick_target = 1; a->show_color = 1; return; }

    for (int s = 0; s < slots; s++) {
        Rectangle cell = pal_cell(a, s);
        if (cell.y < r.y || cell.y + cell.height > r.y + r.height || !CheckCollisionPointRec(m, cell)) continue;
        if (s == slots - 1) {
            /* + : 지금 색을 팔레트에 추가 */
            if (l && a->scene.npalette < MAX_PALETTE) {
                app_mark(a);
                a->scene.palette[a->scene.npalette++] = a->color;
                a->pal_sel = a->scene.npalette - 1;
            }
            return;
        }
        Color c = s == 0 ? BLANK : a->scene.palette[s - 1];
        if (l) { a->color = c; a->pal_sel = s - 1; }
        if (rt) a->color2 = c;
        return;
    }
}

static void draw_checker(Rectangle r, int block) {
    for (int y = 0; y < (int)r.height; y += block)
        for (int x = 0; x < (int)r.width; x += block) {
            int w = (int)r.width - x < block ? (int)r.width - x : block;
            int h = (int)r.height - y < block ? (int)r.height - y : block;
            DrawRectangle((int)r.x + x, (int)r.y + y, w, h,
                          ((x + y) / block) & 1 ? (Color){ 150, 150, 160, 255 } : (Color){ 210, 210, 216, 255 });
        }
}

/* 투명이면 체크무늬 위에 색 */
static void draw_swatch(Rectangle r, Color c) {
    if (c.a < 255) draw_checker(r, 5);
    DrawRectangleRec(r, c);
}

static void draw_palette_bar(App *a) {
    Rectangle r = palbar_rect(a);
    DrawRectangleRec(r, (Color){ 30, 28, 40, 255 });

    /* 지금 색 두 개 (뒤가 color2) */
    Rectangle c1 = { r.x + 8, r.y + 8, 36, 36 }, c2 = { r.x + 30, r.y + 22, 36, 36 };
    draw_swatch(c2, a->color2);
    DrawRectangleLinesEx(c2, 2, (Color){ 20, 18, 28, 255 });
    draw_swatch(c1, a->color);
    DrawRectangleLinesEx(c1, 2, WHITE);

    BeginScissorMode((int)r.x, (int)r.y + 4, (int)r.width, (int)r.height - 8);
    int slots = a->scene.npalette + 2;
    for (int s = 0; s < slots; s++) {
        Rectangle cell = pal_cell(a, s);
        if (s == slots - 1) {
            DrawRectangleLinesEx(cell, 1, (Color){ 120, 116, 150, 255 });
            DrawTextEx(GetFontDefault(), "+", (Vector2){ cell.x + 5, cell.y }, 20, 2, WHITE);
            continue;
        }
        Color c = s == 0 ? BLANK : a->scene.palette[s - 1];
        draw_swatch(cell, c);
        if (s - 1 == a->pal_sel && s > 0) DrawRectangleLinesEx(cell, 2, WHITE);
    }
    EndScissorMode();

    if (ui_button_rect((Rectangle){ r.x + r.width - 104, r.y + 14, 96, 32 }, "color (P)", a->show_color, a->ui_hover))
        a->show_color = !a->show_color;
}

void paint_update(App *a, bool input_ok) {
    Vector2 m = GetMousePosition();
    bool over = CheckCollisionPointRec(m, area_rect(a)) && !a->ui_hover;

    if (g_drag == P_NONE && over && input_ok) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) zoom_step(a, wheel > 0 ? 1 : -1, m);
        g_press = m;
        g_moved = false;
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) ||
            (IsKeyDown(KEY_SPACE) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            g_drag = P_PAN;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            canvas_press(a, m, false);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (a->tool == TOOL_UV && uv_target(a)) app_open_menu(a, MENU_UV);
            else canvas_press(a, m, true);
        }
    }
    if (g_drag != P_NONE) {
        if (Vector2Distance(m, g_press) > 2.0f) g_moved = true;
        canvas_drag(a, m);
        if (!any_button_down()) g_drag = P_NONE;
    }
    if (input_ok && g_drag == P_NONE) update_palette_bar(a);

    /* 3D 뷰: 면 선택/회전 (모델 단축키는 제외) */
    model_update(a, input_ok && g_drag == P_NONE, false);
    if (input_ok && g_drag == P_NONE) update_keys(a);
}

/* ---- 화면 ---- */

static void draw_toolbar(App *a) {
    Rectangle c = a->canvas_rect;
    DrawRectangle((int)c.x, (int)c.y, (int)c.width, TOOLBAR_H, (Color){ 30, 28, 40, 255 });
    static const char *labels[TOOL_COUNT] = { "uv", "pen", "pick", "fill", "line", "rect", "circ", "sel" };
    float x = c.x + 4, y = c.y + 3, h = TOOLBAR_H - 6;
    for (int i = 0; i < TOOL_COUNT; i++) {
        const char *l = labels[i];
        if (i == TOOL_RECT && a->rect_filled) l = "RECT";
        if (i == TOOL_CIRCLE && a->circle_filled) l = "CIRC";
        float w = MeasureTextEx(GetFontDefault(), l, 20, 2).x + 12;
        if (ui_button_rect((Rectangle){ x, y, w, h }, l, a->tool == i, a->ui_hover)) set_tool(a, i);
        x += w + 3;
    }
    x += 10;
    if (ui_button_rect((Rectangle){ x, y, 50, h }, "grid", a->tex_grid, a->ui_hover)) a->tex_grid = !a->tex_grid;
    x += 60;

    /* 텍스처 크기 (누르면 크기 메뉴) */
    char size[24];
    snprintf(size, sizeof size, "%dx%d", tex_w(a), tex_h(a));
    float sw = MeasureTextEx(GetFontDefault(), size, 20, 2).x + 12;
    if (ui_button_rect((Rectangle){ x, y, sw, h }, size, a->menu == MENU_TEXSIZE, a->ui_hover))
        app_open_menu(a, MENU_TEXSIZE);
    x += sw + 10;

    Rectangle ar = area_rect(a);
    Vector2 center = { ar.x + ar.width / 2, ar.y + ar.height / 2 };
    if (ui_button_rect((Rectangle){ x, y, 24, h }, "-", false, a->ui_hover)) zoom_step(a, -1, center);
    x += 27;
    if (ui_button_rect((Rectangle){ x, y, 24, h }, "+", false, a->ui_hover)) zoom_step(a, 1, center);
    x += 27;
    if (ui_button_rect((Rectangle){ x, y, 40, h }, "fit", a->tex_zoom == 0, a->ui_hover)) zoom_fit(a);
    x += 46;
    char pct[16];
    snprintf(pct, sizeof pct, "%d%%", (int)lroundf(tex_scale(a) * 100));
    DrawTextEx(GetFontDefault(), pct, (Vector2){ x, y + 2 }, 20, 2, (Color){ 200, 196, 210, 255 });
}

void paint_draw(App *a) {
    Rectangle ar = area_rect(a);
    DrawRectangleRec(ar, (Color){ 22, 20, 30, 255 });
    BeginScissorMode((int)ar.x, (int)ar.y, (int)ar.width, (int)ar.height);

    Vector2 o = tex_origin(a);
    float sc = tex_scale(a);
    int tw = tex_w(a), th = tex_h(a);

    /* 투명 부분이 보이도록 체크무늬: 화면에 보이는 부분만, 화면 기준 칸 크기 */
    Rectangle tr = { o.x, o.y, tw * sc, th * sc };
    Rectangle vis = GetCollisionRec(tr, ar);
    if (vis.width > 0 && vis.height > 0) draw_checker(vis, 12);
    DrawTexturePro(a->gpu_tex, (Rectangle){ 0, 0, (float)tw, (float)th }, tr, (Vector2){ 0 }, 0, WHITE);

    if (a->tex_grid) {
        /* 칸이 화면에서 너무 촘촘하지 않은 간격 (8, 16, 32, ...) */
        int step = 8;
        while (step * sc < 16 && step < 1024) step *= 2;
        Color gc = { 0, 0, 0, 70 };
        for (int i = 0; i <= tw; i += step)
            DrawLineV(tex_to_screen(a, (Vector2){ (float)i, 0 }), tex_to_screen(a, (Vector2){ (float)i, (float)th }), gc);
        for (int i = 0; i <= th; i += step)
            DrawLineV(tex_to_screen(a, (Vector2){ 0, (float)i }), tex_to_screen(a, (Vector2){ (float)tw, (float)i }), gc);
    }
    DrawRectangleLinesEx((Rectangle){ tr.x - 1, tr.y - 1, tr.width + 2, tr.height + 2 }, 1, (Color){ 90, 86, 110, 255 });

    /* UV: 활성 메쉬의 다른 면은 흐리게, 선택된 면은 진하게 + 꼭짓점 */
    Object *ob = app_active(a);
    if (ob) {
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < ob->nfaces; i++) {
                const Face *f = &ob->faces[i];
                if (f->sel != (pass == 1) || (f->flags & FACE_NOTEX)) continue;
                Color c = pass ? (Color){ 255, 200, 60, 255 } : (Color){ 255, 255, 255, 90 };
                for (int k = 0; k < f->n; k++)
                    DrawLineEx(tex_to_screen(a, f->uv[k]), tex_to_screen(a, f->uv[(k + 1) % f->n]), pass ? 2.0f : 1.0f, c);
                if (pass && a->tool == TOOL_UV)
                    for (int k = 0; k < f->n; k++) {
                        Vector2 p = tex_to_screen(a, f->uv[k]);
                        DrawRectangle((int)p.x - 3, (int)p.y - 3, 6, 6, c);
                    }
            }
    }

    if (g_sw > 0) {
        Vector2 p = tex_to_screen(a, (Vector2){ (float)g_sx, (float)g_sy });
        Rectangle r = { p.x, p.y, g_sw * sc, g_sh * sc };
        bool phase = fmod(GetTime(), 0.5) < 0.25;
        DrawRectangleLinesEx(r, 1, phase ? WHITE : BLACK);
    }

    /* 커서 텍셀 (충분히 확대됐을 때만) */
    Vector2 m = GetMousePosition();
    if (sc >= 4 && CheckCollisionPointRec(m, ar) && a->tool != TOOL_UV && a->tool != TOOL_MARQUEE) {
        Vector2 t = screen_to_tex(a, m);
        Vector2 p = tex_to_screen(a, (Vector2){ floorf(t.x), floorf(t.y) });
        DrawRectangleLinesEx((Rectangle){ p.x, p.y, sc, sc }, 1, WHITE);
    }
    EndScissorMode();

    draw_toolbar(a);
    draw_palette_bar(a);
}
