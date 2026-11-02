#ifndef VIEW_H
#define VIEW_H

#include "app/app.h"

/* 4분할 뷰포트: 배치, 카메라, 화면 ↔ 월드 좌표, GPU 텍스처 (view.c) */
void ViewInit(App *a);
void ViewLayout(App *a);
void ViewUpdateCameras(App *a);
void ViewUploadTexture(App *a);
void ViewTexDirty(App *a, int x0, int y0, int x1, int y1);   /* 텍스처 일부만 바뀜 (텍셀, 양끝 포함) */
int  ViewAt(const App *a, Vector2 m);
Ray  ViewRay(const Viewport *vp, Vector2 m);
bool ViewProject(const Viewport *vp, Vector3 p, Vector2 *out);
void ViewAxes(const Viewport *vp, Vector3 *right, Vector3 *up, Vector3 *normal);
float ViewUnitsPerPixel(const App *a, const Viewport *vp);

/* 화면 좌표 m 아래의 면·정점 고르기 (view_pick.c) */
bool ViewPickFace(App *a, const Viewport *vp, Vector2 m, int only, int *oi, int *fi);
int  ViewPickVert(App *a, const Viewport *vp, Vector2 m);
int  ViewPickVert3D(App *a, const Viewport *vp, Vector2 m);   /* 강조 원 안의 보이는 정점 */

/* 뷰포트 하나 그리기: 메쉬, 모서리·정점 강조, 격자, 축, 박스 선택 (view_draw.c) */
void ViewDraw(App *a, Viewport *vp);

#endif
