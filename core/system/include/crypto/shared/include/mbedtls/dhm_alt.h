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

#ifndef MBEDTLS_DHM_ALT_H
#define MBEDTLS_DHM_ALT_H

#if defined(MBEDTLS_DHM_ALT)


#if defined(MBEDTLS_CONFIG_FILE)
#include MBEDTLS_CONFIG_FILE
#endif

#include <stddef.h>

/*
 * DHM Error codes
 */
#define MBEDTLS_ERR_DHM_BAD_INPUT_DATA                    -0x3080  /**< Bad input parameters. */
#define MBEDTLS_ERR_DHM_READ_PARAMS_FAILED                -0x3100  /**< Reading of the DHM parameters failed. */
#define MBEDTLS_ERR_DHM_MAKE_PARAMS_FAILED                -0x3180  /**< Making of the DHM parameters failed. */
#define MBEDTLS_ERR_DHM_READ_PUBLIC_FAILED                -0x3200  /**< Reading of the public values failed. */
#define MBEDTLS_ERR_DHM_MAKE_PUBLIC_FAILED                -0x3280  /**< Making of the public value failed. */
#define MBEDTLS_ERR_DHM_CALC_SECRET_FAILED                -0x3300  /**< Calculation of the DHM secret failed. */
#define MBEDTLS_ERR_DHM_INVALID_FORMAT                    -0x3380  /**< The ASN.1 data is not formatted correctly. */
#define MBEDTLS_ERR_DHM_ALLOC_FAILED                      -0x3400  /**< Allocation of memory failed. */
#define MBEDTLS_ERR_DHM_FILE_IO_ERROR                     -0x3480  /**< Read or write of file failed. */
#define MBEDTLS_ERR_DHM_HW_ACCEL_FAILED                   -0x3500  /**< DHM hardware accelerator failed. */
#define MBEDTLS_ERR_DHM_SET_GROUP_FAILED                  -0x3580  /**< Setting the modulus and generator failed. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          The DHM context structure.
 */
typedef struct
{
    size_t len;         /*!<  The size of \p P in Bytes. */
    mbedtls_mpi P;      /*!<  The prime modulus. */
    mbedtls_mpi G;      /*!<  The generator. */
    mbedtls_mpi X;      /*!<  Our secret value. */
    mbedtls_mpi GX;     /*!<  Our public key = \c G^X mod \c P. */
    mbedtls_mpi GY;     /*!<  The public key of the peer = \c G^Y mod \c P. */
    mbedtls_mpi K;      /*!<  The shared secret = \c G^(XY) mod \c P. */
    mbedtls_mpi RP;     /*!<  The cached value = \c R^2 mod \c P. */
    mbedtls_mpi Vi;     /*!<  The blinding value. */
    mbedtls_mpi Vf;     /*!<  The unblinding value. */
    mbedtls_mpi pX;     /*!<  The previous \c X. */
}
mbedtls_dhm_context;

/**
 * \brief          This function initializes the DHM context.
 *
 * \param ctx      The DHM context to initialize.
 */
void mbedtls_dhm_init( mbedtls_dhm_context *ctx );

/**
 * \brief          This function parses the ServerKeyExchange parameters.
 *
 * \param ctx      The DHM context.
 * \param p        On input, *p must be the start of the input buffer.
 *                 On output, *p is updated to point to the end of the data
 *                 that has been read. On success, this is the first byte
 *                 past the end of the ServerKeyExchange parameters.
 *                 On error, this is the point at which an error has been
 *                 detected, which is usually not useful except to debug
 *                 failures.
 * \param end      The end of the input buffer.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_dhm_read_params( mbedtls_dhm_context *ctx,
                     unsigned char **p,
                     const unsigned char *end );

/**
 * \brief          This function sets up and writes the ServerKeyExchange
 *                 parameters.
 *
 * \param ctx      The DHM context.
 * \param x_size   The private value size in Bytes.
 * \param olen     The number of characters written.
 * \param output   The destination buffer.
 * \param f_rng    The RNG function.
 * \param p_rng    The RNG parameter.
 *
 * \note           The destination buffer must be large enough to hold
 *                 the reduced binary presentation of the modulus, the generator
 *                 and the public key, each wrapped with a 2-byte length field.
 *                 It is the responsibility of the caller to ensure that enough
 *                 space is available. Refer to \c mbedtls_mpi_size to computing
 *                 the byte-size of an MPI.
 *
 * \note           This function assumes that \c ctx->P and \c ctx->G
 *                 have already been properly set. For that, use
 *                 mbedtls_dhm_set_group() below in conjunction with
 *                 mbedtls_mpi_read_binary() and mbedtls_mpi_read_string().
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_dhm_make_params( mbedtls_dhm_context *ctx, int x_size,
                     unsigned char *output, size_t *olen,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng );

/**
 * \brief          Set prime modulus and generator
 *
 * \param ctx      The DHM context.
 * \param P        The MPI holding DHM prime modulus.
 * \param G        The MPI holding DHM generator.
 *
 * \note           This function can be used to set P, G
 *                 in preparation for \c mbedtls_dhm_make_params.
 *
 * \return         \c 0 if successful, or an \c MBEDTLS_ERR_DHM_XXX error code
 *                 on failure.
 */
int mbedtls_dhm_set_group( mbedtls_dhm_context *ctx,
                           const mbedtls_mpi *P,
                           const mbedtls_mpi *G );

/**
 * \brief          This function imports the public value G^Y of the peer.
 *
 * \param ctx      The DHM context.
 * \param input    The input buffer.
 * \param ilen     The size of the input buffer.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_dhm_read_public( mbedtls_dhm_context *ctx,
                     const unsigned char *input, size_t ilen );

/**
 * \brief          This function creates its own private value \c X and
 *                 exports \c G^X.
 *
 * \param ctx      The DHM context.
 * \param x_size   The private value size in Bytes.
 * \param output   The destination buffer.
 * \param olen     The length of the destination buffer. Must be at least
                   equal to ctx->len (the size of \c P).
 * \param f_rng    The RNG function.
 * \param p_rng    The RNG parameter.
 *
 * \note           The destination buffer will always be fully written
 *                 so as to contain a big-endian presentation of G^X mod P.
 *                 If it is larger than ctx->len, it will accordingly be
 *                 padded with zero-bytes in the beginning.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_dhm_make_public( mbedtls_dhm_context *ctx, int x_size,
                     unsigned char *output, size_t olen,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng );

/**
 * \brief               This function derives and exports the shared secret
 *                      \c (G^Y)^X mod \c P.
 *
 * \param ctx           The DHM context.
 * \param output        The destination buffer.
 * \param output_size   The size of the destination buffer. Must be at least
 *                      the size of ctx->len.
 * \param olen          On exit, holds the actual number of Bytes written.
 * \param f_rng         The RNG function, for blinding purposes.
 * \param p_rng         The RNG parameter.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           If non-NULL, \p f_rng is used to blind the input as
 *                 a countermeasure against timing attacks. Blinding is used
 *                 only if our secret value \p X is re-used and omitted
 *                 otherwise. Therefore, we recommend always passing a
 *                 non-NULL \p f_rng argument.
 */
int mbedtls_dhm_calc_secret( mbedtls_dhm_context *ctx,
                     unsigned char *output, size_t output_size, size_t *olen,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng );

/**
 * \brief          This function frees and clears the components of a DHM key.
 *
 * \param ctx      The DHM context to free and clear.
 */
void mbedtls_dhm_free( mbedtls_dhm_context *ctx );

#if defined(MBEDTLS_ASN1_PARSE_C)
/** \ingroup x509_module */
/**
 * \brief             This function parses DHM parameters in PEM or DER format.
 *
 * \param dhm         The DHM context to initialize.
 * \param dhmin       The input buffer.
 * \param dhminlen    The size of the buffer, including the terminating null
 *                    Byte for PEM data.
 *
 * \return            \c 0 on success, or a specific DHM or PEM error code
 *                    on failure.
 */
int mbedtls_dhm_parse_dhm( mbedtls_dhm_context *dhm, const unsigned char *dhmin,
                   size_t dhminlen );

#if defined(MBEDTLS_FS_IO)
/** \ingroup x509_module */
/**
 * \brief          This function loads and parses DHM parameters from a file.
 *
 * \param dhm      The DHM context to load the parameters to.
 * \param path     The filename to read the DHM parameters from.
 *
 * \return         \c 0 on success, or a specific DHM or PEM error code
 *                 on failure.
 */
int mbedtls_dhm_parse_dhmfile( mbedtls_dhm_context *dhm, const char *path );
#endif /* MBEDTLS_FS_IO */
#endif /* MBEDTLS_ASN1_PARSE_C */

/**
 * \brief          Checkup routine
 *
 * \return         \c 0 on success, or \c 1 on failure.
 */
int mbedtls_dhm_self_test( int verbose );

#ifdef __cplusplus
}
#endif
#endif  /* MBEDTLS_DHM_ALT  - use alternative code */
#endif  /* MBEDTLS_DHM_ALT_H  - include only once  */
