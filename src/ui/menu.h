#ifndef MENU_H
#define MENU_H

#include "app/app.h"

/* a->menu 종류의 팝업 메뉴 (app_open_menu로 연다). 매 프레임 mu_begin/mu_end 사이.
   bar_top: 선택 막대의 위쪽 y (막대에서 연 메뉴를 그 위에 띄움) */
void menu_update(App *a, int bar_top);

#endif
