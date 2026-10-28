#include "app.h"
#include "io.h"
#include "filebrowser.h"
#include "texture.h"
#include "ui.h"
#include <raymath.h>

#ifndef TINYPOLY_VERSION
#define TINYPOLY_VERSION "dev"     /* CMake가 프로젝트 버전으로 지정 */
#endif
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ---- 파일 ---- */

static void file_new(App *a) {
    app_mark(a);
    scene_release(&a->scene);
    scene_init(&a->scene);
    paint_reset_selection();
    app_msg(a, "new project");
}

static void save_to(App *a, const char *path) {
    if (scene_save(&a->scene, path)) {
        if (path != a->path) snprintf(a->path, sizeof a->path, "%s", path);
        a->has_path = true;
        a->dirty = false;
        app_msg(a, "saved %s", a->path);
    } else {
        app_msg(a, "save failed: %s", path);
    }
}

/* 파일 탐색 창이 무엇을 위해 열렸는지 */
enum { FB_OPEN, FB_SAVE, FB_IMPORT_PNG, FB_EXPORT };
static FileBrowser g_fb;

static void file_save_as(App *a) {
    fb_open(&g_fb, FB_SAVE, "save project", ".tpm", true, a->path);
}

/* 아직 저장한 적 없는 프로젝트면 위치를 묻는다 */
static void file_save(App *a) {
    if (a->has_path) save_to(a, a->path);
    else file_save_as(a);
}

static void open_path(App *a, const char *path) {
    if (scene_load(&a->scene, path)) {
        if (path != a->path) snprintf(a->path, sizeof a->path, "%s", path);
        a->has_path = true;
        undo_reset();
        paint_reset_selection();
        a->dirty = false;
        a->tex_dirty = true;
        a->pal_sel = -1;
        app_msg(a, "opened %s", a->path);
    } else {
        app_msg(a, "open failed: %s", path);
    }
}

static void file_open(App *a) {
    fb_open(&g_fb, FB_OPEN, "open project", ".tpm", false, a->has_path ? a->path : NULL);
}

static void import_png(App *a, const char *path) {
    app_mark(a);
    if (tex_import_png(&a->scene, path)) {
        paint_reset_selection();
        a->tex_dirty = true;
        app_msg(a, "imported %s (%dx%d)", path, a->scene.tex->w, a->scene.tex->h);
    } else {
        app_msg(a, "import failed: %s", path);
    }
}

static void file_import_png(App *a) {
    fb_open(&g_fb, FB_IMPORT_PNG, "import texture (png)", ".png", false, a->has_path ? a->path : NULL);
}

/* 내보내기 창에서 "export..": 프로젝트 이름을 기본값으로 폴더 이름 또는 zip 파일 위치를 묻는다 */
static void file_export(App *a) {
    char start[sizeof a->path];
    path_with_ext(start, sizeof start, a->path, a->exp_zip ? ".zip" : "");
    if (a->exp_zip) fb_open(&g_fb, FB_EXPORT, "export to zip", ".zip", true, start);
    else fb_open(&g_fb, FB_EXPORT, "export to folder (type a folder name)", "", true, start);
}

static void export_to(App *a, const char *out) {
    bool ok = export_scene(&a->scene, (ExportFormat)a->exp_format, a->exp_zip, out);
    app_msg(a, ok ? "exported %s" : "export failed: %s", out);
    if (ok) a->show_export = 0;
}

/* 파일 탐색 창에서 경로가 확정되면 */
static void file_browser_done(App *a, int action, const char *path) {
    switch (action) {
    case FB_OPEN:       open_path(a, path); break;
    case FB_SAVE:       save_to(a, path); break;
    case FB_IMPORT_PNG: import_png(a, path); break;
    case FB_EXPORT:     export_to(a, path); break;
    }
}

static void handle_dropped_files(App *a) {
    if (!IsFileDropped()) return;
    FilePathList files = LoadDroppedFiles();
    if (files.count > 0) {
        const char *p = files.paths[0];
        if (IsFileExtension(p, ".tpm")) {
            open_path(a, p);
        } else if (IsFileExtension(p, ".png")) {
            import_png(a, p);
        } else {
            app_msg(a, "drop a .tpm project or a .png texture");
        }
    }
    UnloadDroppedFiles(files);
}

/* ---- UI 조각 ---- */

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
    if (!o) return NULL;
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) return &o->faces[i];
    return NULL;
}

static void face_flag_checkbox(App *a, const char *label, int flag, bool inverted) {
    Face *f = first_selected_face(a);
    if (!f) return;
    int on = ((f->flags & flag) != 0) != inverted;
    if (mu_checkbox(a->ui, label, &on) & MU_RES_CHANGE) model_set_face_flag(a, flag, inverted ? !on : on);
}

/* 활성 메쉬의 원점(회전·크기 기준점)을 정점들의 가운데로 */
static void center_origin(App *a) {
    if (!app_active(a)) return;
    app_mark(a);
    Object *o = app_active(a);
    Vector3 mn = { 1e9f, 1e9f, 1e9f }, mx = { -1e9f, -1e9f, -1e9f };
    for (int i = 0; i < o->nverts; i++) { mn = Vector3Min(mn, o->verts[i].p); mx = Vector3Max(mx, o->verts[i].p); }
    if (o->nverts) o->origin = Vector3Scale(Vector3Add(mn, mx), 0.5f);
}

static void menu_contents(App *a) {
    mu_Context *ctx = a->ui;
    const int w1[] = { 200 };
    const int w3[] = { 64, 64, 64 };
    Object *o = app_active(a);

    switch (a->menu) {
    case MENU_ADD:
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "add mesh");
        for (int i = 0; i < PRIM_COUNT; i++)
            if (mu_button(ctx, PRIM_NAMES[i])) { model_add_prim(a, i); close_popup(ctx); }
        break;
    case MENU_VERTEX:
        mu_layout_row(ctx, 1, w1, 0);
        if (mu_button(ctx, "merge"))        { model_merge(a); close_popup(ctx); }
        if (mu_button(ctx, "snap to grid")) { model_snap(a); close_popup(ctx); }
        if (mu_button(ctx, "delete (del)")) { model_delete_verts(a); close_popup(ctx); }
        break;
    case MENU_FACE:
    case MENU_MESH:
        mu_layout_row(ctx, 1, w1, 0);
        if (a->menu == MENU_FACE) {
            if (mu_button(ctx, "extrude (E)"))     { model_extrude(a); close_popup(ctx); }
            if (mu_button(ctx, "delete face (X)")) { model_delete_faces(a); close_popup(ctx); }
            if (mu_button(ctx, "face props (F)"))  a->menu = MENU_FACEPROPS;
        } else {
            if (mu_button(ctx, "mesh props (G)"))  a->menu = MENU_MESHPROPS;
        }
        mu_label(ctx, "mirror");
        mu_layout_row(ctx, 3, w3, 0);
        if (mu_button(ctx, "x")) { model_mirror(a, 0); close_popup(ctx); }
        if (mu_button(ctx, "y")) { model_mirror(a, 1); close_popup(ctx); }
        if (mu_button(ctx, "z")) { model_mirror(a, 2); close_popup(ctx); }
        mu_layout_row(ctx, 1, w1, 0);
        if (mu_button(ctx, "clone"))                 { model_clone(a); close_popup(ctx); }
        if (mu_button(ctx, "copy (ctrl+c)"))         { model_copy(a); close_popup(ctx); }
        if (mu_button(ctx, "delete mesh (ctrl+x)"))  { model_delete_mesh(a); close_popup(ctx); }
        break;
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
            if (mu_button(ctx, "use color 1")) model_set_face_color(a, (Color){ a->color.r, a->color.g, a->color.b, 255 });
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
        mu_textbox(ctx, o->name, sizeof o->name);
        int hidden = o->hidden, locked = o->locked;
        if (mu_checkbox(ctx, "hidden", &hidden) & MU_RES_CHANGE) { app_mark(a); o = app_active(a); o->hidden = hidden; }
        if (mu_checkbox(ctx, "locked", &locked) & MU_RES_CHANGE) { app_mark(a); o = app_active(a); o->locked = locked; }
        if (mu_button(ctx, "center to middle")) center_origin(a);
        break;
    }
    case MENU_UV:
        mu_layout_row(ctx, 1, w1, 0);
        if (mu_button(ctx, "rotate (R)"))          { uv_rotate(a); }
        if (mu_button(ctx, "flip h (H)"))          { uv_flip(a, false); }
        if (mu_button(ctx, "flip v (shift+H)"))    { uv_flip(a, true); }
        if (mu_button(ctx, "reset (project)"))     { uv_reset(a); }
        if (mu_button(ctx, "copy uv (ctrl+c)"))    { uv_copy(a); close_popup(ctx); }
        if (mu_button(ctx, "paste uv (ctrl+v)"))   { uv_paste(a); close_popup(ctx); }
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
            if (mu_button(ctx, l) && !cur) {
                app_mark(a);
                scene_resize_tex(&a->scene, n, n);
                paint_reset_selection();
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
        if (mu_button(ctx, "new"))           { file_new(a); close_popup(ctx); }
        if (mu_button(ctx, "open..."))       { close_popup(ctx); file_open(a); }
        if (mu_button(ctx, "save"))          { close_popup(ctx); file_save(a); }
        if (mu_button(ctx, "save as..."))    { close_popup(ctx); file_save_as(a); }
        if (mu_button(ctx, "import png...")) { close_popup(ctx); file_import_png(a); }
        if (mu_button(ctx, "export..."))     { close_popup(ctx); a->show_export = 1; }
        mu_layout_row(ctx, 1, (int[]){ 260 }, 0);
        mu_label(ctx, "or drop .tpm/.png here");
        break;
    default:
        break;
    }
}

static void draw_topbar(App *a) {
    mu_Context *ctx = a->ui;
    mu_Container *cnt = mu_get_container(ctx, "top");
    cnt->rect = mu_rect(0, 0, GetScreenWidth(), TOPBAR_H);
    /* 배경은 화면 끝까지 붙도록 main에서 네모로 칠함 (둥근 모서리 없이) */
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

/* 면·정점을 고르면 뷰 아래쪽 가운데에 뜨는 작은 막대.
   왼쪽은 무엇을 골랐는지, 그다음 동작, 구분선 뒤는 설정 (우클릭 메뉴와 같은 기능) */
static int g_bar_top;   /* 선택 막대의 위쪽 y (막대에서 연 팝업을 그 위에 띄움) */

static void draw_selection_bar(App *a) {
    mu_Context *ctx = a->ui;
    Object *o = app_active(a);
    int nf = 0, nv = 0;
    if (o) {
        for (int i = 0; i < o->nfaces; i++) nf += o->faces[i].sel;
        for (int i = 0; i < o->nverts; i++) nv += o->verts[i].sel;
    }
    if (!nf && !nv) return;

    char label[32];
    if (nf) snprintf(label, sizeof label, "%d %s", nf, nf == 1 ? "face" : "faces");
    else snprintf(label, sizeof label, "%d %s", nv, nv == 1 ? "vertex" : "verts");
    const int B = 34, SEP = 9, L = ctx->text_width(NULL, label, -1) + 14;
    const int sp = ctx->style->spacing, pad = ctx->style->padding;
    int w = nf ? L + B * 4 + SEP + sp * 5 : L + B * 3 + sp * 3;
    w += pad * 2;
    int h = 22 + pad * 2;
    mu_Container *cnt = mu_get_container(ctx, "selection");
    cnt->rect = mu_rect((GetScreenWidth() - w) / 2, GetScreenHeight() - h - 14, w, h);
    g_bar_top = cnt->rect.y;
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
        Face *f = first_selected_face(a);
        if (f && ui_swatch_button(ctx, f->color, "face color (shown where untextured)", a->menu == MENU_FACECOLOR))
            app_open_menu(a, MENU_FACECOLOR);
        if (ui_icon_button(ctx, UI_ICON_MORE, "face options: double sided, shading... (F)", a->menu == MENU_FACEOPTS))
            app_open_menu(a, MENU_FACEOPTS);
    } else {
        mu_layout_row(ctx, 4, (int[]){ L, B, B, B }, 22);
        mu_label(ctx, label);
        if (ui_icon_button(ctx, UI_ICON_MERGE, "merge vertices", false)) model_merge(a);
        if (ui_icon_button(ctx, UI_ICON_SNAP, "snap to grid", false)) model_snap(a);
        if (ui_icon_button(ctx, UI_ICON_DEL_VERT, "delete vertex (del)", false)) model_delete_verts(a);
    }
    mu_end_window(ctx);
}

static void draw_overview(App *a) {
    mu_Context *ctx = a->ui;
    bool closed;
    if (!ui_begin_window(ctx, "project (O)", mu_rect(GetScreenWidth() - 320, TOPBAR_H + 30, 300, 420), 0, &closed)) {
        if (closed) a->show_overview = 0;
        return;
    }
    Scene *s = &a->scene;
    static double last_click;
    static int last_idx = -1;
    for (int i = 0; i < s->count; i++) {
        Object *o = &s->objs[i];
        mu_push_id(ctx, &i, sizeof i);
        mu_layout_row(ctx, 3, (int[]){ 30, 30, -1 }, 0);
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
            if (mu_textbox(ctx, o->name, sizeof o->name) & MU_RES_SUBMIT) a->rename_idx = -1;
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
        }
        mu_pop_id(ctx);
    }
    /* 선택한 메쉬: 원점 가운데로 / 복제 / 삭제 */
    mu_layout_row(ctx, 3, (int[]){ 34, 34, 34 }, 0);
    if (ui_icon_button(ctx, UI_ICON_CENTER, "center origin of selected mesh", false)) center_origin(a);
    if (ui_icon_button(ctx, UI_ICON_CLONE, "clone selected mesh", false)) model_clone(a);
    if (ui_icon_button(ctx, UI_ICON_TRASH, "delete selected mesh (ctrl+x)", false)) model_delete_mesh(a);
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, "double-click: rename");
    ui_end_window(ctx);
}

static void draw_color_window(App *a) {
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
    int res = mu_textbox(ctx, hex, sizeof hex);
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

static void draw_export_window(App *a) {
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
    if (mu_button(ctx, "export...")) file_export(a);
    if (mu_button(ctx, "cancel")) a->show_export = 0;
    ui_end_window(ctx);
}

static void build_ui(App *a) {
    mu_Context *ctx = a->ui;
    ui_tooltip_begin();
    mu_begin(ctx);
    draw_topbar(a);
    if (a->mode == MODE_MODEL) draw_selection_bar(a);
    if (a->show_overview) draw_overview(a);
    if (a->show_color) draw_color_window(a);
    if (a->show_export) draw_export_window(a);

    if (a->open_menu) {
        mu_open_popup(ctx, "menu");
        a->open_menu = false;
    }
    mu_Container *pop = mu_get_container(ctx, "menu");
    if (pop->rect.x + pop->rect.w > GetScreenWidth() - 4) pop->rect.x = GetScreenWidth() - 4 - pop->rect.w;
    if (pop->rect.y + pop->rect.h > GetScreenHeight() - 4) pop->rect.y = GetScreenHeight() - 4 - pop->rect.h;
    if (a->menu == MENU_FACECOLOR || a->menu == MENU_FACEOPTS) pop->rect.y = g_bar_top - pop->rect.h - 6;
    if (mu_begin_popup(ctx, "menu")) {
        menu_contents(a);
        mu_end_popup(ctx);
    } else {
        a->menu = MENU_NONE;
    }

    char path[sizeof a->path];
    int action = g_fb.action;
    if (fb_update(&g_fb, ctx, path, sizeof path)) file_browser_done(a, action, path);
    mu_end(ctx);
}

/* 저장/오류 같은 알림: 왼쪽 아래에 잠깐 떴다 사라짐 */
static void draw_toast(const App *a) {
    if (!a->msg[0] || GetTime() - a->msg_time > 3.0) return;
    Vector2 ts = MeasureTextEx(GetFontDefault(), a->msg, 20, 2);
    Rectangle r = { 10, GetScreenHeight() - ts.y - 22, ts.x + 16, ts.y + 10 };
    ui_fill_round((Rectangle){ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, (Color){ 255, 200, 60, 255 });
    ui_fill_round(r, (Color){ 20, 18, 28, 255 });
    DrawTextEx(GetFontDefault(), a->msg, (Vector2){ r.x + 8, r.y + 5 }, 20, 2, (Color){ 240, 236, 228, 255 });
}

static void global_keys(App *a) {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl) {
        if (IsKeyPressed(KEY_Z)) app_undo(a);
        if (IsKeyPressed(KEY_Y)) app_redo(a);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (IsKeyPressed(KEY_S)) { if (shift) file_save_as(a); else file_save(a); }
        if (IsKeyPressed(KEY_O)) file_open(a);
        return;
    }
    if (IsKeyPressed(KEY_TAB)) a->mode = a->mode == MODE_MODEL ? MODE_TEXTURE : MODE_MODEL;
    if (IsKeyPressed(KEY_O)) a->show_overview = !a->show_overview;
    if (IsKeyPressed(KEY_P)) a->show_color = !a->show_color;
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "TinyPoly");
    if (!IsWindowReady()) {
        fprintf(stderr, "InitWindow failed\n");
        return 1;
    }
    SetWindowMinSize(800, 500);
    {
        /* 창이 화면보다 크면 macOS가 몰래 줄이는데 raylib은 이를 몰라서 위쪽이 잘린다.
           모니터에 맞는 크기로 직접 줄여 resize 콜백이 오게 한다 */
        int mon = GetCurrentMonitor();
        int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
        int w = GetScreenWidth(), h = GetScreenHeight();
        if (mw > 0 && w > mw * 9 / 10) w = mw * 9 / 10;
        if (mh > 0 && h > mh * 8 / 10) h = mh * 8 / 10;
        if (w < 800) w = 800;
        if (h < 500) h = 500;
        if (w != GetScreenWidth() || h != GetScreenHeight()) {
            SetWindowSize(w, h);
            /* 보조 모니터에서도 그 모니터 가운데로 (모니터 위치를 더함) */
            Vector2 mp = GetMonitorPosition(mon);
            SetWindowPosition((int)mp.x + (mw - w) / 2, (int)mp.y + (mh - h) / 2);
        }
    }
    SetExitKey(KEY_NULL);
    HideCursor();                   /* 창 안에서는 도트 커서를 직접 그림 (cursor.c) */

    static App app;
    App *a = &app;
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
    snprintf(a->path, sizeof a->path, "model.tpm");

    a->ui = malloc(sizeof(mu_Context));
    ui_init(a->ui);
    view_init(a);

    a->add_up = (Vector3){ 0, 1, 0 };
    model_add_prim(a, PRIM_CUBE);
    undo_reset();
    a->dirty = false;

    char title[512] = "";
    while (!WindowShouldClose()) {
        /* 직전 프레임 기준: 마우스가 UI 창 위인지, 메뉴가 열려 있었는지 */
        a->ui_hover = a->ui->next_hover_root != NULL;
        bool menu_was_open = a->menu != MENU_NONE;
        /* 파일 탐색 창이 떠 있으면 단축키와 뷰 조작을 막는다 */
        bool typing = a->ui->focus != 0 || g_fb.open;
        bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        bool input_ok = !typing && !(menu_was_open && clicked);

        ui_input(a->ui);
        view_layout(a);
        view_update_cameras(a);

        if (!typing) global_keys(a);
        if (a->mode == MODE_MODEL) model_update(a, input_ok, true);
        else paint_update(a, input_ok);
        handle_dropped_files(a);

        view_update_cameras(a);
        view_upload_texture(a);
        build_ui(a);

        char t[512];
        snprintf(t, sizeof t, "TinyPoly " TINYPOLY_VERSION " - %s%s", a->path, a->dirty ? " *" : "");
        if (strcmp(t, title) != 0) {
            SetWindowTitle(t);
            memcpy(title, t, sizeof title);
        }

        BeginDrawing();
        ClearBackground((Color){ 12, 11, 18, 255 });
        view_upload_texture(a);
        if (a->mode == MODE_TEXTURE) paint_draw(a);
        for (int i = 0; i < VIEW_COUNT; i++)
            if (a->views[i].rect.width > 0) view_draw(a, &a->views[i]);
        DrawRectangle(0, 0, GetScreenWidth(), TOPBAR_H, (Color){ 34, 32, 44, 255 });
        draw_toast(a);
        ui_render(a->ui);
        ui_tooltip_draw();
        cursor_draw(a);
        EndDrawing();
    }

    CloseWindow();
    scene_clear(&a->scene);
    undo_reset();
    free(a->ui);
    return 0;
}
