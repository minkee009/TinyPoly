#ifndef UI_H
#define UI_H

#include <microui/microui.h>

#define UI_FONT_SCALE 2   /* raylib 기본 폰트(10px)를 정수 배율로 키워 도트 느낌 유지 */

void ui_init(mu_Context *ctx);
void ui_input(mu_Context *ctx);
void ui_render(mu_Context *ctx);

#endif
