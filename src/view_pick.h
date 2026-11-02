#ifndef VIEW_PICK_H
#define VIEW_PICK_H

#include "app.h"

/* 뷰포트 화면 좌표 m 아래의 면·정점 고르기 */
bool view_pick_face(App *a, const Viewport *vp, Vector2 m, int only, int *oi, int *fi);
int  view_pick_vert(App *a, const Viewport *vp, Vector2 m);
int  view_pick_vert_3d(App *a, const Viewport *vp, Vector2 m);   /* 강조 원 안의 보이는 정점 */

#endif
