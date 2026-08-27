/**
  ******************************************************************************
  * @file    log.h
  * @brief   System event log, physically stored as a circular buffer of
  *          fixed-size 16-byte packed entries on W25Q32 SPI flash. Owns the
  *          flash region and entry pack/unpack; the two persistent
  *          bookkeeping counters (write index, total count) live in EEPROM
  *          via the Storage module (storage.h) so they survive a reboot.
  *
  * PHYSICAL LAYOUT (W25Q32, base address LOG_FLASH_BASE_ADDRESS):
  *   - Entry size: 16 bytes (padded from 12 actually-used bytes so it
  *     divides the 4KB sector size evenly: 4096 / 16 = 256 entries/sector).
  *   - 4 sectors reserved -> 1024 physical slots, 16KB total (tiny compared
  *     to the 4MB chip). Any future module needing flash space (e.g. the
  *     idle-screen photo gallery) MUST start its own region at or after
  *     LOG_FLASH_REGION_SIZE, never inside it.
  *   - The buffer is circular over 1024 physical slots, but the LOGICAL
  *     capacity exposed to the admin UI is LOG_CAPACITY_TOTAL (800, per
  *     typedef.h = 100 x LOCKER_COUNT). The extra physical headroom
  *     (1024 vs 800) just means sector erase-and-reuse happens less often;
  *     it does not change what the admin sees -- Log_ReadPage() always
  *     exposes at most the newest LOG_CAPACITY_TOTAL entries, oldest ones
  *     beyond that are simply no longer shown even if still physically
  *     present for a little while.
  *   - Wraparound correctness: a sector is erased immediately before the
  *     first entry write lands in it (physical_slot % entries_per_sector
  *     == 0), which naturally covers both the very first use of each
  *     sector and every later wrap back into it -- no separate
  *     "format on first boot" step is needed.
  ******************************************************************************
  */

#ifndef __LOG_H
#define __LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"

#include "storage.h"
#include "w25q32.h"
#include "bcd.h"
#include <string.h>

#define LOG_ENTRY_PACKED_SIZE        16U
#define LOG_ENTRIES_PER_SECTOR       (W25Q32_SECTOR_SIZE / LOG_ENTRY_PACKED_SIZE)   /* 4096/16 = 256 */
#define LOG_PHYSICAL_SECTOR_COUNT    4U   /* 4 x 256 = 1024 physical slots, >= LOG_CAPACITY_TOTAL(800) */
#define LOG_PHYSICAL_CAPACITY        (LOG_ENTRIES_PER_SECTOR * LOG_PHYSICAL_SECTOR_COUNT)
#define LOG_FLASH_BASE_ADDRESS       0x000000U
#define LOG_FLASH_REGION_SIZE        (LOG_PHYSICAL_SECTOR_COUNT * W25Q32_SECTOR_SIZE) /* 16KB; next module's flash region must start here or later */

#define LOG_ENTRY_VALID_MARKER       0xAAU

/* Packed entry layout (16 bytes):
 *   [0]      event (LogEventTypeDef, fits in uint8_t)
 *   [1]      locker_index
 *   [2..7]   phone_number, packed BCD, 6 bytes (11 digits)
 *   [8..11]  unix_time, little-endian uint32
 *   [12]     valid marker (LOG_ENTRY_VALID_MARKER once written)
 *   [13]     checksum: XOR of bytes [0..12]
 *   [14..15] reserved (0xFF)
 */

/**
  * @brief  Loads the persisted write index / total count from Storage into
  *         the module's RAM cache. Call once at boot, after Storage_Init()
  *         and W25Q32_Init() have both already succeeded.
  */
System_StatusTypeDef Log_Init(void);

/**
  * @brief  Appends one event to the log: writes the packed entry to flash
  *         (erasing the containing sector first if this is that sector's
  *         first write since it was last erased) and persists the updated
  *         write index / total count to EEPROM via Storage_SetLogPointers().
  * @param  phone_number  11-digit string, or NULL/empty for events with no
  *                        associated phone number (e.g. admin-only events).
  * @param  unix_time     normally RTC_GetUnixTime() from the caller, kept as
  *                        a parameter here so this module has no direct RTC
  *                        dependency.
  */
System_StatusTypeDef Log_Append(LogEventTypeDef event, uint8_t locker_index,
                                 const char *phone_number, uint32_t unix_time);

/**
  * @brief  Number of entries currently visible to the admin UI, i.e.
  *         min(total_count_ever_written, LOG_CAPACITY_TOTAL).
  */
System_StatusTypeDef Log_GetVisibleCount(uint32_t *out_count);

/**
  * @brief  Number of LOG_PAGE_SIZE(50)-entry pages needed to show all
  *         currently visible entries (ceil(visible_count / LOG_PAGE_SIZE),
  *         at least 1 even when the log is empty so the UI can still show
  *         an empty "page 1 of 1").
  */
System_StatusTypeDef Log_GetPageCount(uint32_t *out_page_count);

/**
  * @brief  Reads one page of log entries, newest-first: page 0 holds the
  *         most recent up-to-LOG_PAGE_SIZE entries, page 1 the next
  *         LOG_PAGE_SIZE older ones, and so on, matching the admin's "browse
  *         backward through history" mental model.
  * @param  page_index      0-based page number (see Log_GetPageCount()).
  * @param  out_entries     caller-provided array of at least LOG_PAGE_SIZE
  *                          LogEntryTypeDef.
  * @param  out_entry_count how many of out_entries[] were actually filled
  *                          (< LOG_PAGE_SIZE only on the last page).
  * @retval SYS_OK, or SYS_NOT_FOUND if page_index is past the last page.
  */
System_StatusTypeDef Log_ReadPage(uint32_t page_index, LogEntryTypeDef *out_entries,
                                   uint8_t *out_entry_count);

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H */
