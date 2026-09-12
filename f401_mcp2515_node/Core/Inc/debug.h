#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Set to 0 for release builds: every LOG(...) call compiles away to nothing,
   so neither the UART traffic nor the vsnprintf cost ends up in the image.
   Same interface as f103_node_sensor/Core/Inc/debug.h. */
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
void Debug_Log(const char *fmt, ...);
#define LOG(...) Debug_Log(__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_H__ */
