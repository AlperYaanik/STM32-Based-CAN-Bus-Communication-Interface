#include "debug.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>

#if DEBUG_ENABLED
void Debug_Log(const char *fmt, ...)
{
  char buf[96];
  va_list args;

  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (len <= 0)
  {
    return;
  }
  /* On truncation vsnprintf returns the length it WOULD have written, not
     what it did write. Passing that length straight to Transmit would send
     bytes from beyond the buffer. */
  if ((size_t)len >= sizeof(buf))
  {
    len = sizeof(buf) - 1;
  }

  HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, HAL_MAX_DELAY);
}
#endif
