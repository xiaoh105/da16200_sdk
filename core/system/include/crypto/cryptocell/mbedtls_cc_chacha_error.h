/****************************************************************************
* The confidential and proprietary information contained in this file may   *
* only be used by a person authorised under and to the extent permitted     *
* by a subsisting licensing agreement from Arm Limited (or its affiliates). *
*     (C) COPYRIGHT [2001-2018] Arm Limited (or its affiliates).            *
*         ALL RIGHTS RESERVED                                               *
* This entire notice must be reproduced on all copies of this file          *
* and copies of this file may only be made by a person if such person is    *
* permitted to do so under the terms of a subsisting license agreement      *
* from Arm Limited (or its affiliates).                                     *
*****************************************************************************/

/*!
@file
@brief This file contains the error definitions of the CryptoCell ChaCha APIs.
@defgroup cc_chacha_error Specific errors of the CryptoCell ChaCha APIs
@brief Contains the CryptoCell ChaCha-API error definitions.

@{
@ingroup cc_chacha
*/

#ifndef _MBEDTLS_CC_CHACHA_ERROR_H
#define _MBEDTLS_CC_CHACHA_ERROR_H

#include "cc_error.h"

#ifdef __cplusplus
extern "C"
{
#endif





/************************ Defines ******************************/

/* The base address of errors for the ChaCha module - 0x00F02200. */
/*! Illegal Nonce. */
#define CC_CHACHA_INVALID_NONCE_ERROR                           (CC_CHACHA_MODULE_ERROR_BASE + 0x01UL)
/*! Illegal key size. */
#define CC_CHACHA_ILLEGAL_KEY_SIZE_ERROR                    (CC_CHACHA_MODULE_ERROR_BASE + 0x02UL)
/*! Illegal key pointer. */
#define CC_CHACHA_INVALID_KEY_POINTER_ERROR                 (CC_CHACHA_MODULE_ERROR_BASE + 0x03UL)
/*! Illegal operation mode. */
#define CC_CHACHA_INVALID_ENCRYPT_MODE_ERROR                (CC_CHACHA_MODULE_ERROR_BASE + 0x04UL)
/*! Illegal data-in pointer. */
#define CC_CHACHA_DATA_IN_POINTER_INVALID_ERROR             (CC_CHACHA_MODULE_ERROR_BASE + 0x05UL)
/*! Illegal data-out pointer. */
#define CC_CHACHA_DATA_OUT_POINTER_INVALID_ERROR            (CC_CHACHA_MODULE_ERROR_BASE + 0x06UL)
/*! Illegal user context. */
#define CC_CHACHA_INVALID_USER_CONTEXT_POINTER_ERROR            (CC_CHACHA_MODULE_ERROR_BASE + 0x07UL)
/*! Illegal user context size. */
#define CC_CHACHA_CTX_SIZES_ERROR                               (CC_CHACHA_MODULE_ERROR_BASE + 0x08UL)
/*! Illegal nonce pointer. */
#define CC_CHACHA_INVALID_NONCE_PTR_ERROR                       (CC_CHACHA_MODULE_ERROR_BASE + 0x09UL)
/*! Illegal data-in size. */
#define CC_CHACHA_DATA_IN_SIZE_ILLEGAL                          (CC_CHACHA_MODULE_ERROR_BASE + 0x0AUL)
/*! General error. */
#define CC_CHACHA_GENERAL_ERROR                         (CC_CHACHA_MODULE_ERROR_BASE + 0x0BUL)
/*! ChaCha is not supported. */
#define CC_CHACHA_IS_NOT_SUPPORTED                          (CC_CHACHA_MODULE_ERROR_BASE + 0xFFUL)

/************************ Enums ********************************/

/************************ Typedefs  ****************************/

/************************ Structs  *****************************/

/************************ Public Variables *********************/

/************************ Public Functions *********************/

#ifdef __cplusplus
}
#endif

#endif//_MBEDTLS_CC_CHACHA_ERROR_H
/**
@}
 */

