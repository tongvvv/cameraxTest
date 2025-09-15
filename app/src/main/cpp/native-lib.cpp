#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/ndk-version.h>
#include <im2d.h>
#include <rknn_api.h>
#include <string.h>
#include <math.h>
#include "yolov8.h"
#include "image_utils.h"
#include "file_utils.h"
#include "native-lib.h"

#define TAG "FROHNDK"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define IMAGE_RECT_CLASS "com/example/cameraxtest/ImageRect"
#define DETECT_RESULT_CLASS "com/example/cameraxtest/DetectResult"
#define DETECT_RESULT_LIST_CLASS "com/example/cameraxtest/DetectResultList"

//NDK Version: 19.2.5345600

static int core_mask = 0;
// 定义线程局部存储的结构体（保持原有全局变量结构）
struct ThreadLocalData {
    rknn_app_context_t rknn_app_ctx;  // 模型推理环境
    uint8_t *background = nullptr;    // 图像转换结果
    image_buffer_t dst_img;           // 推理输入对象
    letterbox_t letter_box;           // letterbox参数
    object_detect_result_list od_results;  // 推理结果

    // JNI引用
    jclass imageRectCls = nullptr;
    jmethodID imageRectCtor = nullptr;
    jclass detectResultCls = nullptr;
    jmethodID detectResultCtor = nullptr;
    jclass resultListCls = nullptr;
    jmethodID resultListCtor = nullptr;
};

// ThreadLocal存储：为每个线程分配独立的实例
static thread_local ThreadLocalData* tlsData = nullptr;

uint8_t* createGrayImage(int width = 640, int height = 640, uint8_t grayValue = 114) {
    int bytesPerRow = width * 3;
    int totalBytes = bytesPerRow * height;
    auto* imageData = new uint8_t[totalBytes];
    memset(imageData, grayValue, totalBytes);
    return imageData;
}

// 初始化当前线程的局部引用
static bool initThreadRefs(JNIEnv *env, ThreadLocalData* data) {
    // 初始化ImageRect相关
    jclass localRectCls = env->FindClass(IMAGE_RECT_CLASS);
    if (localRectCls == nullptr) {
        env->ExceptionDescribe();
        return false;
    }
    data->imageRectCls = (jclass)env->NewGlobalRef(localRectCls);
    data->imageRectCtor = env->GetMethodID(data->imageRectCls, "<init>", "(IIII)V");
    if (data->imageRectCtor == nullptr) {
        env->ExceptionDescribe();
        return false;
    }

    // 初始化DetectResult相关
    jclass localResultCls = env->FindClass(DETECT_RESULT_CLASS);
    if (localResultCls == nullptr) {
        env->ExceptionDescribe();
        return false;
    }
    data->detectResultCls = (jclass)env->NewGlobalRef(localResultCls);
    data->detectResultCtor = env->GetMethodID(data->detectResultCls, "<init>",
                                              "(Lcom/example/cameraxtest/ImageRect;FI)V");
    if (data->detectResultCtor == nullptr) {
        env->ExceptionDescribe();
        return false;
    }

    // 初始化DetectResultList相关
    jclass localListCls = env->FindClass(DETECT_RESULT_LIST_CLASS);
    if (localListCls == nullptr) {
        env->ExceptionDescribe();
        return false;
    }
    data->resultListCls = (jclass)env->NewGlobalRef(localListCls);
    data->resultListCtor = env->GetMethodID(data->resultListCls, "<init>",
                                            "(II[Lcom/example/cameraxtest/DetectResult;)V");
    if (data->resultListCtor == nullptr) {
        env->ExceptionDescribe();
        return false;
    }

    return true;
}

// 释放当前线程的局部引用
static void releaseThreadRefs(JNIEnv *env, ThreadLocalData* data) {
    if (data->imageRectCls != nullptr) {
        env->DeleteGlobalRef(data->imageRectCls);
        data->imageRectCls = nullptr;
    }
    if (data->detectResultCls != nullptr) {
        env->DeleteGlobalRef(data->detectResultCls);
        data->detectResultCls = nullptr;
    }
    if (data->resultListCls != nullptr) {
        env->DeleteGlobalRef(data->resultListCls);
        data->resultListCls = nullptr;
    }
}

// 转换推理结果（使用线程局部数据）
jobject convertResultList(JNIEnv *env, ThreadLocalData* data, object_detect_result_list *cList)
{
    if (cList == nullptr || data == nullptr) return nullptr;
    if (data->imageRectCls == nullptr || data->detectResultCls == nullptr || data->resultListCls == nullptr)
    {
        return nullptr;
    }

    jobjectArray resultArray = env->NewObjectArray(cList->count, data->detectResultCls, nullptr);
    if (resultArray == nullptr)
    {
        env->ExceptionDescribe();
        return nullptr;
    }

    for (int i = 0; i < cList->count; i++)
    {
        object_detect_result *cResult = &(cList->results[i]);
        jobject jRect = env->NewObject(data->imageRectCls, data->imageRectCtor,
                                       cResult->box.left, cResult->box.top,
                                       cResult->box.right, cResult->box.bottom);
        if (jRect == nullptr)
        {
            env->ExceptionDescribe();
            break;
        }

        jobject jResult = env->NewObject(data->detectResultCls, data->detectResultCtor,
                                         jRect, cResult->prop, cResult->cls_id);
        env->DeleteLocalRef(jRect);
        if (jResult == nullptr) {
            env->ExceptionDescribe();
            break;
        }

        env->SetObjectArrayElement(resultArray, i, jResult);
        env->DeleteLocalRef(jResult);
    }

    jobject jList = env->NewObject(data->resultListCls, data->resultListCtor,
                                   cList->id, cList->count, resultArray);
    env->DeleteLocalRef(resultArray);
    return jList;
}

// 推理方法：使用当前线程的局部数据
extern "C" JNIEXPORT jobject JNICALL
Java_com_example_cameraxtest_MainActivity_imgInference(JNIEnv* env,
                                                       jclass clazz, /* this */
                                                       jobject img,
                                                       jint width,
                                                       jint height)
{
    // 检查当前线程的局部数据是否初始化
    if (tlsData == nullptr) {
        LOGI("ThreadLocal data not initialized! Call nativePrepare first.");
        return nullptr;
    }
    ThreadLocalData* data = tlsData;
    int ret;

    // 图像预处理（使用线程局部的letter_box）
    float  scale = (float)(data->rknn_app_ctx.model_width) / width;
    int    height_fixed = scale * height;
    int    padsize = (data->rknn_app_ctx.model_height - height_fixed);
    data->letter_box.x_pad = 0;
    data->letter_box.y_pad = padsize/2;
    data->letter_box.scale = scale;

    auto *imgData = static_cast<uint8_t*>(env->GetDirectBufferAddress(img));
    if (imgData == nullptr)
    {
        return env->NewStringUTF("Error: Failed to get input buffer address");
    }

    const rga_buffer_t src = wrapbuffer_virtualaddr(imgData, width, height, RK_FORMAT_YCbCr_420_SP);

    if(data->background == nullptr)
    {
        data->background = createGrayImage(data->rknn_app_ctx.model_width,
                                           data->rknn_app_ctx.model_height, 114);
        data->dst_img.virt_addr = data->background;
    }
    rga_buffer_t dst1 = wrapbuffer_virtualaddr(data->background + 3*640*data->letter_box.y_pad,
                                               data->rknn_app_ctx.model_width,
                                               height_fixed, RK_FORMAT_RGB_888);

    IM_STATUS blendStatus = imblend(src, dst1, IM_ALPHA_BLEND_SRC_OVER);
    if(blendStatus != IM_STATUS_SUCCESS)
    {
        LOGI("错误码%d", blendStatus);
        return nullptr;
    }

    // 模型推理（使用线程局部的上下文）
    ret = inference_yolov8_model(&data->rknn_app_ctx, &data->dst_img,
                                 data->letter_box, &data->od_results);
    if(ret != 0)
    {
        LOGI("inference_yolov8_model fail! ret=%d\n", ret);
    }

    return convertResultList(env, data, &data->od_results);
}

// 初始化方法：为当前线程创建局部数据
extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_cameraxtest_MainActivity_nativePrepare(JNIEnv *env, jclass thiz,
                                                        jstring modelpath, jstring labelPath)
{
    // 如果当前线程已有数据，先释放
    if (tlsData != nullptr)
    {
        releaseThreadRefs(env, tlsData);
        delete[] tlsData->background;
        release_yolov8_model(&tlsData->rknn_app_ctx);
        delete tlsData;
    }

    // 为当前线程创建新的局部数据
    tlsData = new ThreadLocalData();
    int ret;

    const char *model_path = env->GetStringUTFChars(modelpath, nullptr);
    const char *label_path = env->GetStringUTFChars(labelPath, nullptr);

    ret = init_post_process(label_path);
    if (ret != 0)
    {
        LOGI("init_post_process fail! ret=%d label_path=%s\n", ret, label_path);
        return false;
    }
    ret = init_yolov8_model(model_path, &tlsData->rknn_app_ctx);
    if (ret != 0)
    {
        LOGI("init_yolov8_model fail! ret=%d model_path=%s\n", ret, model_path);
        return false;
    }

    if (core_mask == 0)
    {
        rknn_set_core_mask(tlsData->rknn_app_ctx.rknn_ctx, RKNN_NPU_CORE_0);
        core_mask++;
    }
    else if (core_mask == 1)
    {
        rknn_set_core_mask(tlsData->rknn_app_ctx.rknn_ctx, RKNN_NPU_CORE_1);
        core_mask++;
    }
    else if (core_mask == 2)
    {
        rknn_set_core_mask(tlsData->rknn_app_ctx.rknn_ctx, RKNN_NPU_CORE_2);
        core_mask++;
    }
    else if(core_mask > 2)
    {
        rknn_set_core_mask(tlsData->rknn_app_ctx.rknn_ctx, RKNN_NPU_CORE_AUTO);
    }

    LOGI("%d", core_mask);

    // 初始化当前线程的引用
    if (!initThreadRefs(env, tlsData)) {
        LOGI("initThreadRefs failed");
        return false;
    }

    env->ReleaseStringUTFChars(modelpath, model_path);
    env->ReleaseStringUTFChars(labelPath, label_path);
    return true;
}

// 销毁方法：释放当前线程的局部数据
extern "C"
JNIEXPORT void JNICALL
Java_com_example_cameraxtest_MainActivity_nativeDestroy(JNIEnv *env, jclass thiz)
{
    if (tlsData == nullptr) return;

    // 释放当前线程的资源
    deinit_post_process();
    release_yolov8_model(&tlsData->rknn_app_ctx);
    delete[] tlsData->background;
    releaseThreadRefs(env, tlsData);

    // 清空当前线程的局部数据
    delete tlsData;
    tlsData = nullptr;
}

