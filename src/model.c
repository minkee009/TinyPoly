#include "app.h"
#include <raymath.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { D_NONE, D_VERTS, D_MESH, D_BOX, D_CLICK3D, D_ORBIT, D_PAN, D_RMB } DragKind;

static DragKind g_drag;
static int g_view;              /* 드래그를 시작한 뷰 */
static Vector2 g_press;
static bool g_moved, g_marked, g_box_add;
static Vector3 *g_orig;         /* 드래그 시작 시 활성 메쉬 정점 위치 */
static Vector3 g_orig_origin;

static bool key_shift(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool key_ctrl(void)  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

static bool snapping(const App *a) {
    return a->snap ? !key_ctrl() : key_ctrl();     /* Ctrl을 누르는 동안 반대로 */
}

static float snapf(float v, float g) {
    return roundf(v / g) * g;
}

static int count_sel_verts(const Object *o) {
    int n = 0;
    for (int i = 0; i < o->nverts; i++) n += o->verts[i].sel;
    return n;
}

static int count_sel_faces(const Object *o) {
    int n = 0;
    for (int i = 0; i < o->nfaces; i++) n += o->faces[i].sel;
    return n;
}

/* ---- 드래그 변형 ---- */

static void begin_edit_drag(App *a, DragKind kind) {
    Object *o = app_active(a);
    free(g_orig);
    g_orig = malloc(sizeof(Vector3) * (size_t)(o->nverts + 1));
    for (int i = 0; i < o->nverts; i++) g_orig[i] = o->verts[i].p;
    g_orig_origin = o->origin;
    g_drag = kind;
    g_marked = false;
}

static void restore(Object *o) {
    for (int i = 0; i < o->nverts; i++) o->verts[i].p = g_orig[i];
    o->origin = g_orig_origin;
}

/* 매 프레임 시작 위치에서 다시 계산: R(회전)/T(크기)/Shift(중심점)는 드래그 중 바꿔 눌러도 됨 */
static void apply_edit_drag(App *a, const Viewport *vp, Vector2 m) {
    Object *o = app_active(a);
    if (!o) return;
    float dx = m.x - g_press.x, dy = m.y - g_press.y;
    if (!g_moved) return;

    restore(o);
    if (!g_marked) { app_mark(a); g_marked = true; }

    Vector3 r, u, n;
    view_axes(vp, &r, &u, &n);
    float upp = view_units_per_pixel(a, vp);
    bool snap = snapping(a);
    bool only_sel = g_drag == D_VERTS;

    /* 기준점: 메쉬 전체면 원점, 정점 일부면 그 중심 */
    Vector3 pivot = o->origin;
    if (only_sel) {
        Vector3 c = { 0 };
        int k = 0;
        for (int i = 0; i < o->nverts; i++) if (o->verts[i].sel) { c = Vector3Add(c, o->verts[i].p); k++; }
        if (k) pivot = Vector3Scale(c, 1.0f / (float)k);
    }

    if (IsKeyDown(KEY_R)) {
        float ang = dx * 0.02f;
        if (snap) ang = snapf(ang, PI / 12.0f);     /* 15도 */
        for (int i = 0; i < o->nverts; i++) {
            if (only_sel && !o->verts[i].sel) continue;
            Vector3 d = Vector3RotateByAxisAngle(Vector3Subtract(o->verts[i].p, pivot), n, -ang);
            o->verts[i].p = Vector3Add(pivot, d);
        }
    } else if (IsKeyDown(KEY_T)) {
        float f = fmaxf(0.0f, 1.0f + (dx - dy) * 0.01f);
        if (snap) f = snapf(f, 0.125f);
        for (int i = 0; i < o->nverts; i++) {
            if (only_sel && !o->verts[i].sel) continue;
            Vector3 d = Vector3Subtract(o->verts[i].p, pivot);
            d = Vector3Add(d, Vector3Scale(r, Vector3DotProduct(d, r) * (f - 1.0f)));
            d = Vector3Add(d, Vector3Scale(u, Vector3DotProduct(d, u) * (f - 1.0f)));
            o->verts[i].p = Vector3Add(pivot, d);
        }
    } else {
        Vector3 d = Vector3Add(Vector3Scale(r, dx * upp), Vector3Scale(u, -dy * upp));
        if (snap) d = (Vector3){ snapf(d.x, a->grid), snapf(d.y, a->grid), snapf(d.z, a->grid) };
        if (g_drag == D_MESH && key_shift()) {
            o->origin = Vector3Add(o->origin, d);
        } else {
            for (int i = 0; i < o->nverts; i++)
                if (!only_sel || o->verts[i].sel) o->verts[i].p = Vector3Add(o->verts[i].p, d);
            if (!only_sel) o->origin = Vector3Add(o->origin, d);
        }
    }
}

/* ---- 선택 ---- */

static void box_select(App *a, const Viewport *vp, Rectangle r, bool add) {
    Object *o = app_active(a);
    if (!o) return;
    if (!add) obj_clear_selection(o);
    for (int i = 0; i < o->nverts; i++) {
        Vector2 sp;
        if (view_project(vp, o->verts[i].p, &sp) && CheckCollisionPointRec(sp, r)) o->verts[i].sel = true;
    }
}

/* 3D 뷰에서 좌클릭으로 고를 정점 (텍스처 모드는 면만 다루므로 없음) */
static int pick_vert_3d(App *a, const Viewport *vp, Vector2 m) {
    return a->mode == MODE_MODEL ? view_pick_vert_3d(a, vp, m) : -1;
}

/* 3D 뷰 클릭: 활성 메쉬의 정점 강조 원 안이면 정점, 다른 메쉬면 메쉬 선택, 활성 메쉬면 면 선택 */
static void click_3d(App *a, const Viewport *vp, Vector2 m) {
    int oi, fi;
    Object *o = app_active(a);
    int vi = pick_vert_3d(a, vp, m);
    if (vi >= 0) {
        if (key_shift()) {
            /* 면 선택은 꼭짓점 선택으로 바꿔서 이어감 */
            for (int i = 0; i < o->nfaces; i++) o->faces[i].sel = false;
            o->verts[vi].sel = !o->verts[vi].sel;
        } else {
            obj_clear_selection(o);
            o->verts[vi].sel = true;
        }
    } else if (view_pick_face(a, vp, m, -1, &oi, &fi)) {
        if (oi != a->scene.active) {
            app_select_mesh(a, oi);
            return;
        }
        Face *f = &o->faces[fi];
        if (key_shift()) {
            f->sel = !f->sel;
        } else {
            obj_clear_selection(o);
            f->sel = true;
        }
        for (int i = 0; i < o->nverts; i++) o->verts[i].sel = false;
        obj_select_face_verts(o);
    } else if (o && (count_sel_faces(o) || count_sel_verts(o))) {
        obj_clear_selection(o);
    } else {
        app_select_mesh(a, -1);
    }
}

static void open_context_menu(App *a, const Viewport *vp, Vector2 m) {
    Object *o = app_active(a);
    a->menu_view = vp->kind;
    int vi = vp->kind == VIEW_3D ? pick_vert_3d(a, vp, m) : view_pick_vert(a, vp, m);
    int oi, fi;
    /* 3D 뷰는 직접 고른 정점일 때만 (면을 고르면 꼭짓점도 같이 골라지므로 면이 골라져 있으면 면 메뉴) */
    bool vert_menu = vp->kind != VIEW_3D || (o && vi >= 0 && o->verts[vi].sel && count_sel_faces(o) == 0);
    if (o && vi >= 0 && a->mode == MODE_MODEL && vert_menu) {
        if (!o->verts[vi].sel) {
            obj_clear_selection(o);
            o->verts[vi].sel = true;
        }
        app_open_menu(a, MENU_VERTEX);
    } else if (view_pick_face(a, vp, m, -1, &oi, &fi)) {
        if (oi != a->scene.active) app_select_mesh(a, oi);
        o = app_active(a);
        if (vp->kind == VIEW_3D) {
            if (!o->faces[fi].sel) {
                obj_clear_selection(o);
                o->faces[fi].sel = true;
                obj_select_face_verts(o);
            }
            app_open_menu(a, MENU_FACE);
        } else {
            app_open_menu(a, MENU_MESH);
        }
    } else {
        Vector3 r, u, n;
        view_axes(vp, &r, &u, &n);
        if (vp->kind == VIEW_3D) {
            a->add_at = a->target;
            /* 카메라를 향하는 주축 */
            Vector3 up = { 0 };
            if (fabsf(n.x) >= fabsf(n.y) && fabsf(n.x) >= fabsf(n.z)) up.x = n.x > 0 ? 1.0f : -1.0f;
            else if (fabsf(n.y) >= fabsf(n.z)) up.y = n.y > 0 ? 1.0f : -1.0f;
            else up.z = n.z > 0 ? 1.0f : -1.0f;
            a->add_up = up;
        } else {
            float upp = view_units_per_pixel(a, vp);
            Vector2 c = { vp->rect.x + vp->rect.width / 2, vp->rect.y + vp->rect.height / 2 };
            Vector3 p = Vector3Add(vp->center, Vector3Add(Vector3Scale(r, (m.x - c.x) * upp), Vector3Scale(u, -(m.y - c.y) * upp)));
            p = Vector3Subtract(p, Vector3Scale(n, Vector3DotProduct(p, n)));   /* 깊이는 0 */
            a->add_at = p;
            a->add_up = n;
        }
        a->add_at = (Vector3){ snapf(a->add_at.x, a->grid), snapf(a->add_at.y, a->grid), snapf(a->add_at.z, a->grid) };
        app_open_menu(a, MENU_ADD);
    }
}

/* ---- 마우스 ---- */

static void pan_view(App *a, Viewport *vp, Vector2 d) {
    Vector3 r, u, n;
    view_axes(vp, &r, &u, &n);
    Vector3 move = Vector3Add(Vector3Scale(r, -d.x * view_units_per_pixel(a, vp)),
                              Vector3Scale(u, d.y * view_units_per_pixel(a, vp)));
    if (vp->kind == VIEW_3D) a->target = Vector3Add(a->target, move);
    else vp->center = Vector3Add(vp->center, move);
}

static void update_mouse(App *a, bool input_ok) {
    Vector2 m = GetMousePosition();
    Vector2 md = GetMouseDelta();
    int hv = view_at(a, m);

    if (g_drag == D_NONE && hv >= 0 && input_ok) {
        Viewport *vp = &a->views[hv];
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            if (hv == VIEW_3D) a->dist = Clamp(a->dist * powf(0.88f, wheel), 0.5f, 100.0f);
            else vp->zoom = Clamp(vp->zoom * powf(0.88f, wheel), 0.5f, 100.0f);
        }

        g_press = m;
        g_moved = false;
        g_view = hv;
        Object *o = app_active(a);

        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            g_drag = D_PAN;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            g_drag = D_RMB;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hv == VIEW_3D) {
                g_drag = key_ctrl() ? D_BOX : D_CLICK3D;
                g_box_add = key_shift();
            } else {
                int vi = view_pick_vert(a, vp, m);
                int oi, fi;
                if (vi >= 0) {
                    Vert *v = &o->verts[vi];
                    if (key_shift() && v->sel) {
                        v->sel = false;
                    } else {
                        if (!key_shift() && !v->sel) obj_clear_selection(o);
                        v->sel = true;
                        begin_edit_drag(a, D_VERTS);
                    }
                } else if (view_pick_face(a, vp, m, -1, &oi, &fi)) {
                    if (oi != a->scene.active) app_select_mesh(a, oi);
                    begin_edit_drag(a, D_MESH);
                } else {
                    g_drag = D_BOX;
                    g_box_add = key_shift();
                }
            }
        }
    }
    if (g_drag == D_NONE) return;

    Viewport *vp = &a->views[g_view];
    if (Vector2Distance(m, g_press) > 3.0f) g_moved = true;

    switch (g_drag) {
    case D_VERTS:
    case D_MESH:
        apply_edit_drag(a, vp, m);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) g_drag = D_NONE;
        break;
    case D_BOX:
        if (g_moved)
            g_box = (Rectangle){ fminf(m.x, g_press.x), fminf(m.y, g_press.y),
                                 fabsf(m.x - g_press.x), fabsf(m.y - g_press.y) };
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Object *o = app_active(a);
            if (g_moved) {
                box_select(a, vp, g_box, g_box_add);
            } else if (o && (count_sel_verts(o) || count_sel_faces(o))) {
                obj_clear_selection(o);
            } else {
                app_select_mesh(a, -1);
            }
            g_box.width = 0;
            g_drag = D_NONE;
        }
        break;
    case D_CLICK3D:
        if (g_moved) g_drag = D_ORBIT;
        else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            click_3d(a, vp, m);
            g_drag = D_NONE;
        }
        break;
    case D_ORBIT:
        a->yaw -= md.x * 0.01f;
        a->pitch = Clamp(a->pitch + md.y * 0.01f, -1.55f, 1.55f);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) g_drag = D_NONE;
        break;
    case D_PAN:
        pan_view(a, vp, md);
        if (!IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) g_drag = D_NONE;
        break;
    case D_RMB:
        if (g_moved) {
            /* 우클릭 드래그: 3D는 회전, 직교 뷰는 이동 */
            g_drag = g_view == VIEW_3D ? D_ORBIT : D_PAN;
        } else if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            open_context_menu(a, vp, m);
            g_drag = D_NONE;
        }
        break;
    default:
        break;
    }
}

/* ---- 키보드 ---- */

static Viewport *key_view(App *a) {
    static int last = VIEW_FRONT;
    int hv = view_at(a, GetMousePosition());
    if (hv >= 0) last = hv;
    return &a->views[last];
}

static bool pressed(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

static void arrow_move(App *a) {
    int dx = (pressed(KEY_RIGHT) || pressed(KEY_D)) - (pressed(KEY_LEFT) || pressed(KEY_A));
    int dy = (pressed(KEY_UP) || pressed(KEY_W)) - (pressed(KEY_DOWN) || pressed(KEY_S));
    if (!dx && !dy) return;

    Viewport *vp = key_view(a);
    Vector3 r, u, n;
    view_axes(vp, &r, &u, &n);
    if (vp->kind == VIEW_3D) {
        /* 3D 뷰에서는 가장 가까운 월드 축으로 */
        Vector3 *v[2] = { &r, &u };
        for (int i = 0; i < 2; i++) {
            Vector3 w = *v[i], s = { 0 };
            if (fabsf(w.x) >= fabsf(w.y) && fabsf(w.x) >= fabsf(w.z)) s.x = w.x > 0 ? 1.0f : -1.0f;
            else if (fabsf(w.y) >= fabsf(w.z)) s.y = w.y > 0 ? 1.0f : -1.0f;
            else s.z = w.z > 0 ? 1.0f : -1.0f;
            *v[i] = s;
        }
    }
    float step = key_ctrl() ? a->grid / 4.0f : a->grid;
    Vector3 d = Vector3Add(Vector3Scale(r, (float)dx * step), Vector3Scale(u, (float)dy * step));

    Object *o = app_active(a);
    if (!o) {
        pan_view(a, vp, (Vector2){ (float)-dx * 16, (float)dy * 16 });
        return;
    }
    app_mark(a);
    if (count_sel_verts(o)) {
        for (int i = 0; i < o->nverts; i++)
            if (o->verts[i].sel) o->verts[i].p = Vector3Add(o->verts[i].p, d);
    } else {
        obj_translate(o, d);
    }
}

static void update_keys(App *a) {
    bool ctrl = key_ctrl();
    Object *o = app_active(a);

    if (ctrl) {
        if (IsKeyPressed(KEY_X)) model_delete_mesh(a);
        if (IsKeyPressed(KEY_C)) model_copy(a);
        if (IsKeyPressed(KEY_V)) model_paste(a);
        if (pressed(KEY_UP) || pressed(KEY_DOWN) || pressed(KEY_LEFT) || pressed(KEY_RIGHT)) arrow_move(a);
        return;
    }
    if (IsKeyPressed(KEY_SPACE)) {
        int hv = view_at(a, GetMousePosition());
        a->full = a->full >= 0 ? -1 : hv;
    }
    if (IsKeyPressed(KEY_E)) model_extrude(a);
    if (IsKeyPressed(KEY_X)) model_delete_faces(a);
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) model_delete_verts(a);
    if (IsKeyPressed(KEY_F) && o && count_sel_faces(o)) app_open_menu(a, MENU_FACEPROPS);
    if (IsKeyPressed(KEY_G) && o) app_open_menu(a, MENU_MESHPROPS);
    if (IsKeyPressed(KEY_M)) a->render_mode = (a->render_mode + 1) % RENDER_COUNT;
    if (IsKeyPressed(KEY_L)) a->shading = !a->shading;
    if (!IsKeyDown(KEY_R) && !IsKeyDown(KEY_T)) arrow_move(a);
}

/* update_mouse의 좌클릭 판정과 같은 순서로, 누르기 전에 강조할 대상을 정한다 */
static void update_hover(App *a, bool input_ok) {
    a->hover_obj = a->hover_face = a->hover_vert = -1;
    if (!input_ok || g_drag != D_NONE || a->menu != MENU_NONE) return;
    Vector2 m = GetMousePosition();
    int hv = view_at(a, m);
    if (hv < 0) return;
    const Viewport *vp = &a->views[hv];
    int oi, fi;
    if (hv == VIEW_3D) {
        if (key_ctrl()) return;                       /* Ctrl+드래그는 박스 선택 */
        int vi = pick_vert_3d(a, vp, m);
        if (vi >= 0) {
            a->hover_obj = a->scene.active;
            a->hover_vert = vi;
            return;
        }
        if (!view_pick_face(a, vp, m, -1, &oi, &fi)) return;
        a->hover_obj = oi;
        if (oi == a->scene.active) a->hover_face = fi;   /* 다른 메쉬면 메쉬 전체 */
        return;
    }
    int vi = view_pick_vert(a, vp, m);
    if (vi >= 0) {
        a->hover_obj = a->scene.active;
        a->hover_vert = vi;
    } else if (view_pick_face(a, vp, m, -1, &oi, &fi) && oi != a->scene.active) {
        a->hover_obj = oi;                             /* 직교 뷰에서 면을 누르면 메쉬 선택 */
    }
}

void model_update(App *a, bool input_ok, bool keys) {
    if (a->scene.active >= a->scene.count) a->scene.active = -1;
    update_mouse(a, input_ok && !a->ui_hover);
    update_hover(a, input_ok && !a->ui_hover);
    if (keys && input_ok && g_drag == D_NONE) update_keys(a);
}

/* R/T를 누르고 있으면 그 동작, 아니면 이동 */
static int edit_cursor(void) {
    return IsKeyDown(KEY_R) ? CURSOR_ROTATE : IsKeyDown(KEY_T) ? CURSOR_SCALE : CURSOR_MOVE;
}

int model_cursor(App *a) {
    switch (g_drag) {
    case D_VERTS:
    case D_MESH:  return edit_cursor();
    case D_BOX:   return CURSOR_CROSS;
    case D_ORBIT: return CURSOR_ROTATE;
    case D_PAN:   return CURSOR_GRAB;
    default: break;
    }

    /* 직교 뷰에서 정점이나 메쉬 위에 있으면 끌 수 있다는 표시 */
    Vector2 m = GetMousePosition();
    int hv = view_at(a, m);
    if (hv < 0 || hv == VIEW_3D || a->mode != MODE_MODEL) return CURSOR_ARROW;
    const Viewport *vp = &a->views[hv];
    int oi, fi;
    if (view_pick_vert(a, vp, m) >= 0 || view_pick_face(a, vp, m, -1, &oi, &fi)) return edit_cursor();
    return CURSOR_ARROW;
}

/* ---- 명령 ---- */

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
    if (!o || !count_sel_faces(o)) { app_msg(a, "extrude: select faces in the 3d view"); return; }
    app_mark(a);
    obj_extrude_selected_faces(o, a->grid, scene_uv_scale(&a->scene));
}

void model_delete_faces(App *a) {
    Object *o = app_active(a);
    if (!o || !count_sel_faces(o)) return;
    app_mark(a);
    obj_delete_selected_faces(o);
    obj_clear_selection(o);
}

void model_delete_verts(App *a) {
    Object *o = app_active(a);
    if (!o || !count_sel_verts(o)) return;
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
    if (!o || count_sel_verts(o) < 2) { app_msg(a, "merge: select 2+ vertices"); return; }
    app_mark(a);
    obj_merge_selected_verts(o);
}

void model_snap(App *a) {
    Object *o = app_active(a);
    if (!o || !count_sel_verts(o)) return;
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
