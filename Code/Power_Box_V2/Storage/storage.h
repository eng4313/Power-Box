/**
  ******************************************************************************
  * @file    storage.h
  * @brief   Persistent storage layer on top of AT24Cxx (EEPROM). Owns the
  *          locker records, admin records, and the bookkeeping pointers
  *          (write index / total count) for the log circular buffer that
  *          physically lives on W25Q32 (the Log module built on top of this
  *          layer owns entry serialization and pagination, not this file).
  *
  * EEPROM ADDRESS MAP (fits the AT24C04, 512 bytes physically installed
  * today -- see EEPROM_INSTALLED_SIZE_BYTES in typedef.h):
  *
  *   0x00  Header            9 bytes   magic(2) + version(1) + log_write_index(2) + log_total_count(4)
  *   0x09  Locker records  144 bytes   18 bytes x LOCKER_COUNT(8)
  *   0x99  Admin records   240 bytes   12 bytes x ADMIN_MAX_COUNT(20)
  *   ----  total used      393 bytes,  119 bytes spare on the current chip
  *
  * All addresses below are computed from LOCKER_COUNT / ADMIN_MAX_COUNT in
  * typedef.h, so if those change the map simply grows -- there is no
  * hardcoded offset. When AT24C256 replaces the AT24C04, nothing here needs
  * to change except EEPROM_INSTALLED_SIZE_BYTES becoming large enough to
  * stop being the limiting factor (e.g. admin name length could then grow
  * past the current 8-byte EEPROM-truncated limit).
  ******************************************************************************
  */

#ifndef __STORAGE_H
#define __STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"
#include "AT24Cxx.h"
#include "bcd.h"
#include <string.h>

/* ---------------------------------------------------------------- Address map */
#define EE_HEADER_SIZE               9U
#define EE_LOCKER_RECORD_SIZE        18U
#define EE_ADMIN_NAME_STORED_LEN     8U   /* EEPROM-truncated; RAM copy can hold ADMIN_NAME_MAX_LEN(16) */
#define EE_ADMIN_RECORD_SIZE         (1U + 3U + EE_ADMIN_NAME_STORED_LEN)  /* in_use(1) + password_bcd(3) + name(8) = 12 */

#define EE_ADDR_HEADER                0x0000U
#define EE_ADDR_LOCKERS               (EE_ADDR_HEADER + EE_HEADER_SIZE)
#define EE_ADDR_ADMINS                (EE_ADDR_LOCKERS + (EE_LOCKER_RECORD_SIZE * LOCKER_COUNT))
#define EE_TOTAL_USED_BYTES           (EE_ADDR_ADMINS + (EE_ADMIN_RECORD_SIZE * ADMIN_MAX_COUNT))

#if (EE_TOTAL_USED_BYTES > EEPROM_INSTALLED_SIZE_BYTES)
#error "Storage layout does not fit EEPROM_INSTALLED_SIZE_BYTES -- reduce LOCKER_COUNT/ADMIN_MAX_COUNT or grow the EEPROM"
#endif

#define EE_HEADER_MAGIC               0xA55AU
#define EE_HEADER_VERSION             1U

/**
  * @brief  Reads the EEPROM header and validates the magic/version.
  *         On a genuine cold start / corrupted header, formats EEPROM:
  *         all lockers marked empty, all admin slots cleared except index 0
  *         (main admin, password = ADMIN_DEFAULT_PASSWORD), log pointers
  *         reset to zero, and a fresh valid header is written.
  * @retval SYS_OK / SYS_ERROR (I2C failure)
  */
System_StatusTypeDef Storage_Init(void);

/* ---------------------------------------------------------------- Lockers */

/**
  * @brief  Loads a single locker record from EEPROM into a RAM
  *         LockerRecordTypeDef. locker_index is always written into the
  *         output struct regardless of what was stored, so callers never
  *         need to set it separately.
  */
System_StatusTypeDef Storage_LoadLocker(uint8_t locker_index, LockerRecordTypeDef *out_record);

/**
  * @brief  Persists a single locker record. Only touches this locker's 18
  *         bytes, not the whole table, to minimize EEPROM wear and I2C time.
  */
System_StatusTypeDef Storage_SaveLocker(uint8_t locker_index, const LockerRecordTypeDef *record);

/**
  * @brief  Convenience: loads all LOCKER_COUNT records at once (e.g. at boot,
  *         to rebuild the RAM Channel Manager state array).
  */
System_StatusTypeDef Storage_LoadAllLockers(LockerRecordTypeDef out_records[LOCKER_COUNT]);

/* ----------------------------------------------------------------- Admins */

/**
  * @brief  Loads a single admin slot. out_record->name is at most 8
  *         characters (EEPROM-truncated, see file header note) even though
  *         AdminRecordTypeDef.name can hold ADMIN_NAME_MAX_LEN(16) in RAM.
  */
System_StatusTypeDef Storage_LoadAdmin(uint8_t admin_index, AdminRecordTypeDef *out_record);

/**
  * @brief  Persists a single admin slot. record->name longer than 8 chars is
  *         silently truncated to fit the EEPROM budget.
  */
System_StatusTypeDef Storage_SaveAdmin(uint8_t admin_index, const AdminRecordTypeDef *record);

/**
  * @brief  Convenience: loads all ADMIN_MAX_COUNT slots at once (e.g. for the
  *         admin-list menu, or to search for a password match at login).
  */
System_StatusTypeDef Storage_LoadAllAdmins(AdminRecordTypeDef out_records[ADMIN_MAX_COUNT]);

/**
  * @brief  Handler for the physical reset button (S2, held
  *         ADMIN_RESET_BUTTON_HOLD_MS). Resets ONLY admin index 0's password
  *         back to ADMIN_DEFAULT_PASSWORD. Does not touch admin 0's name,
  *         and does not touch any other admin slot (per project decision:
  *         only the main admin's password is reset by hardware; deleting or
  *         editing other admins is only ever done by the main admin from
  *         the menu).
  */
System_StatusTypeDef Storage_ResetMainAdminPassword(void);

/* ------------------------------------------------- Log bookkeeping (EEPROM
 * side only: the physical circular buffer on W25Q32 is owned and walked by
 * the Log module; this is just where its two persistent counters live so
 * they survive a reboot). */

/**
  * @brief  out_write_index: next physical slot index to write in the W25Q32
  *         circular log region (0 .. physical capacity - 1).
  *         out_total_count: total log entries ever written, monotonically
  *         increasing, used by the Log module to know how many of the
  *         physical slots currently hold valid (vs never-yet-written) data
  *         and to compute the "last N" pagination window.
  */
System_StatusTypeDef Storage_GetLogPointers(uint16_t *out_write_index, uint32_t *out_total_count);

/**
  * @brief  Persists the two log pointers above. Called by the Log module
  *         after every successful entry write.
  */
System_StatusTypeDef Storage_SetLogPointers(uint16_t write_index, uint32_t total_count);

#ifdef __cplusplus
}
#endif

#endif /* __STORAGE_H */
