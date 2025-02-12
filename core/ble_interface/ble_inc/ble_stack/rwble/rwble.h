/**
 ****************************************************************************************
 *
 * @file rwble.h
 *
 * @brief Entry points of the BLE software
 *
 * Copyright (C) RivieraWaves 2009-2014
 *
 ****************************************************************************************
 */

#ifndef RWBLE_H_
#define RWBLE_H_

/**
 ****************************************************************************************
 * @addtogroup ROOT
 * @brief Entry points of the BLE stack
 *
 * This module contains the primitives that allow an application accessing and running the
 * BLE protocol stack
 *
 * @{
 ****************************************************************************************
 */


/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>

#include "../../platform/arch/compiler/compiler.h"
#include "../../platform/core_modules/common/api/co_error.h"
#include "../../platform/core_modules/rwip/api/rwip_config.h"
/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialize the BLE stack.
 ****************************************************************************************
 */
void rwble_init(void);

/**
 ****************************************************************************************
 * @brief Reset the BLE stack.
 ****************************************************************************************
 */
void rwble_reset(void);

/**
 ****************************************************************************************
 * @brief Gives FW/HW versions of RW-BLE stack.
 *
 ****************************************************************************************
 */
void rwble_version(uint8_t* fw_version, uint8_t* hw_version);

#if RW_BLE_SUPPORT
/**
 ****************************************************************************************
 * @brief Send an error message to Host.
 *
 * This function is used to send an error message to Host from platform.
 *
 * @param[in] error    Error detected by FW
 ****************************************************************************************
 */
void rwble_send_message(uint32_t error);
#endif //RW_BLE_SUPPORT

/**
 ****************************************************************************************
 * @brief RWBLE interrupt service routine
 *
 * This function is the interrupt service handler of RWBLE.
 *
 ****************************************************************************************
 */
__BLEIRQ void rwble_isr(void);

/**
 ****************************************************************************************
 * @brief Wake-up from Low Power interrupt handler.
 ****************************************************************************************
 */
void BLE_WAKEUP_LP_Handler(void);

/// @} RWBLE

#endif // RWBLE_H_
