/****************************************************************************
* The confidential and proprietary information contained in this file may   *
* only be used by a person authorised under and to the extent permitted     *
* by a subsisting licensing agreement from Arm Limited (or its affiliates). *
*     (C) COPYRIGHT [2017-2018] Arm Limited (or its affiliates).            *
*         ALL RIGHTS RESERVED                                               *
* This entire notice must be reproduced on all copies of this file          *
* and copies of this file may only be made by a person if such person is    *
* permitted to do so under the terms of a subsisting license agreement      *
* from Arm Limited (or its affiliates).                                     *
*****************************************************************************/

/*!
@file
@brief This file contains the type definitions that are used by the CryptoCell AES APIs.

@defgroup cc_aes_defs Definitions of CryptoCell AES APIs
@brief Contains CryptoCell AES API type definitions.

@{
@ingroup cc_aes
*/

#ifndef CC_AES_DEFS_H
#define CC_AES_DEFS_H

#include "cc_pal_types.h"
#include "cc_aes_defs_proj.h"


#ifdef __cplusplus
extern "C"
{
#endif


/************************ Defines  ******************************/
/*! The size of the AES block in words. */
#define CC_AES_CRYPTO_BLOCK_SIZE_IN_WORDS 4
/*! The size of the AES block in Bytes. */
#define CC_AES_BLOCK_SIZE_IN_BYTES  (CC_AES_CRYPTO_BLOCK_SIZE_IN_WORDS * sizeof(uint32_t))

/*! The size of the IV buffer in words. */
#define CC_AES_IV_SIZE_IN_WORDS   CC_AES_CRYPTO_BLOCK_SIZE_IN_WORDS
/*! The size of the IV buffer in Bytes. */
#define CC_AES_IV_SIZE_IN_BYTES  (CC_AES_IV_SIZE_IN_WORDS * sizeof(uint32_t))


/************************ Enums ********************************/
/*! The AES operation:<ul><li>Encrypt</li><li>Decrypt</li></ul>. */
typedef enum {
    /*! An AES encrypt operation. */
    CC_AES_ENCRYPT = 0,
    /*! An AES decrypt operation. */
    CC_AES_DECRYPT = 1,
    /*! The maximal number of operations. */
    CC_AES_NUM_OF_ENCRYPT_MODES,
    /*! Reserved. */
    CC_AES_ENCRYPT_MODE_LAST = 0x7FFFFFFF
}CCAesEncryptMode_t;

/*! The AES operation mode. */
typedef enum {
    CC_AES_MODE_ECB          = 0,       /*!< ECB mode. */
    CC_AES_MODE_CBC          = 1,       /*!< CBC mode. */
    CC_AES_MODE_CBC_MAC      = 2,       /*!< CBC-MAC mode. */
    CC_AES_MODE_CTR          = 3,       /*!< CTR mode. */
    CC_AES_MODE_XCBC_MAC     = 4,       /*!< XCBC-MAC mode. */
    CC_AES_MODE_CMAC         = 5,       /*!< CMAC mode. */
    CC_AES_MODE_XTS          = 6,       /*!< XTS mode. */
    CC_AES_MODE_CBC_CTS      = 7,       /*!< CBC-CTS mode. */
    CC_AES_MODE_OFB          = 8,       /*!< OFB mode. */

    /*! The maximal number of AES modes. */
    CC_AES_NUM_OF_OPERATION_MODES,
    /*! Reserved. */
    CC_AES_OPERATION_MODE_LAST = 0x7FFFFFFF
}CCAesOperationMode_t;

/*! The AES padding type. */
typedef enum {
       CC_AES_PADDING_NONE  = 0,        /*!< No padding. */
       CC_AES_PADDING_PKCS7 = 1,        /*!< PKCS7 padding. */

       /*! The maximal number of AES padding modes. */
       CC_AES_NUM_OF_PADDING_TYPES,
       /*! Reserved. */
       CC_AES_PADDING_TYPE_LAST = 0x7FFFFFFF
}CCAesPaddingType_t;

/*! The AES key type. */
typedef enum {
    CC_AES_USER_KEY          = 0,       /*!< The user key. */
    CC_AES_PLATFORM_KEY      = 1,       /*!< The Kplt hardware key. */
    CC_AES_CUSTOMER_KEY      = 2,       /*!< The Kcst hardware key. */

    /*! The maximal number of AES key types. */
    CC_AES_NUM_OF_KEY_TYPES,
    /*! Reserved. */
    CC_AES_KEY_TYPE_LAST = 0x7FFFFFFF
}CCAesKeyType_t;

/************************ Typedefs  ****************************/

/*! Defines the IV buffer. A 16-Byte array. */
typedef uint8_t CCAesIv_t[CC_AES_IV_SIZE_IN_BYTES];

/*! Defines the AES key data buffer. */
typedef uint8_t CCAesKeyBuffer_t[CC_AES_KEY_MAX_SIZE_IN_BYTES];

/************************ Structs  ******************************/

/*!
@brief The context prototype of the user.

The argument type that is passed by the user to the AES APIs. The context
saves the state of the operation, and must be saved by the user
till the end of the API flow. */
typedef struct CCAesUserContext_t {
    /*! The context buffer for internal usage. */
    uint32_t buff[CC_AES_USER_CTX_SIZE_IN_WORDS];
}CCAesUserContext_t;


/*! The AES key data of the user. */
typedef struct CCAesUserKeyData_t {
    uint8_t * pKey;     /*!< A pointer to the key. */
    size_t    keySize;  /*!< The size of the key in Bytes. Valid values:
                      <ul><li>For XTS mode (if supportes): 32 Bytes or 64 Bytes, indicating
                      the full size of the double key (2x128 or 2x256 bit).</li>
                      <li>For XCBC-MAC mode: 16 Bytes, as limited by the standard.</li>
                      <li>For all other modes: 16 Bytes, 24 Bytes or 32 Bytes.</li></ul> */
}CCAesUserKeyData_t;

/*! The AES HW key Data. */
typedef struct CCAesHwKeyData_t {
    size_t slotNumber;      /*!< Slot number. */
}CCAesHwKeyData_t;

#endif /* CC_AES_DEFS_H */

/**
@}
*/

