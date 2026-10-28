#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <raylib.h>
#include <microui/microui.h>
#include <stdbool.h>
#include <stddef.h>

/* 앱 안에서 그리는 파일 탐색 창. OS와 상관없이 같은 모양/조작 */
typedef struct {
    bool open;
    bool save;                 /* 저장: 파일 이름 입력 + 덮어쓰기 확인 */
    int action;                /* 호출한 쪽이 정하는 값 (확인 시 그대로 돌려줌) */
    char title[64];
    char ext[16];              /* ".tpm" 등 */
    char dir[512];             /* 항상 '/' 구분자, 끝에 '/' 없음 (루트 제외) */
    char name[256];
    FilePathList list;
    bool listed;
    int selected;
    bool confirm;              /* 덮어쓰기 확인 중 */
    double last_click;
    int last_index;
} FileBrowser;

/* start: 처음 보여줄 파일 또는 폴더 경로 (NULL이면 작업 폴더) */
void fb_open(FileBrowser *fb, int action, const char *title, const char *ext, bool save, const char *start);
void fb_close(FileBrowser *fb);
/* 매 프레임 mu_begin/mu_end 사이에서 호출. 사용자가 확정하면 true와 함께 전체 경로를 out에 */
bool fb_update(FileBrowser *fb, mu_Context *ctx, char *out, size_t size);

#endif
