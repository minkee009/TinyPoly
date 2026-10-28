#include "app.h"
#include "io.h"
#include "filebrowser.h"
#include "texture.h"
#include "ui.h"
#include <raymath.h>

#ifndef TINYPOLY_VERSION
#define TINYPOLY_VERSION "dev"     /* CMake가 프로젝트 버전으로 지정 */
#endif
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
    scene_clear(&a->scene);
    scene_init(&a->scene);
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
enum { FB_OPEN, FB_SAVE, FB_IMPORT_PNG, FB_EXPORT_OBJ, FB_EXPORT_GLTF, FB_EXPORT_GLB, FB_EXPORT_PNG };
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
        a->dirty = false;
        a->tex_dirty = true;
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
    if (tex_import_png(&a->scene, path)) app_msg(a, "imported %s", path);
    else app_msg(a, "import failed: %s", path);
}

static void file_import_png(App *a) {
    fb_open(&g_fb, FB_IMPORT_PNG, "import texture (png)", ".png", false, a->has_path ? a->path : NULL);
}

/* 프로젝트 이름을 기본값으로 저장 위치를 묻는다. 실제 내보내기는 export_to */
static void file_export(App *a, int action) {
    static const struct { const char *ext, *title; } kinds[] = {
        [FB_EXPORT_OBJ]  = { ".obj",  "export obj (+ mtl, png)" },
        [FB_EXPORT_GLTF] = { ".gltf", "export gltf (+ bin, png)" },
        [FB_EXPORT_GLB]  = { ".glb",  "export glb" },
        [FB_EXPORT_PNG]  = { ".png",  "export texture png" },
    };
    char start[sizeof a->path];
    path_with_ext(start, sizeof start, a->path, action == FB_EXPORT_PNG ? "_texture.png" : kinds[action].ext);
    fb_open(&g_fb, action, kinds[action].title, kinds[action].ext, true, start);
}

static void export_to(App *a, int action, const char *out) {
    bool ok;
    if (action == FB_EXPORT_OBJ) ok = export_obj(&a->scene, out);
    else if (action == FB_EXPORT_PNG) ok = tex_export_png(&a->scene, out);
    else ok = export_gltf(&a->scene, out, action == FB_EXPORT_GLB);
    app_msg(a, ok ? "exported %s" : "export failed: %s", out);
}

/* 파일 탐색 창에서 경로가 확정되면 */
static void file_browser_done(App *a, int action, const char *path) {
    switch (action) {
    case FB_OPEN:       open_path(a, path); break;
    case FB_SAVE:       save_to(a, path); break;
    case FB_IMPORT_PNG: import_png(a, path); break;
    default:            export_to(a, action, path); break;
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
    mu_draw_rect(ctx, r, mu_color(c.r, c.g, c.b, 255));
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

static bool toggle_button(mu_Context *ctx, const char *label, bool on) {
    char buf[64];
    snprintf(buf, sizeof buf, on ? "[%s]" : "%s", label);
    mu_push_id(ctx, label, (int)strlen(label));
    bool r = mu_button(ctx, buf);
    mu_pop_id(ctx);
    return r;
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
    case MENU_FACEPROPS: {
        Face *f = first_selected_face(a);
        if (!f) { close_popup(ctx); break; }
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "face color");
        mu_layout_row(ctx, 8, (int[]){ 22, 22, 22, 22, 22, 22, 22, 22 }, 22);
        for (int i = 0; i < PALETTE_SIZE; i++)
            if (swatch(ctx, 2000 + i, a->scene.palette[i], f->color == i, false)) model_set_face_color(a, i);
        mu_layout_row(ctx, 1, w1, 0);
        face_flag_checkbox(a, "double sided", FACE_DOUBLE, false);
        face_flag_checkbox(a, "shaded", FACE_NOSHADE, true);
        face_flag_checkbox(a, "textured", FACE_NOTEX, true);
        face_flag_checkbox(a, "draw behind", FACE_BEHIND, false);
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
        if (mu_button(ctx, "center to middle")) {
            app_mark(a);
            o = app_active(a);
            Vector3 mn = { 1e9f, 1e9f, 1e9f }, mx = { -1e9f, -1e9f, -1e9f };
            for (int i = 0; i < o->nverts; i++) { mn = Vector3Min(mn, o->verts[i].p); mx = Vector3Max(mx, o->verts[i].p); }
            if (o->nverts) o->origin = Vector3Scale(Vector3Add(mn, mx), 0.5f);
        }
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
    case MENU_FILE:
        mu_layout_row(ctx, 2, (int[]){ 128, 128 }, 0);
        if (mu_button(ctx, "new"))           { file_new(a); close_popup(ctx); }
        if (mu_button(ctx, "open..."))       { close_popup(ctx); file_open(a); }
        if (mu_button(ctx, "save"))          { close_popup(ctx); file_save(a); }
        if (mu_button(ctx, "save as..."))    { close_popup(ctx); file_save_as(a); }
        mu_layout_row(ctx, 1, (int[]){ 260 }, 0);
        if (mu_button(ctx, "import texture png...")) { close_popup(ctx); file_import_png(a); }
        mu_label(ctx, "export");
        mu_layout_row(ctx, 3, (int[]){ 84, 84, 84 }, 0);
        if (mu_button(ctx, "obj"))  { close_popup(ctx); file_export(a, FB_EXPORT_OBJ); }
        if (mu_button(ctx, "gltf")) { close_popup(ctx); file_export(a, FB_EXPORT_GLTF); }
        if (mu_button(ctx, "glb"))  { close_popup(ctx); file_export(a, FB_EXPORT_GLB); }
        mu_layout_row(ctx, 1, (int[]){ 260 }, 0);
        if (mu_button(ctx, "export texture png...")) { close_popup(ctx); file_export(a, FB_EXPORT_PNG); }
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
    if (!mu_begin_window_ex(ctx, "top", cnt->rect, MU_OPT_NOTITLE | MU_OPT_NORESIZE | MU_OPT_NOCLOSE | MU_OPT_NOSCROLL))
        return;
    mu_layout_row(ctx, 12, (int[]){ 60, 76, 96, 14, 36, 60, 60, 14, 100, 92, 92, -1 }, 20);
    if (mu_button(ctx, "file")) app_open_menu(a, MENU_FILE);
    if (toggle_button(ctx, "model", a->mode == MODE_MODEL)) a->mode = MODE_MODEL;
    if (toggle_button(ctx, "texture", a->mode == MODE_TEXTURE)) a->mode = MODE_TEXTURE;
    mu_label(ctx, "");
    if (mu_button(ctx, "+")) {
        a->add_at = (Vector3){ 0 };
        a->add_up = (Vector3){ 0, 1, 0 };
        app_open_menu(a, MENU_ADD);
    }
    if (mu_button(ctx, "undo")) app_undo(a);
    if (mu_button(ctx, "redo")) app_redo(a);
    mu_label(ctx, "");
    if (toggle_button(ctx, "project", a->show_overview)) a->show_overview = !a->show_overview;
    static const char *rm[RENDER_COUNT] = { "tex (M)", "color (M)", "wire (M)" };
    if (mu_button(ctx, rm[a->render_mode])) a->render_mode = (a->render_mode + 1) % RENDER_COUNT;
    if (toggle_button(ctx, "shade", a->shading)) a->shading = !a->shading;
    char buf[300];
    snprintf(buf, sizeof buf, "%s%s", GetFileName(a->path), a->dirty ? " *" : "");   /* 전체 경로는 창 제목에 */
    mu_label(ctx, buf);
    mu_end_window(ctx);
}

static void draw_overview(App *a) {
    mu_Context *ctx = a->ui;
    if (!mu_begin_window_ex(ctx, "project (O)", mu_rect(GetScreenWidth() - 320, TOPBAR_H + 30, 300, 420), MU_OPT_NOCLOSE))
        return;
    Scene *s = &a->scene;
    static double last_click;
    static int last_idx = -1;
    for (int i = 0; i < s->count; i++) {
        Object *o = &s->objs[i];
        mu_push_id(ctx, &i, sizeof i);
        mu_layout_row(ctx, 3, (int[]){ 28, 28, -1 }, 0);
        if (mu_button(ctx, o->hidden ? "-" : "o")) { app_mark(a); s->objs[i].hidden = !s->objs[i].hidden; }
        o = &s->objs[i];
        if (mu_button(ctx, o->locked ? "L" : ".")) { app_mark(a); s->objs[i].locked = !s->objs[i].locked; }
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
    mu_layout_row(ctx, 3, (int[]){ 84, 84, -1 }, 0);
    if (mu_button(ctx, "clone"))  model_clone(a);
    if (mu_button(ctx, "delete")) model_delete_mesh(a);
    if (mu_button(ctx, "close"))  a->show_overview = 0;
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_text(ctx, "o visible / L locked\ndouble-click: rename");
    mu_end_window(ctx);
}

static void draw_palette_editor(App *a) {
    mu_Context *ctx = a->ui;
    if (!mu_begin_window_ex(ctx, "palette (P)", mu_rect(GetScreenWidth() / 2 - 180, TOPBAR_H + 40, 380, 420), MU_OPT_NOCLOSE))
        return;
    Scene *s = &a->scene;
    static bool marked;
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) marked = false;
    #define MARK_ONCE() do { if (!marked) { app_mark(a); marked = true; } a->tex_dirty = true; } while (0)

    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, "LMB: color 1   RMB: color 2");
    mu_layout_row(ctx, 8, (int[]){ 40, 40, 40, 40, 40, 40, 40, 40 }, 32);
    for (int i = 0; i < PALETTE_SIZE; i++) {
        int r = swatch(ctx, 3000 + i, s->palette[i], i == a->color, i == a->color2);
        if (r == 1) a->color = i;
        if (r == 2) a->color2 = i;
    }

    Color *c = &s->palette[a->color];
    mu_Real rgb[3] = { c->r, c->g, c->b };
    static const char *names[3] = { "r", "g", "b" };
    for (int k = 0; k < 3; k++) {
        mu_layout_row(ctx, 2, (int[]){ 20, -1 }, 0);
        mu_label(ctx, names[k]);
        mu_push_id(ctx, &k, sizeof k);
        if (mu_slider_ex(ctx, &rgb[k], 0, 255, 1, "%.0f", MU_OPT_ALIGNCENTER) & MU_RES_CHANGE) {
            MARK_ONCE();
            c = &s->palette[a->color];
            *c = (Color){ (unsigned char)rgb[0], (unsigned char)rgb[1], (unsigned char)rgb[2], 255 };
        }
        mu_pop_id(ctx);
    }

    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    mu_label(ctx, "shading ramp (click: set to color 2)");
    mu_layout_row(ctx, 16, (int[]){ 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 }, 16);
    for (int i = 0; i < PALETTE_SIZE; i++) swatch(ctx, 4000 + i, s->palette[i], false, false);
    for (int i = 0; i < PALETTE_SIZE; i++)
        if (swatch(ctx, 5000 + i, s->palette[s->ramp[i]], false, false) == 1) {
            MARK_ONCE();
            a->scene.ramp[i] = (unsigned char)a->color2;
        }

    mu_layout_row(ctx, 3, (int[]){ 116, 116, -1 }, 0);
    if (mu_button(ctx, "auto shade")) { MARK_ONCE(); tex_auto_ramp(&a->scene); }
    if (mu_button(ctx, "swap 1<>2"))  { MARK_ONCE(); tex_swap_colors(&a->scene, a->color, a->color2); }
    if (mu_button(ctx, "ramp 1..2"))  { MARK_ONCE(); tex_make_gradient(&a->scene, a->color, a->color2); }
    if (mu_button(ctx, "bg = 1"))     { MARK_ONCE(); a->scene.bg = a->color; }
    if (mu_button(ctx, "alpha = 1"))  { MARK_ONCE(); a->scene.alpha = a->color; }
    if (mu_button(ctx, "reset"))      { MARK_ONCE(); scene_reset_palette(&a->scene); }
    mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
    if (mu_button(ctx, "close")) a->show_palette = 0;
    #undef MARK_ONCE
    mu_end_window(ctx);
}

static void build_ui(App *a) {
    mu_Context *ctx = a->ui;
    mu_begin(ctx);
    draw_topbar(a);
    if (a->show_overview) draw_overview(a);
    if (a->show_palette) draw_palette_editor(a);

    if (a->open_menu) {
        mu_open_popup(ctx, "menu");
        a->open_menu = false;
    }
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

static void draw_status(const App *a) {
    int y = GetScreenHeight() - STATUS_H;
    DrawRectangle(0, y, GetScreenWidth(), STATUS_H, (Color){ 16, 14, 24, 255 });
    const char *text;
    if (GetTime() - a->msg_time < 3.0 && a->msg[0]) text = a->msg;
    else if (a->mode == MODE_MODEL)
        text = "drag: move  R/T+drag: rotate/scale  shift+drag: center  RMB: menu  space: fullscreen  "
               "E extrude  X del face  F/G props  M render  L shade  O project  tab: texture";
    else
        text = "V uv  B pen  I pick  G fill  ctrl+click: pick  R/H: rotate/flip uv  T+drag: scale uv  "
               "space/MMB: pan  1-4 zoom  P palette  tab: model";
    DrawTextEx(GetFontDefault(), text, (Vector2){ 8, (float)y + 2 }, 20, 2, (Color){ 220, 216, 230, 255 });
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
    if (IsKeyPressed(KEY_P)) a->show_palette = !a->show_palette;
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
    a->color = 12;
    a->color2 = 7;
    a->tex_zoom = 2;
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
        draw_status(a);
        ui_render(a->ui);
        cursor_draw(a);
        EndDrawing();
    }

    CloseWindow();
    scene_clear(&a->scene);
    undo_reset();
    free(a->ui);
    return 0;
}
