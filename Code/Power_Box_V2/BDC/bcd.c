/**
  ******************************************************************************
  * @file    bcd.c
  * @brief   See bcd.h
  ******************************************************************************
  */

#include "bcd.h"

void BCD_PackDigits(const char *digits, uint8_t digit_count, uint8_t *out_bytes, uint8_t out_len)
{
    memset(out_bytes, 0xFF, out_len);

    for (uint8_t i = 0U; i < digit_count; i++)
    {
        char c = digits[i];
        uint8_t nibble = ((c >= '0') && (c <= '9')) ? (uint8_t)(c - '0') : 0U; /* unused/empty records may hold zeroed, non-digit bytes */
        uint8_t byte_index = i / 2U;

        if ((i % 2U) == 0U)
        {
            out_bytes[byte_index] = (uint8_t)((out_bytes[byte_index] & 0x0FU) | (nibble << 4));
        }
        else
        {
            out_bytes[byte_index] = (uint8_t)((out_bytes[byte_index] & 0xF0U) | nibble);
        }
    }
}

void BCD_UnpackDigits(const uint8_t *in_bytes, uint8_t in_len, char *out_digits, uint8_t digit_count)
{
    (void)in_len;

    for (uint8_t i = 0U; i < digit_count; i++)
    {
        uint8_t byte_index = i / 2U;
        uint8_t nibble = ((i % 2U) == 0U) ? (in_bytes[byte_index] >> 4) : (in_bytes[byte_index] & 0x0FU);

        out_digits[i] = (nibble <= 9U) ? (char)('0' + nibble) : '0'; /* 0xF filler / corrupt nibble -> '0' */
    }
    out_digits[digit_count] = '\0';
}
