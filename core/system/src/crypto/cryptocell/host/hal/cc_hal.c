/****************************************************************************
 * The confidential and proprietary information contained in this file may    *
 * only be used by a person authorised under and to the extent permitted      *
 * by a subsisting licensing agreement from ARM Limited or its affiliates.    *
 *  (C) COPYRIGHT [2001-2017] ARM Limited or its affiliates.         *
 *      ALL RIGHTS RESERVED                          *
 * This entire notice must be reproduced on all copies of this file           *
 * and copies of this file may only be made by a person if such person is     *
 * permitted to do so under the terms of a subsisting license agreement       *
 * from ARM Limited or its affiliates.                       *
 *****************************************************************************/

#define CC_PAL_LOG_CUR_COMPONENT CC_LOG_MASK_CCLIB

#include "cc_regs.h"
#include "cc_pal_memmap.h"
#include "cc_hal.h"
#include "dx_crys_kernel.h"
#include "cc_pal_abort.h"
#include "cc_error.h"
#include "cc_regs.h"

#include "cc_pal_interrupt_ctrl_plat.h"
#include "dx_rng.h"

#include "dx_reg_base_host.h"

/******************************************************************************
*               DEFINITIONS
******************************************************************************/
#define DX_CC_REG_AREA_LEN 0x100000

/******************************************************************************
*               GLOBALS
******************************************************************************/

//REMOVE: unsigned long gCcRegBase = 0;

/******************************************************************************
*               FUNCTIONS
******************************************************************************/

/*!
 * HAL layer entry point.
 * Mappes ARM CryptoCell regisers to the HOST virtual address space.
 */
int CC_HalInit(void)
{
    unsigned long *pVirtBuffAddr = NULL;

    CC_PalMemMap(DX_BASE_CC, DX_CC_REG_AREA_LEN, (uint32_t**)&pVirtBuffAddr);
//REMOVE:	 gCcRegBase = (unsigned long)pVirtBuffAddr;
    return 0;
}


/*!
 * HAL exit point.
 * Unmaps ARM CryptoCell registers.
 */
int CC_HalTerminate(void)
{
	CC_PalMemUnMap(((uint32_t *)DA16X_ACRYPT_BASE), DX_CC_REG_AREA_LEN);
//REMOVE:	gCcRegBase = 0;
    return CC_HAL_OK;
}


void CC_HalClearInterruptBit(uint32_t data)
{

    CC_HAL_WRITE_REGISTER( CC_REG_OFFSET(HOST_RGF, HOST_ICR), data);

    return;
}

void CC_HalMaskInterrupt(uint32_t data)
{
    CC_HAL_WRITE_REGISTER( CC_REG_OFFSET(HOST_RGF, HOST_IMR), data);

    return;
}

/*!
 * Wait upon Interrupt Request Register (IRR) signals.
 * This function notifies for any ARM CryptoCell interrupt, it is the caller responsibility
 * to verify and prompt the expected case interrupt source.
 *
 * @param[in] data  - input data for future use
 * \return CCError_t    - CC_OK upon success
 */
CCError_t CC_HalWaitInterrupt(uint32_t data)
{
    CCError_t error = CC_OK;
    if (0 == data) {
        return CC_FATAL_ERROR;
    }
    error = CC_PalWaitInterrupt( data );

    return error;
}



CCError_t CC_HalWaitInterruptRND(uint32_t data)
{
    uint32_t irr = 0;
    CCError_t error = CC_OK;
    if (0 == data) {
        return CC_FATAL_ERROR;
    }

    /* busy wait upon IRR signal */
    do {
        irr = CC_HAL_READ_REGISTER(CC_REG_OFFSET(HOST_RGF, HOST_IRR));
        /* check APB bus error from HOST */
        if( CC_REG_FLD_GET(0, HOST_IRR, AHB_ERR_INT, irr) == CC_TRUE){
            error = CC_FATAL_ERROR;
            /*set data for clearing bus error*/
            CC_REG_FLD_SET(HOST_RGF, HOST_ICR, AXI_ERR_CLEAR, data , 1);
            break;
        }
    } while (!(irr & data));

    /* clear interrupt */
    CC_HAL_WRITE_REGISTER(CC_REG_OFFSET(HOST_RGF, HOST_ICR), data); // IRR and ICR bit map is the same use data to clear interrupt in ICR

    return error;
}
