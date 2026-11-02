#ifndef PANELS_H
#define PANELS_H

#include "app.h"

/* 화면에 붙은 막대와 떠 있는 창들. toast 말고는 매 프레임 mu_begin/mu_end 사이 */
void panel_topbar(App *a);
/* 면·정점(또는 메쉬만)을 고르면 뷰 아래쪽 가운데에 뜨는 막대. 막대가 있으면 위쪽 y를 *top에 */
void panel_selection_bar(App *a, int *top);
void panel_overview(App *a);            /* 메쉬 목록 창 "project (O)" */
void panel_color(App *a);
void panel_export(App *a);
void panel_toast(const App *a);         /* 저장/오류 같은 알림 (raylib로 바로 그림) */

#endif
