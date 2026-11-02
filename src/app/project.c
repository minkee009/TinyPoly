#include "app/project.h"
#include "ui/filebrowser.h"
#include "core/io.h"
#include "paint/paint.h"
#include "core/texture.h"
#include <stdio.h>

/* 파일 탐색 창이 무엇을 위해 열렸는지 */
enum { FB_OPEN, FB_SAVE, FB_IMPORT_PNG, FB_EXPORT };
static FileBrowser g_fb;

void ProjectNew(App *a) {
    AppMark(a);
    SceneRelease(&a->scene);
    SceneInit(&a->scene);
    PaintResetSelection(a);
    AppMsg(a, "new project");
}

static void SaveTo(App *a, const char *path) {
    if (SceneSave(&a->scene, path)) {
        if (path != a->path) snprintf(a->path, sizeof a->path, "%s", path);
        a->has_path = true;
        a->dirty = false;
        AppMsg(a, "saved %s", a->path);
    } else {
        AppMsg(a, "save failed: %s", path);
    }
}

void ProjectSaveAs(App *a) {
    FbOpen(&g_fb, FB_SAVE, "save project", ".tpm", true, a->path);
}

void ProjectSave(App *a) {
    if (a->has_path) SaveTo(a, a->path);
    else ProjectSaveAs(a);
}

static void OpenPath(App *a, const char *path) {
    if (SceneLoad(&a->scene, path)) {
        if (path != a->path) snprintf(a->path, sizeof a->path, "%s", path);
        a->has_path = true;
        UndoReset();
        PaintResetSelection(a);
        a->dirty = false;
        a->tex_dirty = true;
        a->pal_sel = -1;
        AppMsg(a, "opened %s", a->path);
    } else {
        AppMsg(a, "open failed: %s", path);
    }
}

void ProjectOpen(App *a) {
    FbOpen(&g_fb, FB_OPEN, "open project", ".tpm", false, a->has_path ? a->path : NULL);
}

static void ImportPNG(App *a, const char *path) {
    AppMark(a);
    if (TexImportPNG(&a->scene, path)) {
        PaintResetSelection(a);
        a->tex_dirty = true;
        AppMsg(a, "imported %s (%dx%d)", path, a->scene.tex->w, a->scene.tex->h);
    } else {
        AppMsg(a, "import failed: %s", path);
    }
}

void ProjectImportPNG(App *a) {
    FbOpen(&g_fb, FB_IMPORT_PNG, "import texture (png)", ".png", false, a->has_path ? a->path : NULL);
}

/* 프로젝트 이름을 기본값으로 폴더 이름 또는 zip 파일 위치를 묻는다 */
void ProjectExport(App *a) {
    char start[sizeof a->path];
    PathWithExt(start, sizeof start, a->path, a->exp_zip ? ".zip" : "");
    if (a->exp_zip) FbOpen(&g_fb, FB_EXPORT, "export to zip", ".zip", true, start);
    else FbOpen(&g_fb, FB_EXPORT, "export to folder (type a folder name)", "", true, start);
}

static void ExportTo(App *a, const char *out) {
    bool ok = ExportScene(&a->scene, (ExportFormat)a->exp_format, a->exp_zip, out);
    AppMsg(a, ok ? "exported %s" : "export failed: %s", out);
    if (ok) a->show_export = 0;
}

void ProjectHandleDrops(App *a) {
    if (!IsFileDropped()) return;
    FilePathList files = LoadDroppedFiles();
    if (files.count > 0) {
        const char *p = files.paths[0];
        if (IsFileExtension(p, ".tpm")) {
            OpenPath(a, p);
        } else if (IsFileExtension(p, ".png")) {
            ImportPNG(a, p);
        } else {
            AppMsg(a, "drop a .tpm project or a .png texture");
        }
    }
    UnloadDroppedFiles(files);
}

bool ProjectBrowserOpen(void) {
    return g_fb.open;
}

void ProjectBrowserUpdate(App *a) {
    char path[sizeof a->path];
    int action = g_fb.action;
    if (!FbUpdate(&g_fb, a->ui, path, sizeof path)) return;
    switch (action) {
    case FB_OPEN:       OpenPath(a, path); break;
    case FB_SAVE:       SaveTo(a, path); break;
    case FB_IMPORT_PNG: ImportPNG(a, path); break;
    case FB_EXPORT:     ExportTo(a, path); break;
    }
}
