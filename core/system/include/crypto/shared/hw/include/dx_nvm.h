/****************************************************************************
* The confidential and proprietary information contained in this file may    *
* only be used by a person authorised under and to the extent permitted      *
* by a subsisting licensing agreement from ARM Limited or its affiliates.    *
*   (C) COPYRIGHT [2001-2017] ARM Limited or its affiliates.                 *
*       ALL RIGHTS RESERVED                                                  *
* This entire notice must be reproduced on all copies of this file           *
* and copies of this file may only be made by a person if such person is     *
* permitted to do so under the terms of a subsisting license agreement       *
* from ARM Limited or its affiliates.                                        *
*****************************************************************************/

#ifndef __DX_NVM_H__
#define __DX_NVM_H__

// --------------------------------------
// BLOCK: NVM
// --------------------------------------
#define DX_AIB_FUSE_PROG_COMPLETED_REG_OFFSET   0x1F04UL
#define DX_AIB_FUSE_PROG_COMPLETED_VALUE_BIT_SHIFT  0x0UL
#define DX_AIB_FUSE_PROG_COMPLETED_VALUE_BIT_SIZE   0x1UL
#define DX_NVM_DEBUG_STATUS_REG_OFFSET  0x1F08UL
#define DX_NVM_DEBUG_STATUS_VALUE_BIT_SHIFT     0x1UL
#define DX_NVM_DEBUG_STATUS_VALUE_BIT_SIZE  0x3UL
#define DX_LCS_IS_VALID_REG_OFFSET  0x1F0CUL
#define DX_LCS_IS_VALID_VALUE_BIT_SHIFT     0x0UL
#define DX_LCS_IS_VALID_VALUE_BIT_SIZE  0x1UL
#define DX_NVM_IS_IDLE_REG_OFFSET   0x1F10UL
#define DX_NVM_IS_IDLE_VALUE_BIT_SHIFT  0x0UL
#define DX_NVM_IS_IDLE_VALUE_BIT_SIZE   0x1UL
#define DX_LCS_REG_REG_OFFSET   0x1F14UL
#define DX_LCS_REG_LCS_REG_BIT_SHIFT    0x0UL
#define DX_LCS_REG_LCS_REG_BIT_SIZE     0x3UL
#define DX_LCS_REG_ERROR_KDR_ZERO_CNT_BIT_SHIFT     0x8UL
#define DX_LCS_REG_ERROR_KDR_ZERO_CNT_BIT_SIZE  0x1UL
#define DX_LCS_REG_ERROR_PROV_ZERO_CNT_BIT_SHIFT    0x9UL
#define DX_LCS_REG_ERROR_PROV_ZERO_CNT_BIT_SIZE     0x1UL
#define DX_LCS_REG_ERROR_KCE_ZERO_CNT_BIT_SHIFT     0xAUL
#define DX_LCS_REG_ERROR_KCE_ZERO_CNT_BIT_SIZE  0x1UL
#define DX_LCS_REG_ERROR_KPICV_ZERO_CNT_BIT_SHIFT   0xBUL
#define DX_LCS_REG_ERROR_KPICV_ZERO_CNT_BIT_SIZE    0x1UL
#define DX_LCS_REG_ERROR_KCEICV_ZERO_CNT_BIT_SHIFT  0xCUL
#define DX_LCS_REG_ERROR_KCEICV_ZERO_CNT_BIT_SIZE   0x1UL
#define DX_HOST_SHADOW_KDR_REG_REG_OFFSET   0x1F18UL
#define DX_HOST_SHADOW_KDR_REG_VALUE_BIT_SHIFT  0x0UL
#define DX_HOST_SHADOW_KDR_REG_VALUE_BIT_SIZE   0x1UL
#define DX_HOST_SHADOW_KCP_REG_REG_OFFSET   0x1F1CUL
#define DX_HOST_SHADOW_KCP_REG_VALUE_BIT_SHIFT  0x0UL
#define DX_HOST_SHADOW_KCP_REG_VALUE_BIT_SIZE   0x1UL
#define DX_HOST_SHADOW_KCE_REG_REG_OFFSET   0x1F20UL
#define DX_HOST_SHADOW_KCE_REG_VALUE_BIT_SHIFT  0x0UL
#define DX_HOST_SHADOW_KCE_REG_VALUE_BIT_SIZE   0x1UL
#define DX_HOST_SHADOW_KPICV_REG_REG_OFFSET     0x1F24UL
#define DX_HOST_SHADOW_KPICV_REG_VALUE_BIT_SHIFT    0x0UL
#define DX_HOST_SHADOW_KPICV_REG_VALUE_BIT_SIZE     0x1UL
#define DX_HOST_SHADOW_KCEICV_REG_REG_OFFSET    0x1F28UL
#define DX_HOST_SHADOW_KCEICV_REG_VALUE_BIT_SHIFT   0x0UL
#define DX_HOST_SHADOW_KCEICV_REG_VALUE_BIT_SIZE    0x1UL
#define DX_OTP_ADDR_WIDTH_DEF_REG_OFFSET    0x1F2CUL
#define DX_OTP_ADDR_WIDTH_DEF_VALUE_BIT_SHIFT   0x0UL
#define DX_OTP_ADDR_WIDTH_DEF_VALUE_BIT_SIZE    0x4UL

#endif //__DX_NVM_H__

