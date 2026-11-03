#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

/* 창 만들기: 크기·최소 크기, 아이콘, 모니터에 맞추기, 종료 키 끄기, OS 커서 숨기기.
   실패하면 오류를 출력하고 false (window.c) */
bool WindowOpen(void);

/* 창 아이콘 (도트 큐브) (icon.c) */
void WindowSetIcon(void);

#endif
