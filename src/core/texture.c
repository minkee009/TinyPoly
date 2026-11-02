#include "core/texture.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 처음 팔레트. 0~7 어두운 쪽, 8~15 밝은 쪽 */
const Color DEFAULT_PALETTE[16] = {
    {  20,  18,  28, 255 }, {  42,  46,  84, 255 }, { 118,  52, 102, 255 }, {  22,  96,  74, 255 },
    { 148,  86,  60, 255 }, {  88,  84,  92, 255 }, { 180, 178, 186, 255 }, { 248, 244, 236, 255 },
    { 230,  58,  70, 255 }, { 250, 152,  40, 255 }, { 252, 230,  70, 255 }, {  80, 210,  80, 255 },
    {  60, 160, 250, 255 }, { 132, 116, 190, 255 }, { 248, 128, 170, 255 }, { 252, 200, 164, 255 },
};

float ShadeFactor(int level) {
    static const float k[3] = { 1.0f, 0.78f, 0.58f };
    return k[level < 0 ? 0 : level > 2 ? 2 : level];
}

Color ShadeColor(Color c, int level) {
    float k = ShadeFactor(level);
    return (Color){ (unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), c.a };
}

bool ColorEq(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

/* ---- 그리기 ---- */

void TexPlot(Canvas c, int x, int y, Color col) {
    if (x >= 0 && y >= 0 && x < c.w && y < c.h) c.px[y * c.w + x] = col;
}

void TexLine(Canvas c, int x0, int y0, int x1, int y1, Color col) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        TexPlot(c, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void Order(int *a, int *b) {
    if (*a > *b) { int t = *a; *a = *b; *b = t; }
}

void TexRect(Canvas c, int x0, int y0, int x1, int y1, Color col, bool filled) {
    Order(&x0, &x1);
    Order(&y0, &y1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (filled || x == x0 || x == x1 || y == y0 || y == y1) TexPlot(c, x, y, col);
}

static bool InEllipse(float x, float y, float cx, float cy, float rx, float ry) {
    float u = (x - cx) / rx, v = (y - cy) / ry;
    return u * u + v * v <= 1.0f;
}

/* 바운딩 박스 안의 타원. 테두리는 안쪽 픽셀 중 4방향 이웃이 바깥인 것 */
void TexEllipse(Canvas c, int x0, int y0, int x1, int y1, Color col, bool filled) {
    Order(&x0, &x1);
    Order(&y0, &y1);
    float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
    float rx = (x1 - x0) / 2.0f + 0.5f, ry = (y1 - y0) / 2.0f + 0.5f;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!InEllipse((float)x, (float)y, cx, cy, rx, ry)) continue;
            bool edge = !InEllipse((float)x - 1, (float)y, cx, cy, rx, ry) ||
                        !InEllipse((float)x + 1, (float)y, cx, cy, rx, ry) ||
                        !InEllipse((float)x, (float)y - 1, cx, cy, rx, ry) ||
                        !InEllipse((float)x, (float)y + 1, cx, cy, rx, ry);
            if (filled || edge) TexPlot(c, x, y, col);
        }
}

/* 스캔라인 채우기: 큰 텍스처에서도 스택이 작게 유지됨 */
void TexFill(Canvas c, int x, int y, Color col) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h) return;
    Color target = c.px[y * c.w + x];
    if (ColorEq(target, col)) return;

    int cap = 1024, n = 0;
    int *stack = malloc(sizeof(int) * 2 * (size_t)cap);
    stack[n * 2] = x; stack[n * 2 + 1] = y; n++;
    while (n) {
        n--;
        int sx = stack[n * 2], sy = stack[n * 2 + 1];
        if (!ColorEq(c.px[sy * c.w + sx], target)) continue;
        int l = sx, r = sx;
        while (l > 0 && ColorEq(c.px[sy * c.w + l - 1], target)) l--;
        while (r < c.w - 1 && ColorEq(c.px[sy * c.w + r + 1], target)) r++;
        for (int i = l; i <= r; i++) c.px[sy * c.w + i] = col;
        /* 위아래 줄에서 새 구간 시작점만 넣는다 */
        for (int dy = -1; dy <= 1; dy += 2) {
            int ny = sy + dy;
            if (ny < 0 || ny >= c.h) continue;
            bool in = false;
            for (int i = l; i <= r; i++) {
                bool m = ColorEq(c.px[ny * c.w + i], target);
                if (m && !in) {
                    if (n == cap) { cap *= 2; stack = realloc(stack, sizeof(int) * 2 * (size_t)cap); }
                    stack[n * 2] = i; stack[n * 2 + 1] = ny; n++;
                }
                in = m;
            }
        }
    }
    free(stack);
}

/* ---- PNG ---- */

bool TexImportPNG(Scene *s, const char *path) {
    Image img = LoadImage(path);
    if (!IsImageValid(img)) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int w = img.width < MAX_TEX_SIZE ? img.width : MAX_TEX_SIZE;
    int h = img.height < MAX_TEX_SIZE ? img.height : MAX_TEX_SIZE;
    if (w < MIN_TEX_SIZE || h < MIN_TEX_SIZE) { UnloadImage(img); return false; }

    TexBuf *t = TexBufNew(w, h);
    const Color *src = img.data;
    for (int y = 0; y < h; y++)
        memcpy(&t->px[y * w], &src[y * img.width], sizeof(Color) * (size_t)w);
    UnloadImage(img);
    TexBufRelease(s->tex);
    s->tex = t;
    return true;
}

/* ---- HSV ---- */

Color ColorFromHSVA(float h, float s, float v, unsigned char a) {
    Vector3 rgb = { v, v, v };
    if (s > 0) {
        h = fmodf(h, 360.0f) / 60.0f;
        if (h < 0) h += 6.0f;
        int i = (int)h;
        float f = h - (float)i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
        switch (i) {
        case 0:  rgb = (Vector3){ v, t, p }; break;
        case 1:  rgb = (Vector3){ q, v, p }; break;
        case 2:  rgb = (Vector3){ p, v, t }; break;
        case 3:  rgb = (Vector3){ p, q, v }; break;
        case 4:  rgb = (Vector3){ t, p, v }; break;
        default: rgb = (Vector3){ v, p, q }; break;
        }
    }
    return (Color){ (unsigned char)lroundf(rgb.x * 255), (unsigned char)lroundf(rgb.y * 255),
                    (unsigned char)lroundf(rgb.z * 255), a };
}

void ColorGetHSV(Color c, float *h, float *s, float *v) {
    float r = c.r / 255.0f, g = c.g / 255.0f, b = c.b / 255.0f;
    float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
    *v = mx;
    *s = mx > 0 ? d / mx : 0;
    if (d <= 0) { *h = 0; return; }
    if (mx == r) *h = 60.0f * fmodf((g - b) / d, 6.0f);
    else if (mx == g) *h = 60.0f * ((b - r) / d + 2.0f);
    else *h = 60.0f * ((r - g) / d + 4.0f);
    if (*h < 0) *h += 360.0f;
}
