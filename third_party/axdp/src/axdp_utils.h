#ifndef AXDP_UTILS_H_
#define AXDP_UTILS_H_


#include <stdint.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>


#ifdef USE_FREERTOS_HEAP
#include "FreeRTOS.h"
extern void* pvPortRealloc(void* ptr, size_t n);
extern void* pvPortCalloc(size_t m, size_t n);
#define awi_malloc pvPortMalloc
#define awi_calloc pvPortCalloc
#define awi_realloc pvPortRealloc
#define awi_free vPortFree
#else
#define awi_malloc    malloc
#define awi_calloc    calloc
#define awi_realloc realloc
#define awi_free    free
#endif

#define aux_align(d, a) (((d) + (a - 1)) & ~(a - 1))

extern FILE *awxlog;
//#define DEBUG_LOG_FILE 1
//#define DEBUG_LOG_STDERR 1
#ifdef DEBUG_LOG_FILE
#define aux_print(format,...) auxlog?\
                            fprintf(auxlog, "[%s:%d] " format "\r",(char *)__FUNCTION__, __LINE__, ##__VA_ARGS__):\
                            fprintf(stderr, "[%s:%d] " format "\r\n",(char *)__FUNCTION__, __LINE__, ##__VA_ARGS__);\
                            auxlog?fflush(auxlog):0;
#else
#ifdef DEBUG_LOG_STDERR
#define awx_print(format,...) printf("[%s:%d] " format "\r\n", (char *)__FUNCTION__, __LINE__, ##__VA_ARGS__ )
#else
#define awx_print(format, ...)
#endif // DEBUG_LOG_STDERR


#endif // DEBUG_LOG_FILE


#ifndef awx_min
#define awx_min(a, b) (((a)<(b))?(a):(b))
#endif

#ifndef awx_max
#define awx_max(a, b) (((a)>(b))?(a):(b))
#endif

#if defined(__APPLE__)
#ifdef htonl
#undef htonl
#define htonl awx_htonl
#endif

#ifdef ntohl
#undef ntohl
#define ntohl awx_ntohl
#endif

#ifdef htons
#undef htons
#define htons awx_htons
#endif

#ifdef ntohs
#undef ntohs
#define ntohs awx_ntohs
#endif
#endif

namespace axdp {
    namespace utils {
        uint32_t htonl(uint32_t h);

        uint32_t ntohl(uint32_t n);

        uint16_t htons(uint16_t h);

        uint16_t ntohs(uint16_t n);

        void printfBufferHeader(const uint8_t *buffer);

        void printfBufferPayload(const uint8_t *buffer);
    }

}
#endif // !AXDP_UTILS_H_
