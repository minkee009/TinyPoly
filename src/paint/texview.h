#ifndef TEXVIEW_H
#define TEXVIEW_H

#include "app/app.h"

/* 텍스처 모드 화면(a->canvas_rect): 위 도구줄, 가운데 캔버스, 아래 팔레트 줄 */
#define TOOLBAR_H 30
#define PALETTE_BAR_H 60

/* 캔버스 영역과 화면 ↔ 텍셀 좌표. 확대는 a->tex_zoom (0이면 창에 맞춤), 이동은 a->tex_pan */
Rectangle TexViewArea(const App *a);
float   TexViewScale(const App *a);                    /* 텍셀 하나의 화면 픽셀 수 */
Vector2 TexViewOrigin(const App *a);                   /* 텍스처 왼쪽 위의 화면 위치 */
Vector2 TexViewToTex(const App *a, Vector2 m);
Vector2 TexViewToScreen(const App *a, Vector2 t);
bool    TexViewTexel(const App *a, int x, int y, Color *out);   /* 텍스처 밖이면 false */
void    TexViewZoomTo(App *a, float z, Vector2 anchor);
void    TexViewZoomStep(App *a, int dir, Vector2 anchor);
void    TexViewZoomFit(App *a);

#endif
