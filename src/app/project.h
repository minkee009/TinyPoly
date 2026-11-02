#ifndef PROJECT_H
#define PROJECT_H

#include "app/app.h"

/* 프로젝트 파일(.tpm) 작업. 경로가 필요하면 앱 안의 파일 탐색 창을 띄우고,
   확정되면 ProjectBrowserUpdate에서 마저 처리한다 */
void ProjectNew(App *a);
void ProjectOpen(App *a);
void ProjectSave(App *a);              /* 아직 저장한 적 없으면 save_as */
void ProjectSaveAs(App *a);
void ProjectImportPNG(App *a);
void ProjectExport(App *a);            /* 내보내기 창의 설정(a->exp_*)대로 */
void ProjectHandleDrops(App *a);      /* 창에 떨군 .tpm/.png */

bool ProjectBrowserOpen(void);        /* 파일 탐색 창이 떠 있는지 */
void ProjectBrowserUpdate(App *a);    /* 매 프레임 mu_begin/mu_end 사이 */

#endif
