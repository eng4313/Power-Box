/**
  ******************************************************************************
  * @file    rtc.h
  * @brief   Thin wrapper around the CubeMX-generated `hrtc` (RTC.Instance,
  *          clocked from LSE, 24h format -- see MX_RTC_Init() in main.c,
  *          which this module does NOT duplicate, only builds on).
  *
  *          Provides:
  *            - Cold-start detection via an RTC backup register, so the
  *              calendar is only force-set to a default once (first ever
  *              power-up / VBAT never connected), and left alone on every
  *              normal boot afterward (VBAT keeps it running & accurate).
  *            - Unix timestamp <-> calendar conversion, implemented from
  *              scratch (no <time.h>, per project constraints) using the
  *              standard days-from-civil / civil-from-days algorithm.
  *            - Gregorian -> Jalali (Shamsi) date conversion, for the
  *              default-screen date display required by the algorithm doc.
  ******************************************************************************
  */

#ifndef __RTC_H
#define __RTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "typedef.h"

/* Magic value written to a backup register once the calendar has been set
 * at least once. Its presence on boot means VBAT has kept the RTC domain
 * alive since then, so the current calendar value can be trusted. */
#define RTC_COLD_START_MAGIC        0xA5A5U

typedef struct
{
    uint16_t year;    /* full year, e.g. 2026 */
    uint8_t  month;   /* 1-12 */
    uint8_t  day;     /* 1-31 */
    uint8_t  hour;    /* 0-23 */
    uint8_t  minute;  /* 0-59 */
    uint8_t  second;  /* 0-59 */
} RTC_DateTimeTypeDef;

typedef struct
{
    uint16_t year;
    uint8_t  month;   /* 1-12 (Farvardin=1 .. Esfand=12) */
    uint8_t  day;     /* 1-31 */
} RTC_ShamsiDateTypeDef;

/**
  * @brief  Checks the cold-start backup register. If this is the first boot
  *         ever (magic value absent), sets the calendar to a default
  *         date/time (2026-01-01 00:00:00) and writes the magic value so
  *         future boots skip this. If VBAT has been keeping the RTC alive
  *         since a previous boot, does nothing to the calendar (it's
  *         already correct).
  * @retval SYS_OK always (default-set path cannot practically fail with
  *         hardcoded valid values; HAL errors here would indicate an RTC
  *         wiring/clock problem needing hardware attention, not something
  *         to retry).
  */
System_StatusTypeDef RTC_Init(void);

/**
  * @brief  Reads the current calendar into a clean, HAL-independent struct.
  */
System_StatusTypeDef RTC_GetDateTime(RTC_DateTimeTypeDef *datetime);

/**
  * @brief  Writes the calendar (e.g. from the admin "set date/time" menu).
  *         Does NOT touch the cold-start magic register -- once set, a
  *         manual time change should not re-arm the cold-start default.
  */
System_StatusTypeDef RTC_SetDateTime(const RTC_DateTimeTypeDef *datetime);

/**
  * @brief  Current calendar as a Unix timestamp (seconds since 1970-01-01
  *         00:00:00 UTC -- no timezone/DST handling, matches
  *         LockerRecordTypeDef.deposit_unix_time / LogEntryTypeDef.unix_time).
  */
uint32_t RTC_GetUnixTime(void);

/**
  * @brief  Sets the calendar from a Unix timestamp.
  */
System_StatusTypeDef RTC_SetUnixTime(uint32_t unix_time);

/**
  * @brief  Converts a Gregorian date to its Jalali (Shamsi) equivalent, for
  *         the default-screen date display. Time-of-day is unaffected by
  *         calendar system, so hour/minute/second from RTC_GetDateTime()
  *         can be shown alongside this unchanged.
  */
void RTC_GregorianToShamsi(uint16_t g_year, uint8_t g_month, uint8_t g_day, RTC_ShamsiDateTypeDef *shamsi_out);

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H */
