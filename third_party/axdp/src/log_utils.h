#ifndef LOG_UTILS_H_
#define LOG_UTILS_H_

//#define DEBUG_LOG // define this MACRO in CmakeLists.txt to enable log
#ifdef DEBUG_LOG

#ifdef _WIN32
#define __FILENAME__ (strrchr(__FILE__, '\\') + 1)
#include <cstdio>
#define LOGV(format, ...)  fprintf(stderr, "[%s:%d] " format "\r\n",(char *)__FILENAME__, __LINE__, ##__VA_ARGS__);//__FUNCTION__
#elif defined(__ANDROID__)
#define __FILENAME__ (strrchr(__FILE__, '/') + 1)
#include <android/log.h>
#define TAG "JNI-AXDP"
#define LOGV(format, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG,\
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGD(format, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG,\
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGI(format, ...) __android_log_print(ANDROID_LOG_INFO, TAG,\
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGW(format, ...) __android_log_print(ANDROID_LOG_WARN, TAG,\
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, TAG,\
        "[%s][%s][%d]: " format, __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__);
#elif defined(__APPLE__)
#define __FILENAME__ (strrchr(__FILE__, '/') + 1)
#define LOGV(format, ...)   //fprintf(stderr, format, ##__VA_ARGS__)
#define LOGD(format, ...)   //fprintf(stderr, format, ##__VA_ARGS__)
#define LOGI(format, ...)   //fprintf(stderr, format, ##__VA_ARGS__)
#define LOGW(format, ...)   //fprintf(stderr, format, ##__VA_ARGS__)
#define LOGE(format, ...)   //fprintf(stderr, format, ##__VA_ARGS__)
#else//cjb remark: mac, unix like
#define __FILENAME__ (strrchr(__FILE__, '/') + 1)
#define LOGV(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGD(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGI(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGW(format, ...)   printf(format, ##__VA_ARGS__)
#define LOGE(format, ...)   printf(format, ##__VA_ARGS__)
#endif

#else
#define LOGV(format, ...);
#define LOGD(format, ...);
#define LOGI(format, ...);
#define LOGW(format, ...);
#define LOGE(format, ...);
#endif

#endif // !LOG_UTILS_H_
