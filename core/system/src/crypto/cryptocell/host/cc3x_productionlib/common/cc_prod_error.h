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
@brief This file contains the error definitions of the CryptoCell production-library APIs.

@defgroup prod_errors Specific errors of the CryptoCell production-library APIs
@brief Contains the CryptoCell production-library-API error definitions.

@{
@ingroup prod
*/

#ifndef _PROD_ERROR_H
#define _PROD_ERROR_H


#include "cc_error.h"

#ifdef __cplusplus
extern "C"
{
#endif


/************************ Defines ******************************/

/* The error base address of the production library module of CryptoCell - 0x00F02A00 */
/*! Library initialization failure. */
#define CC_PROD_INIT_ERR                        (CC_PROD_MODULE_ERROR_BASE + 0x01UL)
/*! Illegal parameter. */
#define CC_PROD_INVALID_PARAM_ERR               (CC_PROD_MODULE_ERROR_BASE + 0x02UL)
/*! Invalid number of zeroes calculated. */
#define CC_PROD_ILLEGAL_ZERO_COUNT_ERR          (CC_PROD_MODULE_ERROR_BASE + 0x03UL)
/*! LCS is invalid for the operation. */
#define CC_PROD_ILLEGAL_LCS_ERR                 (CC_PROD_MODULE_ERROR_BASE + 0x04UL)
/*! Invalid asset-package fields. */
#define CC_PROD_ASSET_PKG_PARAM_ERR             (CC_PROD_MODULE_ERROR_BASE + 0x05UL)
/*! Failed to validate the asset package. */
#define CC_PROD_ASSET_PKG_VERIFY_ERR            (CC_PROD_MODULE_ERROR_BASE + 0x06UL)
/*! HAL Fatal error occured. */
#define CC_PROD_HAL_FATAL_ERR                   (CC_PROD_MODULE_ERROR_BASE + 0x07UL)

#endif

/**
@}
*/

