#include "app/project.h"
#include "ui/filebrowser.h"
#include "core/io.h"
#include "paint/paint.h"
#include "core/texture.h"
#include <stdio.h>

/* 파일 탐색 창이 무엇을 위해 열렸는지 */
enum { FB_OPEN, FB_SAVE, FB_IMPORT_PNG, FB_EXPORT };
static FileBrowser g_fb;

void project_new(App *a) {
    app_mark(a);
    scene_release(&a->scene);
    scene_init(&a->scene);
    paint_reset_selection(a);
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

void project_save_as(App *a) {
    fb_open(&g_fb, FB_SAVE, "save project", ".tpm", true, a->path);
}

void project_save(App *a) {
    if (a->has_path) save_to(a, a->path);
    else project_save_as(a);
}

static void open_path(App *a, const char *path) {
    if (scene_load(&a->scene, path)) {
        if (path != a->path) snprintf(a->path, sizeof a->path, "%s", path);
        a->has_path = true;
        undo_reset();
        paint_reset_selection(a);
        a->dirty = false;
        a->tex_dirty = true;
        a->pal_sel = -1;
        app_msg(a, "opened %s", a->path);
    } else {
        app_msg(a, "open failed: %s", path);
    }
}

void project_open(App *a) {
    fb_open(&g_fb, FB_OPEN, "open project", ".tpm", false, a->has_path ? a->path : NULL);
}

static void import_png(App *a, const char *path) {
    app_mark(a);
    if (tex_import_png(&a->scene, path)) {
        paint_reset_selection(a);
        a->tex_dirty = true;
        app_msg(a, "imported %s (%dx%d)", path, a->scene.tex->w, a->scene.tex->h);
    } else {
        app_msg(a, "import failed: %s", path);
    }
}

void project_import_png(App *a) {
    fb_open(&g_fb, FB_IMPORT_PNG, "import texture (png)", ".png", false, a->has_path ? a->path : NULL);
}

/* 프로젝트 이름을 기본값으로 폴더 이름 또는 zip 파일 위치를 묻는다 */
void project_export(App *a) {
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

void project_handle_drops(App *a) {
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

bool project_browser_open(void) {
    return g_fb.open;
}

void project_browser_update(App *a) {
    char path[sizeof a->path];
    int action = g_fb.action;
    if (!fb_update(&g_fb, a->ui, path, sizeof path)) return;
    switch (action) {
    case FB_OPEN:       open_path(a, path); break;
    case FB_SAVE:       save_to(a, path); break;
    case FB_IMPORT_PNG: import_png(a, path); break;
    case FB_EXPORT:     export_to(a, path); break;
    }
}
