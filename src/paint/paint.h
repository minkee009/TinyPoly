#ifndef PAINT_H
#define PAINT_H

#include "app/app.h"

/* 텍스처 모드: 그리기 도구, 마키 선택, 도구줄. 캔버스 좌표는 texview.h, UV는 uv.h, 팔레트 줄은 palbar.h */
void PaintUpdate(App *a, bool input_ok);
void PaintDraw(App *a);
void PaintResetSelection(App *a);     /* 텍스처 크기가 바뀌면 선택 해제 */
int  PaintCursor(App *a);              /* 캔버스 위에서 보여줄 커서, 해당 없으면 -1 */
bool PaintHoverTexel(const App *a, Color *out);   /* 커서 아래 텍셀 색, 캔버스 밖이면 false */

#endif
