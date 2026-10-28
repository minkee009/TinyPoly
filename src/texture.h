#ifndef TEXTURE_H
#define TEXTURE_H

#include "mesh.h"

/* 음영 단계(level)만큼 ramp를 따라간 팔레트 인덱스 */
int   tex_shade_index(const Scene *s, int idx, int level);
Color tex_shade_color(const Scene *s, int idx, int level);

/* 128x128 RGBA. 투명색 인덱스는 alpha 0 */
void tex_build_rgba(const Scene *s, int level, Color *out);

/* 그리기: buf는 TEX_W*TEX_H 인덱스 버퍼 (미리보기용 사본에도 그릴 수 있게) */
void tex_plot(unsigned char *buf, int x, int y, int c);
void tex_line(unsigned char *buf, int x0, int y0, int x1, int y1, int c);
void tex_rect(unsigned char *buf, int x0, int y0, int x1, int y1, int c, bool filled);
void tex_ellipse(unsigned char *buf, int x0, int y0, int x1, int y1, int c, bool filled);
void tex_fill(unsigned char *buf, int x, int y, int c);

void tex_auto_ramp(Scene *s);
void tex_swap_colors(Scene *s, int a, int b);     /* 팔레트 + 텍스처 픽셀 모두 */
void tex_make_gradient(Scene *s, int a, int b);   /* a~b 사이 팔레트 색을 보간 */

bool  tex_import_png(Scene *s, const char *path);
bool  tex_export_png(const Scene *s, const char *path);

/* 내보내기용: 텍스처 아래에 팔레트 띠(8px)를 붙인 이미지. 단색 면은 이 띠를 UV로 가리킴 */
#define ATLAS_H (TEX_H + 8)
Image   tex_atlas_image(const Scene *s);
Vector2 tex_atlas_color_uv(int idx);   /* 픽셀 좌표 (띠 칸 중앙) */

#endif
