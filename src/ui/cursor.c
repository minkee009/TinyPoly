#include "ui/cursor.h"
#include "model/model.h"
#include "paint/paint.h"
#include "ui/ui.h"

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
    /* 상하좌우 팔 길이가 같은 십자 */
    [CURSOR_CROSS] = { {
        "    XWX",
        "    XWX",
        "    XWX",
        "    XXX",
        "XXXX   XXXX",
        "WWWX A XWWW",
        "XXXX   XXXX",
        "    XXX",
        "    XWX",
        "    XWX",
        "    XWX",
    }, 5, 5 },
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

static void DrawArt(const CursorArt *art, Vector2 m, Color dyn) {
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

/* macOS는 커서 위에 창이 생기거나 창이 커서 아래로 커지면(타이틀바 더블클릭) enter 이벤트를
   안 보내서 IsCursorOnScreen()이 false로 남는다. 그동안 OS 커서는 이미 숨겨져 있으니,
   마우스가 창 안에서 움직이면 창 안에 있다고 대신 판단한다 */
static bool CursorVisible(void) {
    static bool was_on, stray;
    bool on = IsCursorOnScreen();
    if (on != was_on) {             /* 진짜 enter/leave가 오면 그쪽을 따른다 */
        was_on = on;
        stray = false;
        return on;
    }
    if (on) return true;
    Vector2 d = GetMouseDelta(), m = GetMousePosition();
    if (m.x < 0 || m.y < 0 || m.x >= GetScreenWidth() || m.y >= GetScreenHeight()) stray = false;
    else if (d.x != 0 || d.y != 0) stray = IsWindowFocused();
    return stray;
}

void CursorDraw(App *a) {
    if (!CursorVisible()) return;

    int kind = CURSOR_ARROW;
    if (!a->ui_hover) {
        kind = a->mode == MODE_TEXTURE ? PaintCursor(a) : -1;
        if (kind < 0) kind = ModelCursor(a);
    }

    /* 스포이드는 커서 아래 텍셀 색, 나머지는 현재 색 */
    Color dyn = a->color;
    if (kind == CURSOR_PICKER) PaintHoverTexel(a, &dyn);
    dyn.a = 255;                    /* 커서 안의 색은 불투명하게 */
    DrawArt(&ART[kind], GetMousePosition(), dyn);
}
