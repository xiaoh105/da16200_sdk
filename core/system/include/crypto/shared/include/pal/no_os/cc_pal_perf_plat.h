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

#ifndef _CC_PAL_PERF_PLAT_H__
#define _CC_PAL_PERF_PLAT_H__

typedef unsigned int CCPalPerfData_t;

/**
 * @brief   DSM environment bug - sometimes very long write operation.
 *     to overcome this bug we added while to make sure write opeartion is completed
 *
 * @param[in]
 * *
 * @return None
 */
void CC_PalDsmWorkarround();


#endif /*_CC_PAL_PERF_PLAT_H__*/
