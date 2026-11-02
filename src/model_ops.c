#include "model_ops.h"
#include <raymath.h>
#include <stdio.h>

void model_add_prim(App *a, int kind) {
    Scene *s = &a->scene;
    if (s->count >= MAX_OBJECTS) { app_msg(a, "too many meshes"); return; }
    app_mark(a);
    char name[32];
    snprintf(name, sizeof name, "%s%d", PRIM_NAMES[kind], s->count + 1);
    Object *o = scene_add(s, name);
    /* 투명한 색으로 만들면 안 보이므로 그땐 흰색 */
    prim_build(o, kind, a->color.a ? a->color : WHITE, a->add_up, a->add_at, a->scene.tex);
    app_select_mesh(a, s->count - 1);
}

void model_extrude(App *a) {
    Object *o = app_active(a);
    if (!o || !obj_count_selected(o, true)) { app_msg(a, "extrude: select faces in the 3d view"); return; }
    app_mark(a);
    obj_extrude_selected_faces(o, a->grid, scene_uv_scale(&a->scene));
}

void model_delete_faces(App *a) {
    Object *o = app_active(a);
    if (!o || !obj_count_selected(o, true)) return;
    app_mark(a);
    obj_delete_selected_faces(o);
    obj_clear_selection(o);
}

void model_delete_verts(App *a) {
    Object *o = app_active(a);
    if (!o || !obj_count_selected(o, false)) return;
    app_mark(a);
    obj_delete_selected_verts(o);
    obj_clear_selection(o);
}

void model_delete_mesh(App *a) {
    if (a->scene.active < 0) return;
    char name[sizeof a->scene.objs[0].name];
    snprintf(name, sizeof name, "%s", a->scene.objs[a->scene.active].name);
    app_mark(a);
    scene_delete(&a->scene, a->scene.active);
    app_msg(a, "deleted %s (ctrl+z to undo)", name);
}

void model_merge(App *a) {
    Object *o = app_active(a);
    if (!o || obj_count_selected(o, false) < 2) { app_msg(a, "merge: select 2+ vertices"); return; }
    app_mark(a);
    obj_merge_selected_verts(o);
}

void model_snap(App *a) {
    Object *o = app_active(a);
    if (!o || !obj_count_selected(o, false)) return;
    app_mark(a);
    obj_snap_selected_verts(o, a->grid);
}

void model_mirror(App *a, int axis) {
    Object *o = app_active(a);
    if (!o) return;
    app_mark(a);
    obj_mirror(o, axis);
}

void model_clone(App *a) {
    Scene *s = &a->scene;
    if (s->active < 0 || s->count >= MAX_OBJECTS) return;
    app_mark(a);
    Object *o = &s->objs[s->count];
    obj_copy(o, &s->objs[s->active]);
    obj_clear_selection(o);
    s->count++;
    app_select_mesh(a, s->count - 1);
    app_msg(a, "cloned %s", o->name);
}

/* 활성 메쉬의 원점(회전·크기 기준점)을 정점들의 가운데로 */
void model_center_origin(App *a) {
    Object *o = app_active(a);
    if (!o || !o->nverts) { app_msg(a, "center origin: no mesh selected"); return; }
    Vector3 mn = { 1e9f, 1e9f, 1e9f }, mx = { -1e9f, -1e9f, -1e9f };
    for (int i = 0; i < o->nverts; i++) { mn = Vector3Min(mn, o->verts[i].p); mx = Vector3Max(mx, o->verts[i].p); }
    Vector3 c = Vector3Scale(Vector3Add(mn, mx), 0.5f);
    if (Vector3Distance(c, o->origin) < 1e-4f) { app_msg(a, "origin already centered"); return; }
    app_mark(a);
    o = app_active(a);
    o->origin = c;
    app_msg(a, "origin centered (%g, %g, %g)", c.x, c.y, c.z);
}

void model_copy(App *a) {
    Object *o = app_active(a);
    if (!o) return;
    if (a->has_clipboard) obj_free(&a->clipboard);
    obj_copy(&a->clipboard, o);
    obj_clear_selection(&a->clipboard);
    a->has_clipboard = true;
    app_msg(a, "copied %s", o->name);
}

void model_paste(App *a) {
    Scene *s = &a->scene;
    if (!a->has_clipboard || s->count >= MAX_OBJECTS) return;
    app_mark(a);
    Object *o = &s->objs[s->count++];
    obj_copy(o, &a->clipboard);
    obj_translate(o, (Vector3){ a->grid * 2, 0, a->grid * 2 });
    app_select_mesh(a, s->count - 1);
}

void model_set_face_flag(App *a, int flag, bool on) {
    Object *o = app_active(a);
    if (!o) return;
    app_mark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) o->faces[i].flags = on ? (o->faces[i].flags | flag) : (o->faces[i].flags & ~flag);
}

void model_set_face_color(App *a, Color color) {
    Object *o = app_active(a);
    if (!o) return;
    app_mark(a);
    for (int i = 0; i < o->nfaces; i++)
        if (o->faces[i].sel) o->faces[i].color = color;
}
