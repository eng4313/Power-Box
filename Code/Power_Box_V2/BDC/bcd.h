/**
  ******************************************************************************
  * @file    bcd.h
  * @brief   Small shared helper: pack/unpack ASCII decimal digit strings
  *          (phone numbers, admin passwords) into packed-BCD bytes for
  *          compact storage in EEPROM (Storage module) and on the W25Q32
  *          log circular buffer (Log module). No hardware dependency.
  ******************************************************************************
  */

#ifndef __BCD_H
#define __BCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

/**
  * @brief  Packs `digit_count` ASCII decimal digits (each '0'..'9') into
  *         packed-BCD bytes, 2 digits per byte, high nibble first.
  *         out_len must be ceil(digit_count / 2). If digit_count is odd,
  *         the last byte's low nibble is filled with 0xF.
  *         Non-digit input bytes (e.g. a zeroed/unused record) are treated
  *         as '0' rather than corrupting the packed output.
  */
void BCD_PackDigits(const char *digits, uint8_t digit_count, uint8_t *out_bytes, uint8_t out_len);

/**
  * @brief  Reverses BCD_PackDigits. Writes digit_count ASCII digits plus a
  *         terminating '\0' into out_digits (which must be at least
  *         digit_count + 1 bytes). A 0xF filler nibble or any corrupt
  *         nibble > 9 decodes as '0'.
  */
void BCD_UnpackDigits(const uint8_t *in_bytes, uint8_t in_len, char *out_digits, uint8_t digit_count);

#ifdef __cplusplus
}
#endif

#endif /* __BCD_H */
