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



/************* Include Files ****************/
#include "cc_pal_types.h"
#include "cc_pal_dma.h"
#include "cc_address_defs.h"


/**
 * @brief   Initializes contiguous memory pool required for CC_PalDmaContigBufferAllocate() and CC_PalDmaContigBufferFree(). Our
 *           implementation is to mmap 0x10000000 and call to bpool(), for use of bget() in CC_PalDmaContigBufferAllocate(),
 *           and brel() in CC_PalDmaContigBufferFree().
 *
 * @param[in] buffSize - buffer size in Bytes
 * @param[in] physBuffAddr - physical start address of the memory to map
 *
 * @return Returns a non-zero value in case of failure
 */
uint32_t CC_PalDmaInit(uint32_t  buffSize,
                        CCDmaAddr_t  physBuffAddr)
{
    CC_UNUSED_PARAM(buffSize);
    CC_UNUSED_PARAM(physBuffAddr);

    return 0;
}

/**
 * @brief   free system resources created in PD_PAL_DmaInit()
 *
 * @param[in] buffSize - buffer size in Bytes
 *
 * @return void
 */
void CC_PalDmaTerminate(void)
{
    return;
}

#ifndef CC_IOT
/**
 * @brief   Maps a given buffer of any type. Returns the list of DMA-able blocks that the buffer maps to.
 *
 * @param[in] pDataBuffer -  Address of the buffer to map
 * @param[in] buffSize - Buffer size in bytes
 * @param[in] copyDirection - Copy direction of the buffer. Can be TO_DEVICE, FROM_DEVICE or BI_DIRECTION
 * @param[in/out] numOfBlocks - maximum numOfBlocks to fill, as output the actual number
 * @param[out] pDmaBlockList - List of DMA-able blocks that the buffer maps to
 * @param[out] dmaBuffHandle - A handle to the mapped buffer private resources
 *
 * @return Returns a non-zero value in case of failure
 */
uint32_t CC_PalDmaBufferMap(uint8_t                   *pDataBuffer,
                 uint32_t                     buffSize,
                 CCPalDmaBufferDirection_t  copyDirection,
                 uint32_t                     *pNumOfBlocks,
                 CCPalDmaBlockInfo_t        *pDmaBlockList,
                 CC_PalDmaBufferHandle       *dmaBuffHandle)
{

    return (-1);
}

/**
 * @brief   Unmaps a given buffer, and frees its associated resources, if exist
 *
 * @param[in] pDataBuffer -  Address of the buffer to map
 * @param[in] buffSize - Buffer size in bytes
 * @param[in] copyDirection - Copy direction of the buffer. Can be TO_DEVICE, FROM_DEVICE or BI_DIRECTION
 * @param[in] numOfBlocks - Number of DMA-able blocks that the buffer maps to
 * @param[in] pDmaBlockList - List of DMA-able blocks that the buffer maps to
 * @param[in] dmaBuffHandle - A handle to the mapped buffer private resources
 *
 * @return Returns a non-zero value in case of failure
 */
uint32_t CC_PalDmaBufferUnmap(uint8_t                     *pDataBuffer,
                 uint32_t                     buffSize,
                 CCPalDmaBufferDirection_t  copyDirection,
                 uint32_t                     numOfBlocks,
                 CCPalDmaBlockInfo_t        *pDmaBlockList,
                 CC_PalDmaBufferHandle       dmaBuffHandle)
{
    return (-1);
}



/**
 * @brief   Allocates a DMA-contiguous buffer, and returns both its physical and virtual addresses
 *
 *
 * @param[in] buffSize - Buffer size in bytes
 * @param[out] ppVirtBuffAddr - Virtual address of the allocated buffer
 *
 * @return Returns a non-zero value in case of failure
 */
uint32_t CC_PalDmaContigBufferAllocate(uint32_t          buffSize,
                    uint8_t          **ppVirtBuffAddr)
{
    return (-1);
}



/**
 * @brief   free resources previuosly allocated by CC_PalDmaContigBufferAllocate
 *
 *
 * @param[in] buffSize - buffer size in Bytes
 * @param[in] pVirtBuffAddr - virtual address of the buffer to free
 *
 * @return success/fail
 */
uint32_t CC_PalDmaContigBufferFree(uint32_t          buffSize,
                    uint8_t          *pVirtBuffAddr)
{
    return (-1);
}


/**
 * @brief   Returns TRUE if the buffer is guaranteed to be a single contiguous DMA block, and FALSE otherwise.
 *
 *
 * @param[in] pDataBuffer - User buffer address
 * @param[in] buffSize - User buffer size
 *
 * @return Returns TRUE if the buffer is guaranteed to be a single contiguous DMA block, and FALSE otherwise.
 */
uint32_t CC_PalIsDmaBufferContiguous(uint8_t       *pDataBuffer,
                      uint32_t       buffSize)
{
    return (-1);
}

#endif
