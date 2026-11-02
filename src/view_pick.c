#include "view_pick.h"
#include "view.h"
#include <raymath.h>
#include <float.h>

bool view_pick_face(App *a, const Viewport *vp, Vector2 m, int only, int *out_obj, int *out_face) {
    Ray ray = view_ray(vp, m);
    float best = FLT_MAX;
    bool found = false;
    for (int i = 0; i < a->scene.count; i++) {
        const Object *o = &a->scene.objs[i];
        /* 잠긴 메쉬는 뷰에서 클릭으로 고를 수 없음 */
        if ((only >= 0 && i != only) || o->hidden || (only < 0 && o->locked)) continue;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            if (!(f->flags & FACE_DOUBLE) && Vector3DotProduct(obj_face_normal(o, f), ray.direction) >= 0) continue;
            Vector3 p0 = o->verts[f->v[0]].p;
            for (int j = 1; j + 1 < f->n; j++) {
                RayCollision c = GetRayCollisionTriangle(ray, p0, o->verts[f->v[j]].p, o->verts[f->v[j + 1]].p);
                if (c.hit && c.distance < best) {
                    best = c.distance;
                    *out_obj = i;
                    *out_face = k;
                    found = true;
                }
            }
        }
    }
    return found;
}

int view_pick_vert(App *a, const Viewport *vp, Vector2 m) {
    Object *o = app_active(a);
    if (!o) return -1;
    float best = 6.0f;
    int found = -1;
    for (int i = 0; i < o->nverts; i++) {
        Vector2 sp;
        if (!view_project(vp, o->verts[i].p, &sp)) continue;
        float d = Vector2Distance(sp, m) - (o->verts[i].sel ? 0.5f : 0.0f);   /* 겹치면 선택된 쪽 */
        if (d < best) { best = d; found = i; }
    }
    return found;
}

/* 카메라에서 p까지 사이에 화면에 그려지는 면이 있으면 가려진 것 */
static bool vert_visible(const App *a, const Viewport *vp, Vector3 p) {
    Vector3 d = Vector3Subtract(p, vp->cam.position);
    float dist = Vector3Length(d);
    if (dist < 1e-4f) return true;
    Ray ray = { vp->cam.position, Vector3Scale(d, 1.0f / dist) };
    float limit = dist * 0.995f;         /* 이 정점을 꼭짓점으로 쓰는 면은 거리 dist에서 맞으므로 빼고 */
    for (int i = 0; i < a->scene.count; i++) {
        const Object *o = &a->scene.objs[i];
        if (o->hidden) continue;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            if (!(f->flags & FACE_DOUBLE) && Vector3DotProduct(obj_face_normal(o, f), ray.direction) >= 0) continue;
            Vector3 p0 = o->verts[f->v[0]].p;
            for (int j = 1; j + 1 < f->n; j++) {
                RayCollision c = GetRayCollisionTriangle(ray, p0, o->verts[f->v[j]].p, o->verts[f->v[j + 1]].p);
                if (c.hit && c.distance < limit) return false;
            }
        }
    }
    return true;
}

int view_pick_vert_3d(App *a, const Viewport *vp, Vector2 m) {
    Object *o = app_active(a);
    if (!o || o->hidden) return -1;
    float best = (float)(VERT_RING_R * a->pixel);
    int found = -1;
    for (int i = 0; i < o->nverts; i++) {
        Vector2 sp;
        if (!view_project(vp, o->verts[i].p, &sp)) continue;
        float d = Vector2Distance(sp, m) - (o->verts[i].sel ? 0.5f : 0.0f);   /* 겹치면 선택된 쪽 */
        if (d < best && vert_visible(a, vp, o->verts[i].p)) { best = d; found = i; }
    }
    return found;
}
