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


#ifndef _COMMON_CERT_PARSER_H
#define _COMMON_CERT_PARSER_H

#include "secdebug_defs.h"

/* IMPORATNT NOTE:  The certificate body may not be word aligned.
   For proprietary it is aligned but for x.509 we can not guarantee that */
typedef struct {
        CCSbCertTypes_t certType;
        CCSbCertHeader_t certHeader;
        uint8_t     *pCertBody;
        uint32_t certBodySize;
} CertFieldsInfo_t;

typedef struct {
        uint32_t   *pBuffer;
        uint32_t   bufferSize;
} BufferInfo32_t;

CCError_t CCCertSecDbgParse(uint32_t   *pDebugCertPkg,
                            uint32_t   certPkgSize,
                            BufferInfo32_t  *pKeyCert,       // out
                            BufferInfo32_t  *pEnablerCert,   // out
                            BufferInfo32_t  *pDeveloperCert); // out

uint32_t CCCertLoadCertificate(CCSbFlashReadFunc flashRead_func,
                               void *userContext,
                               CCAddr_t certAddress,
                               uint32_t *pCert,
                               uint32_t *pCertBufferWordSize);

/**
   @brief This function copy N, Np (CCSbNParams_t) and signature
   (certificate start address + sizeof certificate in certificate header) from the certificate to workspace.
   Return pointer to certificate header CCSbCertHeader_t, and pointer to cert body sizeof()

 */
CCError_t CCCertFieldsParse(BufferInfo32_t  *pCertInfo,
                            BufferInfo32_t  *pWorkspaceInfo,
                            CertFieldsInfo_t  *pCertFields,
                            uint32_t    **ppCertStartSign,
                            uint32_t    *pCertSignedSize,
                            BufferInfo32_t  *pX509HeaderInfo);


#endif


