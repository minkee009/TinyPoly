#ifndef UV_H
#define UV_H

#include "app/app.h"

/* 텍스처 모드 UV 도구: 활성 메쉬에서 고른 면들의 UV 편집 */
bool uv_has_target(App *a);             /* 활성 메쉬에 고른 면이 있는지 */
void uv_rotate(App *a);
void uv_flip(App *a, bool vertical);
void uv_reset(App *a);
void uv_copy(App *a);
void uv_paste(App *a);
void uv_nudge(App *a, Vector2 d);       /* 텍셀 단위로 옮김 */

/* 캔버스 마우스: press가 꼭짓점/면을 잡아 a->paint.drag를 PDRAG_UV_*로, 끄는 동안 apply_drag */
void uv_press(App *a, Vector2 m);
void uv_apply_drag(App *a, Vector2 m);
int  uv_cursor(App *a, Vector2 m);      /* UV 도구일 때 캔버스 위 커서 */
void uv_draw(App *a);                   /* 캔버스 위 UV 선 (캔버스 scissor 안에서) */

#endif
