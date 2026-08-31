/**
  ******************************************************************************
  * @file    storage.c
  * @brief   Persistent storage layer implementation (see storage.h for the
  *          EEPROM address map).
  ******************************************************************************
  */

#include "storage.h"

static uint32_t EE_LockerAddress(uint8_t locker_index);
static uint32_t EE_AdminAddress(uint8_t admin_index);
static System_StatusTypeDef Storage_FormatAll(void);

static void PackName(const char *name, uint8_t *out_bytes, uint8_t out_len);
static void UnpackName(const uint8_t *in_bytes, uint8_t in_len, char *out_name, uint8_t out_capacity);

/* =========================================================================
 *                              Public API
 * ========================================================================= */

System_StatusTypeDef Storage_Init(void)
{
    uint8_t header[EE_HEADER_SIZE];

    if (AT24Cxx_ReadEEPROM(EE_ADDR_HEADER, header, EE_HEADER_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    uint16_t magic   = (uint16_t)(header[0] | (header[1] << 8));
    uint8_t  version = header[2];

    if ((magic != EE_HEADER_MAGIC) || (version != EE_HEADER_VERSION))
    {
        /* Cold start or corrupted header: rebuild everything to a known-good state. */
        return Storage_FormatAll();
    }

    return SYS_OK;
}

/* ---------------------------------------------------------------- Lockers */

System_StatusTypeDef Storage_LoadLocker(uint8_t locker_index, LockerRecordTypeDef *out_record)
{
    uint8_t buf[EE_LOCKER_RECORD_SIZE];

    if ((locker_index >= LOCKER_COUNT) || (out_record == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    if (AT24Cxx_ReadEEPROM(EE_LockerAddress(locker_index), buf, EE_LOCKER_RECORD_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    memset(out_record, 0, sizeof(*out_record));
    out_record->locker_index         = locker_index;
    out_record->flags.bits.in_use    = (buf[0] != 0U) ? 1U : 0U;
    out_record->state                = out_record->flags.bits.in_use ? LOCKER_STATE_OCCUPIED : LOCKER_STATE_EMPTY;

    BCD_UnpackDigits(&buf[1], 6U, out_record->phone_number, PHONE_NUMBER_DIGIT_COUNT);

    out_record->flags.bits.phone_type = buf[7] ? 1U : 0U;  /* 0=PHONE_TYPE_ANDROID, 1=PHONE_TYPE_IPHONE */

    out_record->fingerprint_id = (uint16_t)(buf[8] | (buf[9] << 8));

    out_record->lockout_until_unix_time = (uint32_t)buf[10]        |
                                           ((uint32_t)buf[11] << 8) |
                                           ((uint32_t)buf[12] << 16)|
                                           ((uint32_t)buf[13] << 24);
    out_record->flags.bits.lockout_active = (out_record->lockout_until_unix_time != 0U) ? 1U : 0U;

    out_record->deposit_unix_time = (uint32_t)buf[14]        |
                                     ((uint32_t)buf[15] << 8) |
                                     ((uint32_t)buf[16] << 16)|
                                     ((uint32_t)buf[17] << 24);

    return SYS_OK;
}

System_StatusTypeDef Storage_SaveLocker(uint8_t locker_index, const LockerRecordTypeDef *record)
{
    uint8_t buf[EE_LOCKER_RECORD_SIZE];

    if ((locker_index >= LOCKER_COUNT) || (record == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    buf[0] = record->flags.bits.in_use ? 1U : 0U;

    BCD_PackDigits(record->phone_number, PHONE_NUMBER_DIGIT_COUNT, &buf[1], 6U);

    buf[7] = record->flags.bits.phone_type ? 1U : 0U;

    buf[8] = (uint8_t)(record->fingerprint_id & 0xFFU);
    buf[9] = (uint8_t)((record->fingerprint_id >> 8) & 0xFFU);

    uint32_t lockout = record->flags.bits.lockout_active ? record->lockout_until_unix_time : 0U;
    buf[10] = (uint8_t)(lockout & 0xFFU);
    buf[11] = (uint8_t)((lockout >> 8)  & 0xFFU);
    buf[12] = (uint8_t)((lockout >> 16) & 0xFFU);
    buf[13] = (uint8_t)((lockout >> 24) & 0xFFU);

    buf[14] = (uint8_t)(record->deposit_unix_time & 0xFFU);
    buf[15] = (uint8_t)((record->deposit_unix_time >> 8)  & 0xFFU);
    buf[16] = (uint8_t)((record->deposit_unix_time >> 16) & 0xFFU);
    buf[17] = (uint8_t)((record->deposit_unix_time >> 24) & 0xFFU);

    if (AT24Cxx_WriteEEPROM(EE_LockerAddress(locker_index), buf, EE_LOCKER_RECORD_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    return SYS_OK;
}

System_StatusTypeDef Storage_LoadAllLockers(LockerRecordTypeDef out_records[LOCKER_COUNT])
{
    if (out_records == NULL)
    {
        return SYS_INVALID_PARAM;
    }

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        System_StatusTypeDef st = Storage_LoadLocker(i, &out_records[i]);
        if (st != SYS_OK)
        {
            return st;
        }
    }

    return SYS_OK;
}

/* ----------------------------------------------------------------- Admins */

System_StatusTypeDef Storage_LoadAdmin(uint8_t admin_index, AdminRecordTypeDef *out_record)
{
    uint8_t buf[EE_ADMIN_RECORD_SIZE];

    if ((admin_index >= ADMIN_MAX_COUNT) || (out_record == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    if (AT24Cxx_ReadEEPROM(EE_AdminAddress(admin_index), buf, EE_ADMIN_RECORD_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    memset(out_record, 0, sizeof(*out_record));
    out_record->in_use = (buf[0] != 0U);

    BCD_UnpackDigits(&buf[1], 3U, out_record->password, ADMIN_PASSWORD_DIGIT_COUNT);

    UnpackName(&buf[4], EE_ADMIN_NAME_STORED_LEN, out_record->name, sizeof(out_record->name));

    return SYS_OK;
}

System_StatusTypeDef Storage_SaveAdmin(uint8_t admin_index, const AdminRecordTypeDef *record)
{
    uint8_t buf[EE_ADMIN_RECORD_SIZE];

    if ((admin_index >= ADMIN_MAX_COUNT) || (record == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    buf[0] = record->in_use ? 1U : 0U;

    BCD_PackDigits(record->password, ADMIN_PASSWORD_DIGIT_COUNT, &buf[1], 3U);

    PackName(record->name, &buf[4], EE_ADMIN_NAME_STORED_LEN);

    if (AT24Cxx_WriteEEPROM(EE_AdminAddress(admin_index), buf, EE_ADMIN_RECORD_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    return SYS_OK;
}

System_StatusTypeDef Storage_LoadAllAdmins(AdminRecordTypeDef out_records[ADMIN_MAX_COUNT])
{
    if (out_records == NULL)
    {
        return SYS_INVALID_PARAM;
    }

    for (uint8_t i = 0U; i < ADMIN_MAX_COUNT; i++)
    {
        System_StatusTypeDef st = Storage_LoadAdmin(i, &out_records[i]);
        if (st != SYS_OK)
        {
            return st;
        }
    }

    return SYS_OK;
}

System_StatusTypeDef Storage_ResetMainAdminPassword(void)
{
    AdminRecordTypeDef admin;

    System_StatusTypeDef st = Storage_LoadAdmin(ADMIN_MAIN_INDEX, &admin);
    if (st != SYS_OK)
    {
        return st;
    }

    admin.in_use = true; /* main admin always exists */
    strncpy(admin.password, ADMIN_DEFAULT_PASSWORD, sizeof(admin.password) - 1U);
    admin.password[sizeof(admin.password) - 1U] = '\0';
    /* admin.name intentionally left untouched */

    return Storage_SaveAdmin(ADMIN_MAIN_INDEX, &admin);
}

/* ---------------------------------------------------------- Log pointers */

System_StatusTypeDef Storage_GetLogPointers(uint16_t *out_write_index, uint32_t *out_total_count)
{
    uint8_t header[EE_HEADER_SIZE];

    if ((out_write_index == NULL) || (out_total_count == NULL))
    {
        return SYS_INVALID_PARAM;
    }

    if (AT24Cxx_ReadEEPROM(EE_ADDR_HEADER, header, EE_HEADER_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    *out_write_index  = (uint16_t)(header[3] | (header[4] << 8));
    *out_total_count  = (uint32_t)header[5]        |
                         ((uint32_t)header[6] << 8) |
                         ((uint32_t)header[7] << 16)|
                         ((uint32_t)header[8] << 24);

    return SYS_OK;
}

System_StatusTypeDef Storage_SetLogPointers(uint16_t write_index, uint32_t total_count)
{
    uint8_t header[EE_HEADER_SIZE];

    header[0] = (uint8_t)(EE_HEADER_MAGIC & 0xFFU);
    header[1] = (uint8_t)((EE_HEADER_MAGIC >> 8) & 0xFFU);
    header[2] = EE_HEADER_VERSION;
    header[3] = (uint8_t)(write_index & 0xFFU);
    header[4] = (uint8_t)((write_index >> 8) & 0xFFU);
    header[5] = (uint8_t)(total_count & 0xFFU);
    header[6] = (uint8_t)((total_count >> 8)  & 0xFFU);
    header[7] = (uint8_t)((total_count >> 16) & 0xFFU);
    header[8] = (uint8_t)((total_count >> 24) & 0xFFU);

    if (AT24Cxx_WriteEEPROM(EE_ADDR_HEADER, header, EE_HEADER_SIZE) != HAL_OK)
    {
        return SYS_ERROR;
    }

    return SYS_OK;
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

static uint32_t EE_LockerAddress(uint8_t locker_index)
{
    return EE_ADDR_LOCKERS + ((uint32_t)locker_index * EE_LOCKER_RECORD_SIZE);
}

static uint32_t EE_AdminAddress(uint8_t admin_index)
{
    return EE_ADDR_ADMINS + ((uint32_t)admin_index * EE_ADMIN_RECORD_SIZE);
}

static System_StatusTypeDef Storage_FormatAll(void)
{
    LockerRecordTypeDef empty_locker;
    AdminRecordTypeDef  empty_admin;
    AdminRecordTypeDef  main_admin;

    memset(&empty_locker, 0, sizeof(empty_locker));
    empty_locker.state = LOCKER_STATE_EMPTY;

    for (uint8_t i = 0U; i < LOCKER_COUNT; i++)
    {
        empty_locker.locker_index = i;
        if (Storage_SaveLocker(i, &empty_locker) != SYS_OK)
        {
            return SYS_ERROR;
        }
    }

    memset(&empty_admin, 0, sizeof(empty_admin));
    for (uint8_t i = 1U; i < ADMIN_MAX_COUNT; i++) /* index 0 handled separately below */
    {
        if (Storage_SaveAdmin(i, &empty_admin) != SYS_OK)
        {
            return SYS_ERROR;
        }
    }

    memset(&main_admin, 0, sizeof(main_admin));
    main_admin.in_use = true;
    strncpy(main_admin.password, ADMIN_DEFAULT_PASSWORD, sizeof(main_admin.password) - 1U);
    main_admin.password[sizeof(main_admin.password) - 1U] = '\0';
    /* main_admin.name left empty; admin can set it later from the menu */
    if (Storage_SaveAdmin(ADMIN_MAIN_INDEX, &main_admin) != SYS_OK)
    {
        return SYS_ERROR;
    }

    if (Storage_SetLogPointers(0U, 0U) != SYS_OK) /* also writes a valid header/magic as a side effect */
    {
        return SYS_ERROR;
    }

    return SYS_OK;
}

/**
  * @brief  Copies up to out_len raw ASCII bytes of name into out_bytes,
  *         zero-padding the remainder. No BCD packing (text, not digits).
  */
static void PackName(const char *name, uint8_t *out_bytes, uint8_t out_len)
{
    memset(out_bytes, 0, out_len);
    strncpy((char *)out_bytes, name, out_len);
    /* strncpy already stops at out_len; explicit NUL not required since the
     * whole field was zeroed first and UnpackName always NUL-terminates. */
}

static void UnpackName(const uint8_t *in_bytes, uint8_t in_len, char *out_name, uint8_t out_capacity)
{
    uint8_t copy_len = (in_len < (out_capacity - 1U)) ? in_len : (uint8_t)(out_capacity - 1U);

    memcpy(out_name, in_bytes, copy_len);
    out_name[copy_len] = '\0';
}
