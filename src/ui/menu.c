#include "ui/menu.h"
#include "model/model.h"
#include "paint/paint.h"
#include "app/project.h"
#include "core/texture.h"
#include "ui/ui.h"
#include "paint/uv.h"
#include <stdio.h>

static void close_popup(mu_Context *ctx) {
    mu_get_current_container(ctx)->open = 0;
}

/* 팔레트 칸: 좌클릭 1, 우클릭 2 반환 */
static int swatch(mu_Context *ctx, int key, Color c, bool mark1, bool mark2) {
    mu_Id id = mu_get_id(ctx, &key, sizeof key);
    mu_Rect r = mu_layout_next(ctx);
    mu_update_control(ctx, id, r, 0);
    ui_swatch(ctx, r, c);
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

static Face *first_selected_face(App *a) {
    Object *o = app_active(a);
    return o ? obj_first_selected_face(o) : NULL;
}

static void face_flag_checkbox(App *a, const char *label, int flag, bool inverted) {
    Face *f = first_selected_face(a);
    if (!f) return;
    int on = ((f->flags & flag) != 0) != inverted;
    /* mu_checkbox는 &on 주소로 id를 만드는데 네 칸 모두 같은 주소라 flag로 구분 */
    mu_push_id(a->ui, &flag, sizeof flag);
    int res = mu_checkbox(a->ui, label, &on);
    mu_pop_id(a->ui);
    if (res & MU_RES_CHANGE) model_set_face_flag(a, flag, inverted ? !on : on);
}

static int g_menu_need;   /* 이번 프레임 메뉴 글자 중 가장 넓은 것 */

/* 메뉴용 버튼: 글자 폭을 재 두고 그냥 버튼 */
static int menu_button(mu_Context *ctx, const char *label) {
    int w = ctx->text_width(NULL, label, -1) + ctx->style->padding * 2 + 8;
    if (w > g_menu_need) g_menu_need = w;
    return mu_button(ctx, label);
}

static void menu_body(App *a, int width) {
    mu_Context *ctx = a->ui;
    const int w1[] = { width };
    const int third = (width - ctx->style->spacing * 2) / 3;
    const int w3[] = { third, third, third };
    Object *o = app_active(a);

    switch (a->menu) {
    case MENU_ADD:
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "add mesh");
        for (int i = 0; i < PRIM_COUNT; i++)
            if (menu_button(ctx, PRIM_NAMES[i])) { model_add_prim(a, i); close_popup(ctx); }
        break;
    case MENU_VERTEX: {
        int nv = o ? obj_count_selected(o, false) : 0;
        char title[48];
        snprintf(title, sizeof title, "%d %s", nv, nv == 1 ? "vertex" : "vertices");
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, title);
        /* 합치기는 정점 2개 이상일 때만 */
        if (nv < 2) ui_push_disabled(ctx);
        bool merge = menu_button(ctx, "merge");
        if (nv < 2) {
            ui_pop_disabled(ctx);
            ui_tip(ctx, "select 2+ vertices to merge (box drag or shift+click)");
        } else if (merge) {
            model_merge(a);
            close_popup(ctx);
        }
        if (menu_button(ctx, "snap to grid")) { model_snap(a); close_popup(ctx); }
        if (menu_button(ctx, "delete (del)")) { model_delete_verts(a); close_popup(ctx); }
        break;
    }
    case MENU_FACE:
    case MENU_MESH: {
        char title[64];
        if (a->menu == MENU_FACE) {
            int nf = o ? obj_count_selected(o, true) : 0;
            snprintf(title, sizeof title, "%d %s", nf, nf == 1 ? "face" : "faces");
        } else {
            snprintf(title, sizeof title, "mesh: %s", o ? o->name : "");
        }
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, title);
        if (a->menu == MENU_FACE) {
            if (menu_button(ctx, "extrude (E)"))     { model_extrude(a); close_popup(ctx); }
            if (menu_button(ctx, "delete face (X)")) { model_delete_faces(a); close_popup(ctx); }
            if (menu_button(ctx, "face props (F)"))  a->menu = MENU_FACEPROPS;
        } else {
            if (menu_button(ctx, "mesh props (G)"))  a->menu = MENU_MESHPROPS;
            if (menu_button(ctx, "center origin"))   { model_center_origin(a); close_popup(ctx); }
        }
        /* 메쉬 이름 바꾸기: 이름 칸은 project 창에 있으므로 창도 연다 */
        if (menu_button(ctx, "rename mesh"))         { a->rename_idx = a->scene.active; a->show_overview = 1; close_popup(ctx); }
        mu_label(ctx, "mirror");
        mu_layout_row(ctx, 3, w3, 0);
        if (menu_button(ctx, "x")) { model_mirror(a, 0); close_popup(ctx); }
        if (menu_button(ctx, "y")) { model_mirror(a, 1); close_popup(ctx); }
        if (menu_button(ctx, "z")) { model_mirror(a, 2); close_popup(ctx); }
        mu_layout_row(ctx, 1, w1, 0);
        if (menu_button(ctx, "clone"))                 { model_clone(a); close_popup(ctx); }
        if (menu_button(ctx, "copy (ctrl+c)"))         { model_copy(a); close_popup(ctx); }
        if (menu_button(ctx, "delete mesh (ctrl+x)"))  { model_delete_mesh(a); close_popup(ctx); }
        break;
    }
    case MENU_FACEPROPS:
    case MENU_FACECOLOR:
    case MENU_FACEOPTS: {
        /* F/우클릭은 둘 다, 선택 막대의 색 칸은 색만, [...]는 옵션만 */
        Face *f = first_selected_face(a);
        if (!f) { close_popup(ctx); break; }
        if (a->menu != MENU_FACEOPTS) {
            mu_layout_row(ctx, 1, w1, 0);
            mu_label(ctx, "face color");
            mu_layout_row(ctx, 8, (int[]){ 22, 22, 22, 22, 22, 22, 22, 22 }, 22);
            int shown = a->scene.npalette < 64 ? a->scene.npalette : 64;
            for (int i = 0; i < shown; i++)
                if (swatch(ctx, 2000 + i, a->scene.palette[i], color_eq(f->color, a->scene.palette[i]), false) == 1)
                    model_set_face_color(a, a->scene.palette[i]);
            mu_layout_row(ctx, 2, (int[]){ 22, 175 }, 0);
            ui_swatch(ctx, mu_layout_next(ctx), a->color);
            if (menu_button(ctx, "use color 1")) model_set_face_color(a, (Color){ a->color.r, a->color.g, a->color.b, 255 });
        }
        if (a->menu != MENU_FACECOLOR) {
            mu_layout_row(ctx, 1, w1, 0);
            face_flag_checkbox(a, "double sided", FACE_DOUBLE, false);
            face_flag_checkbox(a, "shaded", FACE_NOSHADE, true);
            face_flag_checkbox(a, "textured", FACE_NOTEX, true);
            face_flag_checkbox(a, "draw behind", FACE_BEHIND, false);
        }
        break;
    }
    case MENU_MESHPROPS: {
        if (!o) { close_popup(ctx); break; }
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "name");
        ui_textbox(ctx, o->name, sizeof o->name);
        int hidden = o->hidden, locked = o->locked;
        if (mu_checkbox(ctx, "hidden", &hidden) & MU_RES_CHANGE) { app_mark(a); o = app_active(a); o->hidden = hidden; }
        if (mu_checkbox(ctx, "locked", &locked) & MU_RES_CHANGE) { app_mark(a); o = app_active(a); o->locked = locked; }
        if (menu_button(ctx, "center to middle")) model_center_origin(a);
        break;
    }
    case MENU_UV:
        mu_layout_row(ctx, 1, w1, 0);
        if (menu_button(ctx, "rotate (R)"))          { uv_rotate(a); }
        if (menu_button(ctx, "flip h (H)"))          { uv_flip(a, false); }
        if (menu_button(ctx, "flip v (shift+H)"))    { uv_flip(a, true); }
        if (menu_button(ctx, "reset (project)"))     { uv_reset(a); }
        if (menu_button(ctx, "copy uv (ctrl+c)"))    { uv_copy(a); close_popup(ctx); }
        if (menu_button(ctx, "paste uv (ctrl+v)"))   { uv_paste(a); close_popup(ctx); }
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
            if (menu_button(ctx, l) && !cur) {
                app_mark(a);
                scene_resize_tex(&a->scene, n, n);
                paint_reset_selection(a);
                a->tex_zoom = 0;
                a->tex_pan = (Vector2){ 0 };
                a->tex_dirty = true;
                app_msg(a, "texture %dx%d", n, n);
                close_popup(ctx);
            }
        }
        mu_label(ctx, "keeps top-left");
        break;
    }
    case MENU_FILE:
        mu_layout_row(ctx, 2, (int[]){ 128, 128 }, 0);
        if (menu_button(ctx, "new"))           { project_new(a); close_popup(ctx); }
        if (menu_button(ctx, "open..."))       { close_popup(ctx); project_open(a); }
        if (menu_button(ctx, "save"))          { close_popup(ctx); project_save(a); }
        if (menu_button(ctx, "save as..."))    { close_popup(ctx); project_save_as(a); }
        if (menu_button(ctx, "import png...")) { close_popup(ctx); project_import_png(a); }
        if (menu_button(ctx, "export..."))     { close_popup(ctx); a->show_export = 1; }
        mu_layout_row(ctx, 1, (int[]){ 260 }, 0);
        mu_label(ctx, "or drop .tpm/.png here");
        break;
    default:
        break;
    }
}

/* 메뉴 폭: 지난 프레임에 그 메뉴에서 가장 긴 글자에 맞춤 (처음 열 때 한 프레임만 기본 폭) */
static void menu_contents(App *a) {
    static int widths[MENU_TEXSIZE + 1];
    MenuKind k = a->menu;
    g_menu_need = 0;
    menu_body(a, widths[k] > 200 ? widths[k] : 200);
    widths[k] = g_menu_need;
}

void menu_update(App *a, int bar_top) {
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
        menu_contents(a);
        mu_end_popup(ctx);
    } else {
        a->menu = MENU_NONE;
    }
}
