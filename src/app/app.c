#include "app/app.h"
#include "ui/cursor.h"
#include "core/io.h"
#include "ui/menu.h"
#include "model/model.h"
#include "paint/paint.h"
#include "ui/panels.h"
#include "app/project.h"
#include "ui/ui.h"
#include "view/view.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TINYPOLY_VERSION
#define TINYPOLY_VERSION "dev"     /* CMake가 프로젝트 버전으로 지정 */
#endif

void app_msg(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof a->msg, fmt, ap);
    va_end(ap);
    a->msg_time = GetTime();
}

void app_mark(App *a) {
    undo_push(&a->scene);
    a->dirty = true;
    a->tex_dirty = true;
}

static void after_history(App *a) {
    if (a->scene.active >= a->scene.count) a->scene.active = -1;
    a->dirty = true;
    a->tex_dirty = true;
}

void app_undo(App *a) {
    if (undo_undo(&a->scene)) after_history(a);
    else app_msg(a, "nothing to undo");
}

void app_redo(App *a) {
    if (undo_redo(&a->scene)) after_history(a);
    else app_msg(a, "nothing to redo");
}

Object *app_active(App *a) {
    return a->scene.active >= 0 ? &a->scene.objs[a->scene.active] : NULL;
}

void app_select_mesh(App *a, int i) {
    Object *o = app_active(a);
    if (o) obj_clear_selection(o);
    a->scene.active = i;
}

void app_open_menu(App *a, MenuKind m) {
    a->menu = m;
    a->open_menu = true;
}

/* ---- 수명 ---- */

void app_init(App *a) {
    scene_init(&a->scene);
    a->full = -1;
    a->yaw = 0.7f;
    a->pitch = 0.45f;
    a->dist = 5.0f;
    a->shading = 1;
    a->grid = 0.25f;
    a->snap = 1;
    a->pixel = 2;
    a->tool = TOOL_PEN;
    a->color = DEFAULT_PALETTE[12];
    a->color2 = DEFAULT_PALETTE[7];
    a->pal_sel = -1;
    a->tex_zoom = 0;                /* 창에 맞춤 */
    a->exp_format = EXPORT_GLB;
    a->tex_grid = 1;
    a->rename_idx = -1;
    a->model.key_view = VIEW_FRONT;
    snprintf(a->path, sizeof a->path, "model.tpm");

    a->ui = malloc(sizeof(mu_Context));
    ui_init(a->ui);
    view_init(a);

    a->add_up = (Vector3){ 0, 1, 0 };
    model_add_prim(a, PRIM_CUBE);
    undo_reset();
    a->dirty = false;
}

void app_shutdown(App *a) {
    scene_clear(&a->scene);
    undo_reset();
    free(a->model.orig);
    free(a->paint.backup);
    free(a->paint.float_px);
    free(a->paint.clip);
    free(a->paint.uv0);
    free(a->ui);
}

/* ---- 프레임 ---- */

static void global_keys(App *a) {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl) {
        if (IsKeyPressed(KEY_Z)) app_undo(a);
        if (IsKeyPressed(KEY_Y)) app_redo(a);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (IsKeyPressed(KEY_S)) { if (shift) project_save_as(a); else project_save(a); }
        if (IsKeyPressed(KEY_O)) project_open(a);
        return;
    }
    if (IsKeyPressed(KEY_TAB)) a->mode = a->mode == MODE_MODEL ? MODE_TEXTURE : MODE_MODEL;
    if (IsKeyPressed(KEY_O)) a->show_overview = !a->show_overview;
    if (IsKeyPressed(KEY_P)) a->show_color = !a->show_color;
}

static void build_ui(App *a) {
    static int bar_top;   /* 선택 막대의 위쪽 y (막대가 없는 프레임엔 지난 값) */
    mu_Context *ctx = a->ui;
    ui_tooltip_begin();
    mu_begin(ctx);
    panel_topbar(a);
    if (a->mode == MODE_MODEL) panel_selection_bar(a, &bar_top);
    if (a->show_overview) panel_overview(a);
    if (a->show_color) panel_color(a);
    if (a->show_export) panel_export(a);
    menu_update(a, bar_top);
    project_browser_update(a);
    mu_end(ctx);
}

static void update_title(const App *a) {
    static char title[512];
    char t[512];
    snprintf(t, sizeof t, "TinyPoly " TINYPOLY_VERSION " - %s%s", a->path, a->dirty ? " *" : "");
    if (strcmp(t, title) != 0) {
        SetWindowTitle(t);
        memcpy(title, t, sizeof title);
    }
}

void app_update(App *a) {
    /* 직전 프레임 기준: 마우스가 UI 창 위인지, 메뉴가 열려 있었는지 */
    a->ui_hover = a->ui->next_hover_root != NULL;
    bool menu_was_open = a->menu != MENU_NONE;
    /* 파일 탐색 창이 떠 있으면 단축키와 뷰 조작을 막는다 */
    bool typing = a->ui->focus != 0 || project_browser_open();
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    bool input_ok = !typing && !(menu_was_open && clicked);

    ui_input(a->ui);
    view_layout(a);
    view_update_cameras(a);

    if (!typing) global_keys(a);
    if (a->mode == MODE_MODEL) model_update(a, input_ok, true);
    else paint_update(a, input_ok);
    project_handle_drops(a);

    view_update_cameras(a);
    view_upload_texture(a);
    build_ui(a);
    update_title(a);
}

void app_draw(App *a) {
    BeginDrawing();
    ClearBackground((Color){ 12, 11, 18, 255 });
    view_upload_texture(a);
    if (a->mode == MODE_TEXTURE) paint_draw(a);
    for (int i = 0; i < VIEW_COUNT; i++)
        if (a->views[i].rect.width > 0) view_draw(a, &a->views[i]);
    DrawRectangle(0, 0, GetScreenWidth(), TOPBAR_H, (Color){ 34, 32, 44, 255 });
    panel_toast(a);
    ui_render(a->ui);
    ui_tooltip_draw();
    cursor_draw(a);
    EndDrawing();
}
