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


#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_RSA_C)

#include "mbedtls/rsa.h"
#include "mbedtls/rsa_internal.h"
#include "mbedtls/oid.h"
#include "mbedtls_common.h"
#include "bignum.h"

#include <string.h>

#if defined(MBEDTLS_PKCS1_V21)
#include "mbedtls/md.h"
#endif

#if defined(MBEDTLS_PKCS1_V15) && !defined(__OpenBSD__)
#include <stdlib.h>
#endif

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#define mbedtls_printf printf
#define mbedtls_calloc calloc
#define mbedtls_free   free
#endif

#define mbedtls_rsa_alt_calloc          mbedtls_calloc
#define mbedtls_rsa_alt_free            mbedtls_free

#if defined (MBEDTLS_RSA_ALT)
#include "cc_bitops.h"
#include "rsa_public.h"
#include "rsa_private.h"
#include "cc_pal_mem.h"
#include "cc_rsa_error.h"
#include "cc_rsa_local.h"
#include "cc_common_math.h"
#include "cc_fips_defs.h"
#include "cc_pal_types_plat.h"
#include "cc_pal_log.h"
#include "cc_rsa_schemes.h"
#include "cc_rnd_common.h"
#include "cc_rnd_error.h"

#define IN
#define OUT

#include "cc_rsa_kg.h"
#include "cc_rsa_prim.h"
#include "cc_common.h"
#include "pki/common/pki.h"
#include "pki/rsa/rsa.h"

#include "ctr_drbg.h"

#define GOTO_END(ERR) \
    do { \
        Error = ERR; \
        goto End; \
    } while (0)

#define GOTO_CLEANUP(ERR) \
    do { \
        Error = ERR; \
        goto Cleanup; \
    } while (0)

typedef enum {
    CC_RSA_OP_DEFAULT,
    CC_RSA_OP_PUBLIC,
    CC_RSA_OP_PRIVATE,
} CC_RSA_OP;

/* Implementation that should never be optimized out by the compiler */
static void mbedtls_rsa_zeroize( void *v, size_t n ) {
    volatile uint8_t *p = v; while( n-- ) *p++ = 0;
}

/*
 * The function sallocates mpi inner buffer X->p of required length, copies given data into it.
 * Assumed that the data is pozitive *X->p >= 0, therefor the function sets X->s = 0.
 */
static int32_t mbedtls_rsa_uint32_buf_to_mpi(mbedtls_mpi *X, const uint32_t *buf, size_t sizeInWords)
{
     int32_t err = 0;

     if(X->p != NULL || X->n != 0) {
        err = MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
        goto End;
     }

    if( (err = mbedtls_mpi_grow( X, sizeInWords )) != 0 )
        goto End;

    CC_PalMemCopy(X->p, buf, sizeInWords*CC_32BIT_WORD_SIZE);
    X->s = 1;
    X->n = sizeInWords;

    End:
    return err;
}


/* The function zeroizes size_bytes part on allocated buffer and
 * then releases the memory given by the pointer X
 */
static void mbedtls_buff_free( void *X, size_t size_bytes )
{
    if( X == NULL )
        return;
    if(size_bytes != 0)
        mbedtls_rsa_zeroize( X, size_bytes );

    mbedtls_rsa_alt_free( X );
}


/*
 * The function converts CC errors codes to appropriate mbedtls defined code
 *
 * */
static int error_mapping_cc_to_mbedtls_rsa (CCError_t cc_error, CC_RSA_OP op)
{
    int ret=-1;
    int base = 0;

    switch (op)
    {
        case CC_RSA_OP_PUBLIC:
            base = MBEDTLS_ERR_RSA_PUBLIC_FAILED;
            break;

        case CC_RSA_OP_PRIVATE:
            base = MBEDTLS_ERR_RSA_PRIVATE_FAILED;
            break;

        default:
            base = 0;
            break;
    }

    switch (cc_error)
    {
        case CC_RSA_BASE_MGF_MASK_TOO_LONG:
        case CC_RSA_BASE_OAEP_DECODE_MESSAGE_TOO_LONG:
        case CC_RSA_BASE_OAEP_DECODE_PARAMETER_STRING_TOO_LONG:
        case CC_RSA_BASE_OAEP_ENCODE_MESSAGE_TOO_LONG:
        case CC_RSA_BASE_OAEP_ENCODE_PARAMETER_STRING_TOO_LONG:
        case CC_RSA_CONV_TO_CRT_INVALID_TEMP_BUFF_POINTER_ERROR:
        case CC_RSA_DATA_POINTER_INVALID_ERROR:
        case CC_RSA_DECRYPT_INVALID_OUTPUT_SIZE:
        case CC_RSA_DECRYPT_OUTPUT_SIZE_POINTER_ERROR:
        case CC_RSA_ENCODE_15_MSG_OUT_OF_RANGE:
        case CC_RSA_GET_DER_HASH_MODE_ILLEGAL:
        case CC_RSA_HASH_ILLEGAL_OPERATION_MODE_ERROR:
        case CC_RSA_ILLEGAL_PARAMS_ACCORDING_TO_PRIV_ERROR:
        case CC_RSA_INVALID_CRT_COEFFICIENT_PTR_ERROR:
        case CC_RSA_INVALID_CRT_COEFFICIENT_SIZE_ERROR:
        case CC_RSA_INVALID_CRT_COEFFICIENT_SIZE_PTR_ERROR:
        case CC_RSA_INVALID_CRT_COEFF_VAL:
        case CC_RSA_INVALID_CRT_FIRST_AND_SECOND_FACTOR_SIZE:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_EXPONENT_VAL:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_EXP_PTR_ERROR:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_EXP_SIZE_ERROR:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_EXP_SIZE_PTR_ERROR:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_POINTER_ERROR:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_SIZE:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_SIZE_ERROR:
        case CC_RSA_INVALID_CRT_FIRST_FACTOR_SIZE_POINTER_ERROR:
        case CC_RSA_INVALID_CRT_PARAMETR_SIZE_ERROR:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_EXPONENT_VAL:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_EXP_PTR_ERROR:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_EXP_SIZE_ERROR:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_EXP_SIZE_PTR_ERROR:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_POINTER_ERROR:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_SIZE:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_SIZE_ERROR:
        case CC_RSA_INVALID_CRT_SECOND_FACTOR_SIZE_POINTER_ERROR:
        case CC_RSA_INVALID_DECRYPRION_MODE_ERROR:
        case CC_RSA_INVALID_EXPONENT_POINTER_ERROR:
        case CC_RSA_INVALID_EXPONENT_SIZE:
        case CC_RSA_INVALID_EXPONENT_VAL:
        case CC_RSA_INVALID_EXP_BUFFER_SIZE_POINTER:
        case CC_RSA_INVALID_MESSAGE_BUFFER_SIZE:
        case CC_RSA_INVALID_MESSAGE_DATA_SIZE:
        case CC_RSA_INVALID_MODULUS_ERROR:
        case CC_RSA_INVALID_MODULUS_POINTER_ERROR:
        case CC_RSA_INVALID_MODULUS_SIZE:
        case CC_RSA_INVALID_MOD_BUFFER_SIZE_POINTER:
        case CC_RSA_INVALID_OUTPUT_POINTER_ERROR:
        case CC_RSA_INVALID_OUTPUT_SIZE_POINTER_ERROR:
        case CC_RSA_INVALID_PRIV_KEY_STRUCT_POINTER_ERROR:
        case CC_RSA_INVALID_PTR_ERROR:
        case CC_RSA_INVALID_PUB_KEY_STRUCT_POINTER_ERROR:
        case CC_RSA_INVALID_SIGNATURE_BUFFER_POINTER:
        case CC_RSA_INVALID_SIGNATURE_BUFFER_SIZE:
        case CC_RSA_INVALID_USER_CONTEXT_POINTER_ERROR:
        case CC_RSA_KEY_GEN_DATA_STRUCT_POINTER_INVALID:
        case CC_RSA_MGF_ILLEGAL_ARG_ERROR:
        case CC_RSA_MODULUS_EVEN_ERROR:
        case CC_RSA_PKCS1_VER_ARG_ERROR:
        case CC_RSA_PRIM_DATA_STRUCT_POINTER_INVALID:
        case CC_RSA_PRIV_KEY_VALIDATION_TAG_ERROR:
        case CC_RSA_PSS_ENCODING_MODULUS_HASH_SALT_LENGTHS_ERROR:
        case CC_RSA_PUB_KEY_VALIDATION_TAG_ERROR:
        case CC_RSA_USER_CONTEXT_VALIDATION_TAG_ERROR:
        case CC_RSA_WRONG_PRIVATE_KEY_TYPE:
            ret = MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
            break;

        case CC_RSA_INVALID_MESSAGE_VAL:
            ret = base + MBEDTLS_ERR_MPI_BAD_INPUT_DATA;
            break;

        case CC_RSA_INVALID_MESSAGE_DATA_SIZE_IN_SSL_CASE:
        case CC_RSA_ERROR_IN_DECRYPTED_BLOCK_PARSING:
        case CC_RSA_OAEP_DECODE_ERROR:
            ret = MBEDTLS_ERR_RSA_INVALID_PADDING;
            break;

        case CC_RSA_15_ERROR_IN_DECRYPTED_DATA_SIZE:
            ret = MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE;
            break;

        case CC_RSA_KEY_GEN_CONDITIONAL_TEST_FAIL_ERROR:
        case CC_RSA_GENERATED_PRIV_KEY_IS_TOO_LOW:
        case CC_RSA_KEY_GENERATION_FAILURE_ERROR:
            ret = MBEDTLS_ERR_RSA_KEY_GEN_FAILED;
            break;

        case CC_RSA_CAN_NOT_GENERATE_RAND_IN_RANGE:
        case CC_RSA_ERROR_IN_RANDOM_OPERATION_FOR_ENCODE:
        case CC_RND_STATE_PTR_INVALID_ERROR:
        case CC_RND_GEN_VECTOR_FUNC_ERROR:
            ret = MBEDTLS_ERR_RSA_RNG_FAILED;
            break;

        case CC_RSA_ERROR_VER15_INCONSISTENT_VERIFY:
        case CC_RSA_ERROR_PSS_INCONSISTENT_VERIFY:
            ret = MBEDTLS_ERR_RSA_VERIFY_FAILED;
            break;

        // For now, there is no better error code for malloc failure, both in CC and mbedtls
        case CC_OUT_OF_RESOURCE_ERROR:
            ret = -1;
            break;

        case CC_OK:
            ret = 0;
            break;

#if defined(MBEDTLS_THREADING_C)
        case MBEDTLS_ERR_THREADING_MUTEX_ERROR:
            ret = MBEDTLS_ERR_THREADING_MUTEX_ERROR;
            break;

        case MBEDTLS_ERR_THREADING_BAD_INPUT_DATA:
            ret = MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
            break;
#endif

        default:
            ret = -1;
            CC_PAL_LOG_ERR("Unknown CC_ERROR %d (0x%08x)\n", cc_error, cc_error);
            break;
    }

    if( ret != 0 ){
	CC_PAL_LOG_INFO("Converted CC_ERROR %d (0x%08x) to MBEDTLS_ERR %d\n",
	            cc_error, cc_error, ret);
    }

    return ret;
}

void mbedtls_rsa_init( mbedtls_rsa_context *ctx,
        int padding,
        int hash_id )
{
    CC_PalMemSetZero(ctx, sizeof( mbedtls_rsa_context));

    mbedtls_rsa_set_padding( ctx, padding, hash_id );

#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_init( &ctx->mutex );
#endif
}

/*
 * Set padding for an existing RSA context
 */
void mbedtls_rsa_set_padding( mbedtls_rsa_context *ctx, int padding, int hash_id )
{
    ctx->padding = padding;
    ctx->hash_id = hash_id;
}

#if defined(MBEDTLS_GENPRIME)



/*
 * Generate the RSA key pair (CRT and non CRT parameters) .
 */
int mbedtls_rsa_gen_key( mbedtls_rsa_context *pCtx,    /*!< pointer to context structure, containing RSA parameters */
        int (*f_rng)(void *, unsigned char *, size_t), /*<! random vector generation function prototype */
        void *p_rng,                                   /*<! pointer to PRNG buffer (state) */
        unsigned int nbits,                            /*<! RSA modulus size in bits */
        int pubExp )                                   /*<! public exponent value */
{


    CCRndContext_t rndContext;
    CCRndContext_t *pRndContext = &rndContext;

    /* the Error return code identifier */
    CCError_t err = CC_OK;
    uint32_t pubExpSizeBits, mask;
    uint32_t keySizeWords; /* size of RSA modulus */
    CCRsaPubKey_t  *pCcPubKey = NULL;
    CCRsaPrivKey_t *pCcPrivKey = NULL;
    CCRsaKgData_t  *pKeyGenData = NULL;

    uint32_t keySizeBytes = CALC_FULL_BYTES(nbits);


#ifdef FIPS_CERTIFICATION
    CCRsaKgFipsContext_t  FipsCtx;
#endif


    /* check input parameters and functions */
    if ( pCtx == NULL || f_rng == NULL || p_rng == NULL ){
            return MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
    }

    /* verifing the exponent has legal value (currently only 0x3,0x11 and 0x10001) */
    /* verifying the exponent has legal value (currently only 0x3,0x11 and 0x10001) */
    if (pubExp != CC_RSA_KG_PUB_EXP_ALLOW_VAL_1  &&
        pubExp != CC_RSA_KG_PUB_EXP_ALLOW_VAL_2  &&
        pubExp != CC_RSA_KG_PUB_EXP_ALLOW_VAL_3) {
            return  MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
    }

    /*  check that the key size allowed by CRYS requirements  */
    if (( nbits < CC_RSA_MIN_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
        ( nbits > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
        ( nbits % CC_RSA_VALID_KEY_SIZE_MULTIPLE_VALUE_IN_BITS )) {
            return MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
    }

    /* set random generation function and context */
    pRndContext->rndState = p_rng;

    err = CC_RndSetGenerateVectorFunc(pRndContext, f_rng);
    if ( err != CC_OK ){
            return MBEDTLS_ERR_RSA_RNG_FAILED;
    }

    /* RSA modulus size in bytes and words */
    keySizeWords = CALC_FULL_32BIT_WORDS(nbits);

    /* allocate temp buffers */
    pCcPubKey = (CCRsaPubKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPubKey_t));
    if (pCcPubKey == NULL) {
        err = CC_OUT_OF_RESOURCE_ERROR;
        goto End;
    }
    pCcPrivKey = (CCRsaPrivKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrivKey_t));
    if (pCcPrivKey == NULL) {
        err = CC_OUT_OF_RESOURCE_ERROR;
        goto End;
    }
    pKeyGenData = (CCRsaKgData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaKgData_t));
    if (pKeyGenData == NULL) {
        err = CC_OUT_OF_RESOURCE_ERROR;
        goto End;
    }

    /* get pub.exp size in bits */
    pubExpSizeBits = 32;
    mask = 1UL << 31;
    while((pubExp & mask) == 0) {
        pubExpSizeBits--;
        mask >>= 1;
    }

    /* init sizes */
    pCcPubKey->nSizeInBits  = nbits;
    pCcPubKey->eSizeInBits  = pubExpSizeBits;
    pCcPrivKey->nSizeInBits = nbits;
    pCcPubKey->e[0] = pubExp;

    /* set params for non CRT */
    pCcPrivKey->OperationMode = CC_RSA_NoCrt;
    pCcPrivKey->PriveKeyDb.NonCrt.e[0] = pubExp;
    pCcPrivKey->PriveKeyDb.NonCrt.eSizeInBits = pCcPubKey->eSizeInBits;


    /* .....   calculate primes (P, Q) and nonCRT key (N, D) ..... */
     do{
         err = RsaGenPandQ(
                        pRndContext,
                        nbits,
                        pubExpSizeBits,
                        (uint32_t*)&pubExp,
                        pKeyGenData );
        if (err != CC_OK) {
           goto End;
        }

        /* calculate modulus n and private nonCRT exponent d */
        err = RsaCalculateNandD(
                        pCcPubKey,
                        pCcPrivKey,
                        pKeyGenData,
                        nbits/2 );

        if (err != CC_OK) {
            goto End;
        }

        /* repeat the loop if D is too low */
    } while( (err == CC_RSA_GENERATED_PRIV_KEY_IS_TOO_LOW) );

    /* allocate mbedtls context internal buffers and copy data to them  */
    // RL TBD output all buffs. on end of func.
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->N, pCcPubKey->n, keySizeWords );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->E, pCcPrivKey->PriveKeyDb.NonCrt.e, 1/*sizeInWords*/ );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->D, pCcPrivKey->PriveKeyDb.NonCrt.d, keySizeWords );

    /* calculate CRT parameters */
    err = RsaCalculateCrtParams(
                        (uint32_t*)&pubExp, pubExpSizeBits,
                        nbits,
                        pKeyGenData->KGData.p, pKeyGenData->KGData.q,
                        pCcPrivKey->PriveKeyDb.Crt.dP,
                        pCcPrivKey->PriveKeyDb.Crt.dQ,
                        pCcPrivKey->PriveKeyDb.Crt.qInv );

    if (err !=CC_OK) {
       goto End;
    }


    /* allocate mbedtls context internal buffers and copy data to them  */

    pCtx->len = keySizeBytes; /* full size of modulus in bytes, including leading zeros*/
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->N, pCcPubKey->n, keySizeWords );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->E, pCcPrivKey->PriveKeyDb.NonCrt.e, 1/*sizeInWords*/ );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->D, pCcPrivKey->PriveKeyDb.NonCrt.d, keySizeWords );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->P, pKeyGenData->KGData.p, keySizeWords/2 );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->Q, pKeyGenData->KGData.q, keySizeWords/2 );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->DP, pCcPrivKey->PriveKeyDb.Crt.dP, keySizeWords/2 );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->DQ, pCcPrivKey->PriveKeyDb.Crt.dQ, keySizeWords/2 );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->QP, pCcPrivKey->PriveKeyDb.Crt.qInv, keySizeWords/2 );

    /* calculate Barr. tag for modulus N */
    err = RsaInitPubKeyDb( pCcPubKey );
    if (err != CC_OK) {
       goto End;
    }
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->NP, ((RsaPubKeyDb_t*)(pCcPubKey->ccRSAIntBuff))->NP, CC_PKA_BARRETT_MOD_TAG_BUFF_SIZE_IN_WORDS );

    /*  calculate Barr. tag for CRT Q, Q    */
    pCcPrivKey->OperationMode = CC_RSA_Crt;
    pCcPrivKey->PriveKeyDb.Crt.PSizeInBits = nbits/2;
    pCcPrivKey->PriveKeyDb.Crt.QSizeInBits = nbits/2;
    err = RsaInitPrivKeyDb( pCcPrivKey );
    if (err != CC_OK) {
           goto End;
    }
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->BPP, ((RsaKgIntBuff_t*)&pCcPrivKey->ccRSAPrivKeyIntBuff)->barP, CC_PKA_BARRETT_MOD_TAG_BUFF_SIZE_IN_WORDS );
    mbedtls_rsa_uint32_buf_to_mpi( &pCtx->BQP, ((RsaKgIntBuff_t*)&pCcPrivKey->ccRSAPrivKeyIntBuff)->barQ, CC_PKA_BARRETT_MOD_TAG_BUFF_SIZE_IN_WORDS );

#ifdef FIPS_CERTIFICATION
        CC_CommonReverseMemcpy( rsaKgOutParams.nModulus, (uint8_t*)pCcPubKey->n, keySizeBytes );
        CC_CommonReverseMemcpy( rsaKgOutParams.pPrim, (uint8_t*)pKeyGenData->KGData.p, keySizeBytes/2 );
        CC_CommonReverseMemcpy( rsaKgOutParams.qPrim, (uint8_t*)pKeyGenData->KGData.q, keySizeBytes/2 );
        CC_CommonReverseMemcpy( rsaKgOutParams.dPrivExponent, (uint8_t*)pCcPrivKey->PriveKeyDb.NonCrt.d, keySizeBytes );
#endif

    End:

    /* zeroing temp buffers  */
    mbedtls_buff_free( pKeyGenData, sizeof( CCRsaKgData_t ) );
    mbedtls_buff_free( pCcPrivKey,  sizeof( CCRsaPrivKey_t ) );
    mbedtls_buff_free( pCcPubKey,   sizeof( CCRsaPubKey_t ) );

    if( err != 0 ) {
        mbedtls_rsa_free( pCtx );

    }

    return error_mapping_cc_to_mbedtls_rsa( err, CC_RSA_OP_PUBLIC );

}

#endif /* MBEDTLS_GENPRIME */

/*
 * Check a public RSA key
 */
int mbedtls_rsa_check_pubkey( const mbedtls_rsa_context *ctx )
{
    if( !ctx->N.p || !ctx->E.p )
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );

    if( ( ctx->N.p[0] & 1 ) == 0 ||
        ( ctx->E.p[0] & 1 ) == 0 )
          return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED);

    if( mbedtls_mpi_bitlen( &ctx->N ) < 128 ||
        mbedtls_mpi_bitlen( &ctx->N ) > MBEDTLS_MPI_MAX_BITS )
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );


    if( mbedtls_mpi_bitlen( &ctx->E ) < 2 ||
        mbedtls_mpi_cmp_mpi( &ctx->E, &ctx->N ) >= 0 )
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );


    return( 0 );
}



#if defined(MBEDTLS_RSA_NO_CRT)

/*
 * Check a private RSA key
 */
int mbedtls_rsa_check_privkey( const mbedtls_rsa_context *ctx )
{
    int ret;
    mbedtls_mpi PQ, DE, P1, Q1, H, I, G, G2, L1, L2, DP, DQ, QP;

    if( ( ret = mbedtls_rsa_check_pubkey( ctx ) ) != 0 )
        return( ret );

    if (ctx->D.p == NULL)
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );

    mbedtls_mpi_init( &PQ ); mbedtls_mpi_init( &DE ); mbedtls_mpi_init( &P1 ); mbedtls_mpi_init( &Q1 );
    mbedtls_mpi_init( &H  ); mbedtls_mpi_init( &I  ); mbedtls_mpi_init( &G  ); mbedtls_mpi_init( &G2 );
    mbedtls_mpi_init( &L1 ); mbedtls_mpi_init( &L2 ); mbedtls_mpi_init( &DP ); mbedtls_mpi_init( &DQ );
    mbedtls_mpi_init( &QP );

    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &PQ, &ctx->P, &ctx->Q ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &DE, &ctx->D, &ctx->E ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_sub_int( &P1, &ctx->P, 1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_sub_int( &Q1, &ctx->Q, 1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &H, &P1, &Q1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_gcd( &G, &ctx->E, &H  ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_gcd( &G2, &P1, &Q1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_div_mpi( &L1, &L2, &H, &G2 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &I, &DE, &L1  ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &DP, &ctx->D, &P1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &DQ, &ctx->D, &Q1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_inv_mod( &QP, &ctx->Q, &ctx->P ) );
    /*
     * Check for a valid PKCS1v2 private key
     */
    if( mbedtls_mpi_cmp_mpi( &PQ, &ctx->N ) != 0 ||
            mbedtls_mpi_cmp_mpi( &DP, &ctx->DP ) != 0 ||
            mbedtls_mpi_cmp_mpi( &DQ, &ctx->DQ ) != 0 ||
            mbedtls_mpi_cmp_mpi( &QP, &ctx->QP ) != 0 ||
            mbedtls_mpi_cmp_int( &L2, 0 ) != 0 ||
            mbedtls_mpi_cmp_int( &I, 1 ) != 0 ||
            mbedtls_mpi_cmp_int( &G, 1 ) != 0 )
    {
        ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;

    }

cleanup:
    mbedtls_mpi_free( &PQ ); mbedtls_mpi_free( &DE ); mbedtls_mpi_free( &P1 ); mbedtls_mpi_free( &Q1 );
    mbedtls_mpi_free( &H  ); mbedtls_mpi_free( &I  ); mbedtls_mpi_free( &G  ); mbedtls_mpi_free( &G2 );
    mbedtls_mpi_free( &L1 ); mbedtls_mpi_free( &L2 ); mbedtls_mpi_free( &DP ); mbedtls_mpi_free( &DQ );
    mbedtls_mpi_free( &QP );

    if( ret == MBEDTLS_ERR_RSA_KEY_CHECK_FAILED )
        return( ret );

    if( ret != 0 )
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED + ret );

    return( 0 );
}

#else //defined(MBEDTLS_RSA_NO_CRT)

int mbedtls_rsa_check_privkey( const mbedtls_rsa_context *ctx )
{
    int ret;
    mbedtls_mpi PQ, DE, P1, Q1, H, I, G, G2, L1, L2, DP, DQ, QP;

    if( ( ret = mbedtls_rsa_check_pubkey( ctx ) ) != 0 )
        return( ret );

    if( !ctx->P.p || !ctx->Q.p || !ctx->D.p )
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );



    mbedtls_mpi_init( &PQ ); mbedtls_mpi_init( &DE ); mbedtls_mpi_init( &P1 ); mbedtls_mpi_init( &Q1 );
    mbedtls_mpi_init( &H  ); mbedtls_mpi_init( &I  ); mbedtls_mpi_init( &G  ); mbedtls_mpi_init( &G2 );
    mbedtls_mpi_init( &L1 ); mbedtls_mpi_init( &L2 ); mbedtls_mpi_init( &DP ); mbedtls_mpi_init( &DQ );
    mbedtls_mpi_init( &QP );

    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &PQ, &ctx->P, &ctx->Q ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &DE, &ctx->D, &ctx->E ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_sub_int( &P1, &ctx->P, 1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_sub_int( &Q1, &ctx->Q, 1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mul_mpi( &H, &P1, &Q1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_gcd( &G, &ctx->E, &H  ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_gcd( &G2, &P1, &Q1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_div_mpi( &L1, &L2, &H, &G2 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &I, &DE, &L1  ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &DP, &ctx->D, &P1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_mod_mpi( &DQ, &ctx->D, &Q1 ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_inv_mod( &QP, &ctx->Q, &ctx->P ) );
    /*
     * Check for a valid PKCS1v2 private key
     */
    if( mbedtls_mpi_cmp_mpi( &PQ, &ctx->N ) != 0 ||
        mbedtls_mpi_cmp_mpi( &DP, &ctx->DP ) != 0 ||
        mbedtls_mpi_cmp_mpi( &DQ, &ctx->DQ ) != 0 ||
        mbedtls_mpi_cmp_mpi( &QP, &ctx->QP ) != 0 ||
        mbedtls_mpi_cmp_int( &L2, 0 ) != 0 ||
        mbedtls_mpi_cmp_int( &I, 1 ) != 0 ||
        mbedtls_mpi_cmp_int( &G, 1 ) != 0 )
    {
        ret = MBEDTLS_ERR_RSA_KEY_CHECK_FAILED;
    }

cleanup:
    mbedtls_mpi_free( &PQ ); mbedtls_mpi_free( &DE ); mbedtls_mpi_free( &P1 ); mbedtls_mpi_free( &Q1 );
    mbedtls_mpi_free( &H  ); mbedtls_mpi_free( &I  ); mbedtls_mpi_free( &G  ); mbedtls_mpi_free( &G2 );
    mbedtls_mpi_free( &L1 ); mbedtls_mpi_free( &L2 ); mbedtls_mpi_free( &DP ); mbedtls_mpi_free( &DQ );
    mbedtls_mpi_free( &QP );

    if( ret == MBEDTLS_ERR_RSA_KEY_CHECK_FAILED )
        return( ret );

    if( ret != 0 )
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED + ret );

    return( 0 );

}

#endif

/*
 * Check if contexts holding a public and private key match
 */
int mbedtls_rsa_check_pub_priv( const mbedtls_rsa_context *pub, const mbedtls_rsa_context *prv )
{
    if( mbedtls_rsa_check_pubkey( pub ) != 0 ||
            mbedtls_rsa_check_privkey( prv ) != 0 )
    {
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );
    }

    if( mbedtls_mpi_cmp_mpi( &pub->N, &prv->N ) != 0 ||
            mbedtls_mpi_cmp_mpi( &pub->E, &prv->E ) != 0 )
    {
        return( MBEDTLS_ERR_RSA_KEY_CHECK_FAILED );
    }

    return( 0 );
}
static CCError_t convert_mbedtls_md_type_to_cc_rsa_hash_opmode(IN mbedtls_md_type_t mdType,
                                                    IN  int isRsaHashModeAfter,
                                                    OUT CCRsaHashOpMode_t * hashOpMode,
                                                    OUT size_t * hashOutputSizeBytes)
{
    switch (mdType)
    {
        case MBEDTLS_MD_NONE:
            *hashOpMode = CC_RSA_HASH_NO_HASH_mode;
            *hashOutputSizeBytes = CC_HASH_MD5_DIGEST_SIZE_IN_BYTES + CC_HASH_SHA1_DIGEST_SIZE_IN_BYTES;
            return CC_OK;
        case MBEDTLS_MD_MD5 :
#if 0
            /*MD5 is not recommended in PKCS1 ver 2.1 standard, hence it is not supported*/
            return CC_RSA_HASH_ILLEGAL_OPERATION_MODE_ERROR;
#else		/*This function can be called under PKCS1 ver 1.5 */
            *hashOpMode = CC_RSA_HASH_MD5_mode;
            *hashOutputSizeBytes = CC_HASH_MD5_DIGEST_SIZE_IN_BYTES;
            break;
#endif
        case MBEDTLS_MD_SHA1:
            *hashOpMode = CC_RSA_HASH_SHA1_mode;/*changing the hash mode to CC definition*/
            *hashOutputSizeBytes = CC_HASH_SHA1_DIGEST_SIZE_IN_BYTES;
            break;
        case MBEDTLS_MD_SHA224:
            *hashOpMode = CC_RSA_HASH_SHA224_mode;
            *hashOutputSizeBytes = CC_HASH_SHA224_DIGEST_SIZE_IN_BYTES;
            break;
        case MBEDTLS_MD_SHA256:
            *hashOpMode = CC_RSA_HASH_SHA256_mode;
            *hashOutputSizeBytes = CC_HASH_SHA256_DIGEST_SIZE_IN_BYTES;
            break;
        case MBEDTLS_MD_SHA384:
            *hashOpMode = CC_RSA_HASH_SHA384_mode;
            *hashOutputSizeBytes = CC_HASH_SHA384_DIGEST_SIZE_IN_BYTES;
            break;
        case MBEDTLS_MD_SHA512:
            *hashOpMode = CC_RSA_HASH_SHA512_mode;
            *hashOutputSizeBytes = CC_HASH_SHA512_DIGEST_SIZE_IN_BYTES;
            break;
        default:
            return CC_RSA_HASH_ILLEGAL_OPERATION_MODE_ERROR;
    }
    if (isRsaHashModeAfter)
    {
        *hashOpMode += CC_RSA_After_MD5_mode;

    }
    return CC_OK;
}

static CCError_t validate_mbedtls_rsa_context_private_key(mbedtls_rsa_context * ctx)
{
    CCError_t Error = CC_OK;

    if (ctx == NULL)
    {
        GOTO_END( CC_RSA_INVALID_PRIV_KEY_STRUCT_POINTER_ERROR );
    }

    if (ctx->N.p == NULL)
    {
        GOTO_END( CC_RSA_INVALID_MODULUS_POINTER_ERROR );
    }

    if (ctx->len == 0)
    {
        GOTO_END( CC_RSA_INVALID_MODULUS_SIZE );
    }

#if defined(MBEDTLS_RSA_NO_CRT)
    if (ctx->D.p == NULL)
    {
        GOTO_END( CC_RSA_INVALID_EXPONENT_POINTER_ERROR );
    }
#else
    if (ctx->P.p == NULL)
        GOTO_END( CC_RSA_INVALID_CRT_FIRST_FACTOR_POINTER_ERROR );

    if (ctx->Q.p == NULL)
        GOTO_END( CC_RSA_INVALID_CRT_SECOND_FACTOR_POINTER_ERROR );

    if (ctx->DP.p == NULL)
        GOTO_END( CC_RSA_INVALID_CRT_FIRST_FACTOR_EXP_PTR_ERROR );

    if (ctx->DQ.p == NULL)
        GOTO_END( CC_RSA_INVALID_CRT_SECOND_FACTOR_EXP_PTR_ERROR );

    if (ctx->QP.p == NULL)
        GOTO_END( CC_RSA_INVALID_CRT_COEFFICIENT_PTR_ERROR );

#endif

End:
    return Error;
}

static CCError_t validate_mbedtls_rsa_context_public_key(mbedtls_rsa_context * ctx)
{
    CCError_t Error = CC_OK;

    if (ctx == NULL)
    {
        GOTO_END( CC_RSA_INVALID_PRIV_KEY_STRUCT_POINTER_ERROR );
    }

    /* ...... checking the validity of the exponent pointer ............... */
    if (ctx->E.p == NULL)
        GOTO_END( CC_RSA_INVALID_EXPONENT_POINTER_ERROR );

    /* ...... checking the validity of the modulus pointer .............. */
    if (ctx->N.p == NULL)
        GOTO_END( CC_RSA_INVALID_MODULUS_POINTER_ERROR );

    if (ctx->len == 0)
    {
        GOTO_END( CC_RSA_INVALID_MODULUS_SIZE );
    }

End:
    return Error;
}

#if defined(MBEDTLS_RSA_NO_CRT)
static CCError_t build_cc_priv_non_crt_key(
        IN mbedtls_rsa_context *ctx,
        OUT CCRsaUserPrivKey_t *UserPrivKey_ptr
        )
{
    /* FUNCTION DECLARATIONS */

    /* the counter compare result */
    CCCommonCmpCounter_t CounterCmpResult;

    /* the size in bytes of the modulus buffer from mbedtls_rsa_ctx*/
    size_t ModulusSize;

    /* the effective size in bits of the modulus buffer */
    uint32_t ModulusEffectiveSizeInBits;

    /* the size in bytes of the exponent buffers (private and public) from mbedtls_rsa_ctx*/
    size_t PrivExponentSize, PubExponentSize;

    /* the effective sizes in bits of the private and public exponents */
    uint32_t PrivExponentEffectiveSizeInBits, PubExponentEffectiveSizeInBits;

    /* the private key database pointer */
    CCRsaPrivKey_t *PrivKey_ptr;

    /* the Error return code identifier */
    CCError_t Error = CC_OK;

    /* FUNCTION LOGIC */

    /* ................. checking the validity of the pointer arguments ....... */
    /* ------------------------------------------------------------------------ */
    CHECK_AND_RETURN_ERR_UPON_FIPS_ERROR();

    ModulusSize = (ctx->N.n)*sizeof(mbedtls_mpi_uint);
    PubExponentSize = (ctx->E.n)*sizeof(mbedtls_mpi_uint);
    PrivExponentSize = (ctx->D.n)*sizeof(mbedtls_mpi_uint);

    /* ...... checking the validity of the modulus size, private exponent can not be more than 256 bytes .............. */
    if (ModulusSize > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES)
        GOTO_END(CC_RSA_INVALID_MODULUS_SIZE);

    if (PrivExponentSize > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES)
        GOTO_END(CC_RSA_INVALID_EXPONENT_SIZE);

    if (PubExponentSize > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES)
        GOTO_END(CC_RSA_INVALID_EXPONENT_SIZE);

    /* .................. copy the buffers to the key handle structure .... */
    /* -------------------------------------------------------------------- */

    /* setting the pointer to the key database */
    PrivKey_ptr = (CCRsaPrivKey_t *)UserPrivKey_ptr->PrivateKeyDbBuff;


    /* clear the private key db */
    CC_PalMemSetZero(PrivKey_ptr, sizeof(CCRsaPrivKey_t));

    CC_PalMemCopy(PrivKey_ptr->n, ctx->N.p, ModulusSize);
    CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.NonCrt.d, ctx->D.p, PrivExponentSize);

    /* .................. initializing local variables ................... */
    /* ------------------------------------------------------------------- */

    /* .......... initializing the effective counters size in bits .......... */
    ModulusEffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->n, (ModulusSize + 3)/4);
    PrivExponentEffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.NonCrt.d, (PrivExponentSize + 3) / 4);

    /*  checking the size of the modulus  */
    if ( ( ModulusEffectiveSizeInBits < CC_RSA_MIN_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
            ( ModulusEffectiveSizeInBits > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
            ( ModulusEffectiveSizeInBits % CC_RSA_VALID_KEY_SIZE_MULTIPLE_VALUE_IN_BITS ) ) {
        GOTO_CLEANUP(CC_RSA_INVALID_MODULUS_SIZE);
    }

    /*  verifying the modulus is odd  */
    if ( (PrivKey_ptr->n[0] & 1UL) == 0 ) {
        GOTO_CLEANUP(CC_RSA_MODULUS_EVEN_ERROR);
    }

    /*  checking the priv. exponent size is not 0 in bytes */
    if ( PrivExponentEffectiveSizeInBits == 0 ) {
        GOTO_CLEANUP(CC_RSA_INVALID_EXPONENT_SIZE);
    }

    /* verifying the priv. exponent is less then the modulus */
    CounterCmpResult =
        CC_CommonCmpLsWordsUnsignedCounters(PrivKey_ptr->PriveKeyDb.NonCrt.d, (PrivExponentSize+3)/4,
                PrivKey_ptr->n, (ModulusSize+3)/4);

    if ( CounterCmpResult != CC_COMMON_CmpCounter2GreaterThenCounter1 ) {
        GOTO_CLEANUP(CC_RSA_INVALID_EXPONENT_VAL);
    }

    /* verifying the priv. exponent is not less then 1 */
    if ( PrivExponentEffectiveSizeInBits < 32 &&
            PrivKey_ptr->PriveKeyDb.NonCrt.d[0] < CC_RSA_MIN_PRIV_EXP_VALUE ) {
        GOTO_CLEANUP(CC_RSA_INVALID_EXPONENT_VAL);
    }

    /*  checking that the public exponent is an integer between 3 and modulus - 1 */
    if ( ctx->E.p != NULL ) {
        CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.NonCrt.e, ctx->E.p, PubExponentSize);
        PubExponentEffectiveSizeInBits =
            CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.NonCrt.e, (PubExponentSize+3)/4);

        /* verifying that the exponent is not less than 3 */
        if (PubExponentEffectiveSizeInBits < 32 &&
                PrivKey_ptr->PriveKeyDb.NonCrt.e[0] < CC_RSA_MIN_PUB_EXP_VALUE) {
            GOTO_CLEANUP(CC_RSA_INVALID_EXPONENT_VAL);
        }

        /* verifying that the public exponent is less than the modulus */
        CounterCmpResult =
            CC_CommonCmpLsWordsUnsignedCounters(PrivKey_ptr->PriveKeyDb.NonCrt.e, (PubExponentSize+3)/4,
                    PrivKey_ptr->n, (ModulusSize+3)/4);

        if (CounterCmpResult != CC_COMMON_CmpCounter2GreaterThenCounter1) {
            GOTO_CLEANUP(CC_RSA_INVALID_EXPONENT_VAL);
        }
    } else {
        PubExponentEffectiveSizeInBits = 0;
    }


    /* ................. building the structure ............................. */
    /* ---------------------------------------------------------------------- */

    /* set the mode to non CRT mode */
    PrivKey_ptr->OperationMode = CC_RSA_NoCrt;

    /* set the key source as external */
    PrivKey_ptr->KeySource = CC_RSA_ExternalKey;

    /* setting the modulus and exponent size in bits */
    PrivKey_ptr->nSizeInBits                   = ModulusEffectiveSizeInBits;
    PrivKey_ptr->PriveKeyDb.NonCrt.dSizeInBits = PrivExponentEffectiveSizeInBits;
    PrivKey_ptr->PriveKeyDb.NonCrt.eSizeInBits = PubExponentEffectiveSizeInBits;

    /* ................ calculate the Barret tag .............. */
    Error = RsaInitPrivKeyDb(PrivKey_ptr);

    if ( Error != CC_OK ) {
        GOTO_CLEANUP(CC_RSA_INTERNAL_ERROR);
    }

    /* ................ set the tag ................ */
    UserPrivKey_ptr->valid_tag = CC_RSA_PRIV_KEY_VALIDATION_TAG;

    /* ................. end of the function .................................. */
    /* ------------------------------------------------------------------------ */
Cleanup:
    /* if the structure created is not valid - clear it */
    if ( Error != CC_OK ) {
        CC_PalMemSetZero(UserPrivKey_ptr, sizeof(CCRsaUserPrivKey_t));
    }
End:
    return Error;

}
#else // defined(MBEDTLS_RSA_NO_CRT)
static CCError_t build_cc_priv_crt_key(
        IN mbedtls_rsa_context *ctx,
        OUT CCRsaUserPrivKey_t *UserPrivKey_ptr
        )
{
    /* FUNCTION DECLARATIONS */

    /* the counter compare result */
    CCCommonCmpCounter_t CounterCmpResult;

    /* the size in bytes of the modulus buffer from mbedtls_rsa_ctx*/
    size_t ModulusSize;

    /* the effective size in bits of the modulus buffer */
    uint32_t ModulusEffectiveSizeInBits;

    /* the size in bytes of the exponent buffers (private and public) from mbedtls_rsa_ctx*/
    //size_t PrivExponentSize, PubExponentSize;
    size_t   PSize;
    size_t   QSize;
    size_t   dPSize;
    size_t   dQSize;
    size_t   qInvSize;

    /* the effective size in bits of the modulus factors buffer */
    uint32_t P_EffectiveSizeInBits;
    uint32_t Q_EffectiveSizeInBits;
    uint32_t dP_EffectiveSizeInBits;
    uint32_t dQ_EffectiveSizeInBits;
    uint32_t qInv_EffectiveSizeInBits;

    /* the private key database pointer */
    CCRsaPrivKey_t *PrivKey_ptr;

    /* the Error return code identifier */
    CCError_t Error = CC_OK;

    /* FUNCTION LOGIC */

    /* ................. checking the validity of the pointer arguments ....... */
    /* ------------------------------------------------------------------------ */
    CHECK_AND_RETURN_ERR_UPON_FIPS_ERROR();

    PSize    = (ctx->P.n)*sizeof(mbedtls_mpi_uint);
    QSize    = (ctx->Q.n)*sizeof(mbedtls_mpi_uint);
    dPSize   = (ctx->DP.n)*sizeof(mbedtls_mpi_uint);
    dQSize   = (ctx->DQ.n)*sizeof(mbedtls_mpi_uint);
    qInvSize = (ctx->QP.n)*sizeof(mbedtls_mpi_uint);

    ModulusSize = (ctx->N.n)*sizeof(mbedtls_mpi_uint);


    /* checking the input sizes */
    if (PSize > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES/2 ||
            QSize > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES/2) {
        GOTO_END(CC_RSA_INVALID_CRT_PARAMETR_SIZE_ERROR);
    }

    if (dPSize > PSize ||
            dQSize > QSize ||
            qInvSize > PSize) {
        GOTO_END(CC_RSA_INVALID_CRT_PARAMETR_SIZE_ERROR);
    }

    /* verifying the first factor exponent is less then the first factor */
    CounterCmpResult =
        CC_CommonCmpLsWordsUnsignedCounters(ctx->DP.p, ctx->DP.n, ctx->P.p, ctx->P.n);

    if (CounterCmpResult != CC_COMMON_CmpCounter2GreaterThenCounter1) {
        GOTO_END(CC_RSA_INVALID_CRT_FIRST_FACTOR_EXPONENT_VAL);
    }

    /* verifying the second factor exponent is less then the second factor */
    CounterCmpResult =
        CC_CommonCmpLsWordsUnsignedCounters(ctx->DQ.p, ctx->DQ.n, ctx->Q.p, ctx->Q.n);

    if (CounterCmpResult != CC_COMMON_CmpCounter2GreaterThenCounter1) {
        GOTO_END(CC_RSA_INVALID_CRT_SECOND_FACTOR_EXPONENT_VAL);
    }

    /* verifying the CRT coefficient is less then the first factor */
    CounterCmpResult =
        CC_CommonCmpLsWordsUnsignedCounters(ctx->QP.p, ctx->QP.n, ctx->P.p, ctx->P.n);

    if (CounterCmpResult != CC_COMMON_CmpCounter2GreaterThenCounter1) {
        GOTO_END(CC_RSA_INVALID_CRT_COEFF_VAL);
    }

    /* .................. copy the buffers to the key handle structure .... */
    /* -------------------------------------------------------------------- */

    /* setting the pointer to the key database */
    PrivKey_ptr = (CCRsaPrivKey_t *)UserPrivKey_ptr->PrivateKeyDbBuff;


    /* clear the private key db */
    CC_PalMemSetZero(PrivKey_ptr, sizeof(CCRsaPrivKey_t));

    CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.Crt.P, ctx->P.p, PSize);
    CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.Crt.Q, ctx->Q.p, QSize);
    CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.Crt.dP, ctx->DP.p, dPSize);
    CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.Crt.dQ, ctx->DQ.p, dQSize);
    CC_PalMemCopy(PrivKey_ptr->PriveKeyDb.Crt.qInv, ctx->QP.p, qInvSize);

    /* .................. initializing local variables ................... */
    /* ------------------------------------------------------------------- */

    /* .......... initializing the effective counters size in bits .......... */
    /* initializing the effective counters size in bits */
    P_EffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.Crt.P, (PSize+3)/4);

    Q_EffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.Crt.Q, (QSize+3)/4);

    dP_EffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.Crt.dP, (dPSize+3)/4);

    dQ_EffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.Crt.dQ, (dQSize+3)/4);

    qInv_EffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->PriveKeyDb.Crt.qInv, (qInvSize+3)/4);


    /*  the first factor size is not 0 in bits */
    if (P_EffectiveSizeInBits == 0|| P_EffectiveSizeInBits > 8*PSize) {
        GOTO_CLEANUP(CC_RSA_INVALID_CRT_FIRST_FACTOR_SIZE);
    }

    /* the second factor size is not 0 in bits */
    if (Q_EffectiveSizeInBits == 0 || Q_EffectiveSizeInBits > 8*QSize) {
        GOTO_CLEANUP(CC_RSA_INVALID_CRT_SECOND_FACTOR_SIZE);
    }

    /* checking that sizes of dP, dQ, qInv > 0 */
    if (dP_EffectiveSizeInBits == 0 || dQ_EffectiveSizeInBits == 0 || qInv_EffectiveSizeInBits == 0) {
        GOTO_CLEANUP(CC_RSA_INVALID_CRT_PARAMETR_SIZE_ERROR);
    }


    // The following code is copied from cc_rsa_build.c
    // For now we don't need as mbedtls_rsa_context contains n
    // In case this'll change, the code should be used.
#ifdef MBEDTLS_RSA_PRIVATE_KEY_OPTIMIZED_IMPLEMENTATION
    /* ............... calculate the modulus N ........................... */
    /* -------------------------------------------------------------------- */


    Error = PkiLongNumMul(PrivKey_ptr->PriveKeyDb.Crt.P, P_EffectiveSizeInBits,
            PrivKey_ptr->PriveKeyDb.Crt.Q, PrivKey_ptr->n);
    if ( Error != CC_OK ) {
        GOTO_CLEANUP(CC_RSA_INTERNAL_ERROR);
    }
#else
    CC_PalMemCopy(PrivKey_ptr->n, ctx->N.p, ModulusSize);
#endif

    ModulusEffectiveSizeInBits =
        CC_CommonGetWordsCounterEffectiveSizeInBits(PrivKey_ptr->n, (2*CALC_FULL_32BIT_WORDS(P_EffectiveSizeInBits)));

    /* .................. checking the validity of the counters ............... */
    /* ------------------------------------------------------------------------ */

    /*  checking the size of the modulus  */
    if ( ( ModulusEffectiveSizeInBits < CC_RSA_MIN_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
            ( ModulusEffectiveSizeInBits > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
            ( ModulusEffectiveSizeInBits % CC_RSA_VALID_KEY_SIZE_MULTIPLE_VALUE_IN_BITS ) ) {
        GOTO_CLEANUP(CC_RSA_INVALID_MODULUS_SIZE);
    }

    /*  verifying the modulus is odd  */
    if ((PrivKey_ptr->n[0] & 1UL) == 0) {
        GOTO_CLEANUP(CC_RSA_MODULUS_EVEN_ERROR);
    }

    if ((P_EffectiveSizeInBits + Q_EffectiveSizeInBits != ModulusEffectiveSizeInBits) &&
            (P_EffectiveSizeInBits + Q_EffectiveSizeInBits != ModulusEffectiveSizeInBits - 1)) {
        GOTO_CLEANUP(CC_RSA_INVALID_CRT_FIRST_AND_SECOND_FACTOR_SIZE);
    }


    /* ................. building the structure ............................. */
    /* ---------------------------------------------------------------------- */

    /* set the mode to non CRT mode */
    PrivKey_ptr->OperationMode = CC_RSA_Crt;

    /* set the key source as external */
    PrivKey_ptr->KeySource = CC_RSA_ExternalKey;

    /* loading to structure the buffer sizes... */

    PrivKey_ptr->PriveKeyDb.Crt.PSizeInBits    = P_EffectiveSizeInBits;
    PrivKey_ptr->PriveKeyDb.Crt.QSizeInBits    = Q_EffectiveSizeInBits;
    PrivKey_ptr->PriveKeyDb.Crt.dPSizeInBits   = dP_EffectiveSizeInBits;
    PrivKey_ptr->PriveKeyDb.Crt.dQSizeInBits   = dQ_EffectiveSizeInBits;
    PrivKey_ptr->PriveKeyDb.Crt.qInvSizeInBits = qInv_EffectiveSizeInBits;
    PrivKey_ptr->nSizeInBits = ModulusEffectiveSizeInBits;

    /* ................ initialize the low level data .............. */
    Error = RsaInitPrivKeyDb(PrivKey_ptr);

    if (Error) {
        GOTO_CLEANUP(CC_RSA_INTERNAL_ERROR);
    }

    /* ................ set the tag ................ */
    UserPrivKey_ptr->valid_tag = CC_RSA_PRIV_KEY_VALIDATION_TAG;

    /* ................. end of the function .................................. */
    /* ------------------------------------------------------------------------ */
Cleanup:
    /* if the structure created is not valid - clear it */
    if ( Error != CC_OK ) {
        CC_PalMemSetZero(UserPrivKey_ptr, sizeof(CCRsaUserPrivKey_t));
    }
End:
    return Error;

}
#endif

static CCError_t build_cc_pubkey(
        IN mbedtls_rsa_context *ctx,
        OUT CCRsaUserPubKey_t *UserPubKey_ptr)
{
    /* FUNCTION DECLARATIONS */

    /* the counter compare result */
    CCCommonCmpCounter_t CounterCmpResult;

    /* the size in bytes of the modulus buffer from mbedtls_rsa_ctx*/
    size_t ModulusSize;

    /* the effective size in bits of the modulus buffer */
    uint32_t ModulusEffectiveSizeInBits;

    /* the size in bytes of the exponent buffer from mbedtls_rsa_ctx*/
    size_t   ExponentSize;

    /* the effective size in bits of the exponent buffer */
    uint32_t ExponentEffectiveSizeInBits;

    /* the public key database pointer */
    CCRsaPubKey_t *PubKey_ptr;

    /* the Error return code identifier */
    CCError_t Error = CC_OK;

    /* FUNCTION LOGIC */
    /* ................. checking the validity of the pointer arguments ....... */
    /* ------------------------------------------------------------------------ */

    CHECK_AND_RETURN_ERR_UPON_FIPS_ERROR();

    ModulusSize = (ctx->N.n)*sizeof(mbedtls_mpi_uint);
    ExponentSize = (ctx->E.n)*sizeof(mbedtls_mpi_uint);

    if ((ExponentSize > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES) ||
            (ctx->E.n == 0))
        return CC_RSA_INVALID_EXPONENT_SIZE;

    if ((ModulusSize  > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES) ||
            (ctx->N.n == 0))
    {
        return CC_RSA_INVALID_MODULUS_SIZE;
    }

    /* .................. copy the buffers to the key handle structure .... */
    /* -------------------------------------------------------------------- */
    /* setting the pointer to the key database */
    PubKey_ptr = ( CCRsaPubKey_t * )UserPubKey_ptr->PublicKeyDbBuff;

    /* clear the public key db */
    CC_PalMemSetZero( PubKey_ptr, sizeof(CCRsaPubKey_t) );

    CC_PalMemCopy(PubKey_ptr->n, ctx->N.p, ctx->N.n*sizeof(mbedtls_mpi_uint));
    CC_PalMemCopy(PubKey_ptr->e, ctx->E.p, ctx->E.n*sizeof(mbedtls_mpi_uint));

    /* .................. initializing local variables ................... */
    /* ------------------------------------------------------------------- */

    /* .......... initializing the effective counters size in bits .......... */
    ModulusEffectiveSizeInBits =  CC_CommonGetWordsCounterEffectiveSizeInBits(PubKey_ptr->n, (ModulusSize+3)/4);
    ExponentEffectiveSizeInBits = CC_CommonGetWordsCounterEffectiveSizeInBits(PubKey_ptr->e, (ExponentSize+3)/4);

    /* .................. checking the validity of the counters ............... */
    /* ------------------------------------------------------------------------ */
    if ( ( ModulusEffectiveSizeInBits < CC_RSA_MIN_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
            ( ModulusEffectiveSizeInBits > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BITS ) ||
            ( ModulusEffectiveSizeInBits % CC_RSA_VALID_KEY_SIZE_MULTIPLE_VALUE_IN_BITS )) {
        Error = CC_RSA_INVALID_MODULUS_SIZE;
        goto End;
    }
    /*  verifying the modulus is odd  */
    if ( (PubKey_ptr->n[0] & 1UL) == 0 ) {
        Error = CC_RSA_MODULUS_EVEN_ERROR;
        goto End;
    }

    /*  checking the exponent size is not 0 in bytes */
    if (ExponentEffectiveSizeInBits == 0) {
        Error = CC_RSA_INVALID_EXPONENT_SIZE;
        goto End;
    }

    /*  verifying the exponent is less then the modulus */
    CounterCmpResult = CC_CommonCmpLsWordsUnsignedCounters(PubKey_ptr->e, (ExponentSize+3)/4, PubKey_ptr->n, (ModulusSize+3)/4);

    if (CounterCmpResult != CC_COMMON_CmpCounter2GreaterThenCounter1) {
        Error = CC_RSA_INVALID_EXPONENT_VAL;
        goto End;
    }

    /*  verifying the exponent is not less then 3 */
    if (ExponentEffectiveSizeInBits < 32 && PubKey_ptr->e[0] < CC_RSA_MIN_PUB_EXP_VALUE) {
        Error = CC_RSA_INVALID_EXPONENT_VAL;
        goto End;
    }

    /* ................. building the structure ............................. */
    /* ---------------------------------------------------------------------- */

    /* setting the modulus and exponent size in bits */
    PubKey_ptr->nSizeInBits = ModulusEffectiveSizeInBits;
    PubKey_ptr->eSizeInBits = ExponentEffectiveSizeInBits;

    /* ................ initialize the low level data .............. */
    Error = RsaInitPubKeyDb(PubKey_ptr);

    if ( Error != CC_OK ) {
        Error = CC_RSA_KEY_GENERATION_FAILURE_ERROR;
        goto End;
    }

    /* ................ set the tag ................ */
    UserPubKey_ptr->valid_tag = CC_RSA_PUB_KEY_VALIDATION_TAG;

    /* ................. end of the function .................................. */
    /* ------------------------------------------------------------------------ */

End:
    /* if the structure created is not valid - clear it */
    if ( Error != CC_OK ) {
        CC_PalMemSetZero(UserPubKey_ptr, sizeof(CCRsaUserPubKey_t));
        return Error;
    }

    return CC_OK;

}/* END OF build_cc_pubkey */



/*
 * Do an RSA public key operation
 */
int mbedtls_rsa_public( mbedtls_rsa_context *ctx,
        const unsigned char *input,
        unsigned char *output )
{
#if defined(MBEDTLS_THREADING_C)
    int ret;
#endif
    CCError_t Error = CC_OK;
    CCRsaUserPubKey_t * UserPubKey_ptr = NULL;
    CCRsaPrimeData_t * PrimeData_ptr = NULL;


    if (ctx == NULL) {
        Error = CC_RSA_INVALID_PTR_ERROR;
        goto End;
    }

#if defined(MBEDTLS_THREADING_C)
    if ( (ret = mbedtls_mutex_lock(&ctx->mutex) ) != 0)
        return( ret );
#endif

    /* ...... checking the validity of the exponent pointer ............... */
    if (ctx->E.p == NULL)
        return CC_RSA_INVALID_EXPONENT_POINTER_ERROR;

    /* ...... checking the validity of the modulus pointer .............. */
    if (ctx->N.p == NULL)
        return CC_RSA_INVALID_MODULUS_POINTER_ERROR;

    UserPubKey_ptr = (CCRsaUserPubKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPubKey_t));
    if (UserPubKey_ptr == NULL) {
        Error = CC_OUT_OF_RESOURCE_ERROR;
        goto End;
    }

    PrimeData_ptr = (CCRsaPrimeData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrimeData_t));
    if (PrimeData_ptr == NULL) {
        Error = CC_OUT_OF_RESOURCE_ERROR;
        goto End;
    }

    Error = build_cc_pubkey(ctx, UserPubKey_ptr);
    if ( Error != CC_OK ) {
        goto End;
    }

    Error = CC_RsaPrimEncrypt(UserPubKey_ptr, PrimeData_ptr, (unsigned char *)input, ctx->len, output);
    if ( Error != CC_OK ) {
        goto End;
    }

End:
    mbedtls_zeroize_internal(UserPubKey_ptr, sizeof(CCRsaUserPubKey_t));
    mbedtls_zeroize_internal(PrimeData_ptr, sizeof(CCRsaPrimeData_t));
    mbedtls_rsa_alt_free(PrimeData_ptr);
    mbedtls_rsa_alt_free(UserPubKey_ptr);

#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PUBLIC);
}

/*
 * Do an RSA private key operation
 */
int mbedtls_rsa_private( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        const unsigned char *input,
        unsigned char *output )
{
#if defined(MBEDTLS_THREADING_C)
    int ret;
#endif
    CCError_t Error = CC_OK;
    CCRsaUserPrivKey_t * UserPrivKey_ptr = NULL;
    CCRsaPrimeData_t * PrimeData_ptr = NULL;

    // f_rng and p_rng are used for blinding, which CC does not support
    CC_UNUSED_PARAM(f_rng);
    CC_UNUSED_PARAM(p_rng);

    /* Make sure we have private key info, prevent possible misuse */
    if( input == NULL || output == NULL )
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    /* Validate mbedtls_rsa_context for private key actions*/
    if ( (Error = validate_mbedtls_rsa_context_private_key(ctx)) != 0 )
    {
        GOTO_END( Error );
    }

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    UserPrivKey_ptr = (CCRsaUserPrivKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPrivKey_t));
    if ( UserPrivKey_ptr == NULL ) {
        GOTO_CLEANUP(CC_OUT_OF_RESOURCE_ERROR);
    }

    PrimeData_ptr = (CCRsaPrimeData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrimeData_t));
    if ( PrimeData_ptr == NULL ) {
        GOTO_CLEANUP(CC_OUT_OF_RESOURCE_ERROR);
    }

    // In mbedTLS CRT vs. non-CRT it compilation-time define
#if defined(MBEDTLS_RSA_NO_CRT)
    Error = build_cc_priv_non_crt_key(ctx, UserPrivKey_ptr);
#else
    Error = build_cc_priv_crt_key(ctx, UserPrivKey_ptr);
#endif
    if ( Error != CC_OK ) {
        GOTO_CLEANUP(Error);
    }

    Error = CC_RsaPrimDecrypt(UserPrivKey_ptr, PrimeData_ptr, (unsigned char *)input, ctx->len, output);
    if ( Error != CC_OK ) {
        GOTO_CLEANUP(Error);
    }
Cleanup:
    if ( Error != CC_OK ) {
        mbedtls_zeroize_internal(output, ctx->len);
    }
    mbedtls_zeroize_internal(UserPrivKey_ptr, sizeof(CCRsaUserPrivKey_t));
    mbedtls_zeroize_internal(PrimeData_ptr, sizeof(CCRsaPrimeData_t));
    mbedtls_rsa_alt_free(PrimeData_ptr);
    mbedtls_rsa_alt_free(UserPrivKey_ptr);
End:
#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PRIVATE);
}


#if defined(MBEDTLS_PKCS1_V21)
/*
 * Implementation of the PKCS#1 v2.1 RSAES-OAEP-ENCRYPT function
 */
int mbedtls_rsa_rsaes_oaep_encrypt( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        const unsigned char *label, size_t label_len,
        size_t ilen,
        const unsigned char *input,
        unsigned char *output )
{
    size_t olen;
    unsigned int hlen;
    const mbedtls_md_info_t *md_info;

    CCError_t Error = CC_OK;
    CCRsaUserPubKey_t * UserPubKey_ptr = NULL;
    CCRsaPrimeData_t * PrimeData_ptr = NULL;
    CCRndContext_t rndContext;
    CCRndContext_t *rndContext_ptr = &rndContext;
    CCRsaHashOpMode_t hashOpMode = CC_RSA_HASH_OpModeLast;
    size_t hashOutputSizeBytes = 0;

    if( mode != MBEDTLS_RSA_PUBLIC )
    {
        GOTO_END( CC_RSA_ILLEGAL_PARAMS_ACCORDING_TO_PRIV_ERROR );
    }

    if( input == NULL || output == NULL )
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    if( f_rng == NULL )
    {
        GOTO_END( CC_RND_STATE_PTR_INVALID_ERROR );
    }

    rndContext_ptr->rndState = p_rng;
    if ( (Error = CC_RndSetGenerateVectorFunc(rndContext_ptr, f_rng)) != CC_OK )
    {
        GOTO_END( Error );
    }

    if ( (Error = validate_mbedtls_rsa_context_public_key(ctx)) != CC_OK )
    {
        GOTO_END( Error );
    }

    if (ctx->padding != MBEDTLS_RSA_PKCS_V21)
    {
        GOTO_END( CC_RSA_DATA_POINTER_INVALID_ERROR );
    }


    if ( (Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode((mbedtls_md_type_t)ctx->hash_id,
                                               0,     // HashMode - before
                                               &hashOpMode,
                                               &hashOutputSizeBytes)) != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }

    md_info = mbedtls_md_info_from_type( (mbedtls_md_type_t) ctx->hash_id );
    if( md_info == NULL )
    {
        GOTO_END( CC_RSA_HASH_ILLEGAL_OPERATION_MODE_ERROR );
    }

    olen = ctx->len;
    hlen = mbedtls_md_get_size( md_info );

    /* first comparison checks for overflow */
    if( ilen + 2 * hlen + 2 < ilen || olen < ilen + 2 * hlen + 2 )
    {
        GOTO_END( CC_RSA_INVALID_MESSAGE_DATA_SIZE );
    }

    UserPubKey_ptr = (CCRsaUserPubKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPubKey_t));
    if (UserPubKey_ptr == NULL) {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }

    PrimeData_ptr = (CCRsaPrimeData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrimeData_t));
    if (PrimeData_ptr == NULL)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }

    Error = build_cc_pubkey(ctx, UserPubKey_ptr);
    if ( Error != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }

    Error = CC_RsaOaepEncrypt(rndContext_ptr,
                  UserPubKey_ptr,
                  PrimeData_ptr,
                  hashOpMode,
                  (unsigned char *)label, // Need to remove the const-ness
                  label_len,
                  CC_PKCS1_MGF1,
                  (unsigned char *)input, // Need to remove the const-ness
                  ilen,
                  output);

    if ( Error != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }

Cleanup:
    if ( Error != CC_OK )
    {
        mbedtls_zeroize_internal(output, ctx->len);
    }
    mbedtls_zeroize_internal(UserPubKey_ptr, sizeof(CCRsaUserPubKey_t));
    mbedtls_zeroize_internal(PrimeData_ptr, sizeof(CCRsaPrimeData_t));
    mbedtls_rsa_alt_free(PrimeData_ptr);
    mbedtls_rsa_alt_free(UserPubKey_ptr);
End:
    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PUBLIC);
}
#endif /* MBEDTLS_PKCS1_V21 */

#if defined(MBEDTLS_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSAES-PKCS1-V1_5-ENCRYPT function
 */
int mbedtls_rsa_rsaes_pkcs1_v15_encrypt( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        size_t ilen,
        const unsigned char *input,
        unsigned char *output )
{

    CCError_t Error = CC_OK;
    CCRsaUserPubKey_t * UserPubKey_ptr = NULL;
    CCRsaPrimeData_t * PrimeData_ptr = NULL;
    CCRndContext_t rndContext;
    CCRndContext_t *rndContext_ptr = &rndContext;

    if( mode != MBEDTLS_RSA_PUBLIC )
    {
        GOTO_END( CC_RSA_ILLEGAL_PARAMS_ACCORDING_TO_PRIV_ERROR );
    }

    if( input == NULL || output == NULL )
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    if( f_rng == NULL )
    {
        GOTO_END( CC_RND_STATE_PTR_INVALID_ERROR );
    }

    rndContext_ptr->rndState = p_rng;
    if ( (Error = CC_RndSetGenerateVectorFunc(rndContext_ptr, f_rng)) != CC_OK )
        GOTO_END( Error );

    if ( (Error = validate_mbedtls_rsa_context_public_key(ctx)) != CC_OK )
    {
        GOTO_END( Error );
    }

    if ( ctx->padding != MBEDTLS_RSA_PKCS_V15 )
    {
        GOTO_END( CC_RSA_DATA_POINTER_INVALID_ERROR );
    }

    /* first comparison checks for overflow */
    if( ilen + 11 < ilen || ctx->len < ilen + 11 )
    {
        GOTO_END( CC_RSA_INVALID_MESSAGE_DATA_SIZE );
    }

    UserPubKey_ptr = (CCRsaUserPubKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPubKey_t));
    if ( UserPubKey_ptr == NULL )
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }

    PrimeData_ptr = (CCRsaPrimeData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrimeData_t));
    if ( PrimeData_ptr == NULL )
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }

    Error = build_cc_pubkey(ctx, UserPubKey_ptr);
    if ( Error != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }

    Error = CC_RsaPkcs1V15Encrypt(rndContext_ptr,
                                  UserPubKey_ptr,
                                  PrimeData_ptr,
                                  (unsigned char *)input, // Need to remove the const-ness
                                  ilen,
                                  (unsigned char *)output); // Need to remove the const-ness

    if ( Error != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }

Cleanup:
    if ( Error != CC_OK )
    {
        mbedtls_zeroize_internal(output, ctx->len);
    }
    mbedtls_zeroize_internal(UserPubKey_ptr, sizeof(CCRsaUserPubKey_t));
    mbedtls_zeroize_internal(PrimeData_ptr, sizeof(CCRsaPrimeData_t));
    mbedtls_rsa_alt_free(PrimeData_ptr);
    mbedtls_rsa_alt_free(UserPubKey_ptr);
End:

    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PUBLIC);
}

#endif /* MBEDTLS_PKCS1_V15 */

/*
 * Add the message padding, then do an RSA operation
 */
int mbedtls_rsa_pkcs1_encrypt( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode, size_t ilen,
        const unsigned char *input,
        unsigned char *output )
{
    switch( ctx->padding )
    {
#if defined(MBEDTLS_PKCS1_V15)
        case MBEDTLS_RSA_PKCS_V15:
            return mbedtls_rsa_rsaes_pkcs1_v15_encrypt( ctx, f_rng, p_rng, mode, ilen,
                    input, output );
#endif

#if defined(MBEDTLS_PKCS1_V21)
        case MBEDTLS_RSA_PKCS_V21:
            return mbedtls_rsa_rsaes_oaep_encrypt( ctx, f_rng, p_rng, mode, NULL, 0,
                    ilen, input, output );
#endif

        default:
            return( MBEDTLS_ERR_RSA_INVALID_PADDING );
    }
}

#if defined(MBEDTLS_PKCS1_V21)
/*
 * Implementation of the PKCS#1 v2.1 RSAES-OAEP-DECRYPT function
 */
int mbedtls_rsa_rsaes_oaep_decrypt( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        const unsigned char *label, size_t label_len,
        size_t *olen,
        const unsigned char *input,
        unsigned char *output,
        size_t output_max_len )
{
    CCError_t Error = CC_OK;
    CCRsaUserPrivKey_t * UserPrivKey_ptr = NULL;
    CCRsaPrimeData_t * PrimeData_ptr = NULL;
    CCRsaHashOpMode_t hashOpMode = CC_RSA_HASH_OpModeLast;
    size_t hashOutputSizeBytes = 0;

    // in mbedtls decrypt scheme f_rng and p_rng are used for blinding
    // CC does not support blinding
    CC_UNUSED_PARAM(f_rng);
    CC_UNUSED_PARAM(p_rng);

    // mbedtls supports decryption with public key, CC does not
    if ( mode != MBEDTLS_RSA_PRIVATE )
    {
        GOTO_END( CC_RSA_INVALID_DECRYPRION_MODE_ERROR );
    }

    if( input == NULL || output == NULL )
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    /* Validate mbedtls_rsa_context for private key actions*/
    if ( (Error = validate_mbedtls_rsa_context_private_key(ctx)) != CC_OK )
    {
        GOTO_END( Error );
    }

    if ( ctx->padding != MBEDTLS_RSA_PKCS_V21 )
        GOTO_END( CC_RSA_DATA_POINTER_INVALID_ERROR );

    // Sanity check on input length, not sure it's needed
    if( ctx->len < 16 || ctx->len > MBEDTLS_MPI_MAX_SIZE )
    {
        GOTO_END( CC_RSA_INVALID_MESSAGE_DATA_SIZE );
    }

    if ( ( Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode((mbedtls_md_type_t)ctx->hash_id,
                                               0,    // HashMode - before
                                               &hashOpMode,
                                               &hashOutputSizeBytes) ) != CC_OK )
    {
        GOTO_END( Error );
    }

    // checking for integer underflow
    if( 2 * hashOutputSizeBytes + 2 > ctx->len )
    {
        GOTO_END( CC_RSA_INVALID_MESSAGE_DATA_SIZE );
    }

    UserPrivKey_ptr = (CCRsaUserPrivKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPrivKey_t));
    if ( UserPrivKey_ptr == NULL )
    {
        GOTO_CLEANUP(CC_OUT_OF_RESOURCE_ERROR);
    }

    PrimeData_ptr = (CCRsaPrimeData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrimeData_t));
    if ( PrimeData_ptr == NULL )
    {
        GOTO_CLEANUP(CC_OUT_OF_RESOURCE_ERROR);
    }

    // In mbedTLS CRT vs. non-CRT it compilation-time define
#if defined(MBEDTLS_RSA_NO_CRT)
    Error = build_cc_priv_non_crt_key(ctx, UserPrivKey_ptr);
#else
    Error = build_cc_priv_crt_key(ctx, UserPrivKey_ptr);
#endif
    if ( Error != CC_OK )
    {
        GOTO_CLEANUP(Error);
    }

    *olen = output_max_len;

    Error = CC_RsaOaepDecrypt(UserPrivKey_ptr,
                              PrimeData_ptr,
                              hashOpMode,
                              (unsigned char *)label, // Need to remove the const-ness
                              label_len,
                              CC_PKCS1_MGF1,
                              (unsigned char *)input, // Need to remove the const-ness
                              ctx->len,
                              output,
                              olen);
    if ( Error != CC_OK)
    {
        GOTO_CLEANUP(Error);
    }

    if( *olen > output_max_len )
    {
        GOTO_CLEANUP( CC_RSA_15_ERROR_IN_DECRYPTED_DATA_SIZE );
    }

Cleanup:
    if ( Error != CC_OK )
    {
        mbedtls_zeroize_internal(output, output_max_len);
    }
    mbedtls_zeroize_internal(UserPrivKey_ptr, sizeof(CCRsaUserPrivKey_t));
    mbedtls_zeroize_internal(PrimeData_ptr, sizeof(CCRsaPrimeData_t));
    mbedtls_rsa_alt_free(PrimeData_ptr);
    mbedtls_rsa_alt_free(UserPrivKey_ptr);
End:
    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PRIVATE);
}
#endif /* MBEDTLS_PKCS1_V21 */

#if defined(MBEDTLS_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSAES-PKCS1-V1_5-DECRYPT function
 */
int mbedtls_rsa_rsaes_pkcs1_v15_decrypt( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        size_t *olen,
        const unsigned char *input,
        unsigned char *output,
        size_t output_max_len)
{
    CCError_t Error = CC_OK;
    CCRsaUserPrivKey_t * UserPrivKey_ptr = NULL;
    CCRsaPrimeData_t * PrimeData_ptr = NULL;

    // in mbedtls decrypt scheme f_rng and p_rng are used for blinding
    // CC does not support blinding
    CC_UNUSED_PARAM(f_rng);
    CC_UNUSED_PARAM(p_rng);

#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_lock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    // mbedtls supports decryption with public key, CC does not
    if (mode != MBEDTLS_RSA_PRIVATE)
    {
        GOTO_END( CC_RSA_INVALID_DECRYPRION_MODE_ERROR );
    }

    if( input == NULL || output == NULL )
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    /* Validate mbedtls_rsa_context for private key actions*/
    if ( (Error = validate_mbedtls_rsa_context_private_key(ctx)) != 0 )
    {
        GOTO_END( Error );
    }

    if( ctx->padding != MBEDTLS_RSA_PKCS_V15 )
    {
        GOTO_END( CC_RSA_DATA_POINTER_INVALID_ERROR );
    }

    // Sanity check on input length, not sure it's needed
    if( ctx->len < 16 || ctx->len > MBEDTLS_MPI_MAX_SIZE )
    {
        GOTO_END( CC_RSA_INVALID_MESSAGE_DATA_SIZE );
    }

    UserPrivKey_ptr = (CCRsaUserPrivKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPrivKey_t));
    if ( UserPrivKey_ptr == NULL )
    {
        GOTO_CLEANUP(CC_OUT_OF_RESOURCE_ERROR);
    }

    PrimeData_ptr = (CCRsaPrimeData_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrimeData_t));
    if ( PrimeData_ptr == NULL )
    {
        GOTO_CLEANUP(CC_OUT_OF_RESOURCE_ERROR);
    }

    // In mbedTLS CRT vs. non-CRT it compilation-time define
#if defined(MBEDTLS_RSA_NO_CRT)
    Error = build_cc_priv_non_crt_key(ctx, UserPrivKey_ptr);
#else
    Error = build_cc_priv_crt_key(ctx, UserPrivKey_ptr);
#endif
    if ( Error != CC_OK )
    {
        GOTO_CLEANUP(Error);
    }

    *olen = output_max_len;

    Error = CC_RsaPkcs1V15Decrypt(UserPrivKey_ptr,
                                  PrimeData_ptr,
                                  (unsigned char *)input, // Need to remove the const-ness
                                  ctx->len,
                                  output,
                                  olen);

    if ( Error != CC_OK )
    {
        GOTO_CLEANUP(Error);
    }

    if( *olen > output_max_len )
    {
        GOTO_CLEANUP( CC_RSA_15_ERROR_IN_DECRYPTED_DATA_SIZE );
    }

Cleanup:
    if ( Error != CC_OK )
    {
        mbedtls_zeroize_internal(output, output_max_len);
    }
    mbedtls_zeroize_internal(UserPrivKey_ptr, sizeof(CCRsaUserPrivKey_t));
    mbedtls_zeroize_internal(PrimeData_ptr, sizeof(CCRsaPrimeData_t));
    mbedtls_rsa_alt_free(PrimeData_ptr);
    mbedtls_rsa_alt_free(UserPrivKey_ptr);
End:
#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PRIVATE);
}
#endif /* MBEDTLS_PKCS1_V15 */

/*
 * Do an RSA operation, then remove the message padding
 */
int mbedtls_rsa_pkcs1_decrypt( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode, size_t *olen,
        const unsigned char *input,
        unsigned char *output,
        size_t output_max_len)
{
    switch( ctx->padding )
    {
#if defined(MBEDTLS_PKCS1_V15)
        case MBEDTLS_RSA_PKCS_V15:
            return mbedtls_rsa_rsaes_pkcs1_v15_decrypt( ctx, f_rng, p_rng, mode, olen,
                    input, output, output_max_len );
#endif

#if defined(MBEDTLS_PKCS1_V21)
        case MBEDTLS_RSA_PKCS_V21:
            return mbedtls_rsa_rsaes_oaep_decrypt( ctx, f_rng, p_rng, mode, NULL, 0,
                    olen, input, output,
                    output_max_len );
#endif

        default:
            return( MBEDTLS_ERR_RSA_INVALID_PADDING );
    }
}

#if defined(MBEDTLS_PKCS1_V21)
/*
 * Implementation of the PKCS#1 v2.1 RSASSA-PSS-SIGN function
 */
int mbedtls_rsa_rsassa_pss_sign( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        mbedtls_md_type_t md_alg,
        unsigned int hashlen,
        const unsigned char *hash,
        unsigned char *sig )
{

    CCRndContext_t              rndContext;
    CCRsaPrivUserContext_t      *UserContext_ptr = NULL;
    CCRsaUserPrivKey_t          *UserPrivKey_ptr = NULL;
    CCRsaHashOpMode_t           hashOpMode;
    size_t                      hashOutputSizeBytes;
    size_t                      sig_size;
    CCError_t                   Error = CC_OK;

    CC_UNUSED_PARAM( hashlen );        /* message digest length (for MBEDTLS_MD_NONE only which is not supported.) */
    if (mode != MBEDTLS_RSA_PRIVATE) /* In cryptocell only private key operations are allowed with sign */
    {
        GOTO_END( CC_RSA_WRONG_PRIVATE_KEY_TYPE );
    }
    if ( NULL == sig || NULL == hash )
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

        /* The hash_id in the RSA context is the one used for the
        encoding. md_alg in the function call is the type of hash
        that is encoded. According to RFC 3447 it is advised to keep
        both hashes the same. */
    if ( md_alg != ( mbedtls_md_type_t ) ctx->hash_id )
    {
        GOTO_END( CC_RSA_HASH_ILLEGAL_OPERATION_MODE_ERROR );
    }
    Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode( md_alg,
                                                           1, // After hash.
                                                           &hashOpMode,
                                                           &hashOutputSizeBytes );
    if ( CC_OK!= Error )
    {
        GOTO_END( Error );
    }

    rndContext.rndState = p_rng;
    if ( ( Error = CC_RndSetGenerateVectorFunc(&rndContext, f_rng)) != CC_OK )
    {
        GOTO_END( Error );
    }

    UserPrivKey_ptr = ( CCRsaUserPrivKey_t * )mbedtls_rsa_alt_calloc( 1, sizeof(CCRsaUserPrivKey_t ) );
    if ( NULL == UserPrivKey_ptr )
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    UserContext_ptr = ( CCRsaPrivUserContext_t * )mbedtls_rsa_alt_calloc( 1, sizeof(CCRsaPrivUserContext_t ) );
    if ( NULL == UserContext_ptr )
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    if ( (Error = validate_mbedtls_rsa_context_private_key( ctx ) ) != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }
#if defined(MBEDTLS_RSA_NO_CRT)
    Error = build_cc_priv_non_crt_key( ctx, UserPrivKey_ptr );
#else
    Error = build_cc_priv_crt_key( ctx, UserPrivKey_ptr );
#endif
    if ( CC_OK != Error )
    {
        GOTO_CLEANUP( Error );
    }

    sig_size = mbedtls_mpi_size( ( const mbedtls_mpi *)&( ctx->N ) );

    Error = CC_RsaPssSign( &rndContext,
        UserContext_ptr,
        UserPrivKey_ptr,
        hashOpMode,
        CC_PKCS1_MGF1,
        hashOutputSizeBytes,
        ( uint8_t * )hash,
        hashOutputSizeBytes,
        ( uint8_t * )sig,
        &sig_size );
Cleanup:
        mbedtls_rsa_alt_free( UserPrivKey_ptr );
        mbedtls_rsa_alt_free( UserContext_ptr );
End:
        return error_mapping_cc_to_mbedtls_rsa( Error, CC_RSA_OP_PRIVATE );
}


#endif /* MBEDTLS_PKCS1_V21 */

#if defined(MBEDTLS_PKCS1_V15)
/*
 * Implementation of the PKCS#1 v2.1 RSASSA-PKCS1-V1_5-SIGN function
 */
/*
 * Do an RSA operation to sign the message digest
 */
int mbedtls_rsa_rsassa_pkcs1_v15_sign( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        mbedtls_md_type_t md_alg,
        unsigned int hashlen,
        const unsigned char *hash,
        unsigned char *sig )
{
    CCRndContext_t              rndContext;
    CCRsaPrivUserContext_t      *UserContext_ptr = NULL;
    CCRsaUserPrivKey_t          *UserPrivKey_ptr = NULL;
    CCRsaHashOpMode_t           hashOpMode;
    size_t                      hashOutputSizeBytes;
    size_t                      sig_size;
    CCError_t                   Error = CC_OK;

    CC_UNUSED_PARAM(hashlen); /* message digest length (for MBEDTLS_MD_NONE only which is not supported.) */
    if (mode != MBEDTLS_RSA_PRIVATE) /* In cryptocell only private key operations are allowed with sign */
    {
        GOTO_END( CC_RSA_WRONG_PRIVATE_KEY_TYPE );
    }
    if (NULL == sig || NULL == hash)
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode(md_alg,
                                                           1, // After hash.
                                                           &hashOpMode,
                                                           &hashOutputSizeBytes);
    if (Error != CC_OK)
    {
        GOTO_END(Error);
    }

    rndContext.rndState = p_rng;
    if ( (Error = CC_RndSetGenerateVectorFunc(&rndContext, f_rng)) != CC_OK)
    {
        GOTO_END(Error);
    }

    UserPrivKey_ptr = (CCRsaUserPrivKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPrivKey_t));
    if (NULL == UserPrivKey_ptr)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    UserContext_ptr = (CCRsaPrivUserContext_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPrivUserContext_t));
    if (NULL == UserContext_ptr)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    if ( (Error = validate_mbedtls_rsa_context_private_key(ctx)) != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }
#if defined(MBEDTLS_RSA_NO_CRT)
    Error = build_cc_priv_non_crt_key(ctx, UserPrivKey_ptr);
#else
    Error = build_cc_priv_crt_key(ctx, UserPrivKey_ptr);
#endif
    if ( Error != CC_OK )
    {
        GOTO_CLEANUP(Error);
    }

    sig_size = mbedtls_mpi_size( (const mbedtls_mpi *)&(ctx->N) );
    Error = CC_RsaPkcs1V15Sign(&rndContext,
        UserContext_ptr,
        UserPrivKey_ptr,
        hashOpMode,
        (uint8_t *)hash,
        hashOutputSizeBytes,
        (uint8_t *)sig,
        &sig_size);

Cleanup:
        mbedtls_rsa_alt_free(UserPrivKey_ptr);
        mbedtls_rsa_alt_free(UserContext_ptr);
End:
        return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PRIVATE);
        }

#endif /* MBEDTLS_PKCS1_V15 */

/*
 * Do an RSA operation to sign the message digest
 */
int mbedtls_rsa_pkcs1_sign( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        mbedtls_md_type_t md_alg,
        unsigned int hashlen,
        const unsigned char *hash,
        unsigned char *sig )
{
    switch( ctx->padding )
    {
#if defined(MBEDTLS_PKCS1_V15)
        case MBEDTLS_RSA_PKCS_V15:
            return mbedtls_rsa_rsassa_pkcs1_v15_sign( ctx, f_rng, p_rng, mode, md_alg,
                    hashlen, hash, sig );
#endif

#if defined(MBEDTLS_PKCS1_V21)
        case MBEDTLS_RSA_PKCS_V21:
            return mbedtls_rsa_rsassa_pss_sign( ctx, f_rng, p_rng, mode, md_alg,
                    hashlen, hash, sig );
#endif

        default:
            return( MBEDTLS_ERR_RSA_INVALID_PADDING );
    }
}

#if defined(MBEDTLS_PKCS1_V21)
/*
 * Implementation of the PKCS#1 v2.1 RSASSA-PSS-VERIFY function
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
                               const unsigned char *sig )
{
    CCRsaPubUserContext_t        *UserContext_ptr = NULL;
    CCRsaUserPubKey_t            *UserPubKey_ptr = NULL;
    CCRsaHashOpMode_t            hashOpMode;
    size_t                       hashOutputSizeBytes = 0;
    int                          saltLen = CC_RSA_VERIFY_SALT_LENGTH_UNKNOWN;
    CCError_t Error = CC_OK;

    CC_UNUSED_PARAM(f_rng);
    CC_UNUSED_PARAM(p_rng);

    if( mode == MBEDTLS_RSA_PRIVATE && ctx->padding != MBEDTLS_RSA_PKCS_V21 ) /* In cryptocell only public key operations are allowed with verify */
    {
        GOTO_END( CC_RSA_WRONG_PRIVATE_KEY_TYPE );
    }
    if (NULL == sig || NULL == hash)
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }

    Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode(md_alg,
                                                          1,
                                                          &hashOpMode,
                                                          &hashlen);  // We're only interested in hashlen
    if (Error != CC_OK)
    {
        GOTO_END(Error);
    }

    Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode(mgf1_hash_id,
                                                               1,
                                                               &hashOpMode, // We're only interested in hashOpmode
                                                               &hashOutputSizeBytes);
    if (Error != CC_OK)
    {
        GOTO_END(Error);
    }
    if (expected_salt_len != MBEDTLS_RSA_SALT_LEN_ANY)
    {
        saltLen = expected_salt_len;
    }
    UserPubKey_ptr = (CCRsaUserPubKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPubKey_t));
    if (NULL == UserPubKey_ptr)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    UserContext_ptr = (CCRsaPubUserContext_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPubUserContext_t));
    if (NULL == UserContext_ptr)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    if ( (Error = validate_mbedtls_rsa_context_public_key(ctx)) != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }
    Error = build_cc_pubkey(ctx, UserPubKey_ptr);
    if (CC_OK != Error)
    {
        GOTO_CLEANUP(Error);
    }

    Error = CC_RsaPssVerify(UserContext_ptr,UserPubKey_ptr,hashOpMode,CC_PKCS1_MGF1,saltLen, (uint8_t *)hash, hashlen, (uint8_t *)sig);

Cleanup:
        mbedtls_rsa_alt_free(UserPubKey_ptr);
        mbedtls_rsa_alt_free(UserContext_ptr);
End:
        return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PUBLIC);
}

/*
 * Simplified PKCS#1 v2.1 RSASSA-PSS-VERIFY function
 */
int mbedtls_rsa_rsassa_pss_verify( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        mbedtls_md_type_t md_alg,
        unsigned int hashlen,
        const unsigned char *hash,
        const unsigned char *sig )
{
    mbedtls_md_type_t mgf1_hash_id = ( ctx->hash_id != MBEDTLS_MD_NONE )
        ? (mbedtls_md_type_t) ctx->hash_id
        : md_alg;

    return( mbedtls_rsa_rsassa_pss_verify_ext( ctx, f_rng, p_rng, mode,
                md_alg, hashlen, hash,
                mgf1_hash_id, MBEDTLS_RSA_SALT_LEN_ANY,
                sig ) );

}
#endif /* MBEDTLS_PKCS1_V21 */

#if defined(MBEDTLS_PKCS1_V15)
int mbedtls_rsa_rsassa_pkcs1_v15_verify( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        mbedtls_md_type_t md_alg,
        unsigned int hashlen,
        const unsigned char *hash,
        const unsigned char *sig )
{
    CCRsaPubUserContext_t        *UserContext_ptr = NULL;
    CCRsaUserPubKey_t            *UserPubKey_ptr = NULL;
    CCRsaHashOpMode_t            hashOpMode;
    size_t                       hashOutputSizeBytes = 0;
    CCError_t                    Error = CC_OK;

    CC_UNUSED_PARAM(f_rng);
    CC_UNUSED_PARAM(p_rng);

    Error = convert_mbedtls_md_type_to_cc_rsa_hash_opmode(md_alg,
                                                           1,
                                                           &hashOpMode,
                                                           &hashOutputSizeBytes);
    if (Error != CC_OK)
    {
        GOTO_END(Error);
    }
    if ( hashOutputSizeBytes != hashlen )
    {
        hashlen = hashOutputSizeBytes;
    }
    if (mode != MBEDTLS_RSA_PUBLIC) /* In cryptocell only public key operations are allowed with verify */
    {
        GOTO_END( CC_RSA_WRONG_PRIVATE_KEY_TYPE );
    }
    if (NULL == sig || NULL == hash)
    {
        GOTO_END( CC_RSA_INVALID_PTR_ERROR );
    }
    UserPubKey_ptr = (CCRsaUserPubKey_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaUserPubKey_t));
    if (NULL == UserPubKey_ptr)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    UserContext_ptr = (CCRsaPubUserContext_t *)mbedtls_rsa_alt_calloc(1, sizeof(CCRsaPubUserContext_t));
    if (NULL == UserContext_ptr)
    {
        GOTO_CLEANUP( CC_OUT_OF_RESOURCE_ERROR );
    }
    if ( (Error = validate_mbedtls_rsa_context_public_key(ctx)) != CC_OK )
    {
        GOTO_CLEANUP( Error );
    }
    Error = build_cc_pubkey(ctx, UserPubKey_ptr);
    if (CC_OK != Error)
    {
        GOTO_CLEANUP(Error);
    }
    Error = CC_RsaPkcs1V15Verify(UserContext_ptr,
                                    UserPubKey_ptr,
                                    hashOpMode,
                                    (uint8_t *)hash,
                                    hashlen,
                                    (uint8_t *)sig);
Cleanup:
    mbedtls_rsa_alt_free(UserPubKey_ptr);
    mbedtls_rsa_alt_free(UserContext_ptr);
End:
    return error_mapping_cc_to_mbedtls_rsa(Error, CC_RSA_OP_PUBLIC);

}
#endif /* MBEDTLS_PKCS1_V15 */

/*
 * Do an RSA operation and check the message digest
 */
int mbedtls_rsa_pkcs1_verify( mbedtls_rsa_context *ctx,
        int (*f_rng)(void *, unsigned char *, size_t),
        void *p_rng,
        int mode,
        mbedtls_md_type_t md_alg,
        unsigned int hashlen,
        const unsigned char *hash,
        const unsigned char *sig )
{
    switch( ctx->padding )
    {
#if defined(MBEDTLS_PKCS1_V15)
        case MBEDTLS_RSA_PKCS_V15:
            return mbedtls_rsa_rsassa_pkcs1_v15_verify( ctx, f_rng, p_rng, mode, md_alg,
                    hashlen, hash, sig );
#endif

#if defined(MBEDTLS_PKCS1_V21)
        case MBEDTLS_RSA_PKCS_V21:
            return mbedtls_rsa_rsassa_pss_verify( ctx, f_rng, p_rng, mode, md_alg,
                    hashlen, hash, sig );
#endif

        default:
            return( MBEDTLS_ERR_RSA_INVALID_PADDING );
    }
}

/*
 * Copy the components of an RSA key
 */
int mbedtls_rsa_copy( mbedtls_rsa_context *dst, const mbedtls_rsa_context *src )
{
    int ret;

    dst->ver = src->ver;
    dst->len = src->len;

    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->N, &src->N ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->E, &src->E ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->D, &src->D ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->P, &src->P ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->Q, &src->Q ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->DP, &src->DP ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->DQ, &src->DQ ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->QP, &src->QP ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->RN, &src->RN ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->RP, &src->RP ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->RQ, &src->RQ ) );

    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->Vi, &src->Vi ) );
    MBEDTLS_MPI_CHK( mbedtls_mpi_copy( &dst->Vf, &src->Vf ) );

    dst->padding = src->padding;
    dst->hash_id = src->hash_id;

cleanup:
    if( ret != 0 )
        mbedtls_rsa_free( dst );

    return( ret );
}

/*
 * Free the components of an RSA key
 */
void mbedtls_rsa_free( mbedtls_rsa_context *ctx )
{
    if (ctx != NULL) {
        mbedtls_mpi_free( &ctx->BQP ); mbedtls_mpi_free( &ctx->BPP ); mbedtls_mpi_free( &ctx->NP );
        mbedtls_mpi_free( &ctx->Vi ); mbedtls_mpi_free( &ctx->Vf );
        mbedtls_mpi_free( &ctx->RQ ); mbedtls_mpi_free( &ctx->RP ); mbedtls_mpi_free( &ctx->RN );
        mbedtls_mpi_free( &ctx->QP ); mbedtls_mpi_free( &ctx->DQ ); mbedtls_mpi_free( &ctx->DP );
        mbedtls_mpi_free( &ctx->Q  ); mbedtls_mpi_free( &ctx->P  ); mbedtls_mpi_free( &ctx->D );
        mbedtls_mpi_free( &ctx->E  ); mbedtls_mpi_free( &ctx->N  );

#if defined(MBEDTLS_THREADING_C)
        mbedtls_mutex_free( &ctx->mutex );
#endif
    }
}
/**************************************************************************************/
int mbedtls_rsa_import( mbedtls_rsa_context *ctx,
                        const mbedtls_mpi *N,
                        const mbedtls_mpi *P, const mbedtls_mpi *Q,
                        const mbedtls_mpi *D, const mbedtls_mpi *E )
{
    int ret;

    if( ( N != NULL && ( ret = mbedtls_mpi_copy( &ctx->N, N ) ) != 0 ) ||
        ( P != NULL && ( ret = mbedtls_mpi_copy( &ctx->P, P ) ) != 0 ) ||
        ( Q != NULL && ( ret = mbedtls_mpi_copy( &ctx->Q, Q ) ) != 0 ) ||
        ( D != NULL && ( ret = mbedtls_mpi_copy( &ctx->D, D ) ) != 0 ) ||
        ( E != NULL && ( ret = mbedtls_mpi_copy( &ctx->E, E ) ) != 0 ) )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );
    }

    if( N != NULL )
        ctx->len = mbedtls_mpi_size( &ctx->N );

    return( 0 );
}

int mbedtls_rsa_import_raw( mbedtls_rsa_context *ctx,
                            unsigned char const *N, size_t N_len,
                            unsigned char const *P, size_t P_len,
                            unsigned char const *Q, size_t Q_len,
                            unsigned char const *D, size_t D_len,
                            unsigned char const *E, size_t E_len )
{
    int ret = 0;

    if( N != NULL )
    {
        //Remove extra 0x00 byte if modulus size is larger than 512.
        if( ( N_len > CC_RSA_MAX_VALID_KEY_SIZE_VALUE_IN_BYTES ) && ( *N == 0x00 ) )
        {
            MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->N, N + 1, N_len - 1 ) );
            ctx->len = mbedtls_mpi_size( &ctx->N );
        }
        else
        {
            MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->N, N, N_len ) );
            ctx->len = mbedtls_mpi_size( &ctx->N );
        }
    }

    if( P != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->P, P, P_len ) );

    if( Q != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->Q, Q, Q_len ) );

    if( D != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->D, D, D_len ) );

    if( E != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_read_binary( &ctx->E, E, E_len ) );

cleanup:

    if( ret != 0 )
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );

    return( 0 );
}

/*
 * Checks whether the context fields are set in such a way
 * that the RSA primitives will be able to execute without error.
 * It does *not* make guarantees for consistency of the parameters.
 */
static int rsa_check_context( mbedtls_rsa_context const *ctx, int is_priv,
                              int blinding_needed )
{
#if !defined(MBEDTLS_RSA_NO_CRT)
    /* blinding_needed is only used for NO_CRT to decide whether
     * P,Q need to be present or not. */
    ((void) blinding_needed);
#endif

    if( ctx->len != mbedtls_mpi_size( &ctx->N ) ||
        ctx->len > MBEDTLS_MPI_MAX_SIZE )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
    }

    /*
     * 1. Modular exponentiation needs positive, odd moduli.
     */

    /* Modular exponentiation wrt. N is always used for
     * RSA public key operations. */
    if( mbedtls_mpi_cmp_int( &ctx->N, 0 ) <= 0 ||
        mbedtls_mpi_get_bit( &ctx->N, 0 ) == 0  )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
    }

#if !defined(MBEDTLS_RSA_NO_CRT)
    /* Modular exponentiation for P and Q is only
     * used for private key operations and if CRT
     * is used. */
    if( is_priv &&
        ( mbedtls_mpi_cmp_int( &ctx->P, 0 ) <= 0 ||
          mbedtls_mpi_get_bit( &ctx->P, 0 ) == 0 ||
          mbedtls_mpi_cmp_int( &ctx->Q, 0 ) <= 0 ||
          mbedtls_mpi_get_bit( &ctx->Q, 0 ) == 0  ) )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
    }
#endif /* !MBEDTLS_RSA_NO_CRT */

    /*
     * 2. Exponents must be positive
     */

    /* Always need E for public key operations */
    if( mbedtls_mpi_cmp_int( &ctx->E, 0 ) <= 0 )
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );

#if defined(MBEDTLS_RSA_NO_CRT)
    /* For private key operations, use D or DP & DQ
     * as (unblinded) exponents. */
    if( is_priv && mbedtls_mpi_cmp_int( &ctx->D, 0 ) <= 0 )
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
#else
    if( is_priv &&
        ( mbedtls_mpi_cmp_int( &ctx->DP, 0 ) <= 0 ||
          mbedtls_mpi_cmp_int( &ctx->DQ, 0 ) <= 0  ) )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
    }
#endif /* MBEDTLS_RSA_NO_CRT */

    /* Blinding shouldn't make exponents negative either,
     * so check that P, Q >= 1 if that hasn't yet been
     * done as part of 1. */
#if defined(MBEDTLS_RSA_NO_CRT)
    if( is_priv && blinding_needed &&
        ( mbedtls_mpi_cmp_int( &ctx->P, 0 ) <= 0 ||
          mbedtls_mpi_cmp_int( &ctx->Q, 0 ) <= 0 ) )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
    }
#endif

    /* It wouldn't lead to an error if it wasn't satisfied,
     * but check for QP >= 1 nonetheless. */
#if !defined(MBEDTLS_RSA_NO_CRT)
    if( is_priv &&
        mbedtls_mpi_cmp_int( &ctx->QP, 0 ) <= 0 )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );
    }
#endif

    return( 0 );
}

int mbedtls_rsa_complete( mbedtls_rsa_context *ctx )
{
    int ret = 0;

    const int have_N = ( mbedtls_mpi_cmp_int( &ctx->N, 0 ) != 0 );
    const int have_P = ( mbedtls_mpi_cmp_int( &ctx->P, 0 ) != 0 );
    const int have_Q = ( mbedtls_mpi_cmp_int( &ctx->Q, 0 ) != 0 );
    const int have_D = ( mbedtls_mpi_cmp_int( &ctx->D, 0 ) != 0 );
    const int have_E = ( mbedtls_mpi_cmp_int( &ctx->E, 0 ) != 0 );

    /*
     * Check whether provided parameters are enough
     * to deduce all others. The following incomplete
     * parameter sets for private keys are supported:
     *
     * (1) P, Q missing.
     * (2) D and potentially N missing.
     *
     */

    const int n_missing  =              have_P &&  have_Q &&  have_D && have_E;
    const int pq_missing =   have_N && !have_P && !have_Q &&  have_D && have_E;
    const int d_missing  =              have_P &&  have_Q && !have_D && have_E;
    const int is_pub     =   have_N && !have_P && !have_Q && !have_D && have_E;

    /* These three alternatives are mutually exclusive */
    const int is_priv = n_missing || pq_missing || d_missing;

    if( !is_priv && !is_pub )
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );

    /*
     * Step 1: Deduce N if P, Q are provided.
     */

    if( !have_N && have_P && have_Q )
    {
        if( ( ret = mbedtls_mpi_mul_mpi( &ctx->N, &ctx->P,
                                         &ctx->Q ) ) != 0 )
        {
            return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );
        }

        ctx->len = mbedtls_mpi_size( &ctx->N );
    }

    /*
     * Step 2: Deduce and verify all remaining core parameters.
     */

    if( pq_missing )
    {
        ret = mbedtls_rsa_deduce_primes( &ctx->N, &ctx->E, &ctx->D,
                                         &ctx->P, &ctx->Q );
        if( ret != 0 )
            return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );

    }
    else if( d_missing )
    {
        if( ( ret = mbedtls_rsa_deduce_private_exponent( &ctx->P,
                                                         &ctx->Q,
                                                         &ctx->E,
                                                         &ctx->D ) ) != 0 )
        {
            return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );
        }
    }

    /*
     * Step 3: Deduce all additional parameters specific
     *         to our current RSA implementation.
     */

#if !defined(MBEDTLS_RSA_NO_CRT)
    if( is_priv )
    {
        ret = mbedtls_rsa_deduce_crt( &ctx->P,  &ctx->Q,  &ctx->D,
                                      &ctx->DP, &ctx->DQ, &ctx->QP );
        if( ret != 0 )
            return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );
    }
#endif /* MBEDTLS_RSA_NO_CRT */

    /*
     * Step 3: Basic sanity checks
     */

    return( rsa_check_context( ctx, is_priv, 1 ) );
}

int mbedtls_rsa_export_raw( const mbedtls_rsa_context *ctx,
                            unsigned char *N, size_t N_len,
                            unsigned char *P, size_t P_len,
                            unsigned char *Q, size_t Q_len,
                            unsigned char *D, size_t D_len,
                            unsigned char *E, size_t E_len )
{
    int ret = 0;

    /* Check if key is private or public */
    const int is_priv =
        mbedtls_mpi_cmp_int( &ctx->N, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->P, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->Q, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->D, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->E, 0 ) != 0;

    if( !is_priv )
    {
        /* If we're trying to export private parameters for a public key,
         * something must be wrong. */
        if( P != NULL || Q != NULL || D != NULL )
            return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );

    }

    if( N != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &ctx->N, N, N_len ) );

    if( P != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &ctx->P, P, P_len ) );

    if( Q != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &ctx->Q, Q, Q_len ) );

    if( D != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &ctx->D, D, D_len ) );

    if( E != NULL )
        MBEDTLS_MPI_CHK( mbedtls_mpi_write_binary( &ctx->E, E, E_len ) );

cleanup:

    return( ret );
}

int mbedtls_rsa_export( const mbedtls_rsa_context *ctx,
                        mbedtls_mpi *N, mbedtls_mpi *P, mbedtls_mpi *Q,
                        mbedtls_mpi *D, mbedtls_mpi *E )
{
    int ret;

    /* Check if key is private or public */
    int is_priv =
        mbedtls_mpi_cmp_int( &ctx->N, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->P, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->Q, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->D, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->E, 0 ) != 0;

    if( !is_priv )
    {
        /* If we're trying to export private parameters for a public key,
         * something must be wrong. */
        if( P != NULL || Q != NULL || D != NULL )
            return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );

    }

    /* Export all requested core parameters. */

    if( ( N != NULL && ( ret = mbedtls_mpi_copy( N, &ctx->N ) ) != 0 ) ||
        ( P != NULL && ( ret = mbedtls_mpi_copy( P, &ctx->P ) ) != 0 ) ||
        ( Q != NULL && ( ret = mbedtls_mpi_copy( Q, &ctx->Q ) ) != 0 ) ||
        ( D != NULL && ( ret = mbedtls_mpi_copy( D, &ctx->D ) ) != 0 ) ||
        ( E != NULL && ( ret = mbedtls_mpi_copy( E, &ctx->E ) ) != 0 ) )
    {
        return( ret );
    }

    return( 0 );
}

/*
 * Export CRT parameters
 * This must also be implemented if CRT is not used, for being able to
 * write DER encoded RSA keys. The helper function mbedtls_rsa_deduce_crt
 * can be used in this case.
 */
int mbedtls_rsa_export_crt( const mbedtls_rsa_context *ctx,
                            mbedtls_mpi *DP, mbedtls_mpi *DQ, mbedtls_mpi *QP )
{
    int ret;

    /* Check if key is private or public */
    int is_priv =
        mbedtls_mpi_cmp_int( &ctx->N, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->P, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->Q, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->D, 0 ) != 0 &&
        mbedtls_mpi_cmp_int( &ctx->E, 0 ) != 0;

    if( !is_priv )
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA );

#if !defined(MBEDTLS_RSA_NO_CRT)
    /* Export all requested blinding parameters. */
    if( ( DP != NULL && ( ret = mbedtls_mpi_copy( DP, &ctx->DP ) ) != 0 ) ||
        ( DQ != NULL && ( ret = mbedtls_mpi_copy( DQ, &ctx->DQ ) ) != 0 ) ||
        ( QP != NULL && ( ret = mbedtls_mpi_copy( QP, &ctx->QP ) ) != 0 ) )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );
    }
#else
    if( ( ret = mbedtls_rsa_deduce_crt( &ctx->P, &ctx->Q, &ctx->D,
                                        DP, DQ, QP ) ) != 0 )
    {
        return( MBEDTLS_ERR_RSA_BAD_INPUT_DATA + ret );
    }
#endif

    return( 0 );
}

/*
 * Get length in bytes of RSA modulus
 */

size_t mbedtls_rsa_get_len( const mbedtls_rsa_context *ctx )
{
    return( ctx->len );
}

/**************************************************************************************/

#endif /*  defined (MBEDTLS_RSA_ALT)  */

#endif /*  defined(MBEDTLS_RSA_C)  */
