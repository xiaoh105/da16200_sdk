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

#ifndef __DX_ID_REGS_H__
#define __DX_ID_REGS_H__

// --------------------------------------
// BLOCK: ID_REGISTERS
// --------------------------------------
#define DX_PERIPHERAL_ID_4_REG_OFFSET   0x0FD0UL
#define DX_PERIPHERAL_ID_4_VALUE_BIT_SHIFT  0x0UL
#define DX_PERIPHERAL_ID_4_VALUE_BIT_SIZE   0x4UL
#define DX_PIDRESERVED0_REG_OFFSET  0x0FD4UL
#define DX_PIDRESERVED1_REG_OFFSET  0x0FD8UL
#define DX_PIDRESERVED2_REG_OFFSET  0x0FDCUL
#define DX_PERIPHERAL_ID_0_REG_OFFSET   0x0FE0UL
#define DX_PERIPHERAL_ID_0_VALUE_BIT_SHIFT  0x0UL
#define DX_PERIPHERAL_ID_0_VALUE_BIT_SIZE   0x8UL
#define DX_PERIPHERAL_ID_1_REG_OFFSET   0x0FE4UL
#define DX_PERIPHERAL_ID_1_PART_1_BIT_SHIFT     0x0UL
#define DX_PERIPHERAL_ID_1_PART_1_BIT_SIZE  0x4UL
#define DX_PERIPHERAL_ID_1_DES_0_JEP106_BIT_SHIFT   0x4UL
#define DX_PERIPHERAL_ID_1_DES_0_JEP106_BIT_SIZE    0x4UL
#define DX_PERIPHERAL_ID_2_REG_OFFSET   0x0FE8UL
#define DX_PERIPHERAL_ID_2_DES_1_JEP106_BIT_SHIFT   0x0UL
#define DX_PERIPHERAL_ID_2_DES_1_JEP106_BIT_SIZE    0x3UL
#define DX_PERIPHERAL_ID_2_JEDEC_BIT_SHIFT  0x3UL
#define DX_PERIPHERAL_ID_2_JEDEC_BIT_SIZE   0x1UL
#define DX_PERIPHERAL_ID_2_REVISION_BIT_SHIFT   0x4UL
#define DX_PERIPHERAL_ID_2_REVISION_BIT_SIZE    0x4UL
#define DX_PERIPHERAL_ID_3_REG_OFFSET   0x0FECUL
#define DX_PERIPHERAL_ID_3_CMOD_BIT_SHIFT   0x0UL
#define DX_PERIPHERAL_ID_3_CMOD_BIT_SIZE    0x4UL
#define DX_PERIPHERAL_ID_3_REVAND_BIT_SHIFT     0x4UL
#define DX_PERIPHERAL_ID_3_REVAND_BIT_SIZE  0x4UL
#define DX_COMPONENT_ID_0_REG_OFFSET    0x0FF0UL
#define DX_COMPONENT_ID_0_VALUE_BIT_SHIFT   0x0UL
#define DX_COMPONENT_ID_0_VALUE_BIT_SIZE    0x8UL
#define DX_COMPONENT_ID_1_REG_OFFSET    0x0FF4UL
#define DX_COMPONENT_ID_1_PRMBL_1_BIT_SHIFT     0x0UL
#define DX_COMPONENT_ID_1_PRMBL_1_BIT_SIZE  0x4UL
#define DX_COMPONENT_ID_1_CLASS_BIT_SHIFT   0x4UL
#define DX_COMPONENT_ID_1_CLASS_BIT_SIZE    0x4UL
#define DX_COMPONENT_ID_2_REG_OFFSET    0x0FF8UL
#define DX_COMPONENT_ID_2_VALUE_BIT_SHIFT   0x0UL
#define DX_COMPONENT_ID_2_VALUE_BIT_SIZE    0x8UL
#define DX_COMPONENT_ID_3_REG_OFFSET    0x0FFCUL
#define DX_COMPONENT_ID_3_VALUE_BIT_SHIFT   0x0UL
#define DX_COMPONENT_ID_3_VALUE_BIT_SIZE    0x8UL

#endif //__DX_ID_REGS_H_

