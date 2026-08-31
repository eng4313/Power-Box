/**
  ******************************************************************************
  * @file    flow_manager.h
  * @brief   Top-level application flow: idle screen, deposit, retrieve, and
  *          admin login/menu. Sits directly on top of ui_interface.h (input
  *          events + display, backend-agnostic between the UART debug tool
  *          and the real touchscreen/LCD) and channel_manager.h (locker
  *          allocation, door-close timeout, 30-min lockout, persistence,
  *          logging). Also drives ZFM40 (fingerprint capture/match) and
  *          ISD1730 (voice prompts) directly, since both are inherently
  *          part of the interactive flow, not the locker state machine.
  *
  * INPUT CONVENTION: to avoid ever needing another round of changes to the
  * PC debug tool, every "pick one of a few options" moment in the flow
  * (phone type, admin menu item, yes/no where a dedicated button doesn't
  * already exist) is done with plain number keys on the same keypad used
  * for phone numbers and passwords, not with dedicated buttons. Currently:
  *     Phone type select :  1 = Android, 2 = iPhone
  *     Admin menu        :  1 = open locker, 2 = view logs,
  *                           3 = change my password, 4 = add admin (main
  *                           admin only), 5 = remove admin (main admin
  *                           only), 0 = log out
  * This maps directly onto a numeric touchscreen keypad too, so nothing
  * here needs to change when the real LCD/touch replaces the debug tool.
  *
  * AUDIO MAPPING CAVEAT (carried over from the original design note): only
  * 11 voice clips exist (ISD_MESSAGE_t in isd1730.h) for a longer list of
  * logical prompts. Where no dedicated clip exists, the closest one is
  * reused and marked TODO(audio) in flow_manager.c -- please correct
  * FlowAudioTable[] after listening to the actual recordings.
  ******************************************************************************
  */

#ifndef __FLOW_MANAGER_H
#define __FLOW_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"

/* ==========================================================================
 *  Fingerprint match acceptance threshold. Not in typedef.h yet -- move it
 *  there once confirmed against real fingers on the bench, since typedef.h
 *  is the project's single place for shared/tunable defines.
 * ========================================================================== */
#ifndef ZFM_MATCH_SCORE_THRESHOLD
#define ZFM_MATCH_SCORE_THRESHOLD   50U
#endif

/**
  * @brief  One-time init. Must be called after ChannelManager_Init(),
  *         ZFM40_Init(), ISD1730_Init(), Storage_Init(), Log_Init(), and
  *         UI_Init() have all already succeeded.
  */
System_StatusTypeDef FlowManager_Init(void);

/**
  * @brief  Call once per main loop iteration. Non-blocking: pumps pending UI
  *         events, advances the current flow's state machine, polls the
  *         fingerprint sensor at a safe rate, and runs the once-per-second
  *         background tick (ChannelManager_Poll, clock/date + locker-state
  *         mirror to the UI, door-left-open reminders).
  */
void FlowManager_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLOW_MANAGER_H */
