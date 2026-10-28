#include "io.h"
#include "texture.h"
#include <raymath.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- 자체 포맷 ----
   tinypoly 2
   c <r> <g> <b> <ramp>           x16
   bg <n> / alpha <n>
   tex                             다음 128줄: 한 줄에 16진수 128자
   o <이름>
   origin x y z / state <hidden> <locked>
   v x y z
   f <색> <flags> <개수> i0 i1 ... u0 v0 u1 v1 ...
*/

bool scene_save(const Scene *s, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return false;
    fprintf(fp, "tinypoly 2\n");
    for (int i = 0; i < PALETTE_SIZE; i++) {
        Color c = s->palette[i];
        fprintf(fp, "c %d %d %d %d\n", c.r, c.g, c.b, s->ramp[i]);
    }
    fprintf(fp, "bg %d\nalpha %d\ntex\n", s->bg, s->alpha);
    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) fputc("0123456789abcdef"[s->tex[y * TEX_W + x] & 15], fp);
        fputc('\n', fp);
    }
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
            fprintf(fp, "f %d %d %d", f->color, f->flags, f->n);
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

bool scene_load(Scene *out, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    static Scene s;          /* 크기가 커서 스택 대신 */
    scene_init(&s);
    Object *o = NULL;
    char line[1024];
    int ci = 0;
    bool ok = fgets(line, sizeof line, fp) && strncmp(line, "tinypoly 2", 10) == 0;

    while (ok && fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        int r, g, b, ramp;
        if (strcmp(line, "tex") == 0) {
            for (int y = 0; y < TEX_H && ok; y++) {
                ok = fgets(line, sizeof line, fp) != NULL;
                for (int x = 0; x < TEX_W && ok; x++) {
                    int v = hexval(line[x]);
                    if (v < 0) ok = false;
                    else s.tex[y * TEX_W + x] = (unsigned char)v;
                }
            }
        } else if (sscanf(line, "c %d %d %d %d", &r, &g, &b, &ramp) == 4 && ci < PALETTE_SIZE) {
            s.palette[ci] = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
            s.ramp[ci++] = (unsigned char)(ramp & 15);
        } else if (sscanf(line, "bg %d", &r) == 1) {
            s.bg = r & 15;
        } else if (sscanf(line, "alpha %d", &r) == 1) {
            s.alpha = r & 15;
        } else if (line[0] == 'o' && line[1] == ' ') {
            o = scene_add(&s, line + 2);
            if (!o) ok = false;
        } else if (o && sscanf(line, "origin %f %f %f", &o->origin.x, &o->origin.y, &o->origin.z) == 3) {
            /* 읽기만 */
        } else if (o && sscanf(line, "state %d %d", &r, &g) == 2) {
            o->hidden = r != 0;
            o->locked = g != 0;
        } else if (line[0] == 'v' && o) {
            Vector3 p;
            if (sscanf(line + 1, "%f %f %f", &p.x, &p.y, &p.z) == 3) obj_add_vert(o, p);
            else ok = false;
        } else if (line[0] == 'f' && o) {
            int color, flags, n, used, idx[MAX_FACE_VERTS];
            char *c = line + 1;
            if (sscanf(c, "%d %d %d%n", &color, &flags, &n, &used) != 3 || n < 3 || n > MAX_FACE_VERTS) {
                ok = false;
                break;
            }
            c += used;
            for (int j = 0; j < n && ok; j++) {
                if (sscanf(c, "%d%n", &idx[j], &used) != 1 || idx[j] < 0 || idx[j] >= o->nverts) ok = false;
                c += used;
            }
            if (!ok) break;
            int fi = obj_add_face(o, idx, n, color & 15);   /* 배열이 옮겨질 수 있어 먼저 호출 */
            Face *f = &o->faces[fi];
            f->flags = flags;
            for (int j = 0; j < n && ok; j++) {
                if (sscanf(c, "%f %f%n", &f->uv[j].x, &f->uv[j].y, &used) != 2) ok = false;
                c += used;
            }
        }
    }
    fclose(fp);

    if (!ok) { scene_clear(&s); return false; }
    scene_clear(out);
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

static const char *base_name(const char *path) {
    const char *a = strrchr(path, '/'), *b = strrchr(path, '\\');
    const char *p = a > b ? a : b;
    return p ? p + 1 : path;
}

static bool write_atlas_png(const Scene *s, const char *path) {
    Image img = tex_atlas_image(s);
    bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}

/* 면 꼭짓점의 아틀라스 픽셀 좌표 (단색 면은 팔레트 띠) */
static Vector2 face_uv(const Face *f, int k) {
    return (f->flags & FACE_NOTEX) ? tex_atlas_color_uv(f->color) : f->uv[k];
}

/* ---- OBJ ---- */

bool export_obj(const Scene *s, const char *path) {
    char mtl_path[1024], png_path[1024];
    path_with_ext(mtl_path, sizeof mtl_path, path, ".mtl");
    path_with_ext(png_path, sizeof png_path, path, ".png");
    if (!write_atlas_png(s, png_path)) return false;

    FILE *fp = fopen(mtl_path, "w");
    if (!fp) return false;
    fprintf(fp, "newmtl texture\nKa 0 0 0\nKd 1 1 1\nKs 0 0 0\nillum 1\nmap_Kd %s\nmap_d %s\n",
            base_name(png_path), base_name(png_path));
    fclose(fp);

    fp = fopen(path, "w");
    if (!fp) return false;
    fprintf(fp, "# TinyPoly\nmtllib %s\nusemtl texture\n", base_name(mtl_path));

    int voff = 1, toff = 1, noff = 1;
    for (int i = 0; i < s->count; i++) {
        const Object *o = &s->objs[i];
        if (o->hidden) continue;
        fprintf(fp, "o %s\n", o->name);
        for (int k = 0; k < o->nverts; k++) {
            Vector3 p = o->verts[k].p;
            fprintf(fp, "v %g %g %g\n", p.x, p.y, p.z);
        }
        int nt = 0;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            Vector3 n = obj_face_normal(o, f);
            fprintf(fp, "vn %g %g %g\n", n.x, n.y, n.z);
            for (int j = 0; j < f->n; j++, nt++) {
                Vector2 uv = face_uv(f, j);
                fprintf(fp, "vt %g %g\n", uv.x / TEX_W, 1.0f - uv.y / ATLAS_H);
            }
        }
        int t = toff;
        for (int k = 0; k < o->nfaces; k++) {
            const Face *f = &o->faces[k];
            fputc('f', fp);
            for (int j = 0; j < f->n; j++) fprintf(fp, " %d/%d/%d", f->v[j] + voff, t++, k + noff);
            fputc('\n', fp);
        }
        voff += o->nverts;
        toff += nt;
        noff += o->nfaces;
    }
    return fclose(fp) == 0;
}

/* ---- glTF ---- */

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

bool export_gltf(const Scene *s, const char *path, bool binary) {
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
                        Vector2 uv = face_uv(f, c[m]);
                        pos[n] = o->verts[f->v[c[m]]].p;
                        nrm[n] = fn;
                        tc[n] = (Vector2){ uv.x / TEX_W, uv.y / ATLAS_H };
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
            int at = add_accessor(&g, (const float *)tc, n, 2, false);
            comma(&meshes, nprim++);
            buf_printf(&meshes, "{\"attributes\":{\"POSITION\":%d,\"NORMAL\":%d,\"TEXCOORD_0\":%d},\"material\":%d,\"mode\":4}",
                       ap, an, at, mat_of_key[key]);
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

    char bin_path[1024], png_path[1024];
    path_with_ext(bin_path, sizeof bin_path, path, ".bin");
    path_with_ext(png_path, sizeof png_path, path, ".png");

    bool ok = nmesh > 0;
    int image_view = -1;
    if (ok && binary) {
        Image img = tex_atlas_image(s);
        int size = 0;
        unsigned char *png = ExportImageToMemory(img, ".png", &size);
        UnloadImage(img);
        ok = png != NULL;
        if (ok) image_view = add_view(&g, png, (size_t)size, false);
        MemFree(png);
    } else if (ok) {
        ok = write_atlas_png(s, png_path);
    }

    if (ok) {
        buf_align4(&g.bin, 0);
        Buf js = { 0 };
        buf_str(&js, "{\"asset\":{\"version\":\"2.0\",\"generator\":\"TinyPoly\"},");
        if (unlit) buf_str(&js, "\"extensionsUsed\":[\"KHR_materials_unlit\"],");
        buf_str(&js, "\"scene\":0,\"scenes\":[{\"nodes\":[");
        for (int i = 0; i < nmesh; i++) buf_printf(&js, i ? ",%d" : "%d", i);
        buf_str(&js, "]}],\"nodes\":[");        buf_put(&js, nodes.d, nodes.n);
        buf_str(&js, "],\"meshes\":[");         buf_put(&js, meshes.d, meshes.n);
        buf_str(&js, "],\"materials\":[");      buf_put(&js, mats.d, mats.n);
        buf_str(&js, "],\"textures\":[{\"sampler\":0,\"source\":0}]");
        buf_str(&js, ",\"samplers\":[{\"magFilter\":9728,\"minFilter\":9728}]");
        if (binary) {
            buf_printf(&js, ",\"images\":[{\"bufferView\":%d,\"mimeType\":\"image/png\"}]", image_view);
        } else {
            buf_str(&js, ",\"images\":[{\"uri\":");
            buf_json_str(&js, base_name(png_path));
            buf_str(&js, "}]");
        }
        buf_str(&js, ",\"accessors\":[");       buf_put(&js, g.accs.d, g.accs.n);
        buf_str(&js, "],\"bufferViews\":[");    buf_put(&js, g.views.d, g.views.n);
        buf_printf(&js, "],\"buffers\":[{\"byteLength\":%zu", g.bin.n);
        if (!binary) {
            buf_str(&js, ",\"uri\":");
            buf_json_str(&js, base_name(bin_path));
        }
        buf_str(&js, "}]}");

        FILE *fp = fopen(path, "wb");
        ok = fp != NULL;
        if (ok && !binary) {
            ok = fwrite(js.d, 1, js.n, fp) == js.n;
            FILE *bf = fopen(bin_path, "wb");
            ok = ok && bf && fwrite(g.bin.d, 1, g.bin.n, bf) == g.bin.n;
            if (bf && fclose(bf) != 0) ok = false;
        } else if (ok) {
            /* GLB: 헤더 + JSON 청크(공백 패딩) + BIN 청크, 4바이트 정렬 */
            buf_align4(&js, ' ');
            uint32_t total = 12 + 8 + (uint32_t)js.n + 8 + (uint32_t)g.bin.n;
            uint32_t hdr[5] = { 0x46546C67u, 2, total, (uint32_t)js.n, 0x4E4F534Au };
            uint32_t binhdr[2] = { (uint32_t)g.bin.n, 0x004E4942u };
            ok = fwrite(hdr, 4, 5, fp) == 5
              && fwrite(js.d, 1, js.n, fp) == js.n
              && fwrite(binhdr, 4, 2, fp) == 2
              && fwrite(g.bin.d, 1, g.bin.n, fp) == g.bin.n;
        }
        if (fp && fclose(fp) != 0) ok = false;
        free(js.d);
    }
    free(g.bin.d); free(g.views.d); free(g.accs.d);
    free(nodes.d); free(meshes.d); free(mats.d);
    return ok;
}
