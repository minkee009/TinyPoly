#include "app.h"
#include "texture.h"
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOOLBAR_H 30
#define PALBAR_H  52

typedef enum { P_NONE, P_STROKE, P_SHAPE, P_PAN, P_UV_MOVE, P_UV_VERT, P_UV_SCALE, P_SEL, P_SEL_MOVE } PaintDrag;

static PaintDrag g_drag;
static int g_x0, g_y0, g_lx, g_ly;           /* 시작/직전 텍셀 */
static int g_draw_color;
static Vector2 g_press;
static bool g_moved, g_marked;
static unsigned char g_backup[TEX_W * TEX_H];

/* 선택 영역(마키): 텍셀 좌표. w == 0이면 없음 */
static int g_sx, g_sy, g_sw, g_sh;
static bool g_floating;                      /* g_backup 위에 g_float를 띄워 둔 상태 */
static unsigned char g_float[TEX_W * TEX_H];
static int g_fw, g_fh, g_fx, g_fy, g_fx0, g_fy0;
static unsigned char g_clip[TEX_W * TEX_H];
static int g_cw, g_ch;

/* UV 드래그: 드래그 시작 시 활성 메쉬 모든 면의 UV */
static Vector2 (*g_uv0)[MAX_FACE_VERTS];
static int g_uv_face, g_uv_vert;

static bool key_shift(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool key_ctrl(void)  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

/* ---- 캔버스 좌표 ---- */

static Rectangle area_rect(const App *a) {
    Rectangle c = a->canvas_rect;
    return (Rectangle){ c.x, c.y + TOOLBAR_H, c.width, c.height - TOOLBAR_H - PALBAR_H };
}

static float tex_scale(const App *a) {
    return (float)(a->tex_zoom * 2);
}

static Vector2 tex_origin(const App *a) {
    Rectangle r = area_rect(a);
    float sc = tex_scale(a);
    return (Vector2){ floorf(r.x + r.width / 2 - TEX_W * sc / 2 + a->tex_pan.x),
                      floorf(r.y + r.height / 2 - TEX_H * sc / 2 + a->tex_pan.y) };
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

static int texel_at(const App *a, int x, int y) {
    if (x < 0 || y < 0 || x >= TEX_W || y >= TEX_H) return -1;
    return a->scene.tex[y * TEX_W + x];
}

/* ---- 마키 ---- */

static void stamp_float(App *a) {
    memcpy(a->scene.tex, g_backup, sizeof g_backup);
    for (int y = 0; y < g_fh; y++)
        for (int x = 0; x < g_fw; x++) tex_plot(a->scene.tex, g_fx + x, g_fy + y, g_float[y * g_fw + x]);
    g_sx = g_fx; g_sy = g_fy; g_sw = g_fw; g_sh = g_fh;
    a->tex_dirty = true;
}

static void commit_float(void) {
    g_floating = false;
}

static void copy_region(const App *a, unsigned char *dst, int sx, int sy, int w, int h) {
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int v = texel_at(a, sx + x, sy + y);
            dst[y * w + x] = (unsigned char)(v < 0 ? a->scene.alpha : v);
        }
}

static void lift_selection(App *a) {
    app_mark(a);
    g_fw = g_sw; g_fh = g_sh; g_fx = g_sx; g_fy = g_sy;
    copy_region(a, g_float, g_sx, g_sy, g_sw, g_sh);
    memcpy(g_backup, a->scene.tex, sizeof g_backup);
    tex_rect(g_backup, g_sx, g_sy, g_sx + g_sw - 1, g_sy + g_sh - 1, a->scene.alpha, true);
    g_floating = true;
}

static void sel_copy(App *a) {
    if (g_sw <= 0) return;
    g_cw = g_sw; g_ch = g_sh;
    copy_region(a, g_clip, g_sx, g_sy, g_sw, g_sh);
    app_msg(a, "copied %dx%d pixels", g_cw, g_ch);
}

static void sel_paste(App *a) {
    if (g_cw <= 0) return;
    commit_float();
    app_mark(a);
    memcpy(g_backup, a->scene.tex, sizeof g_backup);
    memcpy(g_float, g_clip, (size_t)(g_cw * g_ch));
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
        memcpy(a->scene.tex, g_backup, sizeof g_backup);
        commit_float();
    } else {
        app_mark(a);
        tex_rect(a->scene.tex, g_sx, g_sy, g_sx + g_sw - 1, g_sy + g_sh - 1, a->scene.alpha, true);
    }
    g_sw = 0;
    a->tex_dirty = true;
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
        if (o->faces[i].sel) obj_project_uv(o, &o->faces[i]);
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
    memcpy(a->scene.tex, g_backup, sizeof g_backup);
    if (a->tool == TOOL_LINE)  tex_line(a->scene.tex, g_x0, g_y0, x1, y1, g_draw_color);
    if (a->tool == TOOL_RECT)  tex_rect(a->scene.tex, g_x0, g_y0, x1, y1, g_draw_color, a->rect_filled);
    if (a->tool == TOOL_CIRCLE) tex_ellipse(a->scene.tex, g_x0, g_y0, x1, y1, g_draw_color, a->circle_filled);
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

    if (key_ctrl() || a->tool == TOOL_PICK) {
        int v = texel_at(a, x, y);
        if (v >= 0) { if (right) a->color2 = v; else a->color = v; }
        return;
    }
    switch (a->tool) {
    case TOOL_UV:
        if (!right) uv_press(a, m);
        break;
    case TOOL_PEN:
        app_mark(a);
        tex_plot(a->scene.tex, x, y, g_draw_color);
        a->tex_dirty = true;
        g_drag = P_STROKE;
        break;
    case TOOL_FILL:
        if (texel_at(a, x, y) < 0) break;
        app_mark(a);
        tex_fill(a->scene.tex, x, y, g_draw_color);
        a->tex_dirty = true;
        break;
    case TOOL_LINE: case TOOL_RECT: case TOOL_CIRCLE:
        app_mark(a);
        memcpy(g_backup, a->scene.tex, sizeof g_backup);
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
        tex_line(a->scene.tex, g_lx, g_ly, x, y, g_draw_color);
        g_lx = x;
        g_ly = y;
        a->tex_dirty = true;
        break;
    case P_SHAPE:
        draw_shape(a, x, y);
        break;
    case P_SEL: {
        int x0 = (int)Clamp((float)g_x0, 0, TEX_W - 1), y0 = (int)Clamp((float)g_y0, 0, TEX_H - 1);
        int x1 = (int)Clamp((float)x, 0, TEX_W - 1), y1 = (int)Clamp((float)y, 0, TEX_H - 1);
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
    for (int i = 0; i < 4; i++)
        if (IsKeyPressed(KEY_ONE + i)) a->tex_zoom = i + 1;
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

int paint_hover_texel(const App *a) {
    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, area_rect(a))) return -1;
    Vector2 t = screen_to_tex(a, m);
    return texel_at(a, (int)floorf(t.x), (int)floorf(t.y));
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

void paint_update(App *a, bool input_ok) {
    Vector2 m = GetMousePosition();
    bool over = CheckCollisionPointRec(m, area_rect(a)) && !a->ui_hover;

    if (g_drag == P_NONE && over && input_ok) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) a->tex_zoom = (int)Clamp((float)a->tex_zoom + (wheel > 0 ? 1 : -1), 1, 4);
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

    /* 3D 뷰: 면 선택/회전 (모델 단축키는 제외) */
    model_update(a, input_ok && g_drag == P_NONE, false);
    if (input_ok && g_drag == P_NONE) update_keys(a);
}

/* ---- 화면 ---- */

static bool tb_button(App *a, Rectangle r, const char *label, bool on) {
    Vector2 m = GetMousePosition();
    bool hover = CheckCollisionPointRec(m, r) && !a->ui_hover;
    DrawRectangleRec(r, on ? (Color){ 120, 112, 160, 255 } : hover ? (Color){ 84, 80, 108, 255 } : (Color){ 52, 50, 68, 255 });
    Vector2 ts = MeasureTextEx(GetFontDefault(), label, 20, 2);
    DrawTextEx(GetFontDefault(), label, (Vector2){ r.x + (r.width - ts.x) / 2, r.y + (r.height - 20) / 2 }, 20, 2, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void draw_toolbar(App *a) {
    Rectangle c = a->canvas_rect;
    DrawRectangle((int)c.x, (int)c.y, (int)c.width, TOOLBAR_H, (Color){ 30, 28, 40, 255 });
    static const char *labels[TOOL_COUNT] = { "uv", "pen", "pick", "fill", "line", "rect", "circ", "sel" };
    float x = c.x + 4;
    for (int i = 0; i < TOOL_COUNT; i++) {
        const char *l = labels[i];
        if (i == TOOL_RECT && a->rect_filled) l = "RECT";
        if (i == TOOL_CIRCLE && a->circle_filled) l = "CIRC";
        float w = MeasureTextEx(GetFontDefault(), l, 20, 2).x + 12;
        if (tb_button(a, (Rectangle){ x, c.y + 3, w, TOOLBAR_H - 6 }, l, a->tool == i)) set_tool(a, i);
        x += w + 3;
    }
    x += 8;
    if (tb_button(a, (Rectangle){ x, c.y + 3, 50, TOOLBAR_H - 6 }, "grid", a->tex_grid)) a->tex_grid = !a->tex_grid;
    x += 58;
    for (int i = 1; i <= 4; i++) {
        char l[4];
        snprintf(l, sizeof l, "%d", i);
        if (tb_button(a, (Rectangle){ x, c.y + 3, 24, TOOLBAR_H - 6 }, l, a->tex_zoom == i)) a->tex_zoom = i;
        x += 27;
    }
}

static void draw_palette_bar(App *a) {
    Rectangle c = a->canvas_rect;
    float y = c.y + c.height - PALBAR_H;
    DrawRectangle((int)c.x, (int)y, (int)c.width, PALBAR_H, (Color){ 30, 28, 40, 255 });

    float sw = fminf(36.0f, (c.width - 120) / 16);
    Vector2 m = GetMousePosition();
    for (int i = 0; i < PALETTE_SIZE; i++) {
        Rectangle r = { c.x + 6 + i * sw, y + 6, sw - 2, PALBAR_H - 12 };
        DrawRectangleRec(r, a->scene.palette[i]);
        if (i == a->color)  DrawRectangleLinesEx(r, 3, WHITE);
        if (i == a->color2) DrawRectangleLinesEx((Rectangle){ r.x + 3, r.y + 3, r.width - 6, r.height - 6 }, 2, BLACK);
        if (i == a->scene.alpha) DrawTextEx(GetFontDefault(), "a", (Vector2){ r.x + 4, r.y + r.height - 18 }, 20, 2, MAGENTA);
        if (CheckCollisionPointRec(m, r) && !a->ui_hover) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) a->color = i;
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) a->color2 = i;
        }
    }
    if (tb_button(a, (Rectangle){ c.x + c.width - 100, y + 10, 92, PALBAR_H - 20 }, "pal (P)", a->show_palette))
        a->show_palette = !a->show_palette;
}

void paint_draw(App *a) {
    Rectangle ar = area_rect(a);
    DrawRectangleRec(ar, (Color){ 22, 20, 30, 255 });
    BeginScissorMode((int)ar.x, (int)ar.y, (int)ar.width, (int)ar.height);

    Vector2 o = tex_origin(a);
    float sc = tex_scale(a);
    /* 투명 부분이 보이도록 체크무늬 */
    for (int y = 0; y < TEX_H; y += 8)
        for (int x = 0; x < TEX_W; x += 8)
            DrawRectangle((int)(o.x + x * sc), (int)(o.y + y * sc), (int)(8 * sc), (int)(8 * sc),
                          ((x + y) / 8) & 1 ? (Color){ 70, 70, 80, 255 } : (Color){ 100, 100, 110, 255 });
    DrawTexturePro(a->gpu_tex[0], (Rectangle){ 0, 0, TEX_W, TEX_H }, (Rectangle){ o.x, o.y, TEX_W * sc, TEX_H * sc },
                   (Vector2){ 0 }, 0, WHITE);
    if (a->tex_grid) {
        for (int i = 0; i <= TEX_W; i += 8) {
            DrawLineV(tex_to_screen(a, (Vector2){ (float)i, 0 }), tex_to_screen(a, (Vector2){ (float)i, TEX_H }), (Color){ 0, 0, 0, 70 });
            DrawLineV(tex_to_screen(a, (Vector2){ 0, (float)i }), tex_to_screen(a, (Vector2){ TEX_W, (float)i }), (Color){ 0, 0, 0, 70 });
        }
    }

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

    /* 커서 텍셀 */
    Vector2 m = GetMousePosition();
    if (CheckCollisionPointRec(m, ar) && a->tool != TOOL_UV && a->tool != TOOL_MARQUEE) {
        Vector2 t = screen_to_tex(a, m);
        Vector2 p = tex_to_screen(a, (Vector2){ floorf(t.x), floorf(t.y) });
        DrawRectangleLinesEx((Rectangle){ p.x, p.y, sc, sc }, 1, WHITE);
    }
    EndScissorMode();

    draw_toolbar(a);
    draw_palette_bar(a);
}
