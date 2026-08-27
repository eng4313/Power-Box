/**
  ******************************************************************************
  * @file    channel_hw.h
  * @brief   Raw per-locker GPIO interface: lock (relay/MOSFET coil), status
  *          LED, and door sensor. Pin table below matches the CubeMX .ioc
  *          user labels LOCK_1..LOCK_8 / LD_1..LD_8 / DOOR_1..DOOR_8 (indices
  *          0..LOCKER_COUNT-1 map to locker 1..8 in that order).
  *
  * This module owns no state machine logic -- it is a thin, testable wrapper
  * around HAL_GPIO_*. All timing/state decisions belong to Channel Manager.
  *
  * TODO(hardware, verify before relying on this in production):
  *   - LOCK_x / LD_x active level assumed active-HIGH (HAL_GPIO_WritePin SET
  *     = lock energized / LED on). Confirm against the relay/MOSFET driver
  *     and LED driver circuits on Channel.SchDoc.
  *   - DOOR_x pins are configured GPIO_Input with no GPIO_PuPd parameter in
  *     the .ioc (i.e. CubeMX left them at its default), so this driver
  *     applies GPIO_PULLUP itself in ChannelHW_Init() and treats a closed
  *     door as pulled LOW (reed switch / microswitch to GND). If the door
  *     sensor circuit turns out to work the other way, flip
  *     CHANNEL_HW_DOOR_CLOSED_LEVEL below -- no other code needs to change.
  ******************************************************************************
  */

#ifndef __CHANNEL_HW_H
#define __CHANNEL_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "typedef.h"

/* ---------------------------------------------------------- Active levels */
#define CHANNEL_HW_LOCK_ENERGIZED_LEVEL     GPIO_PIN_SET     /* TODO: verify */
#define CHANNEL_HW_LED_ON_LEVEL             GPIO_PIN_SET     /* TODO: verify */
#define CHANNEL_HW_DOOR_CLOSED_LEVEL        GPIO_PIN_RESET   /* TODO: verify */

/**
  * @brief  Configures clocks + GPIO mode for every LOCK_x / LD_x / DOOR_x
  *         pin (x = 1..LOCKER_COUNT). Locks and LEDs are driven to their
  *         de-energized/off level immediately (safe power-up state) before
  *         this returns. Call once at boot, before ChannelManager_Init().
  * @retval SYS_OK, or SYS_INVALID_PARAM if LOCKER_COUNT exceeds the size of
  *         the pin table below (i.e. typedef.h was bumped past 8 without
  *         adding the matching rows here).
  */
System_StatusTypeDef ChannelHW_Init(void);

/**
  * @brief  Drives locker_index's lock output.
  * @param  energize  true = energize coil (unlock), false = de-energize (lock).
  */
System_StatusTypeDef ChannelHW_SetLock(uint8_t locker_index, bool energize);

/**
  * @brief  Drives locker_index's status LED output.
  */
System_StatusTypeDef ChannelHW_SetLED(uint8_t locker_index, bool on);

/**
  * @brief  Reads locker_index's door sensor.
  * @retval true if the door is currently closed, false if open OR if
  *         locker_index is out of range (fail-safe: an invalid index is
  *         reported as "open" so callers relying on this for a close-timeout
  *         check never fall through as false-closed).
  */
bool ChannelHW_IsDoorClosed(uint8_t locker_index);

#ifdef __cplusplus
}
#endif

#endif /* __CHANNEL_HW_H */
