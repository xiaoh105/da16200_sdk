/**
 ****************************************************************************************
 *
 * @file ke_env.h
 *
 * @brief This file contains the definition of the kernel.
 *
 * Copyright (C) RivieraWaves 2009-2014
 *
 *
 ****************************************************************************************
 */

#ifndef _KE_ENV_H_
#define _KE_ENV_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "../../../../platform/core_modules/common/api/co_list.h"              // kernel queue definition
#include "../../../../platform/core_modules/ke/api/ke_config.h"            // kernel configuration
#include "../../../../platform/core_modules/ke/api/ke_event.h"             // kernel event
#include "../../../../platform/core_modules/rwip/api/rwip_config.h"          // stack configuration

// forward declaration
struct mblock_free;

/// Kernel environment definition
struct ke_env_tag
{
    /// Queue of sent messages but not yet delivered to receiver
    struct co_list queue_sent;
    /// Queue of messages delivered but not consumed by receiver
    struct co_list queue_saved;
    /// Queue of timers
    struct co_list queue_timer;

    #if (KE_MEM_RW)
    /// Root pointer = pointer to first element of heap linked lists
    struct mblock_free * heap[KE_MEM_BLOCK_MAX];
    /// Size of heaps
    uint16_t heap_size[KE_MEM_BLOCK_MAX];

    #if (KE_PROFILING)
    /// Size of heap used
    uint16_t heap_used[KE_MEM_BLOCK_MAX];
    /// Maximum heap memory used
    uint32_t max_heap_used;
    #endif //KE_PROFILING
    #endif //KE_MEM_RW
};

/// Kernel environment
extern struct ke_env_tag ke_env;

#endif // _KE_ENV_H_
