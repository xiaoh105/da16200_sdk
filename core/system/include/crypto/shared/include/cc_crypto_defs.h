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

/** \file
 * \brief This file contains common cryptographic definitions.
 *
 */

#ifndef _CC_CRYPTO_DEFS_H
#define _CC_CRYPTO_DEFS_H


#ifdef __cplusplus
extern "C"
{
#endif


/************************ Hash Definitions ******************************/

#define HASH_MD5_DIGEST_SIZE_IN_BYTES       16
#define HASH_SHA1_DIGEST_SIZE_IN_BYTES      20
#define HASH_SHA224_DIGEST_SIZE_IN_BYTES    28
#define HASH_SHA256_DIGEST_SIZE_IN_BYTES    32
#define HASH_SHA384_DIGEST_SIZE_IN_BYTES    48
#define HASH_SHA512_DIGEST_SIZE_IN_BYTES    64

#define HASH_MD5_BLOCK_SIZE_IN_BYTES        64
#define HASH_SHA1_BLOCK_SIZE_IN_BYTES       64
#define HASH_SHA224_BLOCK_SIZE_IN_BYTES     64
#define HASH_SHA256_BLOCK_SIZE_IN_BYTES     64
#define HASH_SHA384_BLOCK_SIZE_IN_BYTES     128
#define HASH_SHA512_BLOCK_SIZE_IN_BYTES     128



/************************ AES Definitions ******************************/

#define AES_BLOCK_SIZE_IN_BYTES     16

#define AES_IV_SIZE_IN_BYTES        AES_BLOCK_SIZE_IN_BYTES


/* AES-CCM Definitions */
#define AES_CCM_NONCE_LENGTH_MIN    7
#define AES_CCM_NONCE_LENGTH_MAX    13

#define AES_CCM_TAG_LENGTH_MIN      4
#define AES_CCM_TAG_LENGTH_MAX      16



/************************ DES Definitions ******************************/

#define DES_IV_SIZE_IN_BYTES        8






#ifdef __cplusplus
}
#endif

#endif

