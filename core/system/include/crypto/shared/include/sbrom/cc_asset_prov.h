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

#ifndef  _CC_ASSET_PROV_H
#define  _CC_ASSET_PROV_H

/*!
@file
@brief This file contains the functions and definitions for the OEM Asset provisioning.
*/

#ifdef __cplusplus
extern "C"
{
#endif
#include "cc_bitops.h"

#define ASSET_PROV_TOKEN    0x41736574UL
#define ASSET_PROV_VERSION      0x10000UL

#define KPICV_KEY_SIZE      16
#define KPROV_KEY_SIZE      16
#define KPROV_DATA_IN_SIZE  8
#define ASSET_NONCE_SIZE    12
#define ASSET_RESERVED_SIZE     8
#define ASSET_RESERVED_WORD_SIZE    (8/CC_32BIT_WORD_SIZE)
#define ASSET_TAG_SIZE      16
#define ASSET_BLOCK_SIZE    16
#define MAX_ASSET_SIZE      512
#define ASSET_ADATA_SIZE    (3*CC_32BIT_WORD_SIZE+ASSET_RESERVED_SIZE)  // token||version||assetId||reserved

typedef struct {
        uint32_t  token;
        uint32_t  version;
        uint32_t  assetSize;
        uint32_t  reserved[ASSET_RESERVED_WORD_SIZE];
        uint8_t   nonce[ASSET_NONCE_SIZE];
        uint8_t   encAsset[MAX_ASSET_SIZE+ASSET_TAG_SIZE];
}CCBsvAssetProv_t;


#ifdef __cplusplus
}
#endif

#endif /*_CC_ASSET_PROV_H */
