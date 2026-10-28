#include "io.h"
#include "texture.h"
#include <raymath.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- 버퍼 ---- */

typedef struct { char *d; size_t n, cap; } Buf;

static void buf_put(Buf *b, const void *data, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 4096;
        while (nc < b->n + n + 1) nc *= 2;
        b->d = realloc(b->d, nc);
        b->cap = nc;
    }
    memcpy(b->d + b->n, data, n);
    b->n += n;
    b->d[b->n] = '\0';
}

static void buf_str(Buf *b, const char *s) {
    buf_put(b, s, strlen(s));
}

static void buf_printf(Buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) buf_put(b, tmp, (size_t)(n < (int)sizeof tmp ? n : (int)sizeof tmp - 1));
}

static void buf_json_str(Buf *b, const char *s) {
    buf_str(b, "\"");
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { buf_str(b, "\\"); buf_put(b, s, 1); }
        else if (c < 0x20) buf_printf(b, "\\u%04x", c);
        else buf_put(b, s, 1);
    }
    buf_str(b, "\"");
}

static void buf_align4(Buf *b, char pad) {
    while (b->n % 4) buf_put(b, &pad, 1);
}

static void comma(Buf *b, int index) {
    if (index > 0) buf_str(b, ",");
}

/* ---- base64 ---- */

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_encode(Buf *b, const unsigned char *src, size_t n) {
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)src[i] << 16;
        if (i + 1 < n) v |= (uint32_t)src[i + 1] << 8;
        if (i + 2 < n) v |= src[i + 2];
        char out[4] = { B64[(v >> 18) & 63], B64[(v >> 12) & 63],
                        i + 1 < n ? B64[(v >> 6) & 63] : '=', i + 2 < n ? B64[v & 63] : '=' };
        buf_put(b, out, 4);
    }
}

static unsigned char *b64_decode(const char *s, size_t *out_n) {
    static signed char map[256];
    static bool init;
    if (!init) {
        memset(map, -1, sizeof map);
        for (int i = 0; i < 64; i++) map[(unsigned char)B64[i]] = (signed char)i;
        init = true;
    }
    size_t len = strlen(s);
    unsigned char *out = malloc(len / 4 * 3 + 3);
    size_t n = 0;
    uint32_t acc = 0;
    int bits = 0;
    for (size_t i = 0; i < len; i++) {
        int v = map[(unsigned char)s[i]];
        if (v < 0) continue;            /* '=' 와 공백은 건너뜀 */
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[n++] = (unsigned char)(acc >> bits);
        }
    }
    *out_n = n;
    return out;
}

/* 한 줄 읽기 (텍스처 PNG가 한 줄에 들어가므로 길이 제한 없음). EOF면 NULL */
static char *read_line(FILE *fp, Buf *b) {
    b->n = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && c != '\n') {
        char ch = (char)c;
        buf_put(b, &ch, 1);
    }
    if (c == EOF && b->n == 0) return NULL;
    if (!b->d) buf_put(b, "", 0);
    if (b->n && b->d[b->n - 1] == '\r') b->d[--b->n] = '\0';
    return b->d;
}

/* ---- 자체 포맷 ----
   tinypoly 3
   bg r g b a
   pal r g b a                     (팔레트 칸마다)
   tex <w> <h> <PNG를 base64로>
   o <이름>
   origin x y z / state <hidden> <locked>
   v x y z
   f r g b a <flags> <개수> i0 i1 ... u0 v0 u1 v1 ...
*/

bool scene_save(const Scene *s, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return false;
    fprintf(fp, "tinypoly 3\nbg %d %d %d %d\n", s->bg.r, s->bg.g, s->bg.b, s->bg.a);
    for (int i = 0; i < s->npalette; i++) {
        Color c = s->palette[i];
        fprintf(fp, "pal %d %d %d %d\n", c.r, c.g, c.b, c.a);
    }

    Image img = { s->tex->px, s->tex->w, s->tex->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    int size = 0;
    unsigned char *png = ExportImageToMemory(img, ".png", &size);
    if (!png) { fclose(fp); return false; }
    Buf b = { 0 };
    b64_encode(&b, png, (size_t)size);
    MemFree(png);
    fprintf(fp, "tex %d %d ", s->tex->w, s->tex->h);
    fwrite(b.d, 1, b.n, fp);
    fputc('\n', fp);
    free(b.d);

    for (int i = 0; i < s->count; i++) {
        const Object *o = &s->objs[i];
        fprintf(fp, "o %s\norigin %g %g %g\nstate %d %d\n", o->name, o->origin.x, o->origin.y, o->origin.z,
                o->hidden, o->locked);
        for (int k = 0; k < o->nverts; k++) {
            Vector3 p = o->verts[k].p;
            fprintf(fp, "v %g %g %g\n", p.x, p.y, p.z);
        }
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            fprintf(fp, "f %d %d %d %d %d %d", f->color.r, f->color.g, f->color.b, f->color.a, f->flags, f->n);
            for (int j = 0; j < f->n; j++) fprintf(fp, " %d", f->v[j]);
            for (int j = 0; j < f->n; j++) fprintf(fp, " %g %g", f->uv[j].x, f->uv[j].y);
            fputc('\n', fp);
        }
    }
    return fclose(fp) == 0;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static Color rgba(int r, int g, int b, int a) {
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
}

/* 면 한 줄의 인덱스/UV 부분 */
static bool parse_face_rest(Object *o, char *c, Color color, int flags, int n) {
    int used, idx[MAX_FACE_VERTS];
    if (n < 3 || n > MAX_FACE_VERTS) return false;
    for (int j = 0; j < n; j++) {
        if (sscanf(c, "%d%n", &idx[j], &used) != 1 || idx[j] < 0 || idx[j] >= o->nverts) return false;
        c += used;
    }
    int fi = obj_add_face(o, idx, n, color);   /* 배열이 옮겨질 수 있어 먼저 호출 */
    Face *f = &o->faces[fi];
    f->flags = flags;
    for (int j = 0; j < n; j++) {
        if (sscanf(c, "%f %f%n", &f->uv[j].x, &f->uv[j].y, &used) != 2) return false;
        c += used;
    }
    return true;
}

bool scene_load(Scene *out, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    static Scene s;          /* 크기가 커서 스택 대신 */
    scene_init(&s);
    Buf line = { 0 };
    char *l = read_line(fp, &line);
    int version = l && strncmp(l, "tinypoly 3", 10) == 0 ? 3 : l && strncmp(l, "tinypoly 2", 10) == 0 ? 2 : 0;
    bool ok = version != 0;
    bool palette_read = false;

    /* v2: 16색 인덱스 팔레트 → 색으로 변환 */
    Color v2pal[16];
    memcpy(v2pal, DEFAULT_PALETTE, sizeof v2pal);
    int v2n = 0, v2alpha = 0;

    Object *o = NULL;
    while (ok && (l = read_line(fp, &line))) {
        int r, g, b, a, n, used;
        if (version == 3 && strncmp(l, "tex ", 4) == 0) {
            int w, h;
            if (sscanf(l + 4, "%d %d %n", &w, &h, &used) != 2) { ok = false; break; }
            size_t size;
            unsigned char *png = b64_decode(l + 4 + used, &size);
            Image img = LoadImageFromMemory(".png", png, (int)size);
            free(png);
            if (!IsImageValid(img) || img.width != w || img.height != h || w > MAX_TEX_SIZE || h > MAX_TEX_SIZE) {
                UnloadImage(img);
                ok = false;
                break;
            }
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            texbuf_release(s.tex);
            s.tex = texbuf_new(w, h);
            memcpy(s.tex->px, img.data, sizeof(Color) * (size_t)w * (size_t)h);
            UnloadImage(img);
        } else if (version == 3 && sscanf(l, "bg %d %d %d %d", &r, &g, &b, &a) == 4) {
            s.bg = rgba(r, g, b, a);
        } else if (version == 3 && sscanf(l, "pal %d %d %d %d", &r, &g, &b, &a) == 4) {
            if (!palette_read) { s.npalette = 0; palette_read = true; }
            if (s.npalette < MAX_PALETTE) s.palette[s.npalette++] = rgba(r, g, b, a);
        } else if (version == 2 && sscanf(l, "c %d %d %d %d", &r, &g, &b, &a) == 4) {
            if (v2n < 16) v2pal[v2n++] = rgba(r, g, b, 255);
        } else if (version == 2 && sscanf(l, "bg %d", &r) == 1) {
            s.bg = v2pal[r & 15];
        } else if (version == 2 && sscanf(l, "alpha %d", &r) == 1) {
            v2alpha = r & 15;
        } else if (version == 2 && strcmp(l, "tex") == 0) {
            Color *px = scene_tex_mut(&s);
            for (int y = 0; y < 128 && ok; y++) {
                l = read_line(fp, &line);
                ok = l && strlen(l) >= 128;
                for (int x = 0; x < 128 && ok; x++) {
                    int v = hexval(l[x]);
                    if (v < 0) ok = false;
                    else px[y * 128 + x] = v == v2alpha ? BLANK : v2pal[v];
                }
            }
        } else if (l[0] == 'o' && l[1] == ' ') {
            o = scene_add(&s, l + 2);
            if (!o) ok = false;
        } else if (o && sscanf(l, "origin %f %f %f", &o->origin.x, &o->origin.y, &o->origin.z) == 3) {
            /* 읽기만 */
        } else if (o && sscanf(l, "state %d %d", &r, &g) == 2) {
            o->hidden = r != 0;
            o->locked = g != 0;
        } else if (l[0] == 'v' && o) {
            Vector3 p;
            if (sscanf(l + 1, "%f %f %f", &p.x, &p.y, &p.z) == 3) obj_add_vert(o, p);
            else ok = false;
        } else if (l[0] == 'f' && o) {
            int flags;
            if (version == 3 && sscanf(l + 1, "%d %d %d %d %d %d%n", &r, &g, &b, &a, &flags, &n, &used) == 6)
                ok = parse_face_rest(o, l + 1 + used, rgba(r, g, b, a), flags, n);
            else if (version == 2 && sscanf(l + 1, "%d %d %d%n", &r, &flags, &n, &used) == 3)
                ok = parse_face_rest(o, l + 1 + used, v2pal[r & 15], flags, n);
            else
                ok = false;
        }
    }
    fclose(fp);
    free(line.d);
    if (version == 2) {
        memcpy(s.palette, v2pal, sizeof v2pal);
        s.npalette = 16;
    }

    if (!ok) { scene_release(&s); return false; }
    scene_release(out);
    *out = s;
    return true;
}

/* ---- 공통 ---- */

void path_with_ext(char *dst, size_t size, const char *path, const char *ext) {
    snprintf(dst, size, "%s", path);
    char *dot = strrchr(dst, '.');
    char *sep = strpbrk(dot ? dot : dst, "/\\");
    if (dot && !sep) *dot = '\0';
    strncat(dst, ext, size - strlen(dst) - 1);
}

/* ---- 내보내기용 텍스처: 원래 텍스처 + 아래쪽에 단색 면 색 띠 (4x4 칸) ---- */

#define CELL 4

typedef struct {
    int w, h, tex_h;          /* 전체 크기, 원래 텍스처 높이 */
    Color colors[1024];
    int ncolors;
} Atlas;

static void atlas_init(Atlas *at, const Scene *s) {
    at->w = s->tex->w;
    at->tex_h = s->tex->h;
    at->ncolors = 0;
    for (int i = 0; i < s->count; i++) {
        const Object *o = &s->objs[i];
        if (o->hidden) continue;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            if (!(f->flags & FACE_NOTEX)) continue;
            bool seen = false;
            for (int c = 0; c < at->ncolors && !seen; c++) seen = color_eq(at->colors[c], f->color);
            if (!seen && at->ncolors < 1024) at->colors[at->ncolors++] = f->color;
        }
    }
    int per_row = at->w / CELL;
    int rows = (at->ncolors + per_row - 1) / per_row;
    at->h = at->tex_h + rows * CELL;
}

/* 아틀라스 픽셀 좌표 (단색 면은 색 띠 칸 가운데) */
static Vector2 atlas_uv(const Atlas *at, const Face *f, int k) {
    if (!(f->flags & FACE_NOTEX)) return f->uv[k];
    int c = 0;
    while (c < at->ncolors && !color_eq(at->colors[c], f->color)) c++;
    int per_row = at->w / CELL;
    return (Vector2){ (float)(c % per_row) * CELL + CELL / 2.0f, (float)at->tex_h + (float)(c / per_row) * CELL + CELL / 2.0f };
}

static unsigned char *atlas_png(const Atlas *at, const Scene *s, size_t *size) {
    Color *px = calloc((size_t)at->w * (size_t)at->h, sizeof(Color));
    memcpy(px, s->tex->px, sizeof(Color) * (size_t)at->w * (size_t)at->tex_h);
    int per_row = at->w / CELL;
    for (int c = 0; c < at->ncolors; c++) {
        int cx = (c % per_row) * CELL, cy = at->tex_h + (c / per_row) * CELL;
        for (int y = 0; y < CELL; y++)
            for (int x = 0; x < CELL; x++) px[(cy + y) * at->w + cx + x] = at->colors[c];
    }
    Image img = { px, at->w, at->h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    int n = 0;
    unsigned char *png = ExportImageToMemory(img, ".png", &n);
    free(px);
    if (!png) return NULL;
    unsigned char *copy = malloc((size_t)n);
    memcpy(copy, png, (size_t)n);
    MemFree(png);
    *size = (size_t)n;
    return copy;
}

/* ---- 내보낼 파일 모음 ---- */

typedef struct {
    char name[96];
    unsigned char *data;
    size_t size;
} OutFile;

typedef struct {
    OutFile f[4];
    int n;
} OutSet;

static void out_add(OutSet *o, const char *name, void *data, size_t size) {
    OutFile *f = &o->f[o->n++];
    snprintf(f->name, sizeof f->name, "%s", name);
    f->data = data;
    f->size = size;
}

static void out_add_buf(OutSet *o, const char *name, Buf *b) {
    out_add(o, name, b->d, b->n);
    *b = (Buf){ 0 };
}

static void build_obj(const Scene *s, const Atlas *at, const char *base, OutSet *out) {
    char png[96], mtl[96];
    snprintf(png, sizeof png, "%s.png", base);
    snprintf(mtl, sizeof mtl, "%s.mtl", base);

    Buf m = { 0 };
    buf_printf(&m, "newmtl texture\nKa 0 0 0\nKd 1 1 1\nKs 0 0 0\nillum 1\nmap_Kd %s\nmap_d %s\n", png, png);

    Buf b = { 0 };
    buf_printf(&b, "# TinyPoly\nmtllib %s\nusemtl texture\n", mtl);
    int voff = 1, toff = 1, noff = 1;
    for (int i = 0; i < s->count; i++) {
        const Object *o = &s->objs[i];
        if (o->hidden) continue;
        buf_printf(&b, "o %s\n", o->name);
        for (int k = 0; k < o->nverts; k++) {
            Vector3 p = o->verts[k].p;
            buf_printf(&b, "v %g %g %g\n", p.x, p.y, p.z);
        }
        int nt = 0;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            Vector3 n = obj_face_normal(o, f);
            buf_printf(&b, "vn %g %g %g\n", n.x, n.y, n.z);
            for (int j = 0; j < f->n; j++, nt++) {
                Vector2 uv = atlas_uv(at, f, j);
                buf_printf(&b, "vt %g %g\n", uv.x / at->w, 1.0f - uv.y / at->h);
            }
        }
        int t = toff;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            buf_str(&b, "f");
            for (int j = 0; j < f->n; j++) buf_printf(&b, " %d/%d/%d", f->v[j] + voff, t++, k + noff);
            buf_str(&b, "\n");
        }
        voff += o->nverts;
        toff += nt;
        noff += o->nfaces;
    }
    char name[96];
    snprintf(name, sizeof name, "%s.obj", base);
    out_add_buf(out, name, &b);
    out_add_buf(out, mtl, &m);
}

typedef struct {
    Buf bin, views, accs;
    int nviews;
} Gltf;

static int add_view(Gltf *g, const void *data, size_t size, bool vertex_data) {
    buf_align4(&g->bin, 0);
    size_t off = g->bin.n;
    buf_put(&g->bin, data, size);
    comma(&g->views, g->nviews);
    buf_printf(&g->views, "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu%s}", off, size,
               vertex_data ? ",\"target\":34962" : "");
    return g->nviews++;
}

/* float 배열(ncomp = 2 또는 3) 하나를 bufferView + accessor로 추가.
   이미지 view는 정점 데이터를 다 넣은 뒤에 붙이므로 accessor 인덱스 = view 인덱스 */
static int add_accessor(Gltf *g, const float *data, int count, int ncomp, bool minmax) {
    int idx = add_view(g, data, sizeof(float) * (size_t)(count * ncomp), true);
    comma(&g->accs, idx);
    buf_printf(&g->accs, "{\"bufferView\":%d,\"componentType\":5126,\"count\":%d,\"type\":\"VEC%d\"",
               idx, count, ncomp);
    if (minmax) {
        float lo[3], hi[3];
        for (int c = 0; c < 3; c++) lo[c] = hi[c] = data[c];
        for (int i = 1; i < count; i++)
            for (int c = 0; c < 3; c++) {
                lo[c] = fminf(lo[c], data[i * 3 + c]);
                hi[c] = fmaxf(hi[c], data[i * 3 + c]);
            }
        buf_printf(&g->accs, ",\"min\":[%g,%g,%g],\"max\":[%g,%g,%g]", lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
    }
    buf_str(&g->accs, "}");
    return idx;
}

/* 재질은 (양면, 음영 없음) 조합 4가지. 모두 같은 텍스처를 씀 */
static int material_key(const Face *f) {
    return ((f->flags & FACE_DOUBLE) ? 1 : 0) | ((f->flags & FACE_NOSHADE) ? 2 : 0);
}

static void build_gltf(const Scene *s, const Atlas *at, const char *base, bool binary,
                       unsigned char *png, size_t png_size, OutSet *out) {
    Gltf g = { 0 };
    Buf nodes = { 0 }, meshes = { 0 }, mats = { 0 };
    int mat_of_key[4] = { -1, -1, -1, -1 }, nmats = 0, nmesh = 0;
    bool unlit = false;

    int cap = 0;
    for (int i = 0; i < s->count; i++)
        for (int k = 0; k < s->objs[i].nfaces; k++) cap += (s->objs[i].faces[k].n - 2) * 3;
    Vector3 *pos = malloc(sizeof(Vector3) * (size_t)(cap + 1));
    Vector3 *nrm = malloc(sizeof(Vector3) * (size_t)(cap + 1));
    Vector2 *tc = malloc(sizeof(Vector2) * (size_t)(cap + 1));

    for (int i = 0; i < s->count; i++) {
        const Object *o = &s->objs[i];
        if (o->nfaces == 0 || o->hidden) continue;

        comma(&meshes, nmesh);
        buf_str(&meshes, "{\"name\":");
        buf_json_str(&meshes, o->name);
        buf_str(&meshes, ",\"primitives\":[");

        /* 재질 하나당 primitive 하나. 평면 셰이딩이라 정점을 면마다 따로 둔다 */
        int nprim = 0;
        for (int key = 0; key < 4; key++) {
            int n = 0;
            for (int k = 0; k < o->nfaces; k++) {
                const Face *f = &o->faces[k];
                if (material_key(f) != key) continue;
                Vector3 fn = obj_face_normal(o, f);
                for (int j = 1; j + 1 < f->n; j++) {
                    int c[3] = { 0, j, j + 1 };
                    for (int m = 0; m < 3; m++) {
                        Vector2 uv = atlas_uv(at, f, c[m]);
                        pos[n] = o->verts[f->v[c[m]]].p;
                        nrm[n] = fn;
                        tc[n] = (Vector2){ uv.x / at->w, uv.y / at->h };
                        n++;
                    }
                }
            }
            if (n == 0) continue;

            if (mat_of_key[key] < 0) {
                comma(&mats, nmats);
                buf_printf(&mats, "{\"name\":\"mat%d\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},"
                           "\"metallicFactor\":0,\"roughnessFactor\":1},\"alphaMode\":\"MASK\",\"alphaCutoff\":0.5%s%s}",
                           key, (key & 1) ? ",\"doubleSided\":true" : "",
                           (key & 2) ? ",\"extensions\":{\"KHR_materials_unlit\":{}}" : "");
                if (key & 2) unlit = true;
                mat_of_key[key] = nmats++;
            }
            int ap = add_accessor(&g, (const float *)pos, n, 3, true);
            int an = add_accessor(&g, (const float *)nrm, n, 3, false);
            int atc = add_accessor(&g, (const float *)tc, n, 2, false);
            comma(&meshes, nprim++);
            buf_printf(&meshes, "{\"attributes\":{\"POSITION\":%d,\"NORMAL\":%d,\"TEXCOORD_0\":%d},\"material\":%d,\"mode\":4}",
                       ap, an, atc, mat_of_key[key]);
        }
        buf_str(&meshes, "]}");

        comma(&nodes, nmesh);
        buf_str(&nodes, "{\"name\":");
        buf_json_str(&nodes, o->name);
        buf_printf(&nodes, ",\"mesh\":%d}", nmesh);
        nmesh++;
    }
    free(pos);
    free(nrm);
    free(tc);

    char png_name[96], bin_name[96], name[96];
    snprintf(png_name, sizeof png_name, "%s.png", base);
    snprintf(bin_name, sizeof bin_name, "%s.bin", base);
    int image_view = binary ? add_view(&g, png, png_size, false) : -1;
    buf_align4(&g.bin, 0);

    Buf js = { 0 };
    buf_str(&js, "{\"asset\":{\"version\":\"2.0\",\"generator\":\"TinyPoly\"},");
    if (unlit) buf_str(&js, "\"extensionsUsed\":[\"KHR_materials_unlit\"],");
    buf_str(&js, "\"scene\":0,\"scenes\":[{\"nodes\":[");
    for (int i = 0; i < nmesh; i++) buf_printf(&js, i ? ",%d" : "%d", i);
    buf_str(&js, "]}],\"nodes\":[");        if (nodes.d) buf_put(&js, nodes.d, nodes.n);
    buf_str(&js, "],\"meshes\":[");         if (meshes.d) buf_put(&js, meshes.d, meshes.n);
    buf_str(&js, "],\"materials\":[");      if (mats.d) buf_put(&js, mats.d, mats.n);
    buf_str(&js, "],\"textures\":[{\"sampler\":0,\"source\":0}]");
    buf_str(&js, ",\"samplers\":[{\"magFilter\":9728,\"minFilter\":9728}]");
    if (binary) {
        buf_printf(&js, ",\"images\":[{\"bufferView\":%d,\"mimeType\":\"image/png\"}]", image_view);
    } else {
        buf_str(&js, ",\"images\":[{\"uri\":");
        buf_json_str(&js, png_name);
        buf_str(&js, "}]");
    }
    buf_str(&js, ",\"accessors\":[");       if (g.accs.d) buf_put(&js, g.accs.d, g.accs.n);
    buf_str(&js, "],\"bufferViews\":[");    if (g.views.d) buf_put(&js, g.views.d, g.views.n);
    buf_printf(&js, "],\"buffers\":[{\"byteLength\":%zu", g.bin.n);
    if (!binary) {
        buf_str(&js, ",\"uri\":");
        buf_json_str(&js, bin_name);
    }
    buf_str(&js, "}]}");

    if (binary) {
        /* GLB: 헤더 + JSON 청크(공백 패딩) + BIN 청크, 4바이트 정렬 */
        buf_align4(&js, ' ');
        Buf glb = { 0 };
        uint32_t total = 12 + 8 + (uint32_t)js.n + 8 + (uint32_t)g.bin.n;
        uint32_t hdr[5] = { 0x46546C67u, 2, total, (uint32_t)js.n, 0x4E4F534Au };
        uint32_t binhdr[2] = { (uint32_t)g.bin.n, 0x004E4942u };
        buf_put(&glb, hdr, sizeof hdr);
        buf_put(&glb, js.d, js.n);
        buf_put(&glb, binhdr, sizeof binhdr);
        if (g.bin.n) buf_put(&glb, g.bin.d, g.bin.n);
        snprintf(name, sizeof name, "%s.glb", base);
        out_add_buf(out, name, &glb);
        free(js.d);
        free(g.bin.d);
    } else {
        snprintf(name, sizeof name, "%s.gltf", base);
        out_add_buf(out, name, &js);
        out_add_buf(out, bin_name, &g.bin);
    }
    free(g.views.d); free(g.accs.d);
    free(nodes.d); free(meshes.d); free(mats.d);
}

/* ---- zip (압축 없이 저장. PNG는 이미 압축돼 있음) ---- */

static uint32_t crc32_of(const unsigned char *d, size_t n) {
    static uint32_t table[256];
    static bool init;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put16(Buf *b, uint16_t v) { unsigned char x[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; buf_put(b, x, 2); }
static void put32(Buf *b, uint32_t v) { put16(b, (uint16_t)v); put16(b, (uint16_t)(v >> 16)); }

static bool write_zip(const char *path, const OutSet *out) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    uint16_t dtime = (uint16_t)((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2));
    uint16_t ddate = (uint16_t)(((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday);

    Buf z = { 0 }, cd = { 0 };
    for (int i = 0; i < out->n; i++) {
        const OutFile *f = &out->f[i];
        uint32_t crc = crc32_of(f->data, f->size), off = (uint32_t)z.n;
        uint16_t nlen = (uint16_t)strlen(f->name);
        /* 로컬 헤더 */
        put32(&z, 0x04034b50); put16(&z, 20); put16(&z, 0x0800); put16(&z, 0);
        put16(&z, dtime); put16(&z, ddate); put32(&z, crc);
        put32(&z, (uint32_t)f->size); put32(&z, (uint32_t)f->size);
        put16(&z, nlen); put16(&z, 0);
        buf_put(&z, f->name, nlen);
        if (f->size) buf_put(&z, f->data, f->size);
        /* 중앙 디렉터리 */
        put32(&cd, 0x02014b50); put16(&cd, 20); put16(&cd, 20); put16(&cd, 0x0800); put16(&cd, 0);
        put16(&cd, dtime); put16(&cd, ddate); put32(&cd, crc);
        put32(&cd, (uint32_t)f->size); put32(&cd, (uint32_t)f->size);
        put16(&cd, nlen); put16(&cd, 0); put16(&cd, 0); put16(&cd, 0); put16(&cd, 0);
        put32(&cd, 0); put32(&cd, off);
        buf_put(&cd, f->name, nlen);
    }
    uint32_t cd_off = (uint32_t)z.n;
    buf_put(&z, cd.d, cd.n);
    put32(&z, 0x06054b50); put16(&z, 0); put16(&z, 0);
    put16(&z, (uint16_t)out->n); put16(&z, (uint16_t)out->n);
    put32(&z, (uint32_t)cd.n); put32(&z, cd_off); put16(&z, 0);

    FILE *fp = fopen(path, "wb");
    bool ok = fp && fwrite(z.d, 1, z.n, fp) == z.n;
    if (fp && fclose(fp) != 0) ok = false;
    free(z.d);
    free(cd.d);
    return ok;
}

static bool write_folder(const char *dir, const OutSet *out) {
    if (!DirectoryExists(dir) && MakeDirectory(dir) != 0) return false;
    for (int i = 0; i < out->n; i++) {
        char p[1200];
        snprintf(p, sizeof p, "%s/%s", dir, out->f[i].name);
        FILE *fp = fopen(p, "wb");
        bool ok = fp && fwrite(out->f[i].data, 1, out->f[i].size, fp) == out->f[i].size;
        if (fp && fclose(fp) != 0) ok = false;
        if (!ok) return false;
    }
    return true;
}

int export_file_names(ExportFormat fmt, const char *base, char names[][96], int max) {
    static const char *exts[EXPORT_FORMAT_COUNT][4] = {
        [EXPORT_OBJ]  = { ".obj", ".mtl", ".png", NULL },
        [EXPORT_GLTF] = { ".gltf", ".bin", ".png", NULL },
        [EXPORT_GLB]  = { ".glb", NULL },
    };
    int n = 0;
    for (int i = 0; exts[fmt][i] && n < max; i++, n++) snprintf(names[n], 96, "%s%s", base, exts[fmt][i]);
    return n;
}

bool export_scene(const Scene *s, ExportFormat fmt, bool zip, const char *out_path) {
    char base[64];                      /* 이름 + 확장자가 96자 안에 들어가도록 */
    snprintf(base, sizeof base, "%s", GetFileNameWithoutExt(out_path));
    if (!base[0]) snprintf(base, sizeof base, "model");

    static Atlas at;
    atlas_init(&at, s);
    size_t png_size = 0;
    unsigned char *png = atlas_png(&at, s, &png_size);
    if (!png) return false;

    OutSet out = { 0 };
    if (fmt == EXPORT_OBJ) build_obj(s, &at, base, &out);
    else build_gltf(s, &at, base, fmt == EXPORT_GLB, png, png_size, &out);

    char png_name[96];
    snprintf(png_name, sizeof png_name, "%s.png", base);
    if (fmt == EXPORT_GLB) free(png);          /* GLB 안에 들어감 */
    else out_add(&out, png_name, png, png_size);

    bool ok = zip ? write_zip(out_path, &out) : write_folder(out_path, &out);
    for (int i = 0; i < out.n; i++) free(out.f[i].data);
    return ok;
}
