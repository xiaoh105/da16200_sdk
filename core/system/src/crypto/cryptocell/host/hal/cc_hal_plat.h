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


#ifndef __CC_HAL_PLAT_H__
#define __CC_HAL_PLAT_H__

#include "da16200_map.h"

#include "dx_host.h"
#include "cc_bitops.h"

#include "dx_reg_common.h" /*temporary (missing HW defines)*/

#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

/******************************************************************************
*               DEFINITIONS
******************************************************************************/
#define CC_LARGE_SECRET_KEY_NUM_OF_BYTES 32
#define CC_SMALL_SECRET_KEY_NUM_OF_BYTES 16

/* Peripheral ID registers values */
#define CC_BSV_PID_0_VAL        0x000000C0UL
#define CC_BSV_PID_1_VAL        0x000000B0UL
#define CC_BSV_PID_2_VAL        0x0000000BUL
#define CC_BSV_PID_3_VAL        0x00000000UL
#define CC_BSV_PID_4_VAL        0x00000004UL
#define CC_BSV_PID_SIZE_WORDS       5

/* Component ID registers values */
#define CC_BSV_CID_0_VAL        0x0DUL
#define CC_BSV_CID_1_VAL        0xF0UL
#define CC_BSV_CID_2_VAL        0x05UL
#define CC_BSV_CID_3_VAL        0xB1UL
#define CC_BSV_CID_SIZE_WORDS       4


/******************************************************************************
*                               MACROS
******************************************************************************/
//REMOVE: extern unsigned long gCcRegBase;

/******************************************************************************
*                               MACROS
******************************************************************************/
/*get the size of the RKEK from HW */
//(key_size >> DX_NVM_CC_BOOT_LARGE_RKEK_LOCAL_BIT_SHIFT) & DX_NVM_CC_BOOT_LARGE_RKEK_LOCAL_BIT_SIZE
#define GET_ROOT_KEY_SIZE(key_size) \
do{ \
    key_size = CC_HAL_READ_REGISTER(CC_REG_OFFSET(CRY_KERNEL, NVM_CC_BOOT));\
        if (CC_REG_FLD_GET(CRY_KERNEL, NVM_CC_BOOT, LARGE_RKEK_LOCAL, key_size)) \
                key_size = CC_LARGE_SECRET_KEY_NUM_OF_BYTES; \
        else \
                key_size = CC_SMALL_SECRET_KEY_NUM_OF_BYTES; \
}while (0)


/*!
 * Read CryptoCell memory-mapped-IO register.
 *
 * \param regOffset The offset of the ARM CryptoCell register to read
 * \return uint32_t Return the value of the given register
 */
#define CC_HAL_READ_REGISTER(regOffset)                 \
		(*((volatile uint32_t *)(DA16X_ACRYPT_BASE | (regOffset))))

/*!
 * Write CryptoCell memory-mapped-IO register.
 * \note This macro must be modified to make the operation synchronous, i.e. the write operation must complete,
 *       and the new value must be written to the register before the macro returns. The mechanisms required to
 *       achieve this are architecture-dependent (e.g., the memory barrier in ARM architecture).
 *
 * \param regOffset The offset of the ARM CryptoCell register to write
 * \param val The value to write
 */
#define CC_HAL_WRITE_REGISTER(regOffset, val)       \
		(*((volatile uint32_t *)(DA16X_ACRYPT_BASE | (regOffset))) = (val))



#endif /*__CC_HAL_PLAT_H__*/

