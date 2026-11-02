#ifndef APP_H
#define APP_H

#include <raylib.h>
#include <microui/microui.h>
#include "core/mesh.h"
#include "app/state.h"

#define TOPBAR_H 30
#define VERT_RING_R 6              /* 정점 강조 원 반지름 (뷰 도트 단위). 3D 뷰는 이 안을 누르면 정점 선택 */

typedef enum { MODE_MODEL, MODE_TEXTURE } AppMode;

/* 4분할: 좌상 top, 우상 3D, 좌하 front, 우하 side */
enum { VIEW_TOP, VIEW_3D, VIEW_FRONT, VIEW_SIDE, VIEW_COUNT };
enum { RENDER_TEXTURED, RENDER_COLOR, RENDER_WIRE, RENDER_COUNT };
enum { TOOL_UV, TOOL_PEN, TOOL_PICK, TOOL_FILL, TOOL_LINE, TOOL_RECT, TOOL_CIRCLE, TOOL_MARQUEE, TOOL_COUNT };

typedef enum {
    MENU_NONE, MENU_ADD, MENU_VERTEX, MENU_FACE, MENU_MESH,
    MENU_FACEPROPS, MENU_FACECOLOR, MENU_FACEOPTS, MENU_MESHPROPS, MENU_FILE, MENU_UV, MENU_TEXSIZE,
} MenuKind;

typedef struct {
    int kind;
    Rectangle rect;            /* 화면 영역. w == 0이면 숨김 */
    RenderTexture2D rt;
    int rtw, rth;
    Camera3D cam;
    Vector3 center;            /* 직교 뷰: 보는 중심 */
    float zoom;                /* 직교 뷰: 화면 높이(월드 단위) */
} Viewport;

typedef struct {
    Scene scene;
    AppMode mode;

    Viewport views[VIEW_COUNT];
    int full;                  /* 전체화면 뷰, -1이면 4분할 */
    float yaw, pitch, dist;    /* 3D 뷰 궤도 카메라 */
    Vector3 target;
    int render_mode, shading;
    float grid;
    int snap;
    int pixel;                 /* 뷰 저해상도 배율 */

    /* 텍스처 모드 */
    int tool;
    Color color, color2;       /* 좌클릭 / 우클릭으로 칠하는 색 */
    int pick_target;           /* 색 선택 창이 고치는 색: 0 = color, 1 = color2 */
    int pal_sel;               /* 팔레트에서 고른 칸, 없으면 -1 */
    int rect_filled, circle_filled;
    float tex_zoom;            /* 텍셀 하나의 화면 픽셀 수, 0이면 창에 맞춤 */
    int tex_grid;
    Vector2 tex_pan;
    int pal_scroll;            /* 팔레트 줄이 넘칠 때 첫 줄 */
    Rectangle canvas_rect;

    /* 모드별 조작 상태 (state.h) */
    ModelState model;
    PaintState paint;

    /* 메뉴/창 */
    MenuKind menu;
    bool open_menu;            /* 이번 프레임에 팝업을 연다 */
    int menu_view;
    Vector3 add_at, add_up;
    int show_overview, show_color, show_export;
    int rename_idx;
    int exp_format, exp_zip;   /* 내보내기 창 설정 */

    /* GPU 텍스처: 장면 텍스처를 그대로 올림 (음영은 정점 색으로 곱함) */
    Texture2D gpu_tex;
    int gpu_w, gpu_h;
    const TexBuf *gpu_src;
    bool tex_dirty;            /* 전체를 다시 올림 */
    bool tex_rect_dirty;       /* 아래 영역만 다시 올림 */
    int dx0, dy0, dx1, dy1;
    Shader shader;

    Object clipboard;
    bool has_clipboard;
    Vector2 uv_clip[MAX_FACE_VERTS];
    int uv_clip_n;

    char path[256];
    bool has_path;             /* 저장/열기로 정해진 경로인지 (아니면 첫 저장 때 위치를 물음) */
    char msg[128];
    double msg_time;
    bool dirty;
    bool ui_hover;             /* 마우스가 UI 창 위 */
    /* 지금 좌클릭하면 고를 대상 (없으면 -1). face와 vert가 둘 다 -1이면 메쉬 전체 */
    int hover_obj, hover_face, hover_vert;
    mu_Context *ui;
} App;

void app_msg(App *a, const char *fmt, ...);
void app_mark(App *a);         /* 변경 직전: undo 저장 + dirty */
void app_undo(App *a);
void app_redo(App *a);
Object *app_active(App *a);
void app_select_mesh(App *a, int i);
void app_open_menu(App *a, MenuKind m);

/* 수명: 창을 연 뒤 init, 매 프레임 update → draw, 창을 닫은 뒤 shutdown */
void app_init(App *a);
void app_update(App *a);       /* 입력 처리 + UI 만들기 */
void app_draw(App *a);
void app_shutdown(App *a);

#endif
