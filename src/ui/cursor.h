#ifndef CURSOR_H
#define CURSOR_H

#include "app/app.h"

/* OS 커서 대신 그리는 도트 커서 (맨 마지막에 그림) */
enum {
    CURSOR_ARROW, CURSOR_CROSS, CURSOR_PEN, CURSOR_PICKER, CURSOR_BUCKET,
    CURSOR_MOVE, CURSOR_HAND, CURSOR_GRAB, CURSOR_ROTATE, CURSOR_SCALE, CURSOR_COUNT
};
void cursor_draw(App *a);

#endif
