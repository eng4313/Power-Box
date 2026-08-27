/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   See rtc.h.
  ******************************************************************************
  */

#include "rtc.h"

extern RTC_HandleTypeDef hrtc; /* CubeMX-generated, see MX_RTC_Init() in main.c */

/* ==========================================================================
 *  Unix timestamp <-> calendar (no <time.h>, per project constraints)
 *
 *  Standard "days from civil" / "civil from days" algorithm (Howard
 *  Hinnant, public domain), proleptic Gregorian, valid across the entire
 *  range this system will ever see. No leap seconds, no timezone/DST --
 *  matches how deposit_unix_time / LogEntryTypeDef.unix_time are already
 *  used elsewhere (naive UTC-like seconds counter).
 * ========================================================================== */

static int64_t RTC_DaysFromCivil(int32_t y, uint32_t m, uint32_t d)
{
    y -= (m <= 2) ? 1 : 0;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    uint32_t yoe = (uint32_t)(y - era * 400);                                   /* 0..399 */
    uint32_t doy = (153U * (m + ((m > 2U) ? (uint32_t)-3 : 9U)) + 2U) / 5U + d - 1U; /* 0..365 */
    uint32_t doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;                     /* 0..146096 */

    return era * 146097 + (int64_t)doe - 719468; /* days since 1970-01-01 */
}

static void RTC_CivilFromDays(int64_t z, int32_t *y, uint32_t *m, uint32_t *d)
{
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);                                /* 0..146096 */
    uint32_t yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;    /* 0..399 */
    int64_t  yr  = (int64_t)yoe + era * 400;
    uint32_t doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);                   /* 0..365 */
    uint32_t mp  = (5U * doy + 2U) / 153U;                                      /* 0..11 */
    uint32_t day = doy - (153U * mp + 2U) / 5U + 1U;                            /* 1..31 */
    uint32_t mon = mp + ((mp < 10U) ? 3U : (uint32_t)-9);                       /* 1..12 */

    yr += (mon <= 2U) ? 1 : 0;

    *y = (int32_t)yr;
    *m = mon;
    *d = day;
}

uint32_t RTC_GetUnixTime(void)
{
    RTC_DateTimeTypeDef dt;

    if (RTC_GetDateTime(&dt) != SYS_OK)
    {
        return 0U;
    }

    int64_t days = RTC_DaysFromCivil((int32_t)dt.year, dt.month, dt.day);
    int64_t seconds = days * 86400 + (int64_t)dt.hour * 3600 + (int64_t)dt.minute * 60 + dt.second;

    return (seconds > 0) ? (uint32_t)seconds : 0U;
}

System_StatusTypeDef RTC_SetUnixTime(uint32_t unix_time)
{
    RTC_DateTimeTypeDef dt;
    int64_t days = (int64_t)(unix_time / 86400U);
    uint32_t rem = unix_time % 86400U;
    int32_t  y;
    uint32_t m, d;

    RTC_CivilFromDays(days, &y, &m, &d);

    dt.year   = (uint16_t)y;
    dt.month  = (uint8_t)m;
    dt.day    = (uint8_t)d;
    dt.hour   = (uint8_t)(rem / 3600U);
    dt.minute = (uint8_t)((rem % 3600U) / 60U);
    dt.second = (uint8_t)(rem % 60U);

    return RTC_SetDateTime(&dt);
}

/* ==========================================================================
 *  Gregorian -> Jalali (Shamsi)
 *
 *  Classic div/mod based conversion (public-domain algorithm widely used
 *  for this exact purpose, e.g. jdf.scr.ir). Correct for the full range of
 *  dates this system will ever display.
 * ========================================================================== */

void RTC_GregorianToShamsi(uint16_t g_year, uint8_t g_month, uint8_t g_day, RTC_ShamsiDateTypeDef *shamsi_out)
{
    static const int32_t g_days_in_month[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int32_t gy = g_year;
    int32_t gy2 = (g_month > 2U) ? (gy + 1) : gy;
    int32_t days;
    int32_t jy;

    if (shamsi_out == NULL)
    {
        return;
    }

    days = 355666 + (365 * gy) + ((gy2 + 3) / 4) - ((gy2 + 99) / 100) + ((gy2 + 399) / 400)
           + g_day + g_days_in_month[g_month - 1U];

    jy = -1595 + (33 * (days / 12053));
    days %= 12053;

    jy += 4 * (days / 1461);
    days %= 1461;

    if (days > 365)
    {
        jy += (days - 1) / 365;
        days = (days - 1) % 365;
    }

    if (days < 186)
    {
        shamsi_out->month = (uint8_t)(1 + (days / 31));
        shamsi_out->day   = (uint8_t)(1 + (days % 31));
    }
    else
    {
        shamsi_out->month = (uint8_t)(7 + (days - 186) / 30);
        shamsi_out->day   = (uint8_t)(1 + (days - 186) % 30);
    }

    shamsi_out->year = (uint16_t)jy;
}

/* ==========================================================================
 *  Public API
 * ========================================================================== */

System_StatusTypeDef RTC_Init(void)
{
    uint32_t cold_start_flag = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);

    if (cold_start_flag != RTC_COLD_START_MAGIC)
    {
        /* First boot ever (or VBAT was never connected / was fully
         * discharged since last boot) -- force a sane default so the UI
         * never shows garbage. TODO: replace with the actual assembly/
         * first-power-on date once known, or let the admin menu's
         * "set date/time" screen overwrite this on first setup. */
        RTC_DateTimeTypeDef default_dt = { 2026U, 1U, 1U, 0U, 0U, 0U };

        RTC_SetDateTime(&default_dt);
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_COLD_START_MAGIC);
    }

    return SYS_OK;
}

System_StatusTypeDef RTC_GetDateTime(RTC_DateTimeTypeDef *datetime)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (datetime == NULL)
    {
        return SYS_INVALID_PARAM;
    }

    /* Per HAL requirement: always read RTC_GetTime() before RTC_GetDate()
     * to correctly unlock the calendar shadow registers. */
    if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return SYS_ERROR;
    }
    if (HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return SYS_ERROR;
    }

    datetime->year   = 2000U + sDate.Year;
    datetime->month  = sDate.Month;
    datetime->day    = sDate.Date;
    datetime->hour   = sTime.Hours;
    datetime->minute = sTime.Minutes;
    datetime->second = sTime.Seconds;

    return SYS_OK;
}

System_StatusTypeDef RTC_SetDateTime(const RTC_DateTimeTypeDef *datetime)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if ((datetime == NULL) || (datetime->year < 2000U) || (datetime->year > 2099U))
    {
        return SYS_INVALID_PARAM;
    }

    sTime.Hours   = datetime->hour;
    sTime.Minutes = datetime->minute;
    sTime.Seconds = datetime->second;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation  = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return SYS_ERROR;
    }

    sDate.Year    = (uint8_t)(datetime->year - 2000U);
    sDate.Month   = datetime->month;
    sDate.Date    = datetime->day;
    sDate.WeekDay = RTC_WEEKDAY_MONDAY; /* TODO: compute real weekday if needed for UI; unused by timestamp math */

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return SYS_ERROR;
    }

    return SYS_OK;
}
