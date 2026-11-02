#ifndef PANELS_H
#define PANELS_H

#include "app/app.h"

/* 화면에 붙은 막대와 떠 있는 창들. toast 말고는 매 프레임 mu_begin/mu_end 사이 */
void PanelTopbar(App *a);
/* 면·정점(또는 메쉬만)을 고르면 뷰 아래쪽 가운데에 뜨는 막대. 막대가 있으면 위쪽 y를 *top에 */
void PanelSelectionBar(App *a, int *top);
void PanelOverview(App *a);            /* 메쉬 목록 창 "project (O)" */
void PanelColor(App *a);
void PanelExport(App *a);
void PanelToast(const App *a);         /* 저장/오류 같은 알림 (raylib로 바로 그림) */

#endif
