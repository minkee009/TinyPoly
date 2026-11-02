#ifndef PROJECT_H
#define PROJECT_H

#include "app.h"

/* 프로젝트 파일(.tpm) 작업. 경로가 필요하면 앱 안의 파일 탐색 창을 띄우고,
   확정되면 project_browser_update에서 마저 처리한다 */
void project_new(App *a);
void project_open(App *a);
void project_save(App *a);              /* 아직 저장한 적 없으면 save_as */
void project_save_as(App *a);
void project_import_png(App *a);
void project_export(App *a);            /* 내보내기 창의 설정(a->exp_*)대로 */
void project_handle_drops(App *a);      /* 창에 떨군 .tpm/.png */

bool project_browser_open(void);        /* 파일 탐색 창이 떠 있는지 */
void project_browser_update(App *a);    /* 매 프레임 mu_begin/mu_end 사이 */

#endif
