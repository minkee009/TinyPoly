#ifndef IO_H
#define IO_H

#include "core/mesh.h"
#include <stddef.h>

/* 자체 포맷(.tpm, 텍스트): 메쉬 + UV + 텍스처(PNG) + 팔레트. v2 파일도 읽음 */
bool SceneSave(const Scene *s, const char *path);
bool SceneLoad(Scene *s, const char *path);

typedef enum { EXPORT_OBJ, EXPORT_GLTF, EXPORT_GLB, EXPORT_FORMAT_COUNT } ExportFormat;

/* 3D 파일 + 텍스처 PNG를 폴더(out = 폴더 경로) 또는 zip(out = .zip 경로)으로 내보냄.
   파일 이름은 폴더/zip 이름을 따름. 단색 면 색은 텍스처 아래 색 띠로 들어감 */
bool ExportScene(const Scene *s, ExportFormat fmt, bool zip, const char *out);

/* 결과물에 들어갈 파일 이름 (UI 미리보기용). 개수 반환 */
int ExportFileNames(ExportFormat fmt, const char *base, char names[][96], int max);

/* path의 확장자를 ext(".obj" 등)로 바꿔 dst에 씀 */
void PathWithExt(char *dst, size_t size, const char *path, const char *ext);

#endif
