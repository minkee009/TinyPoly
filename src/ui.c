#include "ui.h"
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

void ui_init(mu_Context *ctx) {
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;
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

static void draw_icon(int id, mu_Rect rect, mu_Color mc) {
    if (id >= UI_ICON_FOLDER && id < UI_ICON_END) {
        draw_pixel_icon(id, rect, to_color(mc));
        return;
    }
    float s  = (rect.w < rect.h ? rect.w : rect.h) * 0.5f;
    float cx = rect.x + rect.w / 2.0f;
    float cy = rect.y + rect.h / 2.0f;
    float h  = s / 2.0f;
    Color c  = to_color(mc);

    /* DrawTriangle은 화면 기준 반시계 방향으로 꼭짓점을 넘겨야 그려짐 */
    switch (id) {
    case MU_ICON_CLOSE:
        DrawLineEx((Vector2){ cx - h, cy - h }, (Vector2){ cx + h, cy + h }, 2.0f, c);
        DrawLineEx((Vector2){ cx + h, cy - h }, (Vector2){ cx - h, cy + h }, 2.0f, c);
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

    mu_Id id = ctx->last_id;
    if (tip && ctx->hover == id) {
        if (g_tip_id != id) {
            g_tip_id = id;
            g_tip_since = GetTime();
        }
        g_tip = tip;
        g_tip_seen = true;
    }
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
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){ 20, 18, 28, 240 });
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, (Color){ 255, 200, 60, 255 });
    DrawTextEx(f, g_tip, (Vector2){ x + 6, y + 4 }, font_size(), (float)UI_FONT_SCALE, (Color){ 240, 236, 228, 255 });
}
