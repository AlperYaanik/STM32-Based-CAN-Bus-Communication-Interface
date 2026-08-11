#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Release derlemesinde 0 yapılınca LOG(...) çağrıları hiç derlenmez (no-op),
   UART/vsnprintf maliyeti release'e karışmaz. */
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
