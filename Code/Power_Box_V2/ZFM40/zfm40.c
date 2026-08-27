/**
  ******************************************************************************
  * @file    zfm40.c
  * @brief   ZFM-40 fingerprint module driver implementation
  *
  * Pinout (extracted from Power_Box_V2 schematic):
  *   MCU_TXD -> PA9  (USART1_TX, AF7)  -> module RX (via connector CN4)
  *   MCU_RXD -> PA10 (USART1_RX, AF7)  -> module TX (via connector CN4)
  ******************************************************************************
  */

#include "zfm40.h"

static UART_HandleTypeDef huart1;

static void               ZFM_MspInit(void);
static ZFM_StatusTypeDef  ZFM_SendPacket(uint8_t pid, const uint8_t *payload, uint16_t payload_len);
static ZFM_StatusTypeDef  ZFM_ReceivePacket(uint8_t *pid_out, uint8_t *payload_out,
                                             uint16_t max_payload, uint16_t *payload_len_out);
static ZFM_StatusTypeDef  ZFM_SendPacketEx(uint8_t pid, const uint8_t *payload, uint16_t payload_len,
                                            uint32_t timeout_ms);
static ZFM_StatusTypeDef  ZFM_ReceivePacketEx(uint8_t *pid_out, uint8_t *payload_out,
                                               uint16_t max_payload, uint16_t *payload_len_out,
                                               uint32_t timeout_ms);
static ZFM_StatusTypeDef  ZFM_SimpleCommand(uint8_t cmd, uint8_t *pConfirmCode);

/* =========================================================================
 *                              Public API
 * ========================================================================= */

ZFM_StatusTypeDef ZFM40_Init(void)
{
    ZFM_MspInit();

    huart1.Instance          = ZFM_UART_INSTANCE;
    huart1.Init.BaudRate     = ZFM_UART_BAUDRATE;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        return ZFM_ERROR;
    }

    return ZFM_OK;
}

/**
  * @brief  TEMPORARY debug helper: sends the same VerifyPassword command
  *         bytes as ZFM40_VerifyPassword() would, then reads back raw
  *         bytes one at a time (50ms each) with NO protocol parsing at
  *         all, for as long as something keeps arriving. Fills raw_out
  *         with whatever bytes were actually received and *count_out
  *         with how many. Remove once the timeout issue is resolved.
  */
ZFM_StatusTypeDef ZFM40_DebugRawCapture(uint8_t *raw_out, uint8_t max_bytes, uint8_t *count_out)
{
    uint8_t payload[5] = { ZFM_CMD_VERIFY_PASSWORD, 0, 0, 0, 0 };
    uint8_t count = 0;

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    while (count < max_bytes)
    {
        if (HAL_UART_Receive(&huart1, &raw_out[count], 1, 50) != HAL_OK)
        {
            break; /* 50ms of silence -- stop, whatever we got is in raw_out */
        }
        count++;
    }

    if (count_out != NULL)
    {
        *count_out = count;
    }

    return ZFM_OK;
}

ZFM_StatusTypeDef ZFM40_VerifyPassword(uint32_t password, uint8_t *pConfirmCode)
{
    uint8_t payload[5];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_VERIFY_PASSWORD;
    payload[1] = (uint8_t)(password >> 24);
    payload[2] = (uint8_t)(password >> 16);
    payload[3] = (uint8_t)(password >> 8);
    payload[4] = (uint8_t)(password);

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

ZFM_StatusTypeDef ZFM40_GetImage(uint8_t *pConfirmCode)
{
    return ZFM_SimpleCommand(ZFM_CMD_GET_IMAGE, pConfirmCode);
}

/**
  * @brief  Short-timeout GetImage, meant to be called periodically (e.g.
  *         once per 100ms timer tick) to check whether a finger is on
  *         the sensor. The module always answers a GetImage command
  *         immediately, with either ZFM_CONF_OK or ZFM_CONF_NO_FINGER,
  *         so ZFM_POLL_TIMEOUT_MS is enough here and avoids blocking the
  *         caller for the full ZFM_UART_TIMEOUT_MS on every tick.
  */
ZFM_StatusTypeDef ZFM40_PollImage(uint8_t *pConfirmCode)
{
    uint8_t payload[1];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_GET_IMAGE;

    if (ZFM_SendPacketEx(ZFM_PID_COMMAND, payload, sizeof(payload), ZFM_UART_TIMEOUT_MS) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacketEx(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len, ZFM_POLL_TIMEOUT_MS) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

ZFM_StatusTypeDef ZFM40_GenChar(uint8_t bufferId, uint8_t *pConfirmCode)
{
    uint8_t payload[2];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_GEN_CHAR;
    payload[1] = bufferId;

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

ZFM_StatusTypeDef ZFM40_RegModel(uint8_t *pConfirmCode)
{
    return ZFM_SimpleCommand(ZFM_CMD_REG_MODEL, pConfirmCode);
}

ZFM_StatusTypeDef ZFM40_StoreChar(uint8_t bufferId, uint16_t pageId, uint8_t *pConfirmCode)
{
    uint8_t payload[4];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_STORE;
    payload[1] = bufferId;
    payload[2] = (uint8_t)(pageId >> 8);
    payload[3] = (uint8_t)(pageId);

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

ZFM_StatusTypeDef ZFM40_LoadChar(uint8_t bufferId, uint16_t pageId, uint8_t *pConfirmCode)
{
    uint8_t payload[4];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_LOAD_CHAR;
    payload[1] = bufferId;
    payload[2] = (uint8_t)(pageId >> 8);
    payload[3] = (uint8_t)(pageId);

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

ZFM_StatusTypeDef ZFM40_DeleteChar(uint16_t pageId, uint16_t count, uint8_t *pConfirmCode)
{
    uint8_t payload[5];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_DELETE_CHAR;
    payload[1] = (uint8_t)(pageId >> 8);
    payload[2] = (uint8_t)(pageId);
    payload[3] = (uint8_t)(count >> 8);
    payload[4] = (uint8_t)(count);

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

ZFM_StatusTypeDef ZFM40_EmptyDatabase(uint8_t *pConfirmCode)
{
    return ZFM_SimpleCommand(ZFM_CMD_EMPTY, pConfirmCode);
}

ZFM_StatusTypeDef ZFM40_Match(uint16_t *pScore, uint8_t *pConfirmCode)
{
    uint8_t payload[1];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_MATCH;

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 3U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    if (rx_payload[0] != ZFM_CONF_OK)
    {
        return ZFM_NACK;
    }

    if (pScore != NULL)
    {
        *pScore = ((uint16_t)rx_payload[1] << 8) | (uint16_t)rx_payload[2];
    }

    return ZFM_OK;
}

ZFM_StatusTypeDef ZFM40_Search(uint8_t bufferId, uint16_t startPage, uint16_t pageCount,
                                uint16_t *pMatchPageId, uint16_t *pMatchScore,
                                uint8_t *pConfirmCode)
{
    uint8_t payload[6];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_SEARCH;
    payload[1] = bufferId;
    payload[2] = (uint8_t)(startPage >> 8);
    payload[3] = (uint8_t)(startPage);
    payload[4] = (uint8_t)(pageCount >> 8);
    payload[5] = (uint8_t)(pageCount);

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 5U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    if (rx_payload[0] != ZFM_CONF_OK)
    {
        return ZFM_NACK;
    }

    if (pMatchPageId != NULL)
    {
        *pMatchPageId = ((uint16_t)rx_payload[1] << 8) | (uint16_t)rx_payload[2];
    }

    if (pMatchScore != NULL)
    {
        *pMatchScore = ((uint16_t)rx_payload[3] << 8) | (uint16_t)rx_payload[4];
    }

    return ZFM_OK;
}

ZFM_StatusTypeDef ZFM40_GetTemplateCount(uint16_t *pCount, uint8_t *pConfirmCode)
{
    uint8_t payload[1];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = ZFM_CMD_TEMPLATE_COUNT;

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 3U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    if (rx_payload[0] != ZFM_CONF_OK)
    {
        return ZFM_NACK;
    }

    if (pCount != NULL)
    {
        *pCount = ((uint16_t)rx_payload[1] << 8) | (uint16_t)rx_payload[2];
    }

    return ZFM_OK;
}

/* =========================================================================
 *                          Private helpers
 * ========================================================================= */

/**
  * @brief  Shared implementation for commands that take no parameters
  *         and return only a 1-byte confirmation code (GetImage,
  *         RegModel, Empty, ...).
  */
static ZFM_StatusTypeDef ZFM_SimpleCommand(uint8_t cmd, uint8_t *pConfirmCode)
{
    uint8_t payload[1];
    uint8_t rx_pid;
    uint8_t rx_payload[ZFM_MAX_PAYLOAD_SIZE];
    uint16_t rx_len;

    payload[0] = cmd;

    if (ZFM_SendPacket(ZFM_PID_COMMAND, payload, sizeof(payload)) != ZFM_OK)
    {
        return ZFM_ERROR;
    }

    if (ZFM_ReceivePacket(&rx_pid, rx_payload, sizeof(rx_payload), &rx_len) != ZFM_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((rx_pid != ZFM_PID_ACK) || (rx_len < 1U))
    {
        return ZFM_BAD_PACKET;
    }

    if (pConfirmCode != NULL)
    {
        *pConfirmCode = rx_payload[0];
    }

    return (rx_payload[0] == ZFM_CONF_OK) ? ZFM_OK : ZFM_NACK;
}

/**
  * @brief  Build and transmit one full packet: header, default address,
  *         PID, length, payload, checksum. Uses the default protocol
  *         timeout; see ZFM_SendPacketEx() for a caller-selectable one.
  */
static ZFM_StatusTypeDef ZFM_SendPacket(uint8_t pid, const uint8_t *payload, uint16_t payload_len)
{
    return ZFM_SendPacketEx(pid, payload, payload_len, ZFM_UART_TIMEOUT_MS);
}

/**
  * @brief  Same as ZFM_SendPacket(), with an explicit UART timeout.
  */
static ZFM_StatusTypeDef ZFM_SendPacketEx(uint8_t pid, const uint8_t *payload, uint16_t payload_len,
                                           uint32_t timeout_ms)
{
    uint8_t header[9];
    uint16_t length = payload_len + 2U; /* payload + 2-byte checksum */
    uint16_t checksum;
    uint8_t checksum_bytes[2];
    uint16_t i;

    header[0] = ZFM_HEADER_HI;
    header[1] = ZFM_HEADER_LO;
    header[2] = (uint8_t)(ZFM_DEFAULT_ADDRESS >> 24);
    header[3] = (uint8_t)(ZFM_DEFAULT_ADDRESS >> 16);
    header[4] = (uint8_t)(ZFM_DEFAULT_ADDRESS >> 8);
    header[5] = (uint8_t)(ZFM_DEFAULT_ADDRESS);
    header[6] = pid;
    header[7] = (uint8_t)(length >> 8);
    header[8] = (uint8_t)(length);

    checksum = pid + header[7] + header[8];
    for (i = 0; i < payload_len; i++)
    {
        checksum += payload[i];
    }
    checksum_bytes[0] = (uint8_t)(checksum >> 8);
    checksum_bytes[1] = (uint8_t)(checksum);

    if (HAL_UART_Transmit(&huart1, header, sizeof(header), timeout_ms) != HAL_OK)
    {
        return ZFM_ERROR;
    }

    if (payload_len > 0U)
    {
        if (HAL_UART_Transmit(&huart1, (uint8_t *)payload, payload_len, timeout_ms) != HAL_OK)
        {
            return ZFM_ERROR;
        }
    }

    if (HAL_UART_Transmit(&huart1, checksum_bytes, sizeof(checksum_bytes), timeout_ms) != HAL_OK)
    {
        return ZFM_ERROR;
    }

    return ZFM_OK;
}

/**
  * @brief  Receive and parse one full packet: verify header magic bytes,
  *         read PID + length, read payload + checksum, verify checksum.
  *         Uses the default protocol timeout; see ZFM_ReceivePacketEx()
  *         for a caller-selectable one.
  * @param  payload_out: receives payload WITHOUT the trailing 2-byte
  *         checksum (i.e. just the confirmation code + return data).
  */
static ZFM_StatusTypeDef ZFM_ReceivePacket(uint8_t *pid_out, uint8_t *payload_out,
                                            uint16_t max_payload, uint16_t *payload_len_out)
{
    return ZFM_ReceivePacketEx(pid_out, payload_out, max_payload, payload_len_out, ZFM_UART_TIMEOUT_MS);
}

/**
  * @brief  Same as ZFM_ReceivePacket(), with an explicit UART timeout.
  */
static ZFM_StatusTypeDef ZFM_ReceivePacketEx(uint8_t *pid_out, uint8_t *payload_out,
                                              uint16_t max_payload, uint16_t *payload_len_out,
                                              uint32_t timeout_ms)
{
    uint8_t header[9];
    uint16_t length;
    uint16_t payload_len;
    uint8_t rx_buf[ZFM_MAX_PAYLOAD_SIZE + 2U];
    uint16_t checksum, expected_checksum;
    uint16_t i;

    if (HAL_UART_Receive(&huart1, header, sizeof(header), timeout_ms) != HAL_OK)
    {
        return ZFM_TIMEOUT;
    }

    if ((header[0] != ZFM_HEADER_HI) || (header[1] != ZFM_HEADER_LO))
    {
        return ZFM_BAD_PACKET;
    }

    *pid_out = header[6];
    length = ((uint16_t)header[7] << 8) | (uint16_t)header[8];

    if (length < 2U)
    {
        return ZFM_BAD_PACKET; /* must at least contain the checksum */
    }

    payload_len = length - 2U;

    if (payload_len > max_payload)
    {
        return ZFM_BAD_PACKET; /* caller's buffer too small */
    }

    if (HAL_UART_Receive(&huart1, rx_buf, length, timeout_ms) != HAL_OK)
    {
        return ZFM_TIMEOUT;
    }

    /* verify checksum: PID + length(hi+lo) + payload bytes */
    checksum = header[6] + header[7] + header[8];
    for (i = 0; i < payload_len; i++)
    {
        checksum += rx_buf[i];
    }
    expected_checksum = ((uint16_t)rx_buf[payload_len] << 8) | (uint16_t)rx_buf[payload_len + 1U];

    if (checksum != expected_checksum)
    {
        return ZFM_BAD_PACKET;
    }

    memcpy(payload_out, rx_buf, payload_len);
    *payload_len_out = payload_len;

    return ZFM_OK;
}

static void ZFM_MspInit(void)
{
    GPIO_InitTypeDef GPIO_Init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    /* TX: PA9, RX: PA10, AF7 */
    GPIO_Init.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_Init.Mode      = GPIO_MODE_AF_PP;
    GPIO_Init.Pull      = GPIO_PULLUP;
    GPIO_Init.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_Init.Alternate  = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_Init);
}
