#ifndef STATE_H
#define STATE_H

#include <raylib.h>
#include "mesh.h"

/* 모드별 조작 상태의 모양. 값은 App이 들고 있다 (a->model, a->paint) */

/* 모델 모드: 뷰포트 마우스 드래그 (model.c) */
typedef enum {
    MDRAG_NONE, MDRAG_VERTS, MDRAG_MESH, MDRAG_BOX, MDRAG_CLICK3D, MDRAG_ORBIT, MDRAG_PAN, MDRAG_RMB
} ModelDragKind;

typedef struct {
    ModelDragKind drag;
    int view;                  /* 드래그를 시작한 뷰 */
    Vector2 press;
    bool moved, marked, box_add;
    Vector3 *orig;             /* 드래그 시작 시 활성 메쉬 정점 위치 */
    Vector3 orig_origin;
    Rectangle box;             /* 박스 선택 표시 (w == 0이면 없음) */
    int key_view;              /* 화살표 키로 움직일 때 기준 뷰 (마우스가 마지막에 있던 뷰) */
} ModelState;

/* 텍스처 모드: 캔버스 드래그, 마키 선택, UV 드래그 (paint.c) */
typedef enum {
    PDRAG_NONE, PDRAG_STROKE, PDRAG_SHAPE, PDRAG_PAN, PDRAG_UV_MOVE, PDRAG_UV_VERT, PDRAG_UV_SCALE,
    PDRAG_SEL, PDRAG_SEL_MOVE
} PaintDragKind;

typedef struct {
    PaintDragKind drag;
    int x0, y0, lx, ly;        /* 시작/직전 텍셀 */
    Color draw_color;
    Vector2 press;
    bool moved, marked;
    Color *backup;             /* 도형 미리보기/선택 이동용 원본 (텍스처 크기) */
    int backup_n;

    /* 선택 영역(마키): 텍셀 좌표. sw == 0이면 없음 */
    int sx, sy, sw, sh;
    bool floating;             /* backup 위에 float_px를 띄워 둔 상태 */
    Color *float_px;
    int fw, fh, fx, fy, fx0, fy0;
    Color *clip;
    int cw, ch;

    /* UV 드래그: 드래그 시작 시 활성 메쉬 모든 면의 UV */
    Vector2 (*uv0)[MAX_FACE_VERTS];
    int uv_face, uv_vert;
} PaintState;

#endif
