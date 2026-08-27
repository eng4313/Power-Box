/*
 * debug_link.c
 * -----------------------------------------------------------------------
 * See debug_link.h for protocol description and purpose.
 * -----------------------------------------------------------------------
 */

#include "debug_link.h"
#include <string.h>
#include <stdio.h>

#define DEBUG_LINK_RX_BUF_SIZE   128U
#define DEBUG_LINK_TX_BUF_SIZE   160U
#define DEBUG_LINK_TX_PERIOD_MS  1000U
#define DEBUG_LINK_LOCKER_PERIOD_MS 5000U

static UART_HandleTypeDef *s_huart = NULL;

/* Raw DMA/IT receive buffer - HAL writes here, refilled after every event */
static uint8_t s_rx_raw[DEBUG_LINK_RX_BUF_SIZE];

/* Snapshot of the last completed line, copied out of s_rx_raw inside the
 * ISR callback so the main loop can process it safely without racing the
 * next reception. */
static uint8_t s_rx_line[DEBUG_LINK_RX_BUF_SIZE];
static volatile uint16_t s_rx_line_len = 0;
static volatile uint8_t s_rx_line_ready = 0;

static char s_tx_buf[DEBUG_LINK_TX_BUF_SIZE];

static uint32_t s_last_tx_tick = 0;
static uint32_t s_last_locker_tick = 0;
static uint32_t s_fake_seconds = 0; /* fake HH:MM:SS counter, no RTC dependency for this test */
static uint8_t s_locker_cycle_state = 0; /* 0=CLOSED, 1=OPEN, 2=BLINK */

/* ------------------------------------------------------------------- */
static void DebugLink_StartReceive(void)
{
    /* Re-arm reception after every completed event (idle-line detection).
     * ReceiveToIdle_IT will call HAL_UARTEx_RxEventCallback once either the
     * buffer is full OR the line goes idle (i.e. a full "\n"-terminated
     * message, or any pause in transmission). */
    HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_raw, DEBUG_LINK_RX_BUF_SIZE);
}

DebugLinkStatusTypeDef DebugLink_Init(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
    {
        return DEBUG_LINK_ERROR;
    }

    s_huart = huart;
    s_rx_line_ready = 0;
    s_rx_line_len = 0;
    s_last_tx_tick = HAL_GetTick();
    s_last_locker_tick = HAL_GetTick();
    s_fake_seconds = 0;
    s_locker_cycle_state = 0;

    DebugLink_StartReceive();

    DebugLink_SendLine("OUT:LOG:debug_link initialized, UART7 test mode");

    return DEBUG_LINK_OK;
}

DebugLinkStatusTypeDef DebugLink_SendLine(const char *text)
{
    int len;

    if (s_huart == NULL || text == NULL)
    {
        return DEBUG_LINK_ERROR;
    }

    len = snprintf(s_tx_buf, DEBUG_LINK_TX_BUF_SIZE, "%s\n", text);
    if (len <= 0)
    {
        return DEBUG_LINK_ERROR;
    }
    if (len >= (int)DEBUG_LINK_TX_BUF_SIZE)
    {
        len = DEBUG_LINK_TX_BUF_SIZE - 1; /* truncated, still send what fits */
    }

    if (HAL_UART_Transmit(s_huart, (uint8_t *)s_tx_buf, (uint16_t)len, 100) != HAL_OK)
    {
        return DEBUG_LINK_ERROR;
    }

    return DEBUG_LINK_OK;
}

/* ------------------------------------------------------------------- */
/* Called from HAL - keep it short, just copy the buffer and re-arm.   */
void DebugLink_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart != s_huart)
    {
        return; /* not our UART, ignore */
    }

    if (Size > 0 && Size < DEBUG_LINK_RX_BUF_SIZE && s_rx_line_ready == 0)
    {
        memcpy(s_rx_line, s_rx_raw, Size);
        s_rx_line_len = Size;
        s_rx_line_ready = 1;
    }
    /* If a previous line hasn't been processed yet, this event is dropped.
     * Fine for a test link; the real implementation may want a small
     * ring buffer of lines instead. */

    DebugLink_StartReceive();
}

/* ------------------------------------------------------------------- */
static void DebugLink_HandleReceivedLine(void)
{
    char line[DEBUG_LINK_RX_BUF_SIZE + 1];
    uint16_t len = s_rx_line_len;

    if (len >= sizeof(line))
    {
        len = sizeof(line) - 1;
    }

    memcpy(line, s_rx_line, len);
    line[len] = '\0';

    /* Strip trailing \r or \n if present */
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
    {
        line[--len] = '\0';
    }

    if (len == 0)
    {
        s_rx_line_ready = 0;
        return;
    }

    /* Echo back whatever was received, prefixed so it's obviously a test echo */
    snprintf(s_tx_buf, DEBUG_LINK_TX_BUF_SIZE, "OUT:LOG:RX_OK -> %s", line);
    DebugLink_SendLine(s_tx_buf);

    s_rx_line_ready = 0;
}

/* ------------------------------------------------------------------- */
static void DebugLink_PeriodicTx(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t h, m, s;
    char msg[64];

    if ((now - s_last_tx_tick) < DEBUG_LINK_TX_PERIOD_MS)
    {
        return;
    }
    s_last_tx_tick = now;

    s_fake_seconds++;
    h = (s_fake_seconds / 3600U) % 24U;
    m = (s_fake_seconds / 60U) % 60U;
    s = s_fake_seconds % 60U;

    snprintf(s_tx_buf, DEBUG_LINK_TX_BUF_SIZE, "OUT:CLOCK:%02lu:%02lu:%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)s);
    DebugLink_SendLine(s_tx_buf);

    snprintf(msg, sizeof(msg), "OUT:MSG:UART7 test running (%lu s)", (unsigned long)s_fake_seconds);
    DebugLink_SendLine(msg);
}

static void DebugLink_PeriodicLockerCycle(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_last_locker_tick) < DEBUG_LINK_LOCKER_PERIOD_MS)
    {
        return;
    }
    s_last_locker_tick = now;

    switch (s_locker_cycle_state)
    {
        case 0:
            DebugLink_SendLine("OUT:LOCKER:1:OPEN");
            s_locker_cycle_state = 1;
            break;
        case 1:
            DebugLink_SendLine("OUT:LOCKER:1:CLOSED");
            s_locker_cycle_state = 2;
            break;
        default:
            DebugLink_SendLine("OUT:LOCKER:1:LED_BLINK");
            s_locker_cycle_state = 0;
            break;
    }
}

/* ------------------------------------------------------------------- */
void DebugLink_Process(void)
{
    if (s_rx_line_ready)
    {
        DebugLink_HandleReceivedLine();
    }

    DebugLink_PeriodicTx();
    DebugLink_PeriodicLockerCycle();
}
