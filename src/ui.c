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

static void draw_icon(int id, mu_Rect rect, mu_Color mc) {
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
