#include "app.h"

/* 앱 아이콘: 도트 아이소메트릭 큐브 (16x16).
   tools/make_icon.py가 이 그림을 읽어 Windows exe 아이콘(assets/icon.ico)을 만든다.
   K: 테두리, H: 빛 받는 모서리, T/U: 윗면 두 조각, L/M: 왼쪽 면 두 조각, R: 오른쪽 면, .: 투명
   (면을 조각내 칠해서 로우폴리 느낌) */
static const char *const ICON_ART[16] = {
    /* ICON_ART_BEGIN */
    "......KKKK......",
    "....KKHHUUKK....",
    "..KKHHTTUUUUKK..",
    "KKHTTTTTUUUUUUKK",
    "KLLTTTTTUUUUURRK",
    "KLLLLTTTUUURRRRK",
    "KHLLLLLTURRRRRRK",
    "KHLLLLLLRRRRRRRK",
    "KHMLLLLLRRRRRRRK",
    "KMMMLLLLRRRRRRRK",
    "KMMMMLLLRRRRRRRK",
    "KMMMMMLLRRRRRRRK",
    "KKMMMMMLRRRRRRKK",
    "..KKMMMMRRRRKK..",
    "....KKMMRRKK....",
    "......KKKK......",
    /* ICON_ART_END */
};

static Color icon_color(char ch) {
    switch (ch) {
    case 'K': return (Color){ 24, 20, 36, 255 };
    case 'H': return (Color){ 255, 244, 200, 255 };
    case 'T': return (Color){ 255, 214, 100, 255 };
    case 'U': return (Color){ 236, 172, 62, 255 };
    case 'L': return (Color){ 232, 150, 48, 255 };
    case 'M': return (Color){ 194, 108, 36, 255 };
    case 'R': return (Color){ 150, 84, 44, 255 };
    default:  return (Color){ 0, 0, 0, 0 };
    }
}

void app_set_icon(void) {
    /* 32x32로 키워서 창 아이콘에 (macOS는 창 아이콘이 없어서 무시됨) */
    const int s = 2;
    Image img = GenImageColor(16 * s, 16 * s, BLANK);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
            ImageDrawRectangle(&img, x * s, y * s, s, s, icon_color(ICON_ART[y][x]));
    SetWindowIcon(img);
    UnloadImage(img);
}
