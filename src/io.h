#ifndef IO_H
#define IO_H

#include "mesh.h"
#include <stddef.h>

/* 자체 포맷(.tpm, 텍스트): 메쉬 + UV + 텍스처 + 팔레트 */
bool scene_save(const Scene *s, const char *path);
bool scene_load(Scene *s, const char *path);

/* 내보내기. 텍스처는 팔레트 띠를 붙인 PNG로 함께 나감 (단색 면은 그 띠를 가리킴) */
bool export_obj(const Scene *s, const char *path);               /* .obj + .mtl + .png */
bool export_gltf(const Scene *s, const char *path, bool binary);  /* .gltf + .bin + .png / .glb */

/* path의 확장자를 ext(".obj" 등)로 바꿔 dst에 씀 */
void path_with_ext(char *dst, size_t size, const char *path, const char *ext);

#endif
