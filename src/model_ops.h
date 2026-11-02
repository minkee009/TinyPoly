#ifndef MODEL_OPS_H
#define MODEL_OPS_H

#include "app.h"

/* 활성 메쉬 편집 명령 (메뉴·막대·단축키에서 부름). 바꾸기 전에 app_mark */
void model_add_prim(App *a, int kind);
void model_extrude(App *a);
void model_delete_faces(App *a);
void model_delete_verts(App *a);
void model_delete_mesh(App *a);
void model_merge(App *a);
void model_snap(App *a);
void model_mirror(App *a, int axis);
void model_clone(App *a);
void model_copy(App *a);
void model_paste(App *a);
void model_set_face_flag(App *a, int flag, bool on);
void model_set_face_color(App *a, Color color);
void model_center_origin(App *a);

#endif
