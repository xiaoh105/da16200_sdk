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
#ifndef ECP_COMMON_H
#define ECP_COMMON_H
#ifdef __cplusplus
extern "C"
{
#endif


#include "cc_pal_types_plat.h"
#include "cc_ecpki_types.h"
#include "mbedtls/ecp.h"

/*
 *\brief         Curve types
 *
 */
typedef enum
{
    ECP_TYPE_NONE = 0,
    ECP_TYPE_SHORT_WEIERSTRASS,    /* y^2 = x^3 + a x + b      */
    ECP_TYPE_25519,         /* MONTGOMERY : y^2 = x^3 + a x^2 + x  EDWARDS: x^2 + y^2 = 1 + dx^2y^2 (modp) */
} ecp_curve_type;

/**
 * \brief           mapping CC ECP return codes to mbedtls
 *
 */
int error_mapping_cc_to_mbedtls_ecc (CCError_t cc_error);

/**
 * \brief           get the cfurve type
 *
 */
static inline ecp_curve_type ecp_get_type(const mbedtls_ecp_group *grp)
{
    if (grp->G.X.p == NULL)
        return (ECP_TYPE_NONE);

    if (grp->G.Y.p == NULL)
        return (ECP_TYPE_25519);
    else
        return (ECP_TYPE_SHORT_WEIERSTRASS);
}

/**
 * \brief           map mbedtls group id to CC domain id
 *
 */

int ecp_grp_id_to_domain_id (const mbedtls_ecp_group_id id, CCEcpkiDomainID_t *domain_id);

#ifdef __cplusplus
}
#endif

#endif  /* MBEDTLS_COMMON_H */
