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

rknn_app_context_t rknn_app_ctx; //模型推理需要用的全局环境,包含了推理需要的各种数据
uint8_t *background = nullptr;   //存放图像转换的结果
image_buffer_t dst_img;          //需要传入给推理接口的对象
letterbox_t letter_box;          //letterbox操作的参数
object_detect_result_list od_results; //推理的结果

// 全局变量：存储类引用和方法ID（生命周期与JNI一致）
static jclass g_imageRectCls = nullptr;
static jmethodID g_imageRectCtor = nullptr;
static jclass g_detectResultCls = nullptr;
static jmethodID g_detectResultCtor = nullptr;
static jclass g_resultListCls = nullptr;
static jmethodID g_resultListCtor = nullptr;

uint8_t* createGrayImage(int width = 640, int height = 640, uint8_t grayValue = 114) {
    // 计算每行字节数（RGB格式，每个像素3字节）
    int bytesPerRow = width * 3;

    // 计算总字节数
    int totalBytes = bytesPerRow * height;

    // 分配内存
    auto* imageData = new uint8_t[totalBytes];

    // 填充灰色像素（RGB三个通道值相同）
    memset(imageData, grayValue, totalBytes);

    return imageData;
}

// 初始化全局引用
static bool initRefs(JNIEnv *env) {
    // 初始化ImageRect相关
    jclass localRectCls = env->FindClass(IMAGE_RECT_CLASS);
    if (localRectCls == nullptr) {
        env->ExceptionDescribe();
        return false;
    }
    g_imageRectCls = (jclass)env->NewGlobalRef(localRectCls);
    g_imageRectCtor = env->GetMethodID(g_imageRectCls, "<init>", "(IIII)V");
    if (g_imageRectCtor == nullptr) {
        env->ExceptionDescribe();
        return false;
    }

    // 初始化DetectResult相关
    jclass localResultCls = env->FindClass(DETECT_RESULT_CLASS);
    if (localResultCls == nullptr) {
        env->ExceptionDescribe();
        return false;
    }
    g_detectResultCls = (jclass)env->NewGlobalRef(localResultCls);
    g_detectResultCtor = env->GetMethodID(g_detectResultCls, "<init>",
                                          "(Lcom/example/cameraxtest/ImageRect;FI)V");
    if (g_detectResultCtor == nullptr) {
        env->ExceptionDescribe();
        return false;
    }

    // 初始化DetectResultList相关
    jclass localListCls = env->FindClass(DETECT_RESULT_LIST_CLASS);
    if (localListCls == nullptr) {
        env->ExceptionDescribe();
        return false;
    }
    g_resultListCls = (jclass)env->NewGlobalRef(localListCls);
    g_resultListCtor = env->GetMethodID(g_resultListCls, "<init>",
                                        "(II[Lcom/example/cameraxtest/DetectResult;)V");
    if (g_resultListCtor == nullptr) {
        env->ExceptionDescribe();
        return false;
    }

    return true;
}
// 释放全局引用
static void releaseRefs(JNIEnv *env) {
    if (g_imageRectCls != nullptr) {
        env->DeleteGlobalRef(g_imageRectCls);
        g_imageRectCls = nullptr;
    }
    if (g_detectResultCls != nullptr) {
        env->DeleteGlobalRef(g_detectResultCls);
        g_detectResultCls = nullptr;
    }
    if (g_resultListCls != nullptr) {
        env->DeleteGlobalRef(g_resultListCls);
        g_resultListCls = nullptr;
    }
    // 方法ID不需要手动释放（随类引用生命周期）
}

// JNI方法：将C的object_detect_result_list转换为Java的DetectResultList对象
jobject convertResultList(JNIEnv *env, object_detect_result_list *cList)
{
    if (cList == nullptr) return nullptr;
    // 检查全局引用是否有效（防止意外释放）
    if (g_imageRectCls == nullptr || g_detectResultCls == nullptr || g_resultListCls == nullptr)
    {
        return nullptr;
    }

    // 创建DetectResult数组
    jobjectArray resultArray = env->NewObjectArray(cList->count, g_detectResultCls, nullptr);
    if (resultArray == nullptr)
    {
        env->ExceptionDescribe();
        return nullptr;
    }

    // 遍历转换每个检测结果
    for (int i = 0; i < cList->count; i++)
    {
        object_detect_result *cResult = &(cList->results[i]);

        // 创建ImageRect对象（复用全局引用）
        jobject jRect = env->NewObject(g_imageRectCls, g_imageRectCtor,
                                       cResult->box.left,
                                       cResult->box.top,
                                       cResult->box.right,
                                       cResult->box.bottom);
        if (jRect == nullptr)
        {
            env->ExceptionDescribe();
            break;
        }

        // 创建DetectResult对象（复用全局引用）
        jobject jResult = env->NewObject(g_detectResultCls, g_detectResultCtor,
                                         jRect,
                                         cResult->prop,
                                         cResult->cls_id);
        env->DeleteLocalRef(jRect); // 释放局部引用
        if (jResult == nullptr) {
            env->ExceptionDescribe();
            break;
        }

        // 添加到数组
        env->SetObjectArrayElement(resultArray, i, jResult);
        env->DeleteLocalRef(jResult);
    }

    // 创建最终结果列表对象
    jobject jList = env->NewObject(g_resultListCls, g_resultListCtor,
                                   cList->id,
                                   cList->count,
                                   resultArray);
    env->DeleteLocalRef(resultArray); // 释放数组引用

    return jList;
}

extern "C" JNIEXPORT jobject JNICALL
        Java_com_example_cameraxtest_MainActivity_imgInference(JNIEnv* env,
                                 jobject, /* this */
                                 jobject img,
                                 jint width,
                                 jint height)
{
    int ret;
    //图像预处理
    auto *imgData = static_cast<uint8_t*>(env->GetDirectBufferAddress(img));
    if (imgData == nullptr)
    {
        return env->NewStringUTF("Error: Failed to get input buffer address");
    }
    //输入: 1280*1024  yuv420sp(nv12)
    const rga_buffer_t src = wrapbuffer_virtualaddr(imgData, width, height, RK_FORMAT_YCbCr_420_SP);

    if(background == nullptr)
    {
        background = createGrayImage(640,640,114); //生成RGB格式图片, 纯灰色的背景图
        dst_img.virt_addr = background;
    }
    rga_buffer_t dst1 = wrapbuffer_virtualaddr(background+3*640*64, 640, 512, RK_FORMAT_RGB_888);

    IM_STATUS blendStatus = imblend(src, dst1, IM_ALPHA_BLEND_SRC_OVER);
    if(blendStatus != IM_STATUS_SUCCESS)
    {
        LOGI("错误码%d", blendStatus);
        return nullptr;
    }

    ret = inference_yolov8_model(&rknn_app_ctx, &dst_img, letter_box, &od_results);
    if(ret != 0)
    {
        LOGI("inference_yolov8_model fail! ret=%d\n", ret);
    }

    return convertResultList(env, &od_results);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_example_cameraxtest_MainActivity_nativePrepare(JNIEnv *env, jobject thiz, jstring modelpath, jstring labelPath)
{
    int ret;

    const char *model_path = env->GetStringUTFChars(modelpath, nullptr);
    const char *label_path = env->GetStringUTFChars(labelPath, nullptr);

    ret = init_post_process(label_path);
    if(ret != 0)
    {
        LOGI("init_post_process fail! ret=%d label_path=%s\n", ret, label_path);
        return false;
    }
    ret = init_yolov8_model(model_path, &rknn_app_ctx);
    if (ret != 0)
    {
        LOGI("init_yolov8_model fail! ret=%d model_path=%s\n", ret, model_path);
        return false;
    }

    //letterbox是必要的
    letter_box.x_pad = 0;
    letter_box.y_pad = 64;
    letter_box.scale = 0.5f;

    //查找类的引用和方法
    initRefs(env);

    return true;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_example_cameraxtest_MainActivity_nativeDestroy(JNIEnv *env, jobject thiz)
{
    int ret;

    deinit_post_process();
    ret = release_yolov8_model(&rknn_app_ctx);
    if (ret != 0)
    {
        LOGI("release_yolov8_model fail! ret=%d\n", ret);
    }
    delete[] background;

    releaseRefs(env);
}

