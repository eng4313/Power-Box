/**
  ******************************************************************************
  * @file    zfm40.h
  * @brief   Driver for ZFM-40 fingerprint module over UART (USART1).
  *          TX = PA9, RX = PA10, exposed through connector CN4.
  *          Default module baud rate: 57600, 8N1.
  *
  * This driver implements the ZFM-40 packet protocol (header 0xEF01,
  * address, package ID, length, payload, checksum) plus a set of common
  * high-level operations (image capture, template generation, matching,
  * searching, storage). It is intentionally generic/reusable and has no
  * knowledge of the channel-management application logic that will sit
  * on top of it later.
  ******************************************************************************
  */

#ifndef __ZFM40_H
#define __ZFM40_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <string.h>

/* ---- Protocol constants ---- */
#define ZFM_HEADER_HI              0xEFU
#define ZFM_HEADER_LO              0x01U
#define ZFM_DEFAULT_ADDRESS        0xFFFFFFFFU

#define ZFM_PID_COMMAND            0x01U
#define ZFM_PID_DATA               0x02U
#define ZFM_PID_ACK                0x07U
#define ZFM_PID_END_OF_DATA        0x08U

/* ---- Instruction codes ---- */
#define ZFM_CMD_GET_IMAGE          0x01U
#define ZFM_CMD_GEN_CHAR           0x02U
#define ZFM_CMD_MATCH              0x03U
#define ZFM_CMD_SEARCH             0x04U
#define ZFM_CMD_REG_MODEL          0x05U
#define ZFM_CMD_STORE              0x06U
#define ZFM_CMD_LOAD_CHAR          0x07U
#define ZFM_CMD_DELETE_CHAR        0x0CU
#define ZFM_CMD_EMPTY              0x0DU
#define ZFM_CMD_VERIFY_PASSWORD    0x13U
#define ZFM_CMD_TEMPLATE_COUNT     0x1DU

#define ZFM_UART_INSTANCE          USART1
#define ZFM_UART_BAUDRATE          57600U
#define ZFM_UART_TIMEOUT_MS        10000U /* image capture / search can be slow */
#define ZFM_POLL_TIMEOUT_MS        150U   /* GetImage ack is always fast (OK or NO_FINGER),
                                              so a periodic presence poll can use a much
                                              shorter receive timeout than other commands */

#define ZFM_MAX_PAYLOAD_SIZE       32U   /* enough for all commands used here */

/* ---- Driver-level status (transport layer) ---- */
typedef enum
{
    ZFM_OK           = 0x00U,
    ZFM_ERROR        = 0x01U,
    ZFM_TIMEOUT       = 0x02U,
    ZFM_BAD_PACKET    = 0x03U,
    ZFM_NACK          = 0x04U /* module replied, but confirmation code != success */
} ZFM_StatusTypeDef;

/* ---- Module confirmation codes (payload byte 0 of ack packets) ----
 * Only the ones relevant to normal operation are listed; anything else
 * is reported to the caller as the raw value via pConfirmCode. */
#define ZFM_CONF_OK                       0x00U
#define ZFM_CONF_PACKET_ERROR             0x01U
#define ZFM_CONF_NO_FINGER                0x02U
#define ZFM_CONF_ENROLL_FAIL              0x03U
#define ZFM_CONF_IMAGE_TOO_DISORDERLY     0x06U
#define ZFM_CONF_IMAGE_TOO_NOISY          0x07U
#define ZFM_CONF_FINGER_MISMATCH          0x08U
#define ZFM_CONF_SEARCH_NOT_FOUND         0x09U
#define ZFM_CONF_MERGE_FAIL               0x0AU
#define ZFM_CONF_PAGEID_OUT_OF_RANGE      0x0BU
#define ZFM_CONF_TEMPLATE_READ_ERROR      0x0CU
#define ZFM_CONF_DELETE_FAIL              0x10U
#define ZFM_CONF_CLEAR_DB_FAIL            0x11U
#define ZFM_CONF_WRONG_PASSWORD           0x13U
#define ZFM_CONF_FLASH_ERROR              0x18U

/**
  * @brief  Init USART1 peripheral + GPIO for the fingerprint module.
  */
ZFM_StatusTypeDef ZFM40_Init(void);

/**
  * @brief  Verify handshake password (default 0x00000000 from factory).
  * @param  password: 4-byte password configured in the module
  * @param  pConfirmCode: raw confirmation code returned by the module
  */
ZFM_StatusTypeDef ZFM40_VerifyPassword(uint32_t password, uint8_t *pConfirmCode);

/**
  * @brief  TEMPORARY debug helper -- see zfm40.c. Sends VerifyPassword(0)
  *         and dumps whatever raw bytes come back, unparsed. Remove once
  *         the current timeout issue is resolved.
  */
ZFM_StatusTypeDef ZFM40_DebugRawCapture(uint8_t *raw_out, uint8_t max_bytes, uint8_t *count_out);

/**
  * @brief  Capture a fingerprint image from the sensor into the module's
  *         internal image buffer. Caller should poll this (or wait for a
  *         "finger present" mechanism external to this driver) until it
  *         returns ZFM_OK rather than ZFM_CONF_NO_FINGER.
  */
ZFM_StatusTypeDef ZFM40_GetImage(uint8_t *pConfirmCode);

/**
  * @brief  Same as ZFM40_GetImage(), but uses ZFM_POLL_TIMEOUT_MS instead
  *         of ZFM_UART_TIMEOUT_MS for the reply. Intended to be called
  *         periodically (e.g. from a 100ms timer-driven flag) to check
  *         whether a finger is present, instead of blocking for up to
  *         ZFM_UART_TIMEOUT_MS on every poll.
  */
ZFM_StatusTypeDef ZFM40_PollImage(uint8_t *pConfirmCode);

/**
  * @brief  Generate a character file (feature set) from the image
  *         currently in the image buffer, storing it into CharBuffer
  *         1 or 2.
  * @param  bufferId: 1 or 2
  */
ZFM_StatusTypeDef ZFM40_GenChar(uint8_t bufferId, uint8_t *pConfirmCode);

/**
  * @brief  Combine CharBuffer1 and CharBuffer2 into a single template
  *         stored back into both buffers (used during enrollment, after
  *         two GetImage+GenChar passes).
  */
ZFM_StatusTypeDef ZFM40_RegModel(uint8_t *pConfirmCode);

/**
  * @brief  Store the template currently in the given char buffer into
  *         the module's on-chip flash library at pageId.
  */
ZFM_StatusTypeDef ZFM40_StoreChar(uint8_t bufferId, uint16_t pageId, uint8_t *pConfirmCode);

/**
  * @brief  Load a stored template from the library at pageId into the
  *         given char buffer (needed before Match()).
  */
ZFM_StatusTypeDef ZFM40_LoadChar(uint8_t bufferId, uint16_t pageId, uint8_t *pConfirmCode);

/**
  * @brief  Delete 'count' templates starting at pageId from the library.
  */
ZFM_StatusTypeDef ZFM40_DeleteChar(uint16_t pageId, uint16_t count, uint8_t *pConfirmCode);

/**
  * @brief  Erase the entire on-chip fingerprint library.
  */
ZFM_StatusTypeDef ZFM40_EmptyDatabase(uint8_t *pConfirmCode);

/**
  * @brief  Compare CharBuffer1 against CharBuffer2, returning a match
  *         score (higher = better match, 0 = no match).
  */
ZFM_StatusTypeDef ZFM40_Match(uint16_t *pScore, uint8_t *pConfirmCode);

/**
  * @brief  Search the on-chip library for a template matching the
  *         character file in bufferId, across the given page range.
  * @param  pMatchPageId: (out) page ID of the matching template
  * @param  pMatchScore:  (out) match score
  */
ZFM_StatusTypeDef ZFM40_Search(uint8_t bufferId, uint16_t startPage, uint16_t pageCount,
                                uint16_t *pMatchPageId, uint16_t *pMatchScore,
                                uint8_t *pConfirmCode);

/**
  * @brief  Read how many valid templates are currently stored.
  */
ZFM_StatusTypeDef ZFM40_GetTemplateCount(uint16_t *pCount, uint8_t *pConfirmCode);

#ifdef __cplusplus
}
#endif

#endif /* __ZFM40_H */
