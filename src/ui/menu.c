#include "ui/menu.h"
#include "model/model.h"
#include "paint/paint.h"
#include "app/project.h"
#include "core/texture.h"
#include "ui/ui.h"
#include "paint/uv.h"
#include <stdio.h>

#define SWATCH_SIZE 22   /* 면 색 메뉴의 색 칸 크기 (픽셀) */

static void ClosePopup(mu_Context *ctx) {
    mu_get_current_container(ctx)->open = 0;
}

/* 팔레트 칸: 좌클릭 1, 우클릭 2 반환 */
static int Swatch(mu_Context *ctx, int key, Color c, bool mark1, bool mark2) {
    mu_Id id = mu_get_id(ctx, &key, sizeof key);
    mu_Rect r = mu_layout_next(ctx);
    mu_update_control(ctx, id, r, 0);
    UISwatch(ctx, r, c);
    if (mark1) {
        mu_draw_box(ctx, r, mu_color(255, 255, 255, 255));
        mu_draw_box(ctx, mu_rect(r.x + 1, r.y + 1, r.w - 2, r.h - 2), mu_color(255, 255, 255, 255));
    }
    if (mark2) mu_draw_box(ctx, mu_rect(r.x + 3, r.y + 3, r.w - 6, r.h - 6), mu_color(0, 0, 0, 255));
    if (!mu_mouse_over(ctx, r)) return 0;
    if (ctx->mouse_pressed == MU_MOUSE_LEFT) return 1;
    if (ctx->mouse_pressed == MU_MOUSE_RIGHT) return 2;
    return 0;
}

static Face *FirstSelectedFace(App *a) {
    Object *o = AppActive(a);
    return o ? ObjFirstSelectedFace(o) : NULL;
}

static void FaceFlagCheckbox(App *a, const char *label, int flag, bool inverted) {
    Face *f = FirstSelectedFace(a);
    if (!f) return;
    int on = ((f->flags & flag) != 0) != inverted;
    /* mu_checkbox는 &on 주소로 id를 만드는데 네 칸 모두 같은 주소라 flag로 구분 */
    mu_push_id(a->ui, &flag, sizeof flag);
    int res = mu_checkbox(a->ui, label, &on);
    mu_pop_id(a->ui);
    if (res & MU_RES_CHANGE) ModelSetFaceFlag(a, flag, inverted ? !on : on);
}

static int g_menu_need;   /* 이번 프레임 메뉴 글자 중 가장 넓은 것 */

/* 메뉴용 버튼: 글자 폭을 재 두고 그냥 버튼 */
static int MenuButton(mu_Context *ctx, const char *label) {
    int w = ctx->text_width(NULL, label, -1) + ctx->style->padding * 2 + 8;
    if (w > g_menu_need) g_menu_need = w;
    return mu_button(ctx, label);
}

static void MenuBody(App *a, int width) {
    mu_Context *ctx = a->ui;
    const int w1[] = { width };
    const int third = (width - ctx->style->spacing * 2) / 3;
    const int w3[] = { third, third, third };
    Object *o = AppActive(a);

    switch (a->menu) {
    case MENU_ADD:
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "add mesh");
        for (int i = 0; i < PRIM_COUNT; i++)
            if (MenuButton(ctx, PRIM_NAMES[i])) { ModelAddPrim(a, i); ClosePopup(ctx); }
        break;
    case MENU_VERTEX: {
        int nv = o ? ObjCountSelected(o, false) : 0;
        char title[48];
        snprintf(title, sizeof title, "%d %s", nv, nv == 1 ? "vertex" : "vertices");
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, title);
        /* 합치기는 정점 2개 이상일 때만 */
        if (nv < 2) UIPushDisabled(ctx);
        bool merge = MenuButton(ctx, "merge");
        if (nv < 2) {
            UIPopDisabled(ctx);
            UITip(ctx, "select 2+ vertices to merge (box drag or shift+click)");
        } else if (merge) {
            ModelMerge(a);
            ClosePopup(ctx);
        }
        if (MenuButton(ctx, "snap to grid")) { ModelSnap(a); ClosePopup(ctx); }
        if (MenuButton(ctx, "delete (del)")) { ModelDeleteVerts(a); ClosePopup(ctx); }
        break;
    }
    case MENU_FACE:
    case MENU_MESH: {
        char title[64];
        if (a->menu == MENU_FACE) {
            int nf = o ? ObjCountSelected(o, true) : 0;
            snprintf(title, sizeof title, "%d %s", nf, nf == 1 ? "face" : "faces");
        } else {
            snprintf(title, sizeof title, "mesh: %s", o ? o->name : "");
        }
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, title);
        if (a->menu == MENU_FACE) {
            if (MenuButton(ctx, "extrude (E)"))     { ModelExtrude(a); ClosePopup(ctx); }
            if (MenuButton(ctx, "delete face (X)")) { ModelDeleteFaces(a); ClosePopup(ctx); }
            if (MenuButton(ctx, "face props (F)"))  a->menu = MENU_FACEPROPS;
        } else {
            if (MenuButton(ctx, "mesh props (G)"))  a->menu = MENU_MESHPROPS;
            if (MenuButton(ctx, "center origin"))   { ModelCenterOrigin(a); ClosePopup(ctx); }
        }
        /* 메쉬 이름 바꾸기: 이름 칸은 project 창에 있으므로 창도 연다 */
        if (MenuButton(ctx, "rename mesh"))         { a->rename_idx = a->scene.active; a->show_overview = 1; ClosePopup(ctx); }
        mu_label(ctx, "mirror");
        mu_layout_row(ctx, 3, w3, 0);
        if (MenuButton(ctx, "x")) { ModelMirror(a, 0); ClosePopup(ctx); }
        if (MenuButton(ctx, "y")) { ModelMirror(a, 1); ClosePopup(ctx); }
        if (MenuButton(ctx, "z")) { ModelMirror(a, 2); ClosePopup(ctx); }
        mu_layout_row(ctx, 1, w1, 0);
        if (MenuButton(ctx, "clone"))                 { ModelClone(a); ClosePopup(ctx); }
        if (MenuButton(ctx, "copy (ctrl+c)"))         { ModelCopy(a); ClosePopup(ctx); }
        if (MenuButton(ctx, "delete mesh (ctrl+x)"))  { ModelDeleteMesh(a); ClosePopup(ctx); }
        break;
    }
    case MENU_FACEPROPS:
    case MENU_FACECOLOR:
    case MENU_FACEOPTS: {
        /* F/우클릭은 둘 다, 선택 막대의 색 칸은 색만, [...]는 옵션만 */
        Face *f = FirstSelectedFace(a);
        if (!f) { ClosePopup(ctx); break; }
        if (a->menu != MENU_FACEOPTS) {
            mu_layout_row(ctx, 1, w1, 0);
            mu_label(ctx, "face color");
            mu_layout_row(ctx, 8, (int[]){ SWATCH_SIZE, SWATCH_SIZE, SWATCH_SIZE, SWATCH_SIZE,
                                           SWATCH_SIZE, SWATCH_SIZE, SWATCH_SIZE, SWATCH_SIZE },
                          SWATCH_SIZE);
            int shown = a->scene.npalette < 64 ? a->scene.npalette : 64;
            for (int i = 0; i < shown; i++)
                if (Swatch(ctx, 2000 + i, a->scene.palette[i], ColorEq(f->color, a->scene.palette[i]), false) == 1)
                    ModelSetFaceColor(a, a->scene.palette[i]);
            mu_layout_row(ctx, 2, (int[]){ SWATCH_SIZE, 175 }, 0);
            UISwatch(ctx, mu_layout_next(ctx), a->color);
            if (MenuButton(ctx, "use color 1")) ModelSetFaceColor(a, (Color){ a->color.r, a->color.g, a->color.b, 255 });
        }
        if (a->menu != MENU_FACECOLOR) {
            mu_layout_row(ctx, 1, w1, 0);
            FaceFlagCheckbox(a, "double sided", FACE_DOUBLE, false);
            FaceFlagCheckbox(a, "shaded", FACE_NOSHADE, true);
            FaceFlagCheckbox(a, "textured", FACE_NOTEX, true);
            FaceFlagCheckbox(a, "draw behind", FACE_BEHIND, false);
        }
        break;
    }
    case MENU_MESHPROPS: {
        if (!o) { ClosePopup(ctx); break; }
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "name");
        UITextbox(ctx, o->name, sizeof o->name);
        int hidden = o->hidden, locked = o->locked;
        if (mu_checkbox(ctx, "hidden", &hidden) & MU_RES_CHANGE) { AppMark(a); o = AppActive(a); o->hidden = hidden; }
        if (mu_checkbox(ctx, "locked", &locked) & MU_RES_CHANGE) { AppMark(a); o = AppActive(a); o->locked = locked; }
        if (MenuButton(ctx, "center to middle")) ModelCenterOrigin(a);
        break;
    }
    case MENU_UV:
        mu_layout_row(ctx, 1, w1, 0);
        if (MenuButton(ctx, "rotate (R)"))          { UVRotate(a); }
        if (MenuButton(ctx, "flip h (H)"))          { UVFlip(a, false); }
        if (MenuButton(ctx, "flip v (shift+H)"))    { UVFlip(a, true); }
        if (MenuButton(ctx, "reset (project)"))     { UVReset(a); }
        if (MenuButton(ctx, "copy uv (ctrl+c)"))    { UVCopy(a); ClosePopup(ctx); }
        if (MenuButton(ctx, "paste uv (ctrl+v)"))   { UVPaste(a); ClosePopup(ctx); }
        break;
    case MENU_TEXSIZE: {
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "texture size");
        static const int sizes[] = { 128, 256, 512, 1024 };
        for (int i = 0; i < 4; i++) {
            char l[24];
            int n = sizes[i];
            bool cur = a->scene.tex->w == n && a->scene.tex->h == n;
            snprintf(l, sizeof l, cur ? "[%dx%d]" : "%dx%d", n, n);
            if (MenuButton(ctx, l) && !cur) {
                AppMark(a);
                SceneResizeTex(&a->scene, n, n);
                PaintResetSelection(a);
                a->tex_zoom = 0;
                a->tex_pan = (Vector2){ 0 };
                a->tex_dirty = true;
                AppMsg(a, "texture %dx%d", n, n);
                ClosePopup(ctx);
            }
        }
        mu_label(ctx, "keeps top-left");
        break;
    }
    case MENU_FILE:
        mu_layout_row(ctx, 2, (int[]){ 128, 128 }, 0);
        if (MenuButton(ctx, "new"))           { ProjectNew(a); ClosePopup(ctx); }
        if (MenuButton(ctx, "open..."))       { ClosePopup(ctx); ProjectOpen(a); }
        if (MenuButton(ctx, "save"))          { ClosePopup(ctx); ProjectSave(a); }
        if (MenuButton(ctx, "save as..."))    { ClosePopup(ctx); ProjectSaveAs(a); }
        if (MenuButton(ctx, "import png...")) { ClosePopup(ctx); ProjectImportPNG(a); }
        if (MenuButton(ctx, "export..."))     { ClosePopup(ctx); a->show_export = 1; }
        mu_layout_row(ctx, 1, (int[]){ 260 }, 0);
        mu_label(ctx, "or drop .tpm/.png here");
        break;
    default:
        break;
    }
}

/* 메뉴 폭: 지난 프레임에 그 메뉴에서 가장 긴 글자에 맞춤 (처음 열 때 한 프레임만 기본 폭) */
static void MenuContents(App *a) {
    static int widths[MENU_TEXSIZE + 1];
    MenuKind k = a->menu;
    g_menu_need = 0;
    MenuBody(a, widths[k] > 200 ? widths[k] : 200);
    widths[k] = g_menu_need;
}

void MenuUpdate(App *a, int bar_top) {
    mu_Context *ctx = a->ui;
    if (a->open_menu) {
        mu_open_popup(ctx, "menu");
        a->open_menu = false;
    }
    mu_Container *pop = mu_get_container(ctx, "menu");
    if (pop->rect.x + pop->rect.w > GetScreenWidth() - 4) pop->rect.x = GetScreenWidth() - 4 - pop->rect.w;
    if (pop->rect.y + pop->rect.h > GetScreenHeight() - 4) pop->rect.y = GetScreenHeight() - 4 - pop->rect.h;
    if (a->menu == MENU_FACECOLOR || a->menu == MENU_FACEOPTS) pop->rect.y = bar_top - pop->rect.h - 6;
    if (mu_begin_popup(ctx, "menu")) {
        MenuContents(a);
        mu_end_popup(ctx);
    } else {
        a->menu = MENU_NONE;
    }
}
