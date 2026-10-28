#include "ui.h"
#include "texture.h"
#include <raylib.h>
#include <string.h>

static float font_size(void) {
    return (float)(GetFontDefault().baseSize * UI_FONT_SCALE);
}

static int text_width(mu_Font font, const char *text, int len) {
    (void)font;
    char buf[1024];
    if (len < 0) len = (int)strlen(text);
    if (len >= (int)sizeof buf) len = (int)sizeof buf - 1;
    memcpy(buf, text, (size_t)len);
    buf[len] = '\0';
    return (int)MeasureTextEx(GetFontDefault(), buf, font_size(), (float)UI_FONT_SCALE).x;
}

static int text_height(mu_Font font) {
    (void)font;
    return (int)font_size();
}

static Color to_color(mu_Color c) {
    return (Color){ c.r, c.g, c.b, c.a };
}

/* ---- 도트 둥근 모서리: 한 칸(UI_FONT_SCALE 픽셀)씩 계단으로 깎음 ----
   rad 2: 첫 줄 2칸, 둘째 줄 1칸 안쪽. 사각형을 가로 띠 몇 개로 나눠 out에 담고 개수를 돌려줌 */

#define PX UI_FONT_SCALE

static int round_bands(int x, int y, int w, int h, int rad, bool square_bottom, int out[][4]) {
    while (rad > 0 && (w < rad * PX * 3 || h < rad * PX * 3)) rad--;
    int n = 0, top = rad * PX, bot = square_bottom ? 0 : rad * PX;
    for (int i = 0; i < rad; i++) {
        int in = (rad - i) * PX;
        out[n][0] = x + in; out[n][1] = y + i * PX; out[n][2] = w - in * 2; out[n][3] = PX; n++;
        if (!square_bottom) {
            out[n][0] = x + in; out[n][1] = y + h - (i + 1) * PX; out[n][2] = w - in * 2; out[n][3] = PX; n++;
        }
    }
    out[n][0] = x; out[n][1] = y + top; out[n][2] = w; out[n][3] = h - top - bot; n++;
    return n;
}

static void mu_round_rect(mu_Context *ctx, mu_Rect r, mu_Color c, int rad, bool square_bottom) {
    int b[8][4], n = round_bands(r.x, r.y, r.w, r.h, rad, square_bottom, b);
    for (int i = 0; i < n; i++) mu_draw_rect(ctx, mu_rect(b[i][0], b[i][1], b[i][2], b[i][3]), c);
}

static void fill_round(Rectangle r, Color c, int rad) {
    int b[8][4], n = round_bands((int)r.x, (int)r.y, (int)r.width, (int)r.height, rad, false, b);
    for (int i = 0; i < n; i++) DrawRectangle(b[i][0], b[i][1], b[i][2], b[i][3], c);
}

/* microui의 기본 테두리/배경 대신. 테두리는 한 칸 두께의 도트 원호 (바깥 rad 2, 안쪽 rad 1).
   창 제목줄은 아래쪽 모서리를 깎지 않음 (창 몸통과 이어지도록) */
static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
    bool title = colorid == MU_COLOR_TITLEBG;
    bool bordered = !title && colorid != MU_COLOR_SCROLLBASE && colorid != MU_COLOR_SCROLLTHUMB;
    mu_Color border = ctx->style->colors[MU_COLOR_BORDER];
    if (bordered && border.a) {
        mu_Rect o = mu_rect(rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2);
        mu_round_rect(ctx, o, border, 2, false);
        mu_round_rect(ctx, mu_rect(o.x + PX, o.y + PX, o.w - PX * 2, o.h - PX * 2), ctx->style->colors[colorid], 1, false);
        return;
    }
    mu_round_rect(ctx, rect, ctx->style->colors[colorid], 2, title);
}

void ui_fill_round(Rectangle r, Color c) {
    fill_round(r, c, 2);
}

/* ---- 제목줄 닫기(X)와 크기 조절 손잡이가 있는 창 ---- */

static bool g_resizable;   /* 창은 겹쳐 열지 않으므로 하나면 충분 */

bool ui_begin_window(mu_Context *ctx, const char *title, mu_Rect rect, int opt, bool *closed) {
    *closed = false;
    mu_Container *cnt = mu_get_container(ctx, title);
    if (!cnt->open) {        /* 지난 프레임에 X를 누름: 다음에 다시 열 수 있게 되돌려 둠 */
        cnt->open = 1;
        *closed = true;
        return false;
    }
    g_resizable = !(opt & MU_OPT_NORESIZE);
    return mu_begin_window_ex(ctx, title, rect, opt) != 0;
}

void ui_end_window(mu_Context *ctx) {
    if (g_resizable) {
        /* 몸통 clip을 잠시 빼고 오른쪽 아래 모서리에 흐린 점 삼각형 (스크롤바 위에 그려지도록 맨 끝에) */
        mu_pop_clip_rect(ctx);
        mu_Rect r = mu_get_current_container(ctx)->rect;
        mu_Color c = ctx->style->colors[MU_COLOR_TEXT];
        c.a = 70;
        static const char *art[5] = { "....X", ".....", "..X.X", ".....", "X.X.X" };
        int x0 = r.x + r.w - 5 * PX - 4, y0 = r.y + r.h - 5 * PX - 4;
        for (int j = 0; j < 5; j++)
            for (int i = 0; i < 5; i++)
                if (art[j][i] == 'X') mu_draw_rect(ctx, mu_rect(x0 + i * PX, y0 + j * PX, PX, PX), c);
        mu_push_clip_rect(ctx, mu_get_clip_rect(ctx));
    }
    g_resizable = false;
    mu_end_window(ctx);
}

void ui_init(mu_Context *ctx) {
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;
    ctx->draw_frame = draw_frame;
    ctx->style->size.y = 16;
    ctx->style->padding = 4;
    ctx->style->spacing = 3;
    ctx->style->colors[MU_COLOR_WINDOWBG] = (mu_Color){ 34, 32, 44, 255 };
    ctx->style->colors[MU_COLOR_BUTTON]   = (mu_Color){ 60, 58, 78, 255 };
    ctx->style->colors[MU_COLOR_BUTTONHOVER] = (mu_Color){ 84, 80, 108, 255 };
    ctx->style->colors[MU_COLOR_BUTTONFOCUS] = (mu_Color){ 110, 104, 140, 255 };
}

/* ---- 도트 아이콘: X는 글자색, A는 강조색(노랑), 나머지는 투명 ---- */

static const char *ICON_ART[UI_ICON_END - UI_ICON_FOLDER][10] = {
    [UI_ICON_FOLDER - UI_ICON_FOLDER] = {
        "XXXX.....",
        "X..XXXXXX",
        "X.......X",
        "XXXXXXXXX",
        "X.......X",
        "X.......X",
        "X.......X",
        "XXXXXXXXX",
    },
    [UI_ICON_CUBE - UI_ICON_FOLDER] = {
        "....X....",
        "..XX.XX..",
        "XX.....XX",
        "X.XX.XX.X",
        "X...X...X",
        "X...X...X",
        "X...X...X",
        ".XX.X.XX.",
        "...XXX...",
    },
    [UI_ICON_BRUSH - UI_ICON_FOLDER] = {
        ".......XX",
        "......XXX",
        ".....XXX.",
        "....XXX..",
        "...XXX...",
        "..AXX....",
        ".AAA.....",
        "AAAA.....",
        "AAA......",
    },
    [UI_ICON_PLUS - UI_ICON_FOLDER] = {
        "...XXX...",
        "...XXX...",
        "...XXX...",
        "XXXXXXXXX",
        "XXXXXXXXX",
        "XXXXXXXXX",
        "...XXX...",
        "...XXX...",
        "...XXX...",
    },
    [UI_ICON_UNDO - UI_ICON_FOLDER] = {
        "..X......",
        ".XX......",
        "XXXXXXX..",
        ".XX....X.",
        "..X.....X",
        "........X",
        "........X",
        ".......X.",
        "...XXXX..",
    },
    [UI_ICON_REDO - UI_ICON_FOLDER] = {
        "......X..",
        "......XX.",
        "..XXXXXXX",
        ".X....XX.",
        "X.....X..",
        "X........",
        "X........",
        ".X.......",
        "..XXXX...",
    },
    [UI_ICON_LIST - UI_ICON_FOLDER] = {
        "AA.XXXXXX",
        "AA.XXXXXX",
        ".........",
        "AA.XXXXXX",
        "AA.XXXXXX",
        ".........",
        "AA.XXXXXX",
        "AA.XXXXXX",
    },
    [UI_ICON_RENDER_TEX - UI_ICON_FOLDER] = {
        "XX..XX..",
        "XX..XX..",
        "..XX..XX",
        "..XX..XX",
        "XX..XX..",
        "XX..XX..",
        "..XX..XX",
        "..XX..XX",
    },
    [UI_ICON_RENDER_COLOR - UI_ICON_FOLDER] = {
        "XXXXXXXX",
        "XAAAAAAX",
        "XAAAAAAX",
        "XAAAAAAX",
        "XAAAAAAX",
        "XAAAAAAX",
        "XAAAAAAX",
        "XXXXXXXX",
    },
    [UI_ICON_RENDER_WIRE - UI_ICON_FOLDER] = {
        "XXXXXXXX",
        "XX.....X",
        "X.X....X",
        "X..X...X",
        "X...X..X",
        "X....X.X",
        "X.....XX",
        "XXXXXXXX",
    },
    /* 반쯤 그늘진 공 */
    [UI_ICON_SHADE - UI_ICON_FOLDER] = {
        "..XXXXX..",
        ".X..XXXX.",
        "X...XXXXX",
        "X...XXXXX",
        "X...XXXXX",
        "X...XXXXX",
        "X...XXXXX",
        ".X..XXXX.",
        "..XXXXX..",
    },
    [UI_ICON_EYE - UI_ICON_FOLDER] = {
        "..XXXXX..",
        ".X.....X.",
        "X..XXX..X",
        "X..XAX..X",
        "X..XXX..X",
        ".X.....X.",
        "..XXXXX..",
    },
    [UI_ICON_EYE_OFF - UI_ICON_FOLDER] = {
        ".........",
        ".........",
        "X.......X",
        ".X.....X.",
        "..XXXXX..",
        ".X..X..X.",
        ".........",
    },
    [UI_ICON_LOCK - UI_ICON_FOLDER] = {
        "..XXXX..",
        ".X....X.",
        ".X....X.",
        "XXXXXXXX",
        "X......X",
        "X..AA..X",
        "X..AA..X",
        "X......X",
        "XXXXXXXX",
    },
    [UI_ICON_UNLOCK - UI_ICON_FOLDER] = {
        "..XXXX..",
        ".X....X.",
        ".X......",
        "XXXXXXXX",
        "X......X",
        "X......X",
        "X......X",
        "X......X",
        "XXXXXXXX",
    },
    [UI_ICON_CLONE - UI_ICON_FOLDER] = {
        "XXXXX....",
        "X...X....",
        "X..XXXXXX",
        "X..X....X",
        "XXXX.A..X",
        "...X.A..X",
        "...XAAA.X",
        "...X....X",
        "...XXXXXX",
    },
    [UI_ICON_TRASH - UI_ICON_FOLDER] = {
        "...XXX...",
        "XXXXXXXXX",
        ".X.....X.",
        ".X.X.X.X.",
        ".X.X.X.X.",
        ".X.X.X.X.",
        ".X.X.X.X.",
        ".X.....X.",
        "..XXXXX..",
    },
    [UI_ICON_CLOSE - UI_ICON_FOLDER] = {
        "X.....X",
        ".X...X.",
        "..X.X..",
        "...X...",
        "..X.X..",
        ".X...X.",
        "X.....X",
    },
    [UI_ICON_EXTRUDE - UI_ICON_FOLDER] = {
        "....A....",
        "...AAA...",
        "..A.A.A..",
        "....A....",
        "XXXXXXXXX",
        "X.......X",
        "X.......X",
        "XXXXXXXXX",
    },
    [UI_ICON_DEL_FACE - UI_ICON_FOLDER] = {
        "XXXXXXXXX",
        "X.......X",
        "X.A...A.X",
        "X..A.A..X",
        "X...A...X",
        "X..A.A..X",
        "X.A...A.X",
        "X.......X",
        "XXXXXXXXX",
    },
    /* 더 보기: 점 세 개 */
    [UI_ICON_MORE - UI_ICON_FOLDER] = {
        "XX..XX..XX",
        "XX..XX..XX",
    },
    /* 원점을 가운데로: 네 모서리 안의 십자 */
    [UI_ICON_CENTER - UI_ICON_FOLDER] = {
        "XX.....XX",
        "X.......X",
        "....A....",
        "....A....",
        "..AAAAA..",
        "....A....",
        "....A....",
        "X.......X",
        "XX.....XX",
    },
    /* 정점 둘이 하나로 */
    [UI_ICON_MERGE - UI_ICON_FOLDER] = {
        "XX.....XX",
        "XX.....XX",
        "..X...X..",
        "...X.X...",
        "....X....",
        "...AAA...",
        "...AAA...",
        "...AAA...",
    },
    /* 격자 위에 붙은 정점 */
    [UI_ICON_SNAP - UI_ICON_FOLDER] = {
        "X...X...X",
        ".........",
        ".........",
        "...AAA...",
        "X..AXA..X",
        "...AAA...",
        ".........",
        ".........",
        "X...X...X",
    },
    [UI_ICON_DEL_VERT - UI_ICON_FOLDER] = {
        "......A.A",
        ".......A.",
        "......A.A",
        ".........",
        "..XXX....",
        "..XXX....",
        "..XXX....",
    },
    [UI_ICON_UV - UI_ICON_FOLDER] = {
        "AA.....AA",
        "AAXXXXXAA",
        ".X.....X.",
        ".X.....X.",
        ".X.....X.",
        ".X.....X.",
        ".X.....X.",
        "AAXXXXXAA",
        "AA.....AA",
    },
    [UI_ICON_PEN - UI_ICON_FOLDER] = {
        ".......X.",
        "......XAX",
        ".....X.X.",
        "....X.X..",
        "...X.X...",
        "..X.X....",
        ".XXX.....",
        ".XX......",
        "X........",
    },
    [UI_ICON_PICK - UI_ICON_FOLDER] = {
        ".......XX",
        "......XXX",
        ".....XXX.",
        "....X.X..",
        "...X.X...",
        "..X.X....",
        ".AAX.....",
        ".AA......",
        "X........",
    },
    [UI_ICON_FILL - UI_ICON_FOLDER] = {
        "...X.....",
        "..X.X....",
        ".X...X...",
        "X.....X..",
        ".X...XXA.",
        "..X.X..AA",
        "...X...AA",
        ".......A.",
    },
    [UI_ICON_LINE - UI_ICON_FOLDER] = {
        "........X",
        ".......X.",
        "......X..",
        ".....X...",
        "....X....",
        "...X.....",
        "..X......",
        ".X.......",
        "X........",
    },
    [UI_ICON_RECT - UI_ICON_FOLDER] = {
        "XXXXXXXXX",
        "X.......X",
        "X.......X",
        "X.......X",
        "X.......X",
        "X.......X",
        "XXXXXXXXX",
    },
    [UI_ICON_RECT_FILL - UI_ICON_FOLDER] = {
        "XXXXXXXXX",
        "XAAAAAAAX",
        "XAAAAAAAX",
        "XAAAAAAAX",
        "XAAAAAAAX",
        "XAAAAAAAX",
        "XXXXXXXXX",
    },
    [UI_ICON_CIRCLE - UI_ICON_FOLDER] = {
        "..XXXXX..",
        ".X.....X.",
        "X.......X",
        "X.......X",
        "X.......X",
        ".X.....X.",
        "..XXXXX..",
    },
    [UI_ICON_CIRCLE_FILL - UI_ICON_FOLDER] = {
        "..XXXXX..",
        ".XAAAAAX.",
        "XAAAAAAAX",
        "XAAAAAAAX",
        "XAAAAAAAX",
        ".XAAAAAX.",
        "..XXXXX..",
    },
    /* 점선 사각형 (영역 선택) */
    [UI_ICON_SELECT - UI_ICON_FOLDER] = {
        "XX.XXX.XX",
        "X.......X",
        ".........",
        "X.......X",
        "X.......X",
        ".........",
        "X.......X",
        "XX.XXX.XX",
    },
    [UI_ICON_GRID - UI_ICON_FOLDER] = {
        "XXXXXXXXXX",
        "X..X..X..X",
        "X..X..X..X",
        "XXXXXXXXXX",
        "X..X..X..X",
        "X..X..X..X",
        "XXXXXXXXXX",
        "X..X..X..X",
        "X..X..X..X",
        "XXXXXXXXXX",
    },
    [UI_ICON_ZOOM_OUT - UI_ICON_FOLDER] = {
        ".XXXXX...",
        "X.....X..",
        "X.....X..",
        "X.AAA.X..",
        "X.....X..",
        "X.....X..",
        ".XXXXXX..",
        "......XX.",
        ".......XX",
    },
    [UI_ICON_ZOOM_IN - UI_ICON_FOLDER] = {
        ".XXXXX...",
        "X.....X..",
        "X..A..X..",
        "X.AAA.X..",
        "X..A..X..",
        "X.....X..",
        ".XXXXXX..",
        "......XX.",
        ".......XX",
    },
    /* 화면에 맞춤: 네 모서리 */
    [UI_ICON_FIT - UI_ICON_FOLDER] = {
        "XXX...XXX",
        "X.......X",
        "X.......X",
        ".........",
        ".........",
        ".........",
        "X.......X",
        "X.......X",
        "XXX...XXX",
    },
};

static void draw_pixel_icon(int id, mu_Rect rect, Color c) {
    const char *const *rows = ICON_ART[id - UI_ICON_FOLDER];
    const int s = UI_FONT_SCALE;
    const Color accent = { 255, 200, 60, 255 };
    int w = 0, h = 0;
    while (h < 10 && rows[h]) {
        int len = (int)strlen(rows[h]);
        if (len > w) w = len;
        h++;
    }
    int ox = rect.x + (rect.w - w * s) / 2, oy = rect.y + (rect.h - h * s) / 2;
    for (int y = 0; y < h; y++)
        for (int x = 0; rows[y][x]; x++) {
            char ch = rows[y][x];
            if (ch == 'X') DrawRectangle(ox + x * s, oy + y * s, s, s, c);
            else if (ch == 'A') DrawRectangle(ox + x * s, oy + y * s, s, s, accent);
        }
}

/* 색 선택 창의 그라데이션들 (microui 아이콘 명령을 빌려 그리는 순서를 지킴) */
static void draw_gfx(int id, mu_Rect r, Color c) {
    int x = r.x, y = r.y, w = r.w, h = r.h;
    if (id == UI_GFX_SV) {
        /* 가로: 흰색 → 색상, 세로: 투명 → 검정 */
        DrawRectangleGradientH(x, y, w, h, WHITE, (Color){ c.r, c.g, c.b, 255 });
        DrawRectangleGradientV(x, y, w, h, (Color){ 0, 0, 0, 0 }, BLACK);
    } else if (id == UI_GFX_HUE) {
        static const Color k[7] = { { 255, 0, 0, 255 }, { 255, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 255, 255 },
                                    { 0, 0, 255, 255 }, { 255, 0, 255, 255 }, { 255, 0, 0, 255 } };
        for (int i = 0; i < 6; i++) {
            int y0 = y + h * i / 6, y1 = y + h * (i + 1) / 6;
            DrawRectangleGradientV(x, y0, w, y1 - y0, k[i], k[i + 1]);
        }
    } else if (id == UI_GFX_ALPHA) {
        for (int i = 0; i * 6 < w; i++)
            for (int j = 0; j * 6 < h; j++) {
                int cw = w - i * 6 < 6 ? w - i * 6 : 6, ch = h - j * 6 < 6 ? h - j * 6 : 6;
                DrawRectangle(x + i * 6, y + j * 6, cw, ch, (i + j) & 1 ? (Color){ 150, 150, 160, 255 } : (Color){ 210, 210, 216, 255 });
            }
        DrawRectangleGradientH(x, y, w, h, (Color){ c.r, c.g, c.b, 0 }, (Color){ c.r, c.g, c.b, 255 });
    } else if (id == UI_GFX_SWATCH) {
        if (c.a < 255)
            for (int i = 0; i * 5 < w; i++)
                for (int j = 0; j * 5 < h; j++) {
                    int cw = w - i * 5 < 5 ? w - i * 5 : 5, ch = h - j * 5 < 5 ? h - j * 5 : 5;
                    DrawRectangle(x + i * 5, y + j * 5, cw, ch, (i + j) & 1 ? (Color){ 150, 150, 160, 255 } : (Color){ 210, 210, 216, 255 });
                }
        DrawRectangle(x, y, w, h, c);
    }
}

static void draw_icon(int id, mu_Rect rect, mu_Color mc) {
    if (id >= UI_ICON_FOLDER && id < UI_ICON_END) {
        draw_pixel_icon(id, rect, to_color(mc));
        return;
    }
    if (id >= UI_GFX_SV && id <= UI_GFX_SWATCH) {
        draw_gfx(id, rect, to_color(mc));
        return;
    }
    float s  = (rect.w < rect.h ? rect.w : rect.h) * 0.5f;
    float cx = rect.x + rect.w / 2.0f;
    float cy = rect.y + rect.h / 2.0f;
    float h  = s / 2.0f;
    Color c  = to_color(mc);

    /* DrawTriangle은 화면 기준 반시계 방향으로 꼭짓점을 넘겨야 그려짐 */
    switch (id) {
    case MU_ICON_CLOSE:   /* 창 제목줄의 X: 도트 아이콘, 올려두면 강조색 */
        if (CheckCollisionPointRec(GetMousePosition(), (Rectangle){ rect.x, rect.y, rect.w, rect.h }))
            c = (Color){ 255, 200, 60, 255 };
        draw_pixel_icon(UI_ICON_CLOSE, rect, c);
        break;
    case MU_ICON_CHECK:
        DrawRectangle((int)(cx - h), (int)(cy - h), (int)(h * 2), (int)(h * 2), c);
        break;
    case MU_ICON_COLLAPSED:
        DrawTriangle((Vector2){ cx - h / 2, cy - h }, (Vector2){ cx - h / 2, cy + h },
                     (Vector2){ cx + h * 0.7f, cy }, c);
        break;
    case MU_ICON_EXPANDED:
        DrawTriangle((Vector2){ cx - h, cy - h / 2 }, (Vector2){ cx, cy + h * 0.7f },
                     (Vector2){ cx + h, cy - h / 2 }, c);
        break;
    }
}

void ui_render(mu_Context *ctx) {
    mu_Command *cmd = NULL;
    bool clipping = false;
    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
        case MU_COMMAND_RECT: {
            mu_Rect r = cmd->rect.rect;
            DrawRectangle(r.x, r.y, r.w, r.h, to_color(cmd->rect.color));
            break;
        }
        case MU_COMMAND_TEXT:
            DrawTextEx(GetFontDefault(), cmd->text.str,
                       (Vector2){ (float)cmd->text.pos.x, (float)cmd->text.pos.y },
                       font_size(), (float)UI_FONT_SCALE, to_color(cmd->text.color));
            break;
        case MU_COMMAND_ICON:
            draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            break;
        case MU_COMMAND_CLIP: {
            mu_Rect cr = cmd->clip.rect;
            if (clipping) EndScissorMode();
            clipping = cr.w < 0x1000000;      /* microui의 "클립 없음" */
            if (clipping) BeginScissorMode(cr.x, cr.y, cr.w, cr.h);
            break;
        }
        }
    }
    if (clipping) EndScissorMode();
}

void ui_input(mu_Context *ctx) {
    Vector2 m = GetMousePosition();
    mu_input_mousemove(ctx, (int)m.x, (int)m.y);

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) mu_input_scroll(ctx, 0, (int)(wheel * -30));

    static const struct { int rl; int mu; } buttons[] = {
        { MOUSE_BUTTON_LEFT,   MU_MOUSE_LEFT   },
        { MOUSE_BUTTON_RIGHT,  MU_MOUSE_RIGHT  },
        { MOUSE_BUTTON_MIDDLE, MU_MOUSE_MIDDLE },
    };
    for (size_t i = 0; i < sizeof buttons / sizeof buttons[0]; i++) {
        if (IsMouseButtonPressed(buttons[i].rl))  mu_input_mousedown(ctx, (int)m.x, (int)m.y, buttons[i].mu);
        if (IsMouseButtonReleased(buttons[i].rl)) mu_input_mouseup(ctx, (int)m.x, (int)m.y, buttons[i].mu);
    }

    static const struct { int rl; int mu; } keys[] = {
        { KEY_LEFT_SHIFT,   MU_KEY_SHIFT }, { KEY_RIGHT_SHIFT,   MU_KEY_SHIFT },
        { KEY_LEFT_CONTROL, MU_KEY_CTRL  }, { KEY_RIGHT_CONTROL, MU_KEY_CTRL  },
        { KEY_LEFT_ALT,     MU_KEY_ALT   }, { KEY_RIGHT_ALT,     MU_KEY_ALT   },
        { KEY_ENTER,        MU_KEY_RETURN }, { KEY_KP_ENTER,     MU_KEY_RETURN },
    };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (IsKeyPressed(keys[i].rl))  mu_input_keydown(ctx, keys[i].mu);
        if (IsKeyReleased(keys[i].rl)) mu_input_keyup(ctx, keys[i].mu);
    }
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) mu_input_keydown(ctx, MU_KEY_BACKSPACE);
    if (IsKeyReleased(KEY_BACKSPACE)) mu_input_keyup(ctx, MU_KEY_BACKSPACE);

    int cp;
    while ((cp = GetCharPressed()) > 0) {
        int len = 0;
        const char *utf8 = CodepointToUTF8(cp, &len);
        char buf[8] = { 0 };
        memcpy(buf, utf8, (size_t)len);
        mu_input_text(ctx, buf);
    }
}

/* ---- 아이콘 버튼 + 툴팁 ---- */

static const char *g_tip;
static mu_Id g_tip_id;
static double g_tip_since;
static bool g_tip_seen;

/* 마우스가 id 위에 머물면 tip을 툴팁으로 */
static void track_tip(mu_Context *ctx, mu_Id id, const char *tip) {
    if (!tip || ctx->hover != id) return;
    if (g_tip_id != id) {
        g_tip_id = id;
        g_tip_since = GetTime();
    }
    g_tip = tip;
    g_tip_seen = true;
}

bool ui_icon_button(mu_Context *ctx, int icon, const char *tip, bool active) {
    /* 켜진 상태는 버튼 색을 강조색 쪽으로 바꿔서 그림 */
    mu_Color saved[3];
    if (active) {
        for (int i = 0; i < 3; i++) saved[i] = ctx->style->colors[MU_COLOR_BUTTON + i];
        ctx->style->colors[MU_COLOR_BUTTON]      = (mu_Color){ 120, 104, 60, 255 };
        ctx->style->colors[MU_COLOR_BUTTONHOVER] = (mu_Color){ 146, 126, 70, 255 };
        ctx->style->colors[MU_COLOR_BUTTONFOCUS] = (mu_Color){ 170, 146, 80, 255 };
    }
    bool clicked = mu_button_ex(ctx, NULL, icon, MU_OPT_ALIGNCENTER) != 0;
    if (active)
        for (int i = 0; i < 3; i++) ctx->style->colors[MU_COLOR_BUTTON + i] = saved[i];

    track_tip(ctx, ctx->last_id, tip);
    return clicked;
}

bool ui_swatch_button(mu_Context *ctx, Color c, const char *tip, bool active) {
    mu_Id id = mu_get_id(ctx, tip, (int)strlen(tip));
    mu_Rect r = mu_layout_next(ctx);
    mu_update_control(ctx, id, r, 0);
    bool clicked = ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id;
    ctx->draw_frame(ctx, r, active ? MU_COLOR_BUTTONFOCUS : ctx->hover == id ? MU_COLOR_BUTTONHOVER : MU_COLOR_BUTTON);
    ui_swatch(ctx, mu_rect(r.x + 7, r.y + 4, r.w - 14, r.h - 8), c);
    track_tip(ctx, id, tip);
    return clicked;
}

/* 글자 버튼인데 켜진 상태를 강조색으로 보여줌 (형식/출력 고르기 등) */
bool ui_toggle_button(mu_Context *ctx, const char *label, bool active) {
    mu_Color saved[3];
    if (active) {
        for (int i = 0; i < 3; i++) saved[i] = ctx->style->colors[MU_COLOR_BUTTON + i];
        ctx->style->colors[MU_COLOR_BUTTON]      = (mu_Color){ 120, 104, 60, 255 };
        ctx->style->colors[MU_COLOR_BUTTONHOVER] = (mu_Color){ 146, 126, 70, 255 };
        ctx->style->colors[MU_COLOR_BUTTONFOCUS] = (mu_Color){ 170, 146, 80, 255 };
    }
    bool clicked = mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER) != 0;
    if (active)
        for (int i = 0; i < 3; i++) ctx->style->colors[MU_COLOR_BUTTON + i] = saved[i];
    return clicked;
}

void ui_tooltip_begin(void) {
    g_tip_seen = false;
}

void ui_tooltip_draw(void) {
    if (!g_tip_seen) { g_tip_id = 0; return; }
    if (GetTime() - g_tip_since < 0.35) return;

    Font f = GetFontDefault();
    Vector2 ts = MeasureTextEx(f, g_tip, font_size(), (float)UI_FONT_SCALE);
    Vector2 m = GetMousePosition();
    float w = ts.x + 12, h = ts.y + 8;
    float x = m.x + 14, y = m.y + 22;
    /* 화면 밖으로 나가지 않게 */
    if (x + w > GetScreenWidth()) x = GetScreenWidth() - w - 2;
    if (y + h > GetScreenHeight()) y = m.y - h - 6;
    fill_round((Rectangle){ x - PX, y - PX, w + PX * 2, h + PX * 2 }, (Color){ 255, 200, 60, 255 }, 2);
    fill_round((Rectangle){ x, y, w, h }, (Color){ 20, 18, 28, 255 }, 1);
    DrawTextEx(f, g_tip, (Vector2){ x + 6, y + 4 }, font_size(), (float)UI_FONT_SCALE, (Color){ 240, 236, 228, 255 });
}

/* ---- raylib로 직접 그리는 버튼 (텍스처 모드 도구줄 등) ---- */

bool ui_icon_button_rect(Rectangle r, int icon, const char *tip, bool active, bool blocked) {
    Vector2 m = GetMousePosition();
    bool hover = !blocked && CheckCollisionPointRec(m, r);
    Color bg = active ? (Color){ 120, 104, 60, 255 } : hover ? (Color){ 84, 80, 108, 255 } : (Color){ 52, 50, 68, 255 };
    ui_fill_round(r, bg);
    draw_pixel_icon(icon, (mu_Rect){ (int)r.x, (int)r.y, (int)r.width, (int)r.height }, (Color){ 230, 230, 230, 255 });
    if (tip && hover) {
        /* microui id와 겹치지 않게 위치로 만든 id */
        mu_Id id = 0x80000000u ^ ((mu_Id)r.x << 12) ^ (mu_Id)r.y;
        if (g_tip_id != id) {
            g_tip_id = id;
            g_tip_since = GetTime();
        }
        g_tip = tip;
        g_tip_seen = true;
    }
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool ui_button_rect(Rectangle r, const char *label, bool active, bool blocked) {
    Vector2 m = GetMousePosition();
    bool hover = !blocked && CheckCollisionPointRec(m, r);
    Color bg = active ? (Color){ 120, 104, 60, 255 } : hover ? (Color){ 84, 80, 108, 255 } : (Color){ 52, 50, 68, 255 };
    ui_fill_round(r, bg);
    Vector2 ts = MeasureTextEx(GetFontDefault(), label, font_size(), (float)UI_FONT_SCALE);
    DrawTextEx(GetFontDefault(), label, (Vector2){ r.x + (r.width - ts.x) / 2, r.y + (r.height - ts.y) / 2 },
               font_size(), (float)UI_FONT_SCALE, WHITE);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* ---- 색 선택 ---- */

static float clamp01(float v) {
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

/* 누르고 있는 동안 rect 안에서의 상대 위치(0~1). 잡고 있지 않으면 false */
static bool drag_area(mu_Context *ctx, const char *name, mu_Rect r, float *u, float *v) {
    mu_Id id = mu_get_id(ctx, name, (int)strlen(name));
    mu_update_control(ctx, id, r, 0);
    if (ctx->focus != id || !(ctx->mouse_down & MU_MOUSE_LEFT)) return false;
    *u = clamp01((float)(ctx->mouse_pos.x - r.x) / (float)(r.w - 1));
    *v = clamp01((float)(ctx->mouse_pos.y - r.y) / (float)(r.h - 1));
    return true;
}

static void marker(mu_Context *ctx, int x, int y) {
    mu_draw_box(ctx, mu_rect(x - 3, y - 3, 7, 7), mu_color(20, 18, 28, 255));
    mu_draw_box(ctx, mu_rect(x - 2, y - 2, 5, 5), mu_color(255, 255, 255, 255));
}

bool ui_color_picker(mu_Context *ctx, float *h, float *s, float *v, float *alpha) {
    bool changed = false;
    float u, w;
    Color hue = color_from_hsv(*h, 1, 1, 255);

    mu_layout_row(ctx, 2, (int[]){ 168, 22 }, 168);
    mu_Rect sv = mu_layout_next(ctx);
    if (drag_area(ctx, "sv", sv, &u, &w)) { *s = u; *v = 1 - w; changed = true; }
    mu_draw_icon(ctx, UI_GFX_SV, sv, mu_color(hue.r, hue.g, hue.b, 255));
    marker(ctx, sv.x + (int)(*s * (sv.w - 1)), sv.y + (int)((1 - *v) * (sv.h - 1)));

    mu_Rect hr = mu_layout_next(ctx);
    if (drag_area(ctx, "hue", hr, &u, &w)) { *h = w * 359.9f; changed = true; }
    mu_draw_icon(ctx, UI_GFX_HUE, hr, mu_color(255, 255, 255, 255));
    marker(ctx, hr.x + hr.w / 2, hr.y + (int)(*h / 360.0f * (hr.h - 1)));

    Color full = color_from_hsv(*h, *s, *v, 255);
    mu_layout_row(ctx, 1, (int[]){ 193 }, 16);
    mu_Rect ar = mu_layout_next(ctx);
    if (drag_area(ctx, "alpha", ar, &u, &w)) { *alpha = u; changed = true; }
    mu_draw_icon(ctx, UI_GFX_ALPHA, ar, mu_color(full.r, full.g, full.b, 255));
    marker(ctx, ar.x + (int)(*alpha * (ar.w - 1)), ar.y + ar.h / 2);
    return changed;
}

void ui_swatch(mu_Context *ctx, mu_Rect r, Color c) {
    mu_draw_icon(ctx, UI_GFX_SWATCH, r, mu_color(c.r, c.g, c.b, c.a));
}
