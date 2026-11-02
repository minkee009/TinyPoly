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

void AppMsg(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->msg, sizeof a->msg, fmt, ap);
    va_end(ap);
    a->msg_time = GetTime();
}

void AppMark(App *a) {
    UndoPush(&a->scene);
    a->dirty = true;
    a->tex_dirty = true;
}

static void AfterHistory(App *a) {
    if (a->scene.active >= a->scene.count) a->scene.active = -1;
    a->dirty = true;
    a->tex_dirty = true;
}

void AppUndo(App *a) {
    if (UndoUndo(&a->scene)) AfterHistory(a);
    else AppMsg(a, "nothing to undo");
}

void AppRedo(App *a) {
    if (UndoRedo(&a->scene)) AfterHistory(a);
    else AppMsg(a, "nothing to redo");
}

Object *AppActive(App *a) {
    return a->scene.active >= 0 ? &a->scene.objs[a->scene.active] : NULL;
}

void AppSelectMesh(App *a, int i) {
    Object *o = AppActive(a);
    if (o) ObjClearSelection(o);
    a->scene.active = i;
}

void AppOpenMenu(App *a, MenuKind m) {
    a->menu = m;
    a->open_menu = true;
}

/* ---- 수명 ---- */

void AppInit(App *a) {
    SceneInit(&a->scene);
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
    UIInit(a->ui);
    ViewInit(a);

    a->add_up = (Vector3){ 0, 1, 0 };
    ModelAddPrim(a, PRIM_CUBE);
    UndoReset();
    a->dirty = false;
}

void AppShutdown(App *a) {
    SceneClear(&a->scene);
    UndoReset();
    free(a->model.orig);
    free(a->paint.backup);
    free(a->paint.float_px);
    free(a->paint.clip);
    free(a->paint.uv0);
    free(a->ui);
}

/* ---- 프레임 ---- */

static void GlobalKeys(App *a) {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl) {
        if (IsKeyPressed(KEY_Z)) AppUndo(a);
        if (IsKeyPressed(KEY_Y)) AppRedo(a);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (IsKeyPressed(KEY_S)) { if (shift) ProjectSaveAs(a); else ProjectSave(a); }
        if (IsKeyPressed(KEY_O)) ProjectOpen(a);
        return;
    }
    if (IsKeyPressed(KEY_TAB)) a->mode = a->mode == MODE_MODEL ? MODE_TEXTURE : MODE_MODEL;
    if (IsKeyPressed(KEY_O)) a->show_overview = !a->show_overview;
    if (IsKeyPressed(KEY_P)) a->show_color = !a->show_color;
}

static void BuildUI(App *a) {
    static int bar_top;   /* 선택 막대의 위쪽 y (막대가 없는 프레임엔 지난 값) */
    mu_Context *ctx = a->ui;
    UITooltipBegin();
    mu_begin(ctx);
    PanelTopbar(a);
    if (a->mode == MODE_MODEL) PanelSelectionBar(a, &bar_top);
    if (a->show_overview) PanelOverview(a);
    if (a->show_color) PanelColor(a);
    if (a->show_export) PanelExport(a);
    MenuUpdate(a, bar_top);
    ProjectBrowserUpdate(a);
    mu_end(ctx);
}

static void UpdateTitle(const App *a) {
    static char title[512];
    char t[512];
    snprintf(t, sizeof t, "TinyPoly " TINYPOLY_VERSION " - %s%s", a->path, a->dirty ? " *" : "");
    if (strcmp(t, title) != 0) {
        SetWindowTitle(t);
        memcpy(title, t, sizeof title);
    }
}

void AppUpdate(App *a) {
    /* 직전 프레임 기준: 마우스가 UI 창 위인지, 메뉴가 열려 있었는지 */
    a->ui_hover = a->ui->next_hover_root != NULL;
    bool menu_was_open = a->menu != MENU_NONE;
    /* 파일 탐색 창이 떠 있으면 단축키와 뷰 조작을 막는다 */
    bool typing = a->ui->focus != 0 || ProjectBrowserOpen();
    bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    bool input_ok = !typing && !(menu_was_open && clicked);

    UIInput(a->ui);
    ViewLayout(a);
    ViewUpdateCameras(a);

    if (!typing) GlobalKeys(a);
    if (a->mode == MODE_MODEL) ModelUpdate(a, input_ok, true);
    else PaintUpdate(a, input_ok);
    ProjectHandleDrops(a);

    ViewUpdateCameras(a);
    ViewUploadTexture(a);
    BuildUI(a);
    UpdateTitle(a);
}

void AppDraw(App *a) {
    BeginDrawing();
    ClearBackground((Color){ 12, 11, 18, 255 });
    ViewUploadTexture(a);
    if (a->mode == MODE_TEXTURE) PaintDraw(a);
    for (int i = 0; i < VIEW_COUNT; i++)
        if (a->views[i].rect.width > 0) ViewDraw(a, &a->views[i]);
    DrawRectangle(0, 0, GetScreenWidth(), TOPBAR_H, (Color){ 34, 32, 44, 255 });
    PanelToast(a);
    UIRender(a->ui);
    UITooltipDraw();
    CursorDraw(a);
    EndDrawing();
}
