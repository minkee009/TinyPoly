#include "model/model.h"
#include "ui/cursor.h"
#include "view/view.h"
#include <raymath.h>
#include <math.h>
#include <stdlib.h>

static bool KeyShift(void) { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
static bool KeyCtrl(void)  { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

static bool Snapping(const App *a) {
    return a->snap ? !KeyCtrl() : KeyCtrl();     /* Ctrl을 누르는 동안 반대로 */
}

static float Snapf(float v, float g) {
    return roundf(v / g) * g;
}

/* ---- 드래그 변형 ---- */

static void BeginEditDrag(App *a, ModelDragKind kind) {
    Object *o = AppActive(a);
    free(a->model.orig);
    a->model.orig = malloc(sizeof(Vector3) * (size_t)(o->nverts + 1));
    for (int i = 0; i < o->nverts; i++) a->model.orig[i] = o->verts[i].p;
    a->model.orig_origin = o->origin;
    a->model.drag = kind;
    a->model.marked = false;
}

static void Restore(App *a, Object *o) {
    for (int i = 0; i < o->nverts; i++) o->verts[i].p = a->model.orig[i];
    o->origin = a->model.orig_origin;
}

/* 매 프레임 시작 위치에서 다시 계산: R(회전)/T(크기)/Shift(중심점)는 드래그 중 바꿔 눌러도 됨 */
static void ApplyEditDrag(App *a, const Viewport *vp, Vector2 m) {
    Object *o = AppActive(a);
    if (!o) return;
    float dx = m.x - a->model.press.x, dy = m.y - a->model.press.y;
    if (!a->model.moved) return;

    Restore(a, o);
    if (!a->model.marked) { AppMark(a); a->model.marked = true; }

    Vector3 r, u, n;
    ViewAxes(vp, &r, &u, &n);
    float upp = ViewUnitsPerPixel(a, vp);
    bool snap = Snapping(a);
    bool only_sel = a->model.drag == MDRAG_VERTS;

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
        if (snap) ang = Snapf(ang, PI / 12.0f);     /* 15도 */
        for (int i = 0; i < o->nverts; i++) {
            if (only_sel && !o->verts[i].sel) continue;
            Vector3 d = Vector3RotateByAxisAngle(Vector3Subtract(o->verts[i].p, pivot), n, -ang);
            o->verts[i].p = Vector3Add(pivot, d);
        }
    } else if (IsKeyDown(KEY_T)) {
        float f = fmaxf(0.0f, 1.0f + (dx - dy) * 0.01f);
        if (snap) f = Snapf(f, 0.125f);
        for (int i = 0; i < o->nverts; i++) {
            if (only_sel && !o->verts[i].sel) continue;
            Vector3 d = Vector3Subtract(o->verts[i].p, pivot);
            d = Vector3Add(d, Vector3Scale(r, Vector3DotProduct(d, r) * (f - 1.0f)));
            d = Vector3Add(d, Vector3Scale(u, Vector3DotProduct(d, u) * (f - 1.0f)));
            o->verts[i].p = Vector3Add(pivot, d);
        }
    } else {
        Vector3 d = Vector3Add(Vector3Scale(r, dx * upp), Vector3Scale(u, -dy * upp));
        if (snap) d = (Vector3){ Snapf(d.x, a->grid), Snapf(d.y, a->grid), Snapf(d.z, a->grid) };
        if (a->model.drag == MDRAG_MESH && KeyShift()) {
            o->origin = Vector3Add(o->origin, d);
        } else {
            for (int i = 0; i < o->nverts; i++)
                if (!only_sel || o->verts[i].sel) o->verts[i].p = Vector3Add(o->verts[i].p, d);
            if (!only_sel) o->origin = Vector3Add(o->origin, d);
        }
    }
}

/* ---- 선택 ---- */

static void BoxSelect(App *a, const Viewport *vp, Rectangle r, bool add) {
    Object *o = AppActive(a);
    if (!o) return;
    if (!add) ObjClearSelection(o);
    for (int i = 0; i < o->nverts; i++) {
        Vector2 sp;
        if (ViewProject(vp, o->verts[i].p, &sp) && CheckCollisionPointRec(sp, r)) o->verts[i].sel = true;
    }
}

/* 3D 뷰에서 좌클릭으로 고를 정점 (텍스처 모드는 면만 다루므로 없음) */
static int PickVert3D(App *a, const Viewport *vp, Vector2 m) {
    return a->mode == MODE_MODEL ? ViewPickVert3D(a, vp, m) : -1;
}

/* 3D 뷰 클릭: 활성 메쉬의 정점 강조 원 안이면 정점, 다른 메쉬면 메쉬 선택, 활성 메쉬면 면 선택 */
static void Click3D(App *a, const Viewport *vp, Vector2 m) {
    int oi, fi;
    Object *o = AppActive(a);
    int vi = PickVert3D(a, vp, m);
    if (vi >= 0) {
        if (KeyShift()) {
            /* 면 선택은 꼭짓점 선택으로 바꿔서 이어감 */
            for (int i = 0; i < o->nfaces; i++) o->faces[i].sel = false;
            o->verts[vi].sel = !o->verts[vi].sel;
        } else {
            ObjClearSelection(o);
            o->verts[vi].sel = true;
        }
    } else if (ViewPickFace(a, vp, m, -1, &oi, &fi)) {
        if (oi != a->scene.active) {
            AppSelectMesh(a, oi);
            return;
        }
        Face *f = &o->faces[fi];
        if (KeyShift()) {
            f->sel = !f->sel;
        } else {
            ObjClearSelection(o);
            f->sel = true;
        }
        for (int i = 0; i < o->nverts; i++) o->verts[i].sel = false;
        ObjSelectFaceVerts(o);
    } else if (o && (ObjCountSelected(o, true) || ObjCountSelected(o, false))) {
        ObjClearSelection(o);
    } else {
        AppSelectMesh(a, -1);
    }
}

static void OpenContextMenu(App *a, const Viewport *vp, Vector2 m) {
    Object *o = AppActive(a);
    a->menu_view = vp->kind;
    int vi = vp->kind == VIEW_3D ? PickVert3D(a, vp, m) : ViewPickVert(a, vp, m);
    int oi, fi;
    /* 3D 뷰는 직접 고른 정점일 때만 (면을 고르면 꼭짓점도 같이 골라지므로 면이 골라져 있으면 면 메뉴) */
    bool vert_menu = vp->kind != VIEW_3D || (o && vi >= 0 && o->verts[vi].sel && ObjCountSelected(o, true) == 0);
    if (o && vi >= 0 && a->mode == MODE_MODEL && vert_menu) {
        if (!o->verts[vi].sel) {
            ObjClearSelection(o);
            o->verts[vi].sel = true;
        }
        AppOpenMenu(a, MENU_VERTEX);
    } else if (ViewPickFace(a, vp, m, -1, &oi, &fi)) {
        if (oi != a->scene.active) AppSelectMesh(a, oi);
        o = AppActive(a);
        if (vp->kind == VIEW_3D) {
            if (!o->faces[fi].sel) {
                ObjClearSelection(o);
                o->faces[fi].sel = true;
                ObjSelectFaceVerts(o);
            }
            AppOpenMenu(a, MENU_FACE);
        } else {
            AppOpenMenu(a, MENU_MESH);
        }
    } else {
        Vector3 r, u, n;
        ViewAxes(vp, &r, &u, &n);
        if (vp->kind == VIEW_3D) {
            a->add_at = a->target;
            /* 카메라를 향하는 주축 */
            Vector3 up = { 0 };
            if (fabsf(n.x) >= fabsf(n.y) && fabsf(n.x) >= fabsf(n.z)) up.x = n.x > 0 ? 1.0f : -1.0f;
            else if (fabsf(n.y) >= fabsf(n.z)) up.y = n.y > 0 ? 1.0f : -1.0f;
            else up.z = n.z > 0 ? 1.0f : -1.0f;
            a->add_up = up;
        } else {
            float upp = ViewUnitsPerPixel(a, vp);
            Vector2 c = { vp->rect.x + vp->rect.width / 2, vp->rect.y + vp->rect.height / 2 };
            Vector3 p = Vector3Add(vp->center, Vector3Add(Vector3Scale(r, (m.x - c.x) * upp), Vector3Scale(u, -(m.y - c.y) * upp)));
            p = Vector3Subtract(p, Vector3Scale(n, Vector3DotProduct(p, n)));   /* 깊이는 0 */
            a->add_at = p;
            a->add_up = n;
        }
        a->add_at = (Vector3){ Snapf(a->add_at.x, a->grid), Snapf(a->add_at.y, a->grid), Snapf(a->add_at.z, a->grid) };
        AppOpenMenu(a, MENU_ADD);
    }
}

/* ---- 마우스 ---- */

static void PanView(App *a, Viewport *vp, Vector2 d) {
    Vector3 r, u, n;
    ViewAxes(vp, &r, &u, &n);
    Vector3 move = Vector3Add(Vector3Scale(r, -d.x * ViewUnitsPerPixel(a, vp)),
                              Vector3Scale(u, d.y * ViewUnitsPerPixel(a, vp)));
    if (vp->kind == VIEW_3D) a->target = Vector3Add(a->target, move);
    else vp->center = Vector3Add(vp->center, move);
}

static void UpdateMouse(App *a, bool input_ok) {
    Vector2 m = GetMousePosition();
    Vector2 md = GetMouseDelta();
    int hv = ViewAt(a, m);

    if (a->model.drag == MDRAG_NONE && hv >= 0 && input_ok) {
        Viewport *vp = &a->views[hv];
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            if (hv == VIEW_3D) a->dist = Clamp(a->dist * powf(0.88f, wheel), 0.5f, 100.0f);
            else vp->zoom = Clamp(vp->zoom * powf(0.88f, wheel), 0.5f, 100.0f);
        }

        a->model.press = m;
        a->model.moved = false;
        a->model.view = hv;
        Object *o = AppActive(a);

        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            a->model.drag = MDRAG_PAN;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            a->model.drag = MDRAG_RMB;
        } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (hv == VIEW_3D) {
                a->model.drag = KeyCtrl() ? MDRAG_BOX : MDRAG_CLICK3D;
                a->model.box_add = KeyShift();
            } else {
                int vi = ViewPickVert(a, vp, m);
                int oi, fi;
                if (vi >= 0) {
                    Vert *v = &o->verts[vi];
                    if (KeyShift() && v->sel) {
                        v->sel = false;
                    } else {
                        if (!KeyShift() && !v->sel) ObjClearSelection(o);
                        v->sel = true;
                        BeginEditDrag(a, MDRAG_VERTS);
                    }
                } else if (ViewPickFace(a, vp, m, -1, &oi, &fi)) {
                    if (oi != a->scene.active) AppSelectMesh(a, oi);
                    BeginEditDrag(a, MDRAG_MESH);
                } else {
                    a->model.drag = MDRAG_BOX;
                    a->model.box_add = KeyShift();
                }
            }
        }
    }
    if (a->model.drag == MDRAG_NONE) return;

    Viewport *vp = &a->views[a->model.view];
    if (Vector2Distance(m, a->model.press) > 3.0f) a->model.moved = true;

    switch (a->model.drag) {
    case MDRAG_VERTS:
    case MDRAG_MESH:
        ApplyEditDrag(a, vp, m);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) a->model.drag = MDRAG_NONE;
        break;
    case MDRAG_BOX:
        if (a->model.moved)
            a->model.box = (Rectangle){ fminf(m.x, a->model.press.x), fminf(m.y, a->model.press.y),
                                 fabsf(m.x - a->model.press.x), fabsf(m.y - a->model.press.y) };
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Object *o = AppActive(a);
            if (a->model.moved) {
                BoxSelect(a, vp, a->model.box, a->model.box_add);
            } else if (o && (ObjCountSelected(o, false) || ObjCountSelected(o, true))) {
                ObjClearSelection(o);
            } else {
                AppSelectMesh(a, -1);
            }
            a->model.box.width = 0;
            a->model.drag = MDRAG_NONE;
        }
        break;
    case MDRAG_CLICK3D:
        if (a->model.moved) a->model.drag = MDRAG_ORBIT;
        else if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Click3D(a, vp, m);
            a->model.drag = MDRAG_NONE;
        }
        break;
    case MDRAG_ORBIT:
        a->yaw -= md.x * 0.01f;
        a->pitch = Clamp(a->pitch + md.y * 0.01f, -1.55f, 1.55f);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) a->model.drag = MDRAG_NONE;
        break;
    case MDRAG_PAN:
        PanView(a, vp, md);
        if (!IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) a->model.drag = MDRAG_NONE;
        break;
    case MDRAG_RMB:
        if (a->model.moved) {
            /* 우클릭 드래그: 3D는 회전, 직교 뷰는 이동 */
            a->model.drag = a->model.view == VIEW_3D ? MDRAG_ORBIT : MDRAG_PAN;
        } else if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            OpenContextMenu(a, vp, m);
            a->model.drag = MDRAG_NONE;
        }
        break;
    default:
        break;
    }
}

/* ---- 키보드 ---- */

static Viewport *KeyView(App *a) {
    int hv = ViewAt(a, GetMousePosition());
    if (hv >= 0) a->model.key_view = hv;
    return &a->views[a->model.key_view];
}

static bool Pressed(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

static void ArrowMove(App *a) {
    int dx = (Pressed(KEY_RIGHT) || Pressed(KEY_D)) - (Pressed(KEY_LEFT) || Pressed(KEY_A));
    int dy = (Pressed(KEY_UP) || Pressed(KEY_W)) - (Pressed(KEY_DOWN) || Pressed(KEY_S));
    if (!dx && !dy) return;

    Viewport *vp = KeyView(a);
    Vector3 r, u, n;
    ViewAxes(vp, &r, &u, &n);
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
    float step = KeyCtrl() ? a->grid / 4.0f : a->grid;
    Vector3 d = Vector3Add(Vector3Scale(r, (float)dx * step), Vector3Scale(u, (float)dy * step));

    Object *o = AppActive(a);
    if (!o) {
        PanView(a, vp, (Vector2){ (float)-dx * 16, (float)dy * 16 });
        return;
    }
    AppMark(a);
    if (ObjCountSelected(o, false)) {
        for (int i = 0; i < o->nverts; i++)
            if (o->verts[i].sel) o->verts[i].p = Vector3Add(o->verts[i].p, d);
    } else {
        ObjTranslate(o, d);
    }
}

static void UpdateKeys(App *a) {
    bool ctrl = KeyCtrl();
    Object *o = AppActive(a);

    if (ctrl) {
        if (IsKeyPressed(KEY_X)) ModelDeleteMesh(a);
        if (IsKeyPressed(KEY_C)) ModelCopy(a);
        if (IsKeyPressed(KEY_V)) ModelPaste(a);
        if (Pressed(KEY_UP) || Pressed(KEY_DOWN) || Pressed(KEY_LEFT) || Pressed(KEY_RIGHT)) ArrowMove(a);
        return;
    }
    if (IsKeyPressed(KEY_SPACE)) {
        int hv = ViewAt(a, GetMousePosition());
        a->full = a->full >= 0 ? -1 : hv;
    }
    if (IsKeyPressed(KEY_E)) ModelExtrude(a);
    if (IsKeyPressed(KEY_X)) ModelDeleteFaces(a);
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) ModelDeleteVerts(a);
    if (IsKeyPressed(KEY_F) && o && ObjCountSelected(o, true)) AppOpenMenu(a, MENU_FACEPROPS);
    if (IsKeyPressed(KEY_G) && o) AppOpenMenu(a, MENU_MESHPROPS);
    if (IsKeyPressed(KEY_M)) a->render_mode = (a->render_mode + 1) % RENDER_COUNT;
    if (IsKeyPressed(KEY_L)) a->shading = !a->shading;
    if (!IsKeyDown(KEY_R) && !IsKeyDown(KEY_T)) ArrowMove(a);
}

/* UpdateMouse의 좌클릭 판정과 같은 순서로, 누르기 전에 강조할 대상을 정한다 */
static void UpdateHover(App *a, bool input_ok) {
    a->hover_obj = a->hover_face = a->hover_vert = -1;
    if (!input_ok || a->model.drag != MDRAG_NONE || a->menu != MENU_NONE) return;
    Vector2 m = GetMousePosition();
    int hv = ViewAt(a, m);
    if (hv < 0) return;
    const Viewport *vp = &a->views[hv];
    int oi, fi;
    if (hv == VIEW_3D) {
        if (KeyCtrl()) return;                       /* Ctrl+드래그는 박스 선택 */
        int vi = PickVert3D(a, vp, m);
        if (vi >= 0) {
            a->hover_obj = a->scene.active;
            a->hover_vert = vi;
            return;
        }
        if (!ViewPickFace(a, vp, m, -1, &oi, &fi)) return;
        a->hover_obj = oi;
        if (oi == a->scene.active) a->hover_face = fi;   /* 다른 메쉬면 메쉬 전체 */
        return;
    }
    int vi = ViewPickVert(a, vp, m);
    if (vi >= 0) {
        a->hover_obj = a->scene.active;
        a->hover_vert = vi;
    } else if (ViewPickFace(a, vp, m, -1, &oi, &fi) && oi != a->scene.active) {
        a->hover_obj = oi;                             /* 직교 뷰에서 면을 누르면 메쉬 선택 */
    }
}

void ModelUpdate(App *a, bool input_ok, bool keys) {
    if (a->scene.active >= a->scene.count) a->scene.active = -1;
    UpdateMouse(a, input_ok && !a->ui_hover);
    UpdateHover(a, input_ok && !a->ui_hover);
    if (keys && input_ok && a->model.drag == MDRAG_NONE) UpdateKeys(a);
}

/* R/T를 누르고 있으면 그 동작, 아니면 이동 */
static int EditCursor(void) {
    return IsKeyDown(KEY_R) ? CURSOR_ROTATE : IsKeyDown(KEY_T) ? CURSOR_SCALE : CURSOR_MOVE;
}

int ModelCursor(App *a) {
    switch (a->model.drag) {
    case MDRAG_VERTS:
    case MDRAG_MESH:  return EditCursor();
    case MDRAG_BOX:   return CURSOR_CROSS;
    case MDRAG_ORBIT: return CURSOR_ROTATE;
    case MDRAG_PAN:   return CURSOR_GRAB;
    default: break;
    }

    /* 직교 뷰에서 정점이나 메쉬 위에 있으면 끌 수 있다는 표시 */
    Vector2 m = GetMousePosition();
    int hv = ViewAt(a, m);
    if (hv < 0 || hv == VIEW_3D || a->mode != MODE_MODEL) return CURSOR_ARROW;
    const Viewport *vp = &a->views[hv];
    int oi, fi;
    if (ViewPickVert(a, vp, m) >= 0 || ViewPickFace(a, vp, m, -1, &oi, &fi)) return EditCursor();
    return CURSOR_ARROW;
}
