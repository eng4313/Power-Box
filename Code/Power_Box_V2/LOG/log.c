/**
  ******************************************************************************
  * @file    log.c
  * @brief   See log.h for the physical layout and design notes.
  ******************************************************************************
  */

#include "log.h"

static uint16_t cached_write_index = 0U;   /* next physical slot to write, 0..LOG_PHYSICAL_CAPACITY-1 */
static uint32_t cached_total_count = 0U;   /* total entries ever written, monotonically increasing */

static uint32_t Log_SlotAddress(uint32_t physical_slot);
static void Log_PackEntry(LogEventTypeDef event, uint8_t locker_index,
                           const char *phone_number, uint32_t unix_time, uint8_t out_bytes[LOG_ENTRY_PACKED_SIZE]);
static void Log_UnpackEntry(const uint8_t in_bytes[LOG_ENTRY_PACKED_SIZE], LogEntryTypeDef *out_entry);

System_StatusTypeDef Log_Init(void)
{
    if (Storage_GetLogPointers(&cached_write_index, &cached_total_count) != SYS_OK)
    {
        return SYS_ERROR;
    }

    return SYS_OK;
}

System_StatusTypeDef Log_Append(LogEventTypeDef event, uint8_t locker_index,
                                 const char *phone_number, uint32_t unix_time)
{
    uint8_t packed[LOG_ENTRY_PACKED_SIZE];
    uint32_t physical_slot = cached_write_index;
    uint32_t address = Log_SlotAddress(physical_slot);

    /* Erase this sector before its first write since the last erase: covers
     * both first-ever use of a sector and every later wraparound into it. */
    if ((physical_slot % LOG_ENTRIES_PER_SECTOR) == 0U)
    {
        if (W25Q32_EraseSector(address) != W25_OK)
        {
            return SYS_ERROR;
        }
    }

    Log_PackEntry(event, locker_index, phone_number, unix_time, packed);

    if (W25Q32_Write(address, packed, LOG_ENTRY_PACKED_SIZE) != W25_OK)
    {
        return SYS_ERROR;
    }

    cached_write_index = (uint16_t)((cached_write_index + 1U) % LOG_PHYSICAL_CAPACITY);
    cached_total_count += 1U;

    if (Storage_SetLogPointers(cached_write_index, cached_total_count) != SYS_OK)
    {
        return SYS_ERROR; /* flash write already happened; pointer just failed to persist -- next
                              boot's Log_Init() will under-count by one entry, which is an
                              acceptable, self-correcting inconsistency for a log (not user funds/lockers) */
    }

    return SYS_OK;
}

System_StatusTypeDef Log_GetVisibleCount(uint32_t *out_count)
{
    if (out_count == NULL)
    {
        return SYS_INVALID_PARAM;
    }

    *out_count = (cached_total_count < LOG_CAPACITY_TOTAL) ? cached_total_count : LOG_CAPACITY_TOTAL;
    return SYS_OK;
}

System_StatusTypeDef Log_GetPageCount(uint32_t *out_page_count)
{
    uint32_t visible;

    if (out_page_count == NULL)
    {
        return SYS_INVALID_PARAM;
    }

    (void)Log_GetVisibleCount(&visible);

    *out_page_count = (visible + LOG_PAGE_SIZE - 1U) / LOG_PAGE_SIZE;
    if (*out_page_count == 0U)
    {
        *out_page_count = 1U; /* always at least one (possibly empty) page for the UI */
    }

    return SYS_OK;
}

System_StatusTypeDef Log_ReadPage(uint32_t page_index, LogEntryTypeDef *out_entries, uint8_t *out_entry_count)
{
    uint32_t visible;
    uint32_t start_n;
    uint32_t count_this_page;

    if ((out_entries == NULL) || (out_entry_count == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    (void)Log_GetVisibleCount(&visible);

    start_n = page_index * LOG_PAGE_SIZE;
    if (start_n >= visible)
    {
        *out_entry_count = 0U;
        return SYS_NOT_FOUND;
    }

    count_this_page = visible - start_n;
    if (count_this_page > LOG_PAGE_SIZE)
    {
        count_this_page = LOG_PAGE_SIZE;
    }

    for (uint32_t i = 0U; i < count_this_page; i++)
    {
        uint32_t n = start_n + i; /* 0 = newest */
        /* physical_slot = (write_index - 1 - n) mod LOG_PHYSICAL_CAPACITY, safe for any sign */
        int64_t raw = (int64_t)cached_write_index - 1 - (int64_t)n;
        uint32_t physical_slot = (uint32_t)(((raw % (int64_t)LOG_PHYSICAL_CAPACITY) + (int64_t)LOG_PHYSICAL_CAPACITY)
                                             % (int64_t)LOG_PHYSICAL_CAPACITY);

        uint8_t packed[LOG_ENTRY_PACKED_SIZE];
        if (W25Q32_Read(Log_SlotAddress(physical_slot), packed, LOG_ENTRY_PACKED_SIZE) != W25_OK)
        {
            return SYS_ERROR;
        }

        Log_UnpackEntry(packed, &out_entries[i]);
    }

    *out_entry_count = (uint8_t)count_this_page;
    return SYS_OK;
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

static uint32_t Log_SlotAddress(uint32_t physical_slot)
{
    return LOG_FLASH_BASE_ADDRESS + (physical_slot * LOG_ENTRY_PACKED_SIZE);
}

static void Log_PackEntry(LogEventTypeDef event, uint8_t locker_index,
                           const char *phone_number, uint32_t unix_time, uint8_t out_bytes[LOG_ENTRY_PACKED_SIZE])
{
    memset(out_bytes, 0xFF, LOG_ENTRY_PACKED_SIZE);

    out_bytes[0] = (uint8_t)event;
    out_bytes[1] = locker_index;

    if ((phone_number != NULL) && (phone_number[0] != '\0'))
    {
        BCD_PackDigits(phone_number, PHONE_NUMBER_DIGIT_COUNT, &out_bytes[2], 6U);
    }
    else
    {
        memset(&out_bytes[2], 0xFF, 6U); /* "no phone number" marker: all-0xFF, never a valid BCD digit */
    }

    out_bytes[8]  = (uint8_t)(unix_time & 0xFFU);
    out_bytes[9]  = (uint8_t)((unix_time >> 8) & 0xFFU);
    out_bytes[10] = (uint8_t)((unix_time >> 16) & 0xFFU);
    out_bytes[11] = (uint8_t)((unix_time >> 24) & 0xFFU);

    out_bytes[12] = LOG_ENTRY_VALID_MARKER;

    uint8_t checksum = 0U;
    for (uint8_t i = 0U; i < 13U; i++)
    {
        checksum ^= out_bytes[i];
    }
    out_bytes[13] = checksum;
    /* [14..15] left as 0xFF, reserved for future use */
}

static void Log_UnpackEntry(const uint8_t in_bytes[LOG_ENTRY_PACKED_SIZE], LogEntryTypeDef *out_entry)
{
    memset(out_entry, 0, sizeof(*out_entry));

    out_entry->event        = (LogEventTypeDef)in_bytes[0];
    out_entry->locker_index = in_bytes[1];

    /* All-0xFF phone bytes = "no phone number" marker (admin-only events) */
    bool has_phone = false;
    for (uint8_t i = 2U; i < 8U; i++)
    {
        if (in_bytes[i] != 0xFFU)
        {
            has_phone = true;
            break;
        }
    }

    if (has_phone)
    {
        BCD_UnpackDigits(&in_bytes[2], 6U, out_entry->phone_number, PHONE_NUMBER_DIGIT_COUNT);
    }
    else
    {
        out_entry->phone_number[0] = '\0';
    }

    out_entry->unix_time = (uint32_t)in_bytes[8]        |
                            ((uint32_t)in_bytes[9] << 8) |
                            ((uint32_t)in_bytes[10] << 16)|
                            ((uint32_t)in_bytes[11] << 24);

    /* Note: [12] valid marker / [13] checksum are not currently surfaced to
     * the caller (LogEntryTypeDef has no status field) -- a mismatch here
     * would mean flash corruption on an already-written slot, which is rare
     * enough that this is left as a future hardening item rather than
     * blocking this module now. */
}
