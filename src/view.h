#ifndef VIEW_H
#define VIEW_H

#include "app.h"

/* 4분할 뷰포트: 배치, 카메라, 화면 ↔ 월드 좌표, GPU 텍스처 (고르기는 view_pick.h, 그리기는 view_draw.h) */
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

#endif
