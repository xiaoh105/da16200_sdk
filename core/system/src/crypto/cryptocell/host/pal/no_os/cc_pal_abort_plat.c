/****************************************************************************
* The confidential and proprietary information contained in this file may    *
* only be used by a person authorised under and to the extent permitted      *
* by a subsisting licensing agreement from ARM Limited or its affiliates.    *
*   (C) COPYRIGHT [2017] ARM Limited or its affiliates.         *
*       ALL RIGHTS RESERVED                          *
* This entire notice must be reproduced on all copies of this file           *
* and copies of this file may only be made by a person if such person is     *
* permitted to do so under the terms of a subsisting license agreement       *
* from ARM Limited or its affiliates.                        *
*****************************************************************************/
#include "cc_pal_log.h"
#include "stdlib.h"

void CC_PalAbort(const char * exp)
{
    CC_PAL_LOG_ERR("ASSERT:%s:%d: %s", __func__, __LINE__, exp);
    CC_UNUSED_PARAM(exp); /* to avoid compilation error in case DEBUG isn't defined*/
    abort();
}
