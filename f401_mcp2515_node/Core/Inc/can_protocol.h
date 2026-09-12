#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Shared contract between the two nodes.
 *
 * This file must be IDENTICAL in f103_node_sensor and f401_mcp2515_node.
 * They are separate CubeIDE projects, so the file is physically duplicated,
 * but logically it is a single file: if one copy changes, the other must too.
 *
 * Bus parameters:
 *   Bitrate    : 500 kbit/s  (bxCAN on F103 and MCP2515 configured to match)
 *   Identifier : 11-bit standard
 *   Byte order : big-endian (MSB first), the usual convention on CAN
 *
 * Frame layout (both use DLC = 6):
 *   0x101 ACCEL : [XH XL YH YL ZH ZL]  raw int16, MPU6050 +/-2 g scale
 *   0x102 GYRO  : [XH XL YH YL ZH ZL]  raw int16, MPU6050 +/-250 deg/s scale
 *
 * Raw counts are sent on purpose: converting to engineering units is the
 * receiver's job. That keeps the sender free of float math and the data
 * lossless on the wire.
 */

#define CAN_ID_ACCEL      0x101u
#define CAN_ID_GYRO       0x102u

/* 3 axes x 2 bytes */
#define CAN_AXES_DLC      6u

/* Bit timing on both controllers was computed for this value. */
#define CAN_BITRATE_BPS   500000u

/* 3 x int16 -> 6 bytes, big-endian. */
static inline void CAN_PackAxes(const int16_t axes[3], uint8_t out[6])
{
  for (uint8_t i = 0u; i < 3u; i++)
  {
    out[2u * i]      = (uint8_t)((uint16_t)axes[i] >> 8);
    out[2u * i + 1u] = (uint8_t)((uint16_t)axes[i] & 0xFFu);
  }
}

/* 6 big-endian bytes -> 3 x int16. Exact inverse of CAN_PackAxes. */
static inline void CAN_UnpackAxes(const uint8_t in[6], int16_t axes[3])
{
  for (uint8_t i = 0u; i < 3u; i++)
  {
    axes[i] = (int16_t)(((uint16_t)in[2u * i] << 8) | (uint16_t)in[2u * i + 1u]);
  }
}

#ifdef __cplusplus
}
#endif

#endif /* __CAN_PROTOCOL_H__ */
