#include "model/model.h"
#include <raymath.h>
#include <stdio.h>

void ModelAddPrim(App *a, int kind) {
    Scene *s = &a->scene;
    if (s->count >= MAX_OBJECTS) { AppMsg(a, "too many meshes"); return; }
    AppMark(a);
    char name[32];
    snprintf(name, sizeof name, "%s%d", PRIM_NAMES[kind], s->count + 1);
    Object *o = SceneAdd(s, name);
    /* 투명한 색으로 만들면 안 보이므로 그땐 흰색 */
    PrimBuild(o, kind, a->color.a ? a->color : WHITE, a->add_up, a->add_at, a->scene.tex);
    AppSelectMesh(a, s->count - 1);
}

void ModelExtrude(App *a) {
    Object *o = AppActive(a);
    if (!o || !ObjCountSelected(o, true)) { AppMsg(a, "extrude: select faces in the 3d view"); return; }
    AppMark(a);
    ObjExtrudeSelectedFaces(o, a->grid, SceneUVScale(&a->scene));
}

void ModelDeleteFaces(App *a) {
    Object *o = AppActive(a);
    if (!o || !ObjCountSelected(o, true)) return;
    AppMark(a);
    ObjDeleteSelectedFaces(o);
    ObjClearSelection(o);
}

void ModelDeleteVerts(App *a) {
    Object *o = AppActive(a);
    if (!o || !ObjCountSelected(o, false)) return;
    AppMark(a);
    ObjDeleteSelectedVerts(o);
    ObjClearSelection(o);
}

void ModelDeleteMesh(App *a) {
    if (a->scene.active < 0) return;
    char name[sizeof a->scene.objs[0].name];
    snprintf(name, sizeof name, "%s", a->scene.objs[a->scene.active].name);
    AppMark(a);
    SceneDelete(&a->scene, a->scene.active);
    AppMsg(a, "deleted %s (ctrl+z to undo)", name);
}

void ModelMerge(App *a) {
    Object *o = AppActive(a);
    if (!o || ObjCountSelected(o, false) < 2) { AppMsg(a, "merge: select 2+ vertices"); return; }
    AppMark(a);
    ObjMergeSelectedVerts(o);
}

void ModelSnap(App *a) {
    Object *o = AppActive(a);
    if (!o || !ObjCountSelected(o, false)) return;
    AppMark(a);
    ObjSnapSelectedVerts(o, a->grid);
}

void ModelMirror(App *a, int axis) {
    Object *o = AppActive(a);
    if (!o) return;
    AppMark(a);
    ObjMirror(o, axis);
}

void ModelClone(App *a) {
    Scene *s = &a->scene;
    if (s->active < 0 || s->count >= MAX_OBJECTS) return;
    AppMark(a);
    Object *o = &s->objs[s->count];
    ObjCopy(o, &s->objs[s->active]);
    ObjClearSelection(o);
    s->count++;
    AppSelectMesh(a, s->count - 1);
    AppMsg(a, "cloned %s", o->name);
}

/* 활성 메쉬의 원점(회전·크기 기준점)을 정점들의 가운데로 */
void ModelCenterOrigin(App *a) {
    Object *o = AppActive(a);
    if (!o || !o->nverts) { AppMsg(a, "center origin: no mesh selected"); return; }
    Vector3 mn = { 1e9f, 1e9f, 1e9f }, mx = { -1e9f, -1e9f, -1e9f };
    for (int i = 0; i < o->nverts; i++) { mn = Vector3Min(mn, o->verts[i].p); mx = Vector3Max(mx, o->verts[i].p); }
    Vector3 c = Vector3Scale(Vector3Add(mn, mx), 0.5f);
    if (Vector3Distance(c, o->origin) < 1e-4f) { AppMsg(a, "origin already centered"); return; }
    AppMark(a);
    o = AppActive(a);
    o->origin = c;
    AppMsg(a, "origin centered (%g, %g, %g)", c.x, c.y, c.z);
}

void ModelCopy(App *a) {
    Object *o = AppActive(a);
    if (!o) return;
    if (a->has_clipboard) ObjFree(&a->clipboard);
    ObjCopy(&a->clipboard, o);
    ObjClearSelection(&a->clipboard);
    a->has_clipboard = true;
    AppMsg(a, "copied %s", o->name);
}

void ModelPaste(App *a) {
    Scene *s = &a->scene;
    if (!a->has_clipboard || s->count >= MAX_OBJECTS) return;
    AppMark(a);
    Object *o = &s->objs[s->count++];
    ObjCopy(o, &a->clipboard);
    ObjTranslate(o, (Vector3){ a->grid * 2, 0, a->grid * 2 });
    AppSelectMesh(a, s->count - 1);
}

void ModelSetFaceFlag(App *a, int flag, bool on) {
    Object *o = AppActive(a);
    if (!o) return;
    AppMark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) o->faces[i].flags = on ? (o->faces[i].flags | flag) : (o->faces[i].flags & ~flag);
}

void ModelSetFaceColor(App *a, Color color) {
    Object *o = AppActive(a);
    if (!o) return;
    AppMark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) o->faces[i].color = color;
}
