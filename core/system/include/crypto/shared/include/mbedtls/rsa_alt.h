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

#ifndef MBEDTLS_RSA_ALT_H
#define MBEDTLS_RSA_ALT_H

#if defined(MBEDTLS_CONFIG_FILE)
#include MBEDTLS_CONFIG_FILE
#endif

#if defined (MBEDTLS_RSA_ALT)

// Regular implementation
//

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief   The RSA context structure.
 *
 * \note    Direct manipulation of the members of this structure
 *          is deprecated. All manipulation should instead be done through
 *          the public interface functions.
 */
typedef struct
{
    int ver;                    /*!<  always 0          */
    size_t len;                 /*!<  size(N) in chars  */

    mbedtls_mpi N;                      /*!<  public modulus    */
    mbedtls_mpi E;                      /*!<  public exponent   */

    mbedtls_mpi D;                      /*!<  private exponent  */
    mbedtls_mpi P;                      /*!<  1st prime factor  */
    mbedtls_mpi Q;                      /*!<  2nd prime factor  */

    mbedtls_mpi DP;                     /*!<  D % (P - 1)       */
    mbedtls_mpi DQ;                     /*!<  D % (Q - 1)       */
    mbedtls_mpi QP;                     /*!<  1 / (Q % P)       */

    mbedtls_mpi RN;                     /*!<  cached R^2 mod N  */

    mbedtls_mpi RP;                     /*!<  cached R^2 mod P  */
    mbedtls_mpi RQ;                     /*!<  cached R^2 mod Q  */

    mbedtls_mpi Vi;                     /*!<  cached blinding value     */
    mbedtls_mpi Vf;                     /*!<  cached un-blinding value  */

    mbedtls_mpi NP;                     /*!< Barrett mod N tag NP for N-modulus */
    mbedtls_mpi BQP;                    /*!< Barrett mod Q tag QP for Q-factor  */
    mbedtls_mpi BPP;                    /*!< Barrett mod P tag PP for P-factor  */

    int padding;                /*!<  MBEDTLS_RSA_PKCS_V15 for 1.5 padding and
                                      MBEDTLS_RSA_PKCS_v21 for OAEP/PSS         */
    int hash_id;                /*!<  Hash identifier of mbedtls_md_type_t as
                                      specified in the mbedtls_md.h header file
                                      for the EME-OAEP and EMSA-PSS
                                      encoding                          */
#if defined(MBEDTLS_THREADING_C)
    mbedtls_threading_mutex_t mutex;    /*!<  Thread-safety mutex       */
#endif
}
mbedtls_rsa_context;

/**
 * \brief          This function initializes an RSA context.
 *
 * \note           Set padding to #MBEDTLS_RSA_PKCS_V21 for the RSAES-OAEP
 *                 encryption scheme and the RSASSA-PSS signature scheme.
 *
 * \param ctx      The RSA context to initialize.
 * \param padding  Selects padding mode: #MBEDTLS_RSA_PKCS_V15 or
 *                 #MBEDTLS_RSA_PKCS_V21.
 * \param hash_id  The hash identifier of #mbedtls_md_type_t type, if
 *                 \p padding is #MBEDTLS_RSA_PKCS_V21.
 *
 * \note           The \p hash_id parameter is ignored when using
 *                 #MBEDTLS_RSA_PKCS_V15 padding.
 *
 * \note           The choice of padding mode is strictly enforced for private key
 *                 operations, since there might be security concerns in
 *                 mixing padding modes. For public key operations it is
 *                 a default value, which can be overriden by calling specific
 *                 \c rsa_rsaes_xxx or \c rsa_rsassa_xxx functions.
 *
 * \note           The hash selected in \p hash_id is always used for OEAP
 *                 encryption. For PSS signatures, it is always used for
 *                 making signatures, but can be overriden for verifying them.
 *                 If set to #MBEDTLS_MD_NONE, it is always overriden.
 */
void mbedtls_rsa_init( mbedtls_rsa_context *ctx,
                       int padding,
                       int hash_id);

/**
 * \brief          This function imports a set of core parameters into an
 *                 RSA context.
 *
 * \param ctx      The initialized RSA context to store the parameters in.
 * \param N        The RSA modulus, or NULL.
 * \param P        The first prime factor of \p N, or NULL.
 * \param Q        The second prime factor of \p N, or NULL.
 * \param D        The private exponent, or NULL.
 * \param E        The public exponent, or NULL.
 *
 * \note           This function can be called multiple times for successive
 *                 imports, if the parameters are not simultaneously present.
 *
 *                 Any sequence of calls to this function should be followed
 *                 by a call to mbedtls_rsa_complete(), which checks and
 *                 completes the provided information to a ready-for-use
 *                 public or private RSA key.
 *
 * \note           See mbedtls_rsa_complete() for more information on which
 *                 parameters are necessary to set up a private or public
 *                 RSA key.
 *
 * \note           The imported parameters are copied and need not be preserved
 *                 for the lifetime of the RSA context being set up.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_rsa_import( mbedtls_rsa_context *ctx,
                        const mbedtls_mpi *N,
                        const mbedtls_mpi *P, const mbedtls_mpi *Q,
                        const mbedtls_mpi *D, const mbedtls_mpi *E );

/**
 * \brief          This function imports core RSA parameters, in raw big-endian
 *                 binary format, into an RSA context.
 *
 * \param ctx      The initialized RSA context to store the parameters in.
 * \param N        The RSA modulus, or NULL.
 * \param N_len    The Byte length of \p N, ignored if \p N == NULL.
 * \param P        The first prime factor of \p N, or NULL.
 * \param P_len    The Byte length of \p P, ignored if \p P == NULL.
 * \param Q        The second prime factor of \p N, or NULL.
 * \param Q_len    The Byte length of \p Q, ignored if \p Q == NULL.
 * \param D        The private exponent, or NULL.
 * \param D_len    The Byte length of \p D, ignored if \p D == NULL.
 * \param E        The public exponent, or NULL.
 * \param E_len    The Byte length of \p E, ignored if \p E == NULL.
 *
 * \note           This function can be called multiple times for successive
 *                 imports, if the parameters are not simultaneously present.
 *
 *                 Any sequence of calls to this function should be followed
 *                 by a call to mbedtls_rsa_complete(), which checks and
 *                 completes the provided information to a ready-for-use
 *                 public or private RSA key.
 *
 * \note           See mbedtls_rsa_complete() for more information on which
 *                 parameters are necessary to set up a private or public
 *                 RSA key.
 *
 * \note           The imported parameters are copied and need not be preserved
 *                 for the lifetime of the RSA context being set up.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_rsa_import_raw( mbedtls_rsa_context *ctx,
                            unsigned char const *N, size_t N_len,
                            unsigned char const *P, size_t P_len,
                            unsigned char const *Q, size_t Q_len,
                            unsigned char const *D, size_t D_len,
                            unsigned char const *E, size_t E_len );

/**
 * \brief          This function completes an RSA context from
 *                 a set of imported core parameters.
 *
 *                 To setup an RSA public key, precisely \p N and \p E
 *                 must have been imported.
 *
 *                 To setup an RSA private key, sufficient information must
 *                 be present for the other parameters to be derivable.
 *
 *                 The default implementation supports the following:
 *                 <ul><li>Derive \p P, \p Q from \p N, \p D, \p E.</li>
 *                 <li>Derive \p N, \p D from \p P, \p Q, \p E.</li></ul>
 *                 Alternative implementations need not support these.
 *
 *                 If this function runs successfully, it guarantees that
 *                 the RSA context can be used for RSA operations without
 *                 the risk of failure or crash.
 *
 * \param ctx      The initialized RSA context holding imported parameters.
 *
 * \return         \c 0 on success, or #MBEDTLS_ERR_RSA_BAD_INPUT_DATA if the
 *                 attempted derivations failed.
 *
 * \warning        This function need not perform consistency checks
 *                 for the imported parameters. In particular, parameters that
 *                 are not needed by the implementation might be silently
 *                 discarded and left unchecked. To check the consistency
 *                 of the key material, see mbedtls_rsa_check_privkey().
 *
 */
int mbedtls_rsa_complete( mbedtls_rsa_context *ctx );

/**
 * \brief          This function exports the core parameters of an RSA key.
 *
 *                 If this function runs successfully, the non-NULL buffers
 *                 pointed to by \p N, \p P, \p Q, \p D, and \p E are fully
 *                 written, with additional unused space filled leading by
 *                 zero Bytes.
 *
 *                 Possible reasons for returning
 *                 #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION:<ul>
 *                 <li>An alternative RSA implementation is in use, which
 *                 stores the key externally, and either cannot or should
 *                 not export it into RAM.</li>
 *                 <li>A SW or HW implementation might not support a certain
 *                 deduction. For example, \p P, \p Q from \p N, \p D,
 *                 and \p E if the former are not part of the
 *                 implementation.</li></ul>
 *
 *                 If the function fails due to an unsupported operation,
 *                 the RSA context stays intact and remains usable.
 *
 * \param ctx      The initialized RSA context.
 * \param N        The MPI to hold the RSA modulus, or NULL.
 * \param P        The MPI to hold the first prime factor of \p N, or NULL.
 * \param Q        The MPI to hold the second prime factor of \p N, or NULL.
 * \param D        The MPI to hold the private exponent, or NULL.
 * \param E        The MPI to hold the public exponent, or NULL.
 *
 * \return         \c 0 on success,
 *                 #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION if exporting the
 *                 requested parameters cannot be done due to missing
 *                 functionality or because of security policies,
 *                 or a non-zero return code on any other failure.
 *
 */
int mbedtls_rsa_export( const mbedtls_rsa_context *ctx,
                        mbedtls_mpi *N, mbedtls_mpi *P, mbedtls_mpi *Q,
                        mbedtls_mpi *D, mbedtls_mpi *E );

/**
 * \brief          This function exports core parameters of an RSA key
 *                 in raw big-endian binary format.
 *
 *                 If this function runs successfully, the non-NULL buffers
 *                 pointed to by \p N, \p P, \p Q, \p D, and \p E are fully
 *                 written, with additional unused space filled leading by
 *                 zero Bytes.
 *
 *                 Possible reasons for returning
 *                 #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION:<ul>
 *                 <li>An alternative RSA implementation is in use, which
 *                 stores the key externally, and either cannot or should
 *                 not export it into RAM.</li>
 *                 <li>A SW or HW implementation might not support a certain
 *                 deduction. For example, \p P, \p Q from \p N, \p D,
 *                 and \p E if the former are not part of the
 *                 implementation.</li></ul>
 *                 If the function fails due to an unsupported operation,
 *                 the RSA context stays intact and remains usable.
 *
 * \param ctx      The initialized RSA context.
 * \param N        The Byte array to store the RSA modulus, or NULL.
 * \param N_len    The size of the buffer for the modulus.
 * \param P        The Byte array to hold the first prime factor of \p N, or
 *                 NULL.
 * \param P_len    The size of the buffer for the first prime factor.
 * \param Q        The Byte array to hold the second prime factor of \p N, or
                   NULL.
 * \param Q_len    The size of the buffer for the second prime factor.
 * \param D        The Byte array to hold the private exponent, or NULL.
 * \param D_len    The size of the buffer for the private exponent.
 * \param E        The Byte array to hold the public exponent, or NULL.
 * \param E_len    The size of the buffer for the public exponent.
 *
 * \note           The length fields are ignored if the corresponding
 *                 buffer pointers are NULL.
 *
 * \return         \c 0 on success,
 *                 #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION if exporting the
 *                 requested parameters cannot be done due to missing
 *                 functionality or because of security policies,
 *                 or a non-zero return code on any other failure.
 */
int mbedtls_rsa_export_raw( const mbedtls_rsa_context *ctx,
                            unsigned char *N, size_t N_len,
                            unsigned char *P, size_t P_len,
                            unsigned char *Q, size_t Q_len,
                            unsigned char *D, size_t D_len,
                            unsigned char *E, size_t E_len );

/**
 * \brief          This function exports CRT parameters of a private RSA key.
 *
 * \param ctx      The initialized RSA context.
 * \param DP       The MPI to hold D modulo P-1, or NULL.
 * \param DQ       The MPI to hold D modulo Q-1, or NULL.
 * \param QP       The MPI to hold modular inverse of Q modulo P, or NULL.
 *
 * \return         \c 0 on success, non-zero error code otherwise.
 *
 * \note           Alternative RSA implementations not using CRT-parameters
 *                 internally can implement this function based on
 *                 mbedtls_rsa_deduce_opt().
 *
 */
int mbedtls_rsa_export_crt( const mbedtls_rsa_context *ctx,
                            mbedtls_mpi *DP, mbedtls_mpi *DQ, mbedtls_mpi *QP );

/**
 * \brief          This function sets padding for an already initialized RSA
 *                 context. See mbedtls_rsa_init() for details.
 *
 * \param ctx      The RSA context to be set.
 * \param padding  Selects padding mode: #MBEDTLS_RSA_PKCS_V15 or
 *                 #MBEDTLS_RSA_PKCS_V21.
 * \param hash_id  The #MBEDTLS_RSA_PKCS_V21 hash identifier.
 */
void mbedtls_rsa_set_padding( mbedtls_rsa_context *ctx, int padding,
                              int hash_id);

/**
 * \brief          This function retrieves the length of RSA modulus in Bytes.
 *
 * \param ctx      The initialized RSA context.
 *
 * \return         The length of the RSA modulus in Bytes.
 *
 */
size_t mbedtls_rsa_get_len( const mbedtls_rsa_context *ctx );

/**
 * \brief          This function generates an RSA keypair.
 *
 * \param ctx      The RSA context used to hold the key.
 * \param f_rng    The RNG function.
 * \param p_rng    The RNG parameter.
 * \param nbits    The size of the public key in bits.
 * \param exponent The public exponent. For example, 65537.
 *
 * \note           mbedtls_rsa_init() must be called before this function,
 *                 to set up the RSA context.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_rsa_gen_key( mbedtls_rsa_context *ctx,
                         int (*f_rng)(void *, unsigned char *, size_t),
                         void *p_rng,
                         unsigned int nbits, int exponent );

/**
 * \brief          This function checks if a context contains at least an RSA
 *                 public key.
 *
 *                 If the function runs successfully, it is guaranteed that
 *                 enough information is present to perform an RSA public key
 *                 operation using mbedtls_rsa_public().
 *
 * \param ctx      The RSA context to check.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 */
int mbedtls_rsa_check_pubkey( const mbedtls_rsa_context *ctx );

/**
 * \brief      This function checks if a context contains an RSA private key
 *             and perform basic consistency checks.
 *
 * \param ctx  The RSA context to check.
 *
 * \return     \c 0 on success, or a non-zero error code on failure.
 *
 * \note       The consistency checks performed by this function not only
 *             ensure that mbedtls_rsa_private() can be called successfully
 *             on the given context, but that the various parameters are
 *             mutually consistent with high probability, in the sense that
 *             mbedtls_rsa_public() and mbedtls_rsa_private() are inverses.
 *
 * \warning    This function should catch accidental misconfigurations
 *             like swapping of parameters, but it cannot establish full
 *             trust in neither the quality nor the consistency of the key
 *             material that was used to setup the given RSA context:
 *             <ul><li>Consistency: Imported parameters that are irrelevant
 *             for the implementation might be silently dropped. If dropped,
 *             the current function does not have access to them,
 *             and therefore cannot check them. See mbedtls_rsa_complete().
 *             If you want to check the consistency of the entire
 *             content of an PKCS1-encoded RSA private key, for example, you
 *             should use mbedtls_rsa_validate_params() before setting
 *             up the RSA context.
 *             Additionally, if the implementation performs empirical checks,
 *             these checks substantiate but do not guarantee consistency.</li>
 *             <li>Quality: This function is not expected to perform
 *             extended quality assessments like checking that the prime
 *             factors are safe. Additionally, it is the responsibility of the
 *             user to ensure the trustworthiness of the source of his RSA
 *             parameters, which goes beyond what is effectively checkable
 *             by the library.</li></ul>
 */
int mbedtls_rsa_check_privkey( const mbedtls_rsa_context *ctx );

/**
 * \brief          This function checks a public-private RSA key pair.
 *
 *                 It checks each of the contexts, and makes sure they match.
 *
 * \param pub      The RSA context holding the public key.
 * \param prv      The RSA context holding the private key.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 */
int mbedtls_rsa_check_pub_priv( const mbedtls_rsa_context *pub,
                                const mbedtls_rsa_context *prv );

/**
 * \brief          This function performs an RSA public key operation.
 *
 * \param ctx      The RSA context.
 * \param input    The input buffer.
 * \param output   The output buffer.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           This function does not handle message padding.
 *
 * \note           Make sure to set \p input[0] = 0 or ensure that
 *                 input is smaller than \p N.
 *
 * \note           The input and output buffers must be large
 *                 enough. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_public( mbedtls_rsa_context *ctx,
                const unsigned char *input,
                unsigned char *output );

/**
 * \brief          This function performs an RSA private key operation.
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Needed for blinding.
 * \param p_rng    The RNG parameter.
 * \param input    The input buffer.
 * \param output   The output buffer.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The input and output buffers must be large
 *                 enough. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_private( mbedtls_rsa_context *ctx,
                 int (*f_rng)(void *, unsigned char *, size_t),
                 void *p_rng,
                 const unsigned char *input,
                 unsigned char *output );

/**
 * \brief          This function adds the message padding, then performs an RSA
 *                 operation.
 *
 *                 It is the generic wrapper for performing a PKCS#1 encryption
 *                 operation using the \p mode from the context.
 *
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Needed for padding, PKCS#1 v2.1
 *                 encoding, and #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param ilen     The length of the plaintext.
 * \param input    The buffer holding the data to encrypt.
 * \param output   The buffer used to hold the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PRIVATE mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PUBLIC.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PRIVATE and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The input and output buffers must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_pkcs1_encrypt( mbedtls_rsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng,
                       int mode, size_t ilen,
                       const unsigned char *input,
                       unsigned char *output );

/**
 * \brief          This function performs a PKCS#1 v1.5 encryption operation
 *                 (RSAES-PKCS1-v1_5-ENCRYPT).
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Needed for padding and
 *                 #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param ilen     The length of the plaintext.
 * \param input    The buffer holding the data to encrypt.
 * \param output   The buffer used to hold the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PRIVATE mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PUBLIC.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PRIVATE and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The output buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_rsaes_pkcs1_v15_encrypt( mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode, size_t ilen,
                                 const unsigned char *input,
                                 unsigned char *output );

/**
 * \brief            This function performs a PKCS#1 v2.1 OAEP encryption
 *                   operation (RSAES-OAEP-ENCRYPT).
 *
 * \param ctx        The RSA context.
 * \param f_rng      The RNG function. Needed for padding and PKCS#1 v2.1
 *                   encoding and #MBEDTLS_RSA_PRIVATE.
 * \param p_rng      The RNG parameter.
 * \param mode       #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param label      The buffer holding the custom label to use.
 * \param label_len  The length of the label.
 * \param ilen       The length of the plaintext.
 * \param input      The buffer holding the data to encrypt.
 * \param output     The buffer used to hold the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PRIVATE mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PUBLIC.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PRIVATE and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The output buffer must be as large as the size
 *                 of ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_rsaes_oaep_encrypt( mbedtls_rsa_context *ctx,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng,
                            int mode,
                            const unsigned char *label, size_t label_len,
                            size_t ilen,
                            const unsigned char *input,
                            unsigned char *output );

/**
 * \brief          This function performs an RSA operation, then removes the
 *                 message padding.
 *
 *                 It is the generic wrapper for performing a PKCS#1 decryption
 *                 operation using the \p mode from the context.
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param olen     The length of the plaintext.
 * \param input    The buffer holding the encrypted data.
 * \param output   The buffer used to hold the plaintext.
 * \param output_max_len    The maximum length of the output buffer.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PUBLIC mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PRIVATE.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PUBLIC and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size \p ctx->len of \p ctx->N (for example,
 *                 128 Bytes if RSA-1024 is used) to be able to hold an
 *                 arbitrary decrypted message. If it is not large enough to
 *                 hold the decryption of the particular ciphertext provided,
 *                 the function returns \c MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \note           The input buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_pkcs1_decrypt( mbedtls_rsa_context *ctx,
                       int (*f_rng)(void *, unsigned char *, size_t),
                       void *p_rng,
                       int mode, size_t *olen,
                       const unsigned char *input,
                       unsigned char *output,
                       size_t output_max_len );

/**
 * \brief          This function performs a PKCS#1 v1.5 decryption
 *                 operation (RSAES-PKCS1-v1_5-DECRYPT).
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param olen     The length of the plaintext.
 * \param input    The buffer holding the encrypted data.
 * \param output   The buffer to hold the plaintext.
 * \param output_max_len    The maximum length of the output buffer.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PUBLIC mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PRIVATE.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PUBLIC and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size \p ctx->len of \p ctx->N, for example,
 *                 128 Bytes if RSA-1024 is used, to be able to hold an
 *                 arbitrary decrypted message. If it is not large enough to
 *                 hold the decryption of the particular ciphertext provided,
 *                 the function returns #MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \note           The input buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_rsaes_pkcs1_v15_decrypt( mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode, size_t *olen,
                                 const unsigned char *input,
                                 unsigned char *output,
                                 size_t output_max_len );

/**
 * \brief          This function performs a PKCS#1 v2.1 OAEP decryption
 *                 operation (RSAES-OAEP-DECRYPT).
 *
 * \param ctx        The RSA context.
 * \param f_rng      The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng      The RNG parameter.
 * \param mode       #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param label      The buffer holding the custom label to use.
 * \param label_len  The length of the label.
 * \param olen       The length of the plaintext.
 * \param input      The buffer holding the encrypted data.
 * \param output     The buffer to hold the plaintext.
 * \param output_max_len    The maximum length of the output buffer.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PUBLIC mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PRIVATE.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PUBLIC and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 on success, or a non-zero error code on failure.
 *
 * \note           The output buffer length \c output_max_len should be
 *                 as large as the size \p ctx->len of \p ctx->N, for
 *                 example, 128 Bytes if RSA-1024 is used, to be able to
 *                 hold an arbitrary decrypted message. If it is not
 *                 large enough to hold the decryption of the particular
 *                 ciphertext provided, the function returns
 *                 #MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE.
 *
 * \note           The input buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_rsaes_oaep_decrypt( mbedtls_rsa_context *ctx,
                            int (*f_rng)(void *, unsigned char *, size_t),
                            void *p_rng,
                            int mode,
                            const unsigned char *label, size_t label_len,
                            size_t *olen,
                            const unsigned char *input,
                            unsigned char *output,
                            size_t output_max_len );

/**
 * \brief          This function performs a private RSA operation to sign
 *                 a message digest using PKCS#1.
 *
 *                 It is the generic wrapper for performing a PKCS#1
 *                 signature using the \p mode from the context.
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Needed for PKCS#1 v2.1 encoding and for
 *                 #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param sig      The buffer to hold the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PUBLIC mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PRIVATE.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PUBLIC and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 if the signing operation was successful,
 *                 or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           For PKCS#1 v2.1 encoding, see comments on
 *                 mbedtls_rsa_rsassa_pss_sign() for details on
 *                 \p md_alg and \p hash_id.
 */
int mbedtls_rsa_pkcs1_sign( mbedtls_rsa_context *ctx,
                    int (*f_rng)(void *, unsigned char *, size_t),
                    void *p_rng,
                    int mode,
                    mbedtls_md_type_t md_alg,
                    unsigned int hashlen,
                    const unsigned char *hash,
                    unsigned char *sig );

/**
 * \brief          This function performs a PKCS#1 v1.5 signature
 *                 operation (RSASSA-PKCS1-v1_5-SIGN).
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param sig      The buffer to hold the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PUBLIC mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PRIVATE.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PUBLIC and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 if the signing operation was successful,
 *                 or or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_rsassa_pkcs1_v15_sign( mbedtls_rsa_context *ctx,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng,
                               int mode,
                               mbedtls_md_type_t md_alg,
                               unsigned int hashlen,
                               const unsigned char *hash,
                               unsigned char *sig );

/**
 * \brief          This function performs a PKCS#1 v2.1 PSS signature
 *                 operation (RSASSA-PSS-SIGN).
 *
 * \param ctx      The RSA context.
 * \param f_rng    The RNG function. Needed for PKCS#1 v2.1 encoding and for
 *                 #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param sig      The buffer to hold the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PUBLIC mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PRIVATE.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PUBLIC and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 if the signing operation was successful,
 *                 or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           The \p hash_id in the RSA context is the one used for the
 *                 encoding. \p md_alg in the function call is the type of hash
 *                 that is encoded. According to <em>RFC-3447: Public-Key
 *                 Cryptography Standards (PKCS) #1 v2.1: RSA Cryptography
 *                 Specifications</em> it is advised to keep both hashes the
 *                 same.
 */
int mbedtls_rsa_rsassa_pss_sign( mbedtls_rsa_context *ctx,
                         int (*f_rng)(void *, unsigned char *, size_t),
                         void *p_rng,
                         int mode,
                         mbedtls_md_type_t md_alg,
                         unsigned int hashlen,
                         const unsigned char *hash,
                         unsigned char *sig );

/**
 * \brief          This function performs a public RSA operation and checks
 *                 the message digest.
 *
 *                 This is the generic wrapper for performing a PKCS#1
 *                 verification using the mode from the context.
 *
 * \param ctx      The RSA public key context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param sig      The buffer holding the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PRIVATE mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 set to #MBEDTLS_RSA_PUBLIC.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PRIVATE and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 if the verify operation was successful,
 *                 or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           For PKCS#1 v2.1 encoding, see comments on
 *                 mbedtls_rsa_rsassa_pss_verify() about \p md_alg and
 *                 \p hash_id.
 */
int mbedtls_rsa_pkcs1_verify( mbedtls_rsa_context *ctx,
                      int (*f_rng)(void *, unsigned char *, size_t),
                      void *p_rng,
                      int mode,
                      mbedtls_md_type_t md_alg,
                      unsigned int hashlen,
                      const unsigned char *hash,
                      const unsigned char *sig );

/**
 * \brief          This function performs a PKCS#1 v1.5 verification
 *                 operation (RSASSA-PKCS1-v1_5-VERIFY).
 *
 * \param ctx      The RSA public key context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param sig      The buffer holding the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PRIVATE mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 set to #MBEDTLS_RSA_PUBLIC.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PRIVATE and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 if the verify operation was successful,
 *                 or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 */
int mbedtls_rsa_rsassa_pkcs1_v15_verify( mbedtls_rsa_context *ctx,
                                 int (*f_rng)(void *, unsigned char *, size_t),
                                 void *p_rng,
                                 int mode,
                                 mbedtls_md_type_t md_alg,
                                 unsigned int hashlen,
                                 const unsigned char *hash,
                                 const unsigned char *sig );

/**
 * \brief          This function performs a PKCS#1 v2.1 PSS verification
 *                 operation (RSASSA-PSS-VERIFY).
 *
 *                 The hash function for the MGF mask generating function
 *                 is that specified in the RSA context.
 *
 * \param ctx      The RSA public key context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param sig      The buffer holding the ciphertext.
 *
 * \deprecated     It is deprecated and discouraged to call this function
 *                 in #MBEDTLS_RSA_PRIVATE mode. Future versions of the library
 *                 are likely to remove the \p mode argument and have it
 *                 implicitly set to #MBEDTLS_RSA_PUBLIC.
 *
 * \note           Alternative implementations of RSA need not support
 *                 mode being set to #MBEDTLS_RSA_PRIVATE and might instead
 *                 return #MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION.
 *
 * \return         \c 0 if the verify operation was successful,
 *                 or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           The \p hash_id in the RSA context is the one used for the
 *                 verification. \p md_alg in the function call is the type of
 *                 hash that is verified. According to <em>RFC-3447: Public-Key
 *                 Cryptography Standards (PKCS) #1 v2.1: RSA Cryptography
 *                 Specifications</em> it is advised to keep both hashes the
 *                 same. If \p hash_id in the RSA context is unset,
 *                 the \p md_alg from the function call is used.
 */
int mbedtls_rsa_rsassa_pss_verify( mbedtls_rsa_context *ctx,
                           int (*f_rng)(void *, unsigned char *, size_t),
                           void *p_rng,
                           int mode,
                           mbedtls_md_type_t md_alg,
                           unsigned int hashlen,
                           const unsigned char *hash,
                           const unsigned char *sig );

/**
 * \brief          This function performs a PKCS#1 v2.1 PSS verification
 *                 operation (RSASSA-PSS-VERIFY).
 *
 *                 The hash function for the MGF mask generating function
 *                 is that specified in \p mgf1_hash_id.
 *
 * \param ctx      The RSA public key context.
 * \param f_rng    The RNG function. Only needed for #MBEDTLS_RSA_PRIVATE.
 * \param p_rng    The RNG parameter.
 * \param mode     #MBEDTLS_RSA_PUBLIC or #MBEDTLS_RSA_PRIVATE.
 * \param md_alg   The message-digest algorithm used to hash the original data.
 *                 Use #MBEDTLS_MD_NONE for signing raw data.
 * \param hashlen  The length of the message digest. Only used if \p md_alg is #MBEDTLS_MD_NONE.
 * \param hash     The buffer holding the message digest.
 * \param mgf1_hash_id The message digest used for mask generation.
 * \param expected_salt_len The length of the salt used in padding. Use
 *                 #MBEDTLS_RSA_SALT_LEN_ANY to accept any salt length.
 * \param sig      The buffer holding the ciphertext.
 *
 * \return         \c 0 if the verify operation was successful,
 *                 or a non-zero error code on failure.
 *
 * \note           The \p sig buffer must be as large as the size
 *                 of \p ctx->N. For example, 128 Bytes if RSA-1024 is used.
 *
 * \note           The \p hash_id in the RSA context is ignored.
 */
int mbedtls_rsa_rsassa_pss_verify_ext( mbedtls_rsa_context *ctx,
                               int (*f_rng)(void *, unsigned char *, size_t),
                               void *p_rng,
                               int mode,
                               mbedtls_md_type_t md_alg,
                               unsigned int hashlen,
                               const unsigned char *hash,
                               mbedtls_md_type_t mgf1_hash_id,
                               int expected_salt_len,
                               const unsigned char *sig );

/**
 * \brief          This function copies the components of an RSA context.
 *
 * \param dst      The destination context.
 * \param src      The source context.
 *
 * \return         \c 0 on success,
 *                 or a non-zero error code on failure.
 */
int mbedtls_rsa_copy( mbedtls_rsa_context *dst, const mbedtls_rsa_context *src );

/**
 * \brief          This function frees the components of an RSA key.
 *
 * \param ctx      The RSA Context to free.
 */
void mbedtls_rsa_free( mbedtls_rsa_context *ctx );

#ifdef __cplusplus
}
#endif

#endif /*  MBEDTLS_RSA_ALT  */

#endif /*  MBEDTLS_RSA_ALT_H  */
