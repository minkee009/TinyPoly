#ifndef MODEL_H
#define MODEL_H

#include "app/app.h"

/* 모델 모드: 뷰포트 마우스·키 조작 (model.c) */
void ModelUpdate(App *a, bool input_ok, bool keys);   /* keys: 모델 단축키까지 처리 */
int  ModelCursor(App *a);              /* 뷰포트 위 상황에 맞는 커서 */

/* 활성 메쉬 편집 명령 (model_ops.c). 메뉴·막대·단축키에서 부름. 바꾸기 전에 AppMark */
void ModelAddPrim(App *a, int kind);
void ModelExtrude(App *a);
void ModelDeleteFaces(App *a);
void ModelDeleteVerts(App *a);
void ModelDeleteMesh(App *a);
void ModelMerge(App *a);
void ModelSnap(App *a);
void ModelMirror(App *a, int axis);
void ModelClone(App *a);
void ModelCopy(App *a);
void ModelPaste(App *a);
void ModelSetFaceFlag(App *a, int flag, bool on);
void ModelSetFaceColor(App *a, Color color);
void ModelCenterOrigin(App *a);

#endif
