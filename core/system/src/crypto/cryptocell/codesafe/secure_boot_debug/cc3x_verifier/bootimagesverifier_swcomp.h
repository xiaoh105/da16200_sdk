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




#ifndef _BOOT_IMAGES_VERIFIER_SWCOMP_H
#define _BOOT_IMAGES_VERIFIER_SWCOMP_H


#ifdef __cplusplus
extern "C"
{
#endif

/*----------------------------
      PUBLIC FUNCTIONS
-----------------------------------*/

/************************ Public Functions ******************************/
/**
 * @brief This function is responsible to verification of the SW components.
 *        The function will go over the SW components load each component,
 *        compute its HASH and compare it with the HASH saved in the certificate.
 *
 *
 * @param[in] flashRead_func - User's Flash read function used to read data from the flash to memory location
 * @param[in] userContext - User's context for the usage of flashRead_func
 * @param[in] hwBaseAddress - base address for the ARM CryptoCell HW engines
 * @param[in] pSwImagesData - s/w comps data, pointers to certificate locations of the s/w comps HASH data
 * @param[in] pSwImagesAddData - s/w comps additional data, pointers to certificate locations of the s/w comps additional data
 * @param[in] workspace_ptr - temporary buffer to load the SW components to (SW components without
 *            loading address).
 * @param[in] workspaceSize - the temporary buffer size in bytes
 *
 * @return CCError_t - On success the value CC_OK is returned,
 *         on failure - a value from BootImagesVerifier_error.h
 */
CCError_t CCCertValidateSWComps(CCSbFlashReadFunc flashRead_func,
                  void *userContext,
                  unsigned long hwBaseAddress,
                  CCSbPubKeyIndexType_t keyIndex,
                  CCSbCertParserSwCompsInfo_t *pSwImagesData,
                  uint32_t *pSwImagesAddData,
                  uint32_t *workspace_ptr,
                  uint32_t workspaceSize);




#ifdef __cplusplus
}
#endif

#endif


