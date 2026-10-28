#include "app.h"
#include "ui.h"

/* 도트 커서. X: 테두리, W: 채움, A: 강조색(노랑), C: 상황에 따른 색(현재 색 등), 공백: 투명 */
typedef struct {
    const char *rows[16];
    int hot_x, hot_y;          /* 클릭 지점 */
} CursorArt;

static const CursorArt ART[CURSOR_COUNT] = {
    [CURSOR_ARROW] = { {
        "X",
        "XX",
        "XWX",
        "XWWX",
        "XWWWX",
        "XWWWWX",
        "XWWWWWX",
        "XWWWWWWX",
        "XWWWWWWWX",
        "XWWWWXXXX",
        "XWWXWX",
        "XWX XWX",
        "XX  XWX",
        "     XX",
    }, 0, 0 },
    [CURSOR_CROSS] = { {
        "    XWX",
        "    XWX",
        "    XWX",
        "XXXX   XXXX",
        "WWW  A  WWW",
        "XXXX   XXXX",
        "    XWX",
        "    XWX",
        "    XWX",
    }, 5, 4 },
    /* 연필: 지우개는 강조색, 심은 현재 색 */
    [CURSOR_PEN] = { {
        "        XXX",
        "       XAAAX",
        "      XWXAAX",
        "     XWWWXX",
        "    XWWWX",
        "   XWWWX",
        "  XWWWX",
        " XCWWX",
        " XCCX",
        "XCX",
        "X",
    }, 0, 10 },
    /* 스포이드: 안에 든 액체가 커서 아래 색 */
    [CURSOR_PICKER] = { {
        "         XX",
        "        XXXX",
        "       XXXXX",
        "      XXXXX",
        "     XWXXX",
        "    XWWWX",
        "   XWWWX",
        "  XCCWX",
        " XCCCX",
        " XCCX",
        "X XX",
        "X",
    }, 0, 11 },
    /* 페인트 통: 흘러내리는 페인트가 현재 색 */
    [CURSOR_BUCKET] = { {
        "    XX",
        "   X  X",
        "   X XXX",
        "  XXXWWWX",
        " XWWWWWWWX",
        "XWWWWWWWXCX",
        " XWWWWWXCCX",
        "  XWWWXCCCX",
        "   XWX XCCX",
        "    X   XCX",
        "         X",
    }, 9, 10 },
    [CURSOR_MOVE] = { {
        "      X",
        "     XWX",
        "    XWWWX",
        "   XXXWXXX",
        "  XX XWX XX",
        " XWXXXWXXXWX",
        "XWWWWWWWWWWWX",
        " XWXXXWXXXWX",
        "  XX XWX XX",
        "   XXXWXXX",
        "    XWWWX",
        "     XWX",
        "      X",
    }, 6, 6 },
    [CURSOR_HAND] = { {
        "    XX",
        "   XWWXXX",
        "   XWWXWWXX",
        " XXXWWXWWXWX",
        "XWWXWWXWWXWWX",
        "XWWXWWWWWWWWX",
        " XWWWWWWWWWWX",
        " XWWWWWWWWWWX",
        "  XWWWWWWWWX",
        "  XWWWWWWWX",
        "   XWWWWWWX",
        "   XWWWWWX",
        "   XXXXXXX",
    }, 6, 6 },
    [CURSOR_GRAB] = { {
        "",
        "",
        "    XXXXXX",
        "   XWWXWWXX",
        "  XXWWXWWXWX",
        " XWWWWWWWWWX",
        " XWWWWWWWWWX",
        "  XWWWWWWWWX",
        "  XWWWWWWWX",
        "   XWWWWWWX",
        "   XWWWWWX",
        "   XXXXXXX",
    }, 6, 6 },
    [CURSOR_ROTATE] = { {
        "    XXXXX",
        "   XWWWWWX",
        "  XWXXXXXWX",
        " XWX     XWX",
        "XXWXX    XWX",
        "XWWWX    XWX",
        " XWX     XWX",
        "  X     XWX",
        "    XXXXWX",
        "   XWWWWX",
        "    XXXX",
    }, 5, 5 },
    [CURSOR_SCALE] = { {
        "XXXXX",
        "XWWWX",
        "XWWX",
        "XWXWX",
        "XX XWX",
        "    XWX",
        "     XWX XX",
        "      XWXWX",
        "       XWWX",
        "      XWWWX",
        "      XXXXX",
    }, 5, 5 },
};

static void draw_art(const CursorArt *art, Vector2 m, Color dyn) {
    const int s = UI_FONT_SCALE;
    const Color outline = { 20, 18, 28, 255 };
    const Color fill = { 248, 244, 236, 255 };
    const Color accent = { 255, 200, 60, 255 };
    const Color shadow = { 0, 0, 0, 90 };
    int ox = (int)m.x - art->hot_x * s, oy = (int)m.y - art->hot_y * s;

    /* 그림자 먼저 (오른쪽 아래로 한 칸) */
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < 16 && art->rows[y]; y++)
            for (int x = 0; art->rows[y][x]; x++) {
                char c = art->rows[y][x];
                if (c == ' ') continue;
                Color col = pass == 0 ? shadow
                          : c == 'X' ? outline : c == 'A' ? accent : c == 'C' ? dyn : fill;
                int d = pass == 0 ? s : 0;
                DrawRectangle(ox + x * s + d, oy + y * s + d, s, s, col);
            }
}

void cursor_draw(App *a) {
    if (!IsCursorOnScreen()) return;

    int kind = CURSOR_ARROW;
    if (!a->ui_hover) {
        kind = a->mode == MODE_TEXTURE ? paint_cursor(a) : -1;
        if (kind < 0) kind = model_cursor(a);
    }

    /* 스포이드는 커서 아래 텍셀 색, 나머지는 현재 색 */
    Color dyn = a->scene.palette[a->color];
    if (kind == CURSOR_PICKER) {
        int t = paint_hover_texel(a);
        if (t >= 0) dyn = a->scene.palette[t];
    }
    draw_art(&ART[kind], GetMousePosition(), dyn);
}
