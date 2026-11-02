#include "panels.h"
#include "io.h"
#include "model_ops.h"
#include "project.h"
#include "texture.h"
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void panel_topbar(App *a) {
    mu_Context *ctx = a->ui;
    mu_Container *cnt = mu_get_container(ctx, "top");
    cnt->rect = mu_rect(0, 0, GetScreenWidth(), TOPBAR_H);
    /* 배경은 화면 끝까지 붙도록 app_draw에서 네모로 칠함 (둥근 모서리 없이) */
    if (!mu_begin_window_ex(ctx, "top", cnt->rect,
                            MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE | MU_OPT_NOSCROLL | MU_OPT_NOFRAME))
        return;
    /* [파일]  ·  [모델][텍스처]  ·  [추가][되돌리기][다시]  ·  [프로젝트][렌더][음영]  파일 이름
       빈 칸(label "")으로 묶음 사이를 띄운다 */
    const int B = 34, GAP = 22;
    mu_layout_row(ctx, 14, (int[]){ B, GAP, B, B, GAP, B, B, B, GAP, B, B, B, 12, -1 }, 22);
    if (ui_icon_button(ctx, UI_ICON_FOLDER, "file", a->menu == MENU_FILE)) app_open_menu(a, MENU_FILE);
    mu_label(ctx, "");
    if (ui_icon_button(ctx, UI_ICON_CUBE, "model mode (tab)", a->mode == MODE_MODEL)) a->mode = MODE_MODEL;
    if (ui_icon_button(ctx, UI_ICON_BRUSH, "texture mode (tab)", a->mode == MODE_TEXTURE)) a->mode = MODE_TEXTURE;
    mu_label(ctx, "");
    if (ui_icon_button(ctx, UI_ICON_PLUS, "add mesh", a->menu == MENU_ADD)) {
        a->add_at = (Vector3){ 0 };
        a->add_up = (Vector3){ 0, 1, 0 };
        app_open_menu(a, MENU_ADD);
    }
    if (ui_icon_button(ctx, UI_ICON_UNDO, "undo (ctrl+z)", false)) app_undo(a);
    if (ui_icon_button(ctx, UI_ICON_REDO, "redo (ctrl+y)", false)) app_redo(a);
    mu_label(ctx, "");
    if (ui_icon_button(ctx, UI_ICON_LIST, "project (O)", a->show_overview)) a->show_overview = !a->show_overview;
    static const int rm_icon[RENDER_COUNT] = { UI_ICON_RENDER_TEX, UI_ICON_RENDER_COLOR, UI_ICON_RENDER_WIRE };
    static const char *rm_tip[RENDER_COUNT] = { "render: textured (M)", "render: color (M)", "render: wireframe (M)" };
    if (ui_icon_button(ctx, rm_icon[a->render_mode], rm_tip[a->render_mode], false))
        a->render_mode = (a->render_mode + 1) % RENDER_COUNT;
    if (ui_icon_button(ctx, UI_ICON_SHADE, a->shading ? "shading: on (L)" : "shading: off (L)", a->shading))
        a->shading = !a->shading;
    mu_label(ctx, "");
    char buf[300];
    snprintf(buf, sizeof buf, "%s%s", GetFileName(a->path), a->dirty ? " *" : "");   /* 전체 경로는 창 제목에 */
    mu_label(ctx, buf);
    mu_end_window(ctx);
}

/* 왼쪽은 무엇을 골랐는지, 그다음 동작, 구분선 뒤는 설정 (우클릭 메뉴와 같은 기능) */
void panel_selection_bar(App *a, int *top) {
    mu_Context *ctx = a->ui;
    Object *o = app_active(a);
    if (!o) return;
    int nf = obj_count_selected(o, true), nv = obj_count_selected(o, false);

    char label[32];
    if (nf) snprintf(label, sizeof label, "%d %s", nf, nf == 1 ? "face" : "faces");
    else if (nv) snprintf(label, sizeof label, "%d %s", nv, nv == 1 ? "vertex" : "vertices");
    else snprintf(label, sizeof label, "%.16s", o->name);
    const int B = 34, SEP = 9, L = ctx->text_width(NULL, label, -1) + 14;
    const int sp = ctx->style->spacing, pad = ctx->style->padding;
    int w = nf ? L + B * 4 + SEP + sp * 5 : L + B * 3 + sp * 3;
    w += pad * 2;
    int h = 22 + pad * 2;
    mu_Container *cnt = mu_get_container(ctx, "selection");
    cnt->rect = mu_rect((GetScreenWidth() - w) / 2, GetScreenHeight() - h - 14, w, h);
    *top = cnt->rect.y;
    if (!mu_begin_window_ex(ctx, "selection", cnt->rect,
                            MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE | MU_OPT_NOSCROLL))
        return;
    if (nf) {
        mu_layout_row(ctx, 6, (int[]){ L, B, B, SEP, B, B }, 22);
        mu_label(ctx, label);
        if (ui_icon_button(ctx, UI_ICON_EXTRUDE, "extrude (E)", false)) model_extrude(a);
        if (ui_icon_button(ctx, UI_ICON_DEL_FACE, "delete face (X)", false)) model_delete_faces(a);
        mu_Rect r = mu_layout_next(ctx);
        mu_draw_rect(ctx, mu_rect(r.x + r.w / 2 - 1, r.y + 3, 2, r.h - 6), mu_color(84, 80, 108, 255));
        o = app_active(a);
        Face *f = o ? obj_first_selected_face(o) : NULL;
        if (f && ui_swatch_button(ctx, f->color, "face color (shown where untextured)", a->menu == MENU_FACECOLOR))
            app_open_menu(a, MENU_FACECOLOR);
        if (ui_icon_button(ctx, UI_ICON_MORE, "face options: double sided, shading... (F)", a->menu == MENU_FACEOPTS))
            app_open_menu(a, MENU_FACEOPTS);
    } else if (nv) {
        mu_layout_row(ctx, 4, (int[]){ L, B, B, B }, 22);
        mu_label(ctx, label);
        if (nv < 2) ui_push_disabled(ctx);
        bool merge = ui_icon_button(ctx, UI_ICON_MERGE, nv < 2 ? "merge: select 2+ vertices" : "merge vertices", false);
        if (nv < 2) ui_pop_disabled(ctx);
        else if (merge) model_merge(a);
        if (ui_icon_button(ctx, UI_ICON_SNAP, "snap to grid", false)) model_snap(a);
        if (ui_icon_button(ctx, UI_ICON_DEL_VERT, "delete vertex (del)", false)) model_delete_verts(a);
    } else {
        mu_layout_row(ctx, 4, (int[]){ L, B, B, B }, 22);
        mu_label(ctx, label);
        if (ui_icon_button(ctx, UI_ICON_CENTER, "center origin", false)) model_center_origin(a);
        if (ui_icon_button(ctx, UI_ICON_CLONE, "clone mesh", false)) model_clone(a);
        if (ui_icon_button(ctx, UI_ICON_TRASH, "delete mesh (ctrl+x)", false)) model_delete_mesh(a);
    }
    mu_end_window(ctx);
}

void panel_overview(App *a) {
    mu_Context *ctx = a->ui;
    bool closed;
    if (!ui_begin_window(ctx, "project (O)", mu_rect(GetScreenWidth() - 320, TOPBAR_H + 30, 300, 420), 0, &closed)) {
        if (closed) a->show_overview = 0;
        return;
    }
    Scene *s = &a->scene;
    static double last_click;
    static int last_idx = -1;
    int del_idx = -1;   /* 목록을 다 그린 뒤에 지움 */
    for (int i = 0; i < s->count; i++) {
        Object *o = &s->objs[i];
        mu_push_id(ctx, &i, sizeof i);
        mu_layout_row(ctx, 4, (int[]){ 30, 30, -34, -1 }, 0);
        if (ui_icon_button(ctx, o->hidden ? UI_ICON_EYE_OFF : UI_ICON_EYE,
                           o->hidden ? "hidden (click to show)" : "visible (click to hide)", false)) {
            app_mark(a);
            s->objs[i].hidden = !s->objs[i].hidden;
        }
        o = &s->objs[i];
        if (ui_icon_button(ctx, o->locked ? UI_ICON_LOCK : UI_ICON_UNLOCK,
                           o->locked ? "locked: can't click in views" : "unlocked (click to lock)", o->locked)) {
            app_mark(a);
            s->objs[i].locked = !s->objs[i].locked;
        }
        o = &s->objs[i];
        if (a->rename_idx == i) {
            if (ui_textbox(ctx, o->name, sizeof o->name) & MU_RES_SUBMIT) a->rename_idx = -1;
        } else {
            char label[48];
            snprintf(label, sizeof label, i == s->active ? "> %s" : "%s", o->name);
            if (mu_button_ex(ctx, label, 0, 0)) {
                /* 더블클릭: 이름 바꾸기 */
                if (last_idx == i && GetTime() - last_click < 0.4) a->rename_idx = i;
                last_idx = i;
                last_click = GetTime();
                app_select_mesh(a, i);
            }
            /* 우클릭: 그 메쉬를 고르고 메쉬 메뉴 */
            if (ctx->mouse_pressed == MU_MOUSE_RIGHT && mu_mouse_over(ctx, ctx->last_rect)) {
                if (i != s->active) app_select_mesh(a, i);
                app_open_menu(a, MENU_MESH);
            }
        }
        if (ui_icon_button(ctx, UI_ICON_TRASH, "delete mesh", false)) del_idx = i;
        mu_pop_id(ctx);
    }
    if (del_idx >= 0) {
        if (del_idx != s->active) app_select_mesh(a, del_idx);
        a->rename_idx = -1;
        model_delete_mesh(a);
    }
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    if (ui_icon_button(ctx, UI_ICON_PLUS, "add mesh", a->menu == MENU_ADD)) {
        a->add_at = (Vector3){ 0 };
        a->add_up = (Vector3){ 0, 1, 0 };
        app_open_menu(a, MENU_ADD);
    }
    ui_end_window(ctx);
}

void panel_color(App *a) {
    mu_Context *ctx = a->ui;
    bool closed;
    if (!ui_begin_window(ctx, "color (P)", mu_rect(GetScreenWidth() / 2 - 112, TOPBAR_H + 40, 225, 372),
                         MU_OPT_NORESIZE, &closed)) {
        if (closed) a->show_color = 0;
        return;
    }
    Scene *sc = &a->scene;
    Color *target = a->pick_target ? &a->color2 : &a->color;

    /* 바깥에서 색이 바뀌면(스포이드, 팔레트 클릭) HSV를 다시 맞춤. 회색이면 색상(h)은 유지 */
    static float h, s, v, al;
    static Color synced;
    static bool init;
    if (!init || !color_eq(*target, synced)) {
        float hh;
        color_to_hsv(*target, &hh, &s, &v);
        if (s > 0 || !init) h = hh;
        al = target->a / 255.0f;
        synced = *target;
        init = true;
    }

    mu_layout_row(ctx, 2, (int[]){ 101, -1 }, 0);
    if (ui_toggle_button(ctx, "color 1", a->pick_target == 0)) a->pick_target = 0;
    if (ui_toggle_button(ctx, "color 2", a->pick_target == 1)) a->pick_target = 1;
    target = a->pick_target ? &a->color2 : &a->color;

    if (ui_color_picker(ctx, &h, &s, &v, &al)) {
        *target = color_from_hsv(h, s, v, (unsigned char)lroundf(al * 255));
        synced = *target;
    }

    /* 미리보기 + 16진수 (RRGGBB 또는 RRGGBBAA) */
    static char hex[12];
    static bool hex_active;
    if (!hex_active) snprintf(hex, sizeof hex, "%02X%02X%02X%02X", target->r, target->g, target->b, target->a);
    mu_layout_row(ctx, 2, (int[]){ 40, -1 }, 24);
    ui_swatch(ctx, mu_layout_next(ctx), *target);
    int res = ui_textbox(ctx, hex, sizeof hex);
    hex_active = ctx->focus == ctx->last_id;
    if (res & MU_RES_CHANGE) {
        unsigned int r, g, b, al8 = 255;
        size_t len = strlen(hex);
        if ((len == 6 && sscanf(hex, "%2x%2x%2x", &r, &g, &b) == 3) ||
            (len == 8 && sscanf(hex, "%2x%2x%2x%2x", &r, &g, &b, &al8) == 4))
            *target = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)al8 };
    }

    /* 팔레트 관리 */
    bool has_sel = a->pal_sel >= 0 && a->pal_sel < sc->npalette;
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, has_sel ? "palette (swatch selected)" : "palette");
    mu_layout_row(ctx, 3, (int[]){ 66, 66, -1 }, 0);
    if (mu_button(ctx, "add") && sc->npalette < MAX_PALETTE) {
        app_mark(a);
        sc->palette[sc->npalette++] = *target;
        a->pal_sel = sc->npalette - 1;
    }
    if (mu_button(ctx, "set") && has_sel) {
        app_mark(a);
        sc->palette[a->pal_sel] = *target;
    }
    if (mu_button(ctx, "del") && has_sel) {
        app_mark(a);
        memmove(&sc->palette[a->pal_sel], &sc->palette[a->pal_sel + 1], sizeof(Color) * (size_t)(sc->npalette - a->pal_sel - 1));
        sc->npalette--;
        a->pal_sel = -1;
    }
    mu_layout_row(ctx, 2, (int[]){ 101, -1 }, 0);
    if (mu_button(ctx, "swap (X)")) { Color t = a->color; a->color = a->color2; a->color2 = t; }
    if (mu_button(ctx, "bg color")) {
        app_mark(a);
        sc->bg = (Color){ target->r, target->g, target->b, 255 };
    }
    ui_end_window(ctx);
}

void panel_export(App *a) {
    mu_Context *ctx = a->ui;
    int W = GetScreenWidth(), H = GetScreenHeight();
    bool closed;
    if (!ui_begin_window(ctx, "export", mu_rect(W / 2 - 200, H / 2 - 145, 400, 290), MU_OPT_NORESIZE, &closed)) {
        if (closed) a->show_export = 0;
        return;
    }
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, "3d file");
    mu_layout_row(ctx, 3, (int[]){ 120, 120, -1 }, 0);
    static const char *formats[EXPORT_FORMAT_COUNT] = { "obj + mtl", "gltf", "glb" };
    for (int i = 0; i < EXPORT_FORMAT_COUNT; i++)
        if (ui_toggle_button(ctx, formats[i], a->exp_format == i)) a->exp_format = i;

    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, "save as");
    mu_layout_row(ctx, 2, (int[]){ 182, -1 }, 0);
    if (ui_toggle_button(ctx, "folder", !a->exp_zip)) a->exp_zip = 0;
    if (ui_toggle_button(ctx, "zip", a->exp_zip)) a->exp_zip = 1;

    /* 무엇이 만들어지는지 미리 보여줌 */
    char base[96], names[4][96];
    snprintf(base, sizeof base, "%s", GetFileNameWithoutExt(a->path));
    int n = export_file_names((ExportFormat)a->exp_format, base, names, 4);
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, "creates:");
    char line[400];
    snprintf(line, sizeof line, a->exp_zip ? "  %s.zip" : "  %s/", base);
    mu_label(ctx, line);
    for (int i = 0; i < n; i++) {
        snprintf(line, sizeof line, "      %s", names[i]);
        mu_label(ctx, line);
    }
    mu_label(ctx, a->exp_format == EXPORT_GLB ? "  (texture is inside the glb)" : "  (png = texture)");

    mu_layout_row(ctx, 2, (int[]){ 182, -1 }, 0);
    if (mu_button(ctx, "export...")) project_export(a);
    if (mu_button(ctx, "cancel")) a->show_export = 0;
    ui_end_window(ctx);
}

/* 왼쪽 아래에 잠깐 떴다 사라짐 */
void panel_toast(const App *a) {
    if (!a->msg[0] || GetTime() - a->msg_time > 3.0) return;
    Vector2 ts = MeasureTextEx(GetFontDefault(), a->msg, 20, 2);
    Rectangle r = { 10, GetScreenHeight() - ts.y - 22, ts.x + 16, ts.y + 10 };
    ui_fill_round((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, (Color){ 255, 200, 60, 255 });
    ui_fill_round(r, (Color){ 20, 18, 28, 255 });
    DrawTextEx(GetFontDefault(), a->msg, (Vector2){ r.x + 8, r.y + 5 }, 20, 2, (Color){ 240, 236, 228, 255 });
}
