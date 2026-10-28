#ifndef APP_H
#define APP_H

#include <raylib.h>
#include <microui/microui.h>
#include "mesh.h"

#define TOPBAR_H 30

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
    mu_Context *ui;
} App;

void app_msg(App *a, const char *fmt, ...);
void app_mark(App *a);         /* 변경 직전: undo 저장 + dirty */
void app_undo(App *a);
void app_redo(App *a);
Object *app_active(App *a);
void app_select_mesh(App *a, int i);
void app_open_menu(App *a, MenuKind m);

/* view.c */
void view_init(App *a);
void view_layout(App *a);
void view_update_cameras(App *a);
void view_upload_texture(App *a);
void view_tex_dirty(App *a, int x0, int y0, int x1, int y1);   /* 텍스처 일부만 바뀜 (텍셀, 양끝 포함) */
void view_draw(App *a, Viewport *vp);
int  view_at(const App *a, Vector2 m);
Ray  view_ray(const Viewport *vp, Vector2 m);
bool view_project(const Viewport *vp, Vector3 p, Vector2 *out);
void view_axes(const Viewport *vp, Vector3 *right, Vector3 *up, Vector3 *normal);
float view_units_per_pixel(const App *a, const Viewport *vp);
bool view_pick_face(App *a, const Viewport *vp, Vector2 m, int only, int *oi, int *fi);
int  view_pick_vert(App *a, const Viewport *vp, Vector2 m);
extern Rectangle g_box;        /* 박스 선택 표시 (w == 0이면 없음) */

/* model.c */
void model_update(App *a, bool input_ok, bool keys);   /* keys: 모델 단축키까지 처리 */
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

/* paint.c */
void paint_update(App *a, bool input_ok);
void paint_draw(App *a);
void paint_reset_selection(void);       /* 텍스처 크기가 바뀌면 선택 해제 */
int  paint_cursor(App *a);              /* 캔버스 위에서 보여줄 커서, 해당 없으면 -1 */
bool paint_hover_texel(const App *a, Color *out);   /* 커서 아래 텍셀 색, 캔버스 밖이면 false */

/* cursor.c: OS 커서 대신 그리는 도트 커서 (맨 마지막에 그림) */
enum {
    CURSOR_ARROW, CURSOR_CROSS, CURSOR_PEN, CURSOR_PICKER, CURSOR_BUCKET,
    CURSOR_MOVE, CURSOR_HAND, CURSOR_GRAB, CURSOR_ROTATE, CURSOR_SCALE, CURSOR_COUNT
};
void cursor_draw(App *a);
int  model_cursor(App *a);              /* 뷰포트 위 상황에 맞는 커서 (model.c) */
void uv_rotate(App *a);
void uv_flip(App *a, bool vertical);
void uv_reset(App *a);
void uv_copy(App *a);
void uv_paste(App *a);

#endif
