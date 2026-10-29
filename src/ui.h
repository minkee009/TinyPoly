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
    /* 선택에 따른 모델 동작 */
    UI_ICON_EXTRUDE, UI_ICON_DEL_FACE, UI_ICON_MORE, UI_ICON_MERGE, UI_ICON_SNAP, UI_ICON_DEL_VERT, UI_ICON_CENTER,
    /* 텍스처 도구줄 */
    UI_ICON_UV, UI_ICON_PEN, UI_ICON_PICK, UI_ICON_FILL, UI_ICON_LINE, UI_ICON_RECT, UI_ICON_RECT_FILL,
    UI_ICON_CIRCLE, UI_ICON_CIRCLE_FILL, UI_ICON_SELECT, UI_ICON_GRID, UI_ICON_ZOOM_OUT, UI_ICON_ZOOM_IN, UI_ICON_FIT,
    UI_ICON_END,
    /* 아이콘 명령으로 그리는 그림 (색 선택 창) */
    UI_GFX_SV = UI_ICON_END, UI_GFX_HUE, UI_GFX_ALPHA, UI_GFX_SWATCH,
};

/* microui에 없는 키 (MU_KEY_* 다음 비트) */
enum { UI_KEY_LEFT = 1 << 5, UI_KEY_RIGHT = 1 << 6, UI_KEY_HOME = 1 << 7, UI_KEY_END = 1 << 8, UI_KEY_DELETE = 1 << 9 };

void ui_init(mu_Context *ctx);
void ui_input(mu_Context *ctx);
void ui_render(mu_Context *ctx);

/* 아이콘 버튼: 잠시 올려두면 tip이 툴팁으로 뜸. active면 눌린 상태로 칠함.
   같은 아이콘을 같은 범위에서 여러 번 쓰면 mu_push_id로 구분할 것 */
bool ui_icon_button(mu_Context *ctx, int icon, const char *tip, bool active);
/* 사이에 만든 버튼을 흐리게 (눌러도 반응 없어 보이게). 클릭은 부르는 쪽에서 무시할 것 */
void ui_push_disabled(mu_Context *ctx);
void ui_pop_disabled(mu_Context *ctx);
/* 방금 만든 컨트롤에 마우스를 올리면 tip을 툴팁으로 */
void ui_tip(mu_Context *ctx, const char *tip);
/* 색 칸이 들어간 버튼 (지금 색을 보여주고 누르면 고르기) */
bool ui_swatch_button(mu_Context *ctx, Color c, const char *tip, bool active);
/* 켜진 상태를 강조색으로 보여주는 글자 버튼 */
bool ui_toggle_button(mu_Context *ctx, const char *label, bool active);

/* 툴팁: UI를 만들기 전에 begin, 화면에 그릴 때 draw (UI 위, 커서 아래) */
void ui_tooltip_begin(void);
void ui_tooltip_draw(void);

/* 모서리를 도트 계단으로 깎은 사각형 (raylib로 바로 그림) */
void ui_fill_round(Rectangle r, Color c);
/* 제목줄 오른쪽에 X가 있는 창 (MU_OPT_NOCLOSE 없이). X를 누르면 다음 프레임에 false + *closed = true.
   크기 조절이 되는 창은 오른쪽 아래에 손잡이 점을 그림. 끝은 ui_end_window로 */
bool ui_begin_window(mu_Context *ctx, const char *title, mu_Rect rect, int opt, bool *closed);
void ui_end_window(mu_Context *ctx);
/* raylib로 바로 그리는 글자 버튼. blocked면 마우스를 무시 (UI 창이 위에 있을 때) */
bool ui_button_rect(Rectangle r, const char *label, bool active, bool blocked);
/* 같은 방식의 아이콘 버튼 (툴팁 포함) */
bool ui_icon_button_rect(Rectangle r, int icon, const char *tip, bool active, bool blocked);

/* 글자 입력칸 (mu_textbox 대신): caret이 깜빡이고, 좌우·Home·End 키와 다시 클릭으로 caret을 옮김.
   끌거나 shift+이동 키로 범위 선택, 더블클릭은 단어·세 번은 전체 선택, ctrl+A/C/X/V.
   반환은 mu_textbox와 같음 (MU_RES_CHANGE, MU_RES_SUBMIT) */
int ui_textbox(mu_Context *ctx, char *buf, int bufsz);

/* 색 선택: 채도/명도 사각형 + 색상 막대 + 알파 막대. 바뀌면 true (h: 0~360, 나머지 0~1) */
bool ui_color_picker(mu_Context *ctx, float *h, float *s, float *v, float *alpha);
/* 투명도를 체크무늬로 보여주는 색 칸 */
void ui_swatch(mu_Context *ctx, mu_Rect r, Color c);

#endif
