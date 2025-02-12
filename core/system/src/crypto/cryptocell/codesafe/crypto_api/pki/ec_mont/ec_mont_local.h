/****************************************************************************
* The confidential and proprietary information contained in this file may    *
* only be used by a person authorised under and to the extent permitted      *
* by a subsisting licensing agreement from ARM Limited or its affiliates.    *
*   (C) COPYRIGHT [2001-2017] ARM Limited or its affiliates.         *
*       ALL RIGHTS RESERVED                          *
* This entire notice must be reproduced on all copies of this file           *
* and copies of this file may only be made by a person if such person is     *
* permitted to do so under the terms of a subsisting license agreement       *
* from ARM Limited or its affiliates.                        *
*****************************************************************************/


#ifndef EC_MONT_LOCAL_H
#define EC_MONT_LOCAL_H

#include "cc_pal_types.h"
#include "cc_pka_defs_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
@file
@brief This file contains the CryptoCell APIs used for EC MONT (Montgomery Curve25519) algorithms.

@note  Algorithms of Montgomery and Edwards elliptic curves cryptography are developed by
       Daniel.J.Bernstein and described in SW library "NaCl" (Networking and
       Cryptographic Library).
*/


/******************************************************************************/
/**          EC Montgomery domain APIs:                                       */
/******************************************************************************/

/*!< EC Montgomery curve domain structure type:
     Elliptic curve over prime fild GFp: y^2 = x^3 + Ax^2 + x */
typedef struct {
        /*!< EC prime modulus P */
        uint32_t ecModP[CC_EC_MONT_EDW_MODULUS_MAX_SIZE_IN_WORDS];
        /*!< modulus size in bits */
        uint32_t ecModSizeInBits;
        /*!< modulus size in words */
        uint32_t ecModSizeInWords;
        /*!< EC generator coordinates X */
        uint32_t ecGenX[CC_EC_MONT_EDW_MODULUS_MAX_SIZE_IN_WORDS];
        /*!< EC generator coordinates Y */
        uint32_t ecGenY[CC_EC_MONT_EDW_MODULUS_MAX_SIZE_IN_WORDS];
        /*!< EC generator order.  */
        uint32_t  ecOrdN[CC_EC_MONT_EDW_MODULUS_MAX_SIZE_IN_WORDS];
        /*!< EC generator order size in bits */
        uint32_t ecOrdSizeInBits;
        /*!< EC generator order size in words */
        uint32_t ecOrdSizeInWords;
        /*!< EC generator order's cofactor */
        uint32_t ecOrdCofactor;
        /*!< EC equation parameter; (A+2)/4 - for Curve25519 */
        uint32_t ecParam[CC_EC_MONT_EDW_MODULUS_MAX_SIZE_IN_WORDS];
        /*!< Barrett tags for EC modulus */
        uint32_t ecModBarrTag[CC_PKA_BARRETT_MOD_TAG_BUFF_SIZE_IN_WORDS];
        /*!< Barrett tags for EC generator order */
        uint32_t ecOrdBarrTag[CC_PKA_BARRETT_MOD_TAG_BUFF_SIZE_IN_WORDS];
        /*!< EC Domain ID - enum */
         CCEcMontDomainId_t domainId;
        /*!< EC Domain name */
        int8_t  name[20];
        /* parameters for bits setting in scalar multiplication LS/MS words */
        uint8_t scalarLsByteAndValue;
        uint8_t scalarMsByteAndValue;
        uint8_t scalarMsByteOrValue;
} CCEcMontDomain_t;



/*!<
 @brief    the function returns the domain pointer if the domain is supported for the product,
       otherwise return NULL
 @return   return domain pointer or NULL

*/
const CCEcMontDomain_t *EcMontGetCurve25519Domain(void);


#ifdef __cplusplus
}
#endif

#endif



