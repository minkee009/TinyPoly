#ifndef VIEW_H
#define VIEW_H

#include "app/app.h"

/* 4분할 뷰포트: 배치, 카메라, 화면 ↔ 월드 좌표, GPU 텍스처 (view.c) */
void view_init(App *a);
void view_layout(App *a);
void view_update_cameras(App *a);
void view_upload_texture(App *a);
void view_tex_dirty(App *a, int x0, int y0, int x1, int y1);   /* 텍스처 일부만 바뀜 (텍셀, 양끝 포함) */
int  view_at(const App *a, Vector2 m);
Ray  view_ray(const Viewport *vp, Vector2 m);
bool view_project(const Viewport *vp, Vector3 p, Vector2 *out);
void view_axes(const Viewport *vp, Vector3 *right, Vector3 *up, Vector3 *normal);
float view_units_per_pixel(const App *a, const Viewport *vp);

/* 화면 좌표 m 아래의 면·정점 고르기 (view_pick.c) */
bool view_pick_face(App *a, const Viewport *vp, Vector2 m, int only, int *oi, int *fi);
int  view_pick_vert(App *a, const Viewport *vp, Vector2 m);
int  view_pick_vert_3d(App *a, const Viewport *vp, Vector2 m);   /* 강조 원 안의 보이는 정점 */

/* 뷰포트 하나 그리기: 메쉬, 모서리·정점 강조, 격자, 축, 박스 선택 (view_draw.c) */
void view_draw(App *a, Viewport *vp);

#endif
