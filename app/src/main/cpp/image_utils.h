#ifndef _RKNN_MODEL_ZOO_IMAGE_UTILS_H_
#define _RKNN_MODEL_ZOO_IMAGE_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

/**
 * @brief LetterBox
 * 
 */
typedef struct {
    int x_pad;
    int y_pad;
    float scale;
} letterbox_t;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // _RKNN_MODEL_ZOO_IMAGE_UTILS_H_