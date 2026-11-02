#ifndef MODEL_H
#define MODEL_H

#include "app.h"

/* 모델 모드: 뷰포트 마우스·키 조작 (편집 명령은 model_ops.h) */
void model_update(App *a, bool input_ok, bool keys);   /* keys: 모델 단축키까지 처리 */
int  model_cursor(App *a);              /* 뷰포트 위 상황에 맞는 커서 */

#endif
