#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <microui/microui.h>
#include <stdbool.h>

#define UI_FONT_SCALE 2   /* raylib 기본 폰트(10px)를 정수 배율로 키워 도트 느낌 유지 */

/* 도트 아이콘. microui 기본 아이콘 번호 다음부터 */
enum {
    UI_ICON_FOLDER = MU_ICON_MAX, UI_ICON_CUBE, UI_ICON_BRUSH, UI_ICON_PLUS,
    UI_ICON_UNDO, UI_ICON_REDO, UI_ICON_LIST,
    UI_ICON_RENDER_TEX, UI_ICON_RENDER_COLOR, UI_ICON_RENDER_WIRE, UI_ICON_SHADE,
    UI_ICON_EYE, UI_ICON_EYE_OFF, UI_ICON_LOCK, UI_ICON_UNLOCK,
    UI_ICON_CLONE, UI_ICON_TRASH, UI_ICON_CLOSE,
    UI_ICON_END,
    /* 아이콘 명령으로 그리는 그림 (색 선택 창) */
    UI_GFX_SV = UI_ICON_END, UI_GFX_HUE, UI_GFX_ALPHA, UI_GFX_SWATCH,
};

void ui_init(mu_Context *ctx);
void ui_input(mu_Context *ctx);
void ui_render(mu_Context *ctx);

/* 아이콘 버튼: 잠시 올려두면 tip이 툴팁으로 뜸. active면 눌린 상태로 칠함.
   같은 아이콘을 같은 범위에서 여러 번 쓰면 mu_push_id로 구분할 것 */
bool ui_icon_button(mu_Context *ctx, int icon, const char *tip, bool active);
/* 켜진 상태를 강조색으로 보여주는 글자 버튼 */
bool ui_toggle_button(mu_Context *ctx, const char *label, bool active);

/* 툴팁: UI를 만들기 전에 begin, 화면에 그릴 때 draw (UI 위, 커서 아래) */
void ui_tooltip_begin(void);
void ui_tooltip_draw(void);

/* 모서리를 2픽셀 깎은 사각형 (raylib로 바로 그림) */
void ui_fill_round(Rectangle r, Color c);
/* raylib로 바로 그리는 글자 버튼. blocked면 마우스를 무시 (UI 창이 위에 있을 때) */
bool ui_button_rect(Rectangle r, const char *label, bool active, bool blocked);

/* 색 선택: 채도/명도 사각형 + 색상 막대 + 알파 막대. 바뀌면 true (h: 0~360, 나머지 0~1) */
bool ui_color_picker(mu_Context *ctx, float *h, float *s, float *v, float *alpha);
/* 투명도를 체크무늬로 보여주는 색 칸 */
void ui_swatch(mu_Context *ctx, mu_Rect r, Color c);

#endif
