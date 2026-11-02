#include "paint/paint.h"
#include "ui/cursor.h"
#include "model/model.h"
#include "paint/palbar.h"
#include "core/texture.h"
#include "paint/texview.h"
#include "ui/ui.h"
#include "paint/uv.h"
#include "view/view.h"
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool KeyShift(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool KeyCtrl(void)  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

static int TexW(const App *a) { return a->scene.tex->w; }
static int TexH(const App *a) { return a->scene.tex->h; }

/* 고칠 수 있는 텍스처 (되돌리기 기록과 공유 중이면 이때 복사됨) */
static Canvas MutableCanvas(App *a) {
    return (Canvas){ SceneTexMut(&a->scene), TexW(a), TexH(a) };
}

static void EnsureBackup(App *a) {
    int n = TexW(a) * TexH(a);
    if (a->paint.backup_n != n) {
        free(a->paint.backup);
        a->paint.backup = malloc(sizeof(Color) * (size_t)n);
        a->paint.backup_n = n;
    }
}

/* ---- 마키 ---- */

static void StampFloat(App *a) {
    Canvas c = MutableCanvas(a);
    memcpy(c.px, a->paint.backup, sizeof(Color) * (size_t)(c.w * c.h));
    for (int y = 0; y < a->paint.fh; y++)
        for (int x = 0; x < a->paint.fw; x++) TexPlot(c, a->paint.fx + x, a->paint.fy + y, a->paint.float_px[y * a->paint.fw + x]);
    a->paint.sx = a->paint.fx; a->paint.sy = a->paint.fy; a->paint.sw = a->paint.fw; a->paint.sh = a->paint.fh;
    a->tex_dirty = true;
}

static void CommitFloat(App *a) {
    a->paint.floating = false;
}

static Color *CopyRegion(const App *a, int sx, int sy, int w, int h) {
    Color *dst = malloc(sizeof(Color) * (size_t)w * (size_t)h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            Color c = BLANK;
            TexViewTexel(a, sx + x, sy + y, &c);
            dst[y * w + x] = c;
        }
    return dst;
}

static void LiftSelection(App *a) {
    AppMark(a);
    a->paint.fw = a->paint.sw; a->paint.fh = a->paint.sh; a->paint.fx = a->paint.sx; a->paint.fy = a->paint.sy;
    free(a->paint.float_px);
    a->paint.float_px = CopyRegion(a, a->paint.sx, a->paint.sy, a->paint.sw, a->paint.sh);
    EnsureBackup(a);
    memcpy(a->paint.backup, a->scene.tex->px, sizeof(Color) * (size_t)a->paint.backup_n);
    TexRect((Canvas){ a->paint.backup, TexW(a), TexH(a) }, a->paint.sx, a->paint.sy, a->paint.sx + a->paint.sw - 1, a->paint.sy + a->paint.sh - 1, BLANK, true);
    a->paint.floating = true;
}

static void SelCopy(App *a) {
    if (a->paint.sw <= 0) return;
    a->paint.cw = a->paint.sw; a->paint.ch = a->paint.sh;
    free(a->paint.clip);
    a->paint.clip = CopyRegion(a, a->paint.sx, a->paint.sy, a->paint.sw, a->paint.sh);
    AppMsg(a, "copied %dx%d pixels", a->paint.cw, a->paint.ch);
}

static void SelPaste(App *a) {
    if (a->paint.cw <= 0) return;
    CommitFloat(a);
    AppMark(a);
    EnsureBackup(a);
    memcpy(a->paint.backup, a->scene.tex->px, sizeof(Color) * (size_t)a->paint.backup_n);
    free(a->paint.float_px);
    a->paint.float_px = malloc(sizeof(Color) * (size_t)a->paint.cw * (size_t)a->paint.ch);
    memcpy(a->paint.float_px, a->paint.clip, sizeof(Color) * (size_t)a->paint.cw * (size_t)a->paint.ch);
    a->paint.fw = a->paint.cw; a->paint.fh = a->paint.ch;
    a->paint.fx = a->paint.sw > 0 ? a->paint.sx : 0;
    a->paint.fy = a->paint.sw > 0 ? a->paint.sy : 0;
    a->paint.floating = true;
    StampFloat(a);
    a->tool = TOOL_MARQUEE;
}

static void SelDelete(App *a) {
    if (a->paint.sw <= 0) return;
    if (a->paint.floating) {
        memcpy(MutableCanvas(a).px, a->paint.backup, sizeof(Color) * (size_t)a->paint.backup_n);
        CommitFloat(a);
    } else {
        AppMark(a);
        TexRect(MutableCanvas(a), a->paint.sx, a->paint.sy, a->paint.sx + a->paint.sw - 1, a->paint.sy + a->paint.sh - 1, BLANK, true);
    }
    a->paint.sw = 0;
    a->tex_dirty = true;
}

/* 텍스처 크기가 바뀌면 선택/떠 있는 조각은 버림 */
void PaintResetSelection(App *a) {
    a->paint.floating = false;
    a->paint.sw = 0;
    a->paint.drag = PDRAG_NONE;
}

/* ---- 그리기 도구 ---- */

static void DrawShape(App *a, int x1, int y1) {
    Canvas c = MutableCanvas(a);
    memcpy(c.px, a->paint.backup, sizeof(Color) * (size_t)a->paint.backup_n);
    if (a->tool == TOOL_LINE)   TexLine(c, a->paint.x0, a->paint.y0, x1, y1, a->paint.draw_color);
    if (a->tool == TOOL_RECT)   TexRect(c, a->paint.x0, a->paint.y0, x1, y1, a->paint.draw_color, a->rect_filled);
    if (a->tool == TOOL_CIRCLE) TexEllipse(c, a->paint.x0, a->paint.y0, x1, y1, a->paint.draw_color, a->circle_filled);
    a->tex_dirty = true;
}

static void SetTool(App *a, int tool) {
    if (tool != TOOL_MARQUEE) { CommitFloat(a); a->paint.sw = 0; }
    /* 사각형/원 버튼을 다시 누르면 채우기 전환 */
    if (tool == a->tool && tool == TOOL_RECT) a->rect_filled = !a->rect_filled;
    if (tool == a->tool && tool == TOOL_CIRCLE) a->circle_filled = !a->circle_filled;
    a->tool = tool;
}

static void CanvasPress(App *a, Vector2 m, bool right) {
    Vector2 t = TexViewToTex(a, m);
    int x = (int)floorf(t.x), y = (int)floorf(t.y);
    a->paint.x0 = a->paint.lx = x;
    a->paint.y0 = a->paint.ly = y;
    a->paint.draw_color = right ? a->color2 : a->color;
    Color under;
    bool inside = TexViewTexel(a, x, y, &under);

    if (KeyCtrl() || a->tool == TOOL_PICK) {
        if (inside) { if (right) a->color2 = under; else a->color = under; }
        return;
    }
    switch (a->tool) {
    case TOOL_UV:
        if (!right) UVPress(a, m);
        break;
    case TOOL_PEN:
        AppMark(a);
        TexPlot(MutableCanvas(a), x, y, a->paint.draw_color);
        ViewTexDirty(a, x, y, x, y);
        a->paint.drag = PDRAG_STROKE;
        break;
    case TOOL_FILL:
        if (!inside) break;
        AppMark(a);
        TexFill(MutableCanvas(a), x, y, a->paint.draw_color);
        a->tex_dirty = true;
        break;
    case TOOL_LINE: case TOOL_RECT: case TOOL_CIRCLE:
        AppMark(a);
        EnsureBackup(a);
        memcpy(a->paint.backup, a->scene.tex->px, sizeof(Color) * (size_t)a->paint.backup_n);
        DrawShape(a, x, y);
        a->paint.drag = PDRAG_SHAPE;
        break;
    case TOOL_MARQUEE:
        if (a->paint.sw > 0 && x >= a->paint.sx && y >= a->paint.sy && x < a->paint.sx + a->paint.sw && y < a->paint.sy + a->paint.sh) {
            if (!a->paint.floating) LiftSelection(a);
            a->paint.fx0 = a->paint.fx;
            a->paint.fy0 = a->paint.fy;
            a->paint.drag = PDRAG_SEL_MOVE;
        } else {
            CommitFloat(a);
            a->paint.sw = 0;
            a->paint.drag = PDRAG_SEL;
        }
        break;
    }
}

static void CanvasDrag(App *a, Vector2 m) {
    Vector2 t = TexViewToTex(a, m);
    int x = (int)floorf(t.x), y = (int)floorf(t.y);
    switch (a->paint.drag) {
    case PDRAG_STROKE:
        if (KeyShift()) {
            /* 시작점 기준 축 고정 */
            if (abs(x - a->paint.x0) >= abs(y - a->paint.y0)) y = a->paint.y0; else x = a->paint.x0;
        }
        TexLine(MutableCanvas(a), a->paint.lx, a->paint.ly, x, y, a->paint.draw_color);
        ViewTexDirty(a, a->paint.lx < x ? a->paint.lx : x, a->paint.ly < y ? a->paint.ly : y, a->paint.lx > x ? a->paint.lx : x, a->paint.ly > y ? a->paint.ly : y);
        a->paint.lx = x;
        a->paint.ly = y;
        break;
    case PDRAG_SHAPE:
        DrawShape(a, x, y);
        break;
    case PDRAG_SEL: {
        int x0 = (int)Clamp((float)a->paint.x0, 0, TexW(a) - 1), y0 = (int)Clamp((float)a->paint.y0, 0, TexH(a) - 1);
        int x1 = (int)Clamp((float)x, 0, TexW(a) - 1), y1 = (int)Clamp((float)y, 0, TexH(a) - 1);
        a->paint.sx = x0 < x1 ? x0 : x1;
        a->paint.sy = y0 < y1 ? y0 : y1;
        a->paint.sw = abs(x1 - x0) + 1;
        a->paint.sh = abs(y1 - y0) + 1;
        break;
    }
    case PDRAG_SEL_MOVE:
        a->paint.fx = a->paint.fx0 + (x - a->paint.x0);
        a->paint.fy = a->paint.fy0 + (y - a->paint.y0);
        StampFloat(a);
        break;
    case PDRAG_UV_MOVE: case PDRAG_UV_VERT: case PDRAG_UV_SCALE:
        UVApplyDrag(a, m);
        break;
    case PDRAG_PAN:
        a->tex_pan = Vector2Add(a->tex_pan, GetMouseDelta());
        break;
    default:
        break;
    }
}

static bool AnyButtonDown(void) {
    return IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
           IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
}

static void UpdateKeys(App *a) {
    bool ctrl = KeyCtrl();
    if (ctrl) {
        if (IsKeyPressed(KEY_C)) { if (a->tool == TOOL_UV) UVCopy(a); else SelCopy(a); }
        if (IsKeyPressed(KEY_V)) { if (a->tool == TOOL_UV) UVPaste(a); else SelPaste(a); }
        return;
    }
    if (IsKeyPressed(KEY_V)) SetTool(a, TOOL_UV);
    if (IsKeyPressed(KEY_B)) SetTool(a, TOOL_PEN);
    if (IsKeyPressed(KEY_I)) SetTool(a, TOOL_PICK);
    if (IsKeyPressed(KEY_G)) SetTool(a, TOOL_FILL);
    Rectangle ar = TexViewArea(a);
    Vector2 center = { ar.x + ar.width / 2, ar.y + ar.height / 2 };
    if (IsKeyPressed(KEY_ONE))   TexViewZoomFit(a);
    if (IsKeyPressed(KEY_TWO))   TexViewZoomTo(a, 1, center);
    if (IsKeyPressed(KEY_THREE)) TexViewZoomTo(a, 2, center);
    if (IsKeyPressed(KEY_FOUR))  TexViewZoomTo(a, 4, center);
    if (IsKeyPressed(KEY_X)) { Color t = a->color; a->color = a->color2; a->color2 = t; }
    if (IsKeyPressed(KEY_R)) UVRotate(a);
    if (IsKeyPressed(KEY_H)) UVFlip(a, KeyShift());
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) SelDelete(a);
    if (IsKeyPressed(KEY_ESCAPE)) { CommitFloat(a); a->paint.sw = 0; }

    int dx = (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_D)) -
             (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_A));
    int dy = (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN) || IsKeyPressed(KEY_S)) -
             (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP) || IsKeyPressed(KEY_W));
    if (dx || dy) UVNudge(a, (Vector2){ (float)dx, (float)dy });
}

/* ---- 커서 ---- */

bool PaintHoverTexel(const App *a, Color *out) {
    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, TexViewArea(a))) return false;
    Vector2 t = TexViewToTex(a, m);
    return TexViewTexel(a, (int)floorf(t.x), (int)floorf(t.y), out);
}

int PaintCursor(App *a) {
    /* 드래그 중이면 그 동작에 맞춰 */
    switch (a->paint.drag) {
    case PDRAG_PAN:      return CURSOR_GRAB;
    case PDRAG_STROKE:   return CURSOR_PEN;
    case PDRAG_SHAPE:
    case PDRAG_SEL:      return CURSOR_CROSS;
    case PDRAG_UV_SCALE: return CURSOR_SCALE;
    case PDRAG_UV_MOVE:
    case PDRAG_UV_VERT:
    case PDRAG_SEL_MOVE: return CURSOR_MOVE;
    default: break;
    }

    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, TexViewArea(a))) return -1;
    if (IsKeyDown(KEY_SPACE)) return CURSOR_HAND;
    if (KeyCtrl()) return CURSOR_PICKER;

    Vector2 t = TexViewToTex(a, m);
    switch (a->tool) {
    case TOOL_PEN:  return CURSOR_PEN;
    case TOOL_PICK: return CURSOR_PICKER;
    case TOOL_FILL: return CURSOR_BUCKET;
    case TOOL_MARQUEE: {
        int x = (int)floorf(t.x), y = (int)floorf(t.y);
        bool inside = a->paint.sw > 0 && x >= a->paint.sx && y >= a->paint.sy && x < a->paint.sx + a->paint.sw && y < a->paint.sy + a->paint.sh;
        return inside ? CURSOR_MOVE : CURSOR_CROSS;
    }
    case TOOL_UV:
        return UVCursor(a, m);
    default:
        return CURSOR_CROSS;
    }
}

void PaintUpdate(App *a, bool input_ok) {
    Vector2 m = GetMousePosition();
    bool over = CheckCollisionPointRec(m, TexViewArea(a)) && !a->ui_hover;

    if (a->paint.drag == PDRAG_NONE && over && input_ok) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) TexViewZoomStep(a, wheel > 0 ? 1 : -1, m);
        a->paint.press = m;
        a->paint.moved = false;
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) ||
            (IsKeyDown(KEY_SPACE) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) {
            a->paint.drag = PDRAG_PAN;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            CanvasPress(a, m, false);
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (a->tool == TOOL_UV && UVHasTarget(a)) AppOpenMenu(a, MENU_UV);
            else CanvasPress(a, m, true);
        }
    }
    if (a->paint.drag != PDRAG_NONE) {
        if (Vector2Distance(m, a->paint.press) > 2.0f) a->paint.moved = true;
        CanvasDrag(a, m);
        if (!AnyButtonDown()) a->paint.drag = PDRAG_NONE;
    }
    if (input_ok && a->paint.drag == PDRAG_NONE) PalBarUpdate(a);

    /* 3D 뷰: 면 선택/회전 (모델 단축키는 제외) */
    ModelUpdate(a, input_ok && a->paint.drag == PDRAG_NONE, false);
    if (input_ok && a->paint.drag == PDRAG_NONE) UpdateKeys(a);
}

/* ---- 화면 ---- */

static void DrawToolbar(App *a) {
    Rectangle c = a->canvas_rect;
    DrawRectangle((int)c.x, (int)c.y, (int)c.width, TOOLBAR_H, (Color){ 30, 28, 40, 255 });
    static const int icons[TOOL_COUNT] = { UI_ICON_UV, UI_ICON_PEN, UI_ICON_PICK, UI_ICON_FILL,
                                           UI_ICON_LINE, UI_ICON_RECT, UI_ICON_CIRCLE, UI_ICON_SELECT };
    static const char *tips[TOOL_COUNT] = { "uv edit (V)", "pen (B)", "pick color (I, ctrl+click)", "fill (G)",
                                            "line", "rect (click again: filled)", "circle (click again: filled)",
                                            "select area" };
    float x = c.x + 4, y = c.y + 3, h = TOOLBAR_H - 6, B = 30;
    for (int i = 0; i < TOOL_COUNT; i++) {
        int icon = icons[i];
        if (i == TOOL_RECT && a->rect_filled) icon = UI_ICON_RECT_FILL;
        if (i == TOOL_CIRCLE && a->circle_filled) icon = UI_ICON_CIRCLE_FILL;
        if (UIIconButtonRect((Rectangle){ x, y, B, h }, icon, tips[i], a->tool == i, a->ui_hover)) SetTool(a, i);
        x += B + 3;
    }
    x += 10;
    if (UIIconButtonRect((Rectangle){ x, y, B, h }, UI_ICON_GRID, "pixel grid", a->tex_grid, a->ui_hover))
        a->tex_grid = !a->tex_grid;
    x += B + 10;

    /* 텍스처 크기 (누르면 크기 메뉴) */
    char size[24];
    snprintf(size, sizeof size, "%dx%d", TexW(a), TexH(a));
    float sw = MeasureTextEx(GetFontDefault(), size, 20, 2).x + 12;
    if (UIButtonRect((Rectangle){ x, y, sw, h }, size, a->menu == MENU_TEXSIZE, a->ui_hover))
        AppOpenMenu(a, MENU_TEXSIZE);
    x += sw + 10;

    Rectangle ar = TexViewArea(a);
    Vector2 center = { ar.x + ar.width / 2, ar.y + ar.height / 2 };
    if (UIIconButtonRect((Rectangle){ x, y, B, h }, UI_ICON_ZOOM_OUT, "zoom out", false, a->ui_hover))
        TexViewZoomStep(a, -1, center);
    x += B + 3;
    if (UIIconButtonRect((Rectangle){ x, y, B, h }, UI_ICON_ZOOM_IN, "zoom in", false, a->ui_hover))
        TexViewZoomStep(a, 1, center);
    x += B + 3;
    if (UIIconButtonRect((Rectangle){ x, y, B, h }, UI_ICON_FIT, "fit to window", a->tex_zoom == 0, a->ui_hover))
        TexViewZoomFit(a);
    x += B + 6;
    char pct[16];
    snprintf(pct, sizeof pct, "%d%%", (int)lroundf(TexViewScale(a) * 100));
    DrawTextEx(GetFontDefault(), pct, (Vector2){ x, y + 2 }, 20, 2, (Color){ 200, 196, 210, 255 });
}

void PaintDraw(App *a) {
    Rectangle ar = TexViewArea(a);
    DrawRectangleRec(ar, (Color){ 22, 20, 30, 255 });
    BeginScissorMode((int)ar.x, (int)ar.y, (int)ar.width, (int)ar.height);

    Vector2 o = TexViewOrigin(a);
    float sc = TexViewScale(a);
    int tw = TexW(a), th = TexH(a);

    /* 투명 부분이 보이도록 체크무늬: 화면에 보이는 부분만, 화면 기준 칸 크기 */
    Rectangle tr = { o.x, o.y, tw * sc, th * sc };
    Rectangle vis = GetCollisionRec(tr, ar);
    if (vis.width > 0 && vis.height > 0) UIChecker(vis, 12);
    DrawTexturePro(a->gpu_tex, (Rectangle){ 0, 0, (float)tw, (float)th }, tr, (Vector2){ 0 }, 0, WHITE);

    if (a->tex_grid) {
        /* 칸이 화면에서 너무 촘촘하지 않은 간격 (8, 16, 32, ...) */
        int step = 8;
        while (step * sc < 16 && step < 1024) step *= 2;
        Color gc = { 0, 0, 0, 70 };
        for (int i = 0; i <= tw; i += step)
            DrawLineV(TexViewToScreen(a, (Vector2){ (float)i, 0 }), TexViewToScreen(a, (Vector2){ (float)i, (float)th }), gc);
        for (int i = 0; i <= th; i += step)
            DrawLineV(TexViewToScreen(a, (Vector2){ 0, (float)i }), TexViewToScreen(a, (Vector2){ (float)tw, (float)i }), gc);
    }
    DrawRectangleLinesEx((Rectangle){ tr.x - 1, tr.y - 1, tr.width + 2, tr.height + 2 }, 1, (Color){ 90, 86, 110, 255 });
    UVDraw(a);

    if (a->paint.sw > 0) {
        Vector2 p = TexViewToScreen(a, (Vector2){ (float)a->paint.sx, (float)a->paint.sy });
        Rectangle r = { p.x, p.y, a->paint.sw * sc, a->paint.sh * sc };
        bool phase = fmod(GetTime(), 0.5) < 0.25;
        DrawRectangleLinesEx(r, 1, phase ? WHITE : BLACK);
    }

    /* 커서 텍셀 (충분히 확대됐을 때만) */
    Vector2 m = GetMousePosition();
    if (sc >= 4 && CheckCollisionPointRec(m, ar) && a->tool != TOOL_UV && a->tool != TOOL_MARQUEE) {
        Vector2 t = TexViewToTex(a, m);
        Vector2 p = TexViewToScreen(a, (Vector2){ floorf(t.x), floorf(t.y) });
        DrawRectangleLinesEx((Rectangle){ p.x, p.y, sc, sc }, 1, WHITE);
    }
    EndScissorMode();

    DrawToolbar(a);
    PalBarDraw(a);
}
