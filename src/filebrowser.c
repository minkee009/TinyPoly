#include "filebrowser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 목록 표시 순서 (폴더 먼저, 이름순). 탐색 창은 하나뿐이라 모듈 변수로 */
static int *g_order;
static int g_count;
static const FilePathList *g_sort_list;
static char g_drives[26];
static int g_ndrives;

static void normalize(char *p) {
    for (char *c = p; *c; c++)
        if (*c == '\\') *c = '/';
    size_t n = strlen(p);
    if (n == 2 && p[1] == ':') { p[2] = '/'; p[3] = '\0'; return; }   /* "C:" → "C:/" */
    while (n > 1 && p[n - 1] == '/' && !(n == 3 && p[1] == ':')) p[--n] = '\0';
}

static bool is_root(const char *p) {
    return strcmp(p, "/") == 0 || (strlen(p) == 3 && p[1] == ':' && p[2] == '/');
}

static void join(char *out, size_t size, const char *dir, const char *name) {
    size_t n = strlen(dir);
    snprintf(out, size, "%s%s%s", dir, (n && dir[n - 1] == '/') ? "" : "/", name);
}

static void parent(char *dir) {
    if (is_root(dir)) return;
    char *s = strrchr(dir, '/');
    if (!s) return;
    if (s == dir) dir[1] = '\0';                        /* "/home" → "/" */
    else if (s == dir + 2 && dir[1] == ':') s[1] = '\0'; /* "C:/x" → "C:/" */
    else *s = '\0';
}

static const char *base(const char *path) {
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

static bool is_dir_entry(const char *path) {
    return !IsPathFile(path);
}

static int cmp_entries(const void *pa, const void *pb) {
    const char *a = g_sort_list->paths[*(const int *)pa], *b = g_sort_list->paths[*(const int *)pb];
    bool da = is_dir_entry(a), db = is_dir_entry(b);
    if (da != db) return da ? -1 : 1;
    const char *x = base(a), *y = base(b);
    for (; *x && *y; x++, y++) {
        int d = tolower((unsigned char)*x) - tolower((unsigned char)*y);
        if (d) return d;
    }
    return (unsigned char)*x - (unsigned char)*y;
}

static void unload(FileBrowser *fb) {
    if (fb->listed) UnloadDirectoryFiles(fb->list);
    fb->listed = false;
    free(g_order);
    g_order = NULL;
    g_count = 0;
}

static void refresh(FileBrowser *fb) {
    unload(fb);
    fb->list = LoadDirectoryFiles(fb->dir);
    fb->listed = true;
    for (unsigned i = 0; i < fb->list.count; i++) normalize(fb->list.paths[i]);

    g_order = malloc(sizeof(int) * (fb->list.count + 1));
    for (unsigned i = 0; i < fb->list.count; i++) {
        const char *p = fb->list.paths[i];
        if (base(p)[0] == '.') continue;                       /* 숨김 파일 */
        /* 폴더 고르기 모드(ext가 빈 문자열)면 폴더만 */
        if (!is_dir_entry(p) && (!fb->ext[0] || !IsFileExtension(p, fb->ext))) continue;
        g_order[g_count++] = (int)i;
    }
    g_sort_list = &fb->list;
    qsort(g_order, (size_t)g_count, sizeof(int), cmp_entries);
    fb->selected = -1;
    fb->confirm = false;
}

static void set_dir(FileBrowser *fb, const char *dir) {
    snprintf(fb->dir, sizeof fb->dir, "%s", dir);
    normalize(fb->dir);
    refresh(fb);
}

void fb_open(FileBrowser *fb, int action, const char *title, const char *ext, bool save, const char *start) {
    unload(fb);
    fb->open = true;
    fb->save = save;
    fb->action = action;
    snprintf(fb->title, sizeof fb->title, "%s", title);
    snprintf(fb->ext, sizeof fb->ext, "%s", ext);
    fb->name[0] = '\0';
    fb->last_index = -1;

    char p[512];
    snprintf(p, sizeof p, "%s", start ? start : GetWorkingDirectory());
    normalize(p);
    if (!DirectoryExists(p) || (save && !ext[0])) {   /* 폴더 고르기는 그 폴더 이름을 기본값으로 */
        /* 파일 경로: 폴더는 그 위치, 이름은 저장할 때 기본값으로 */
        if (save) snprintf(fb->name, sizeof fb->name, "%s", base(p));
        parent(p);
    }
    if (!DirectoryExists(p)) {
        snprintf(p, sizeof p, "%s", GetWorkingDirectory());
        normalize(p);
    }

    g_ndrives = 0;
#ifdef _WIN32
    for (char c = 'C'; c <= 'Z'; c++) {
        char d[4] = { c, ':', '/', '\0' };
        if (DirectoryExists(d)) g_drives[g_ndrives++] = c;
    }
#endif
    set_dir(fb, p);
}

void fb_close(FileBrowser *fb) {
    unload(fb);
    fb->open = false;
}

/* 확정 시도. 결과 경로가 정해지면 true */
static bool submit(FileBrowser *fb, char *out, size_t size) {
    if (!fb->name[0]) return false;
    char full[1024];
    char typed[256];
    snprintf(typed, sizeof typed, "%s", fb->name);
    normalize(typed);
    /* 경로를 통째로 입력해도 됨 */
    if (typed[0] == '/' || (typed[0] && typed[1] == ':')) snprintf(full, sizeof full, "%s", typed);
    else join(full, sizeof full, fb->dir, typed);

    if (!fb->ext[0]) {
        /* 폴더 고르기: 이름이 곧 결과 폴더. 이미 있으면 안의 파일을 덮어쓸지 확인 */
        if (DirectoryExists(full) && !fb->confirm) {
            fb->confirm = true;
            return false;
        }
        snprintf(out, size, "%s", full);
        fb_close(fb);
        return true;
    }
    if (DirectoryExists(full)) {
        set_dir(fb, full);
        fb->name[0] = '\0';
        return false;
    }
    if (!IsFileExtension(full, fb->ext) && strlen(full) + strlen(fb->ext) < sizeof full) strcat(full, fb->ext);

    if (fb->save) {
        if (FileExists(full) && !fb->confirm) {
            fb->confirm = true;
            return false;
        }
    } else if (!FileExists(full)) {
        return false;
    }
    snprintf(out, size, "%s", full);
    fb_close(fb);
    return true;
}

bool fb_update(FileBrowser *fb, mu_Context *ctx, char *out, size_t size) {
    if (!fb->open) return false;
    if (IsKeyPressed(KEY_ESCAPE)) { fb_close(fb); return false; }

    int W = GetScreenWidth(), H = GetScreenHeight();
    mu_Rect r = mu_rect(W / 2 - 320, H / 2 - 250, 640, 500);
    if (!mu_begin_window_ex(ctx, "files", r, MU_OPT_NOCLOSE | MU_OPT_NORESIZE)) return false;
    mu_bring_to_front(ctx, mu_get_current_container(ctx));
    bool done = false;

    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, fb->title);

    /* 위로 / 홈 / 드라이브 / 현재 폴더 */
    int widths[32], n = 0;
    widths[n++] = 44;
    widths[n++] = 60;
    for (int i = 0; i < g_ndrives && n < 30; i++) widths[n++] = 36;
    widths[n++] = -1;
    mu_layout_row(ctx, n, widths, 0);
    if (mu_button(ctx, "up")) { char d[512]; snprintf(d, sizeof d, "%s", fb->dir); parent(d); set_dir(fb, d); }
    if (mu_button(ctx, "home")) {
        const char *h = getenv("USERPROFILE");
        if (!h) h = getenv("HOME");
        if (h && DirectoryExists(h)) set_dir(fb, h);
    }
    for (int i = 0; i < g_ndrives && i < 28; i++) {
        char label[4] = { g_drives[i], ':', '\0' };
        if (mu_button(ctx, label)) { char d[4] = { g_drives[i], ':', '/', '\0' }; set_dir(fb, d); }
    }
    size_t len = strlen(fb->dir);
    mu_label(ctx, len > 34 ? fb->dir + len - 34 : fb->dir);

    /* 목록: 폴더는 한 번 클릭으로 들어가고, 파일은 클릭 선택 / 더블클릭 확정 */
    mu_layout_row(ctx, 1, (int[]){ -1 }, -84);
    mu_begin_panel(ctx, "fblist");
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    if (g_count == 0) mu_label(ctx, "(empty)");
    for (int k = 0; k < g_count && fb->listed; k++) {
        int i = g_order[k];
        const char *p = fb->list.paths[i];
        bool dir = is_dir_entry(p);
        char label[300];
        snprintf(label, sizeof label, "%s%s%s", k == fb->selected ? "> " : "  ", base(p), dir ? "/" : "");
        mu_push_id(ctx, &k, sizeof k);
        if (mu_button_ex(ctx, label, 0, 0)) {
            if (dir) {
                char d[512];
                snprintf(d, sizeof d, "%s", p);
                set_dir(fb, d);
                mu_pop_id(ctx);
                break;                     /* 목록이 바뀌었으니 이번 프레임은 여기까지 */
            }
            bool dbl = fb->last_index == k && GetTime() - fb->last_click < 0.5;
            fb->selected = k;
            fb->last_index = k;
            fb->last_click = GetTime();
            fb->confirm = false;
            snprintf(fb->name, sizeof fb->name, "%s", base(p));
            if (dbl) done = submit(fb, out, size);
        }
        mu_pop_id(ctx);
        if (done) break;
    }
    mu_end_panel(ctx);

    if (!done && fb->open) {
        mu_layout_row(ctx, 2, (int[]){ 60, -1 }, 0);
        mu_label(ctx, "name");
        int res = mu_textbox(ctx, fb->name, sizeof fb->name);
        if (res & MU_RES_CHANGE) fb->confirm = false;     /* 이름을 고치면 덮어쓰기 확인 해제 */
        if (res & MU_RES_SUBMIT) done = submit(fb, out, size);
    }
    if (!done && fb->open) {
        mu_layout_row(ctx, 3, (int[]){ 300, 150, -1 }, 0);
        if (fb->confirm) {
            mu_label(ctx, fb->ext[0] ? "file exists. overwrite?" : "folder exists. overwrite?");
            if (mu_button(ctx, "overwrite")) done = submit(fb, out, size);
            if (mu_button(ctx, "no")) fb->confirm = false;
        } else {
            mu_label(ctx, "");
            if (mu_button(ctx, !fb->ext[0] ? "ok" : fb->save ? "save" : "open")) done = submit(fb, out, size);
            if (mu_button(ctx, "cancel")) fb_close(fb);
        }
    }
    mu_end_window(ctx);
    return done;
}
