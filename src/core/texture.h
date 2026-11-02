#ifndef TEXTURE_H
#define TEXTURE_H

#include "core/mesh.h"

/* 음영 단계(0~2)만큼 어둡게. 플랫 셰이딩이라 단계가 뚝뚝 끊기는 도트 느낌 유지 */
float ShadeFactor(int level);
Color ShadeColor(Color c, int level);

bool ColorEq(Color a, Color b);

/* 그리기: buf는 w*h RGBA (미리보기용 사본에도 그릴 수 있게) */
typedef struct { Color *px; int w, h; } Canvas;
void TexPlot(Canvas c, int x, int y, Color col);
void TexLine(Canvas c, int x0, int y0, int x1, int y1, Color col);
void TexRect(Canvas c, int x0, int y0, int x1, int y1, Color col, bool filled);
void TexEllipse(Canvas c, int x0, int y0, int x1, int y1, Color col, bool filled);
void TexFill(Canvas c, int x, int y, Color col);

/* PNG 가져오기: 크기는 MAX_TEX_SIZE까지 그대로 (넘으면 잘림) */
bool TexImportPNG(Scene *s, const char *path);

/* 색 변환 (h: 0~360, s/v: 0~1) */
Color ColorFromHSVA(float h, float s, float v, unsigned char a);
void  ColorGetHSV(Color c, float *h, float *s, float *v);

#endif
