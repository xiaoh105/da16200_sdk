/**
 ****************************************************************************************
 * @addtogroup Core_Modules
 * @{
 * @addtogroup KERNEL
 * @{
 * @addtogroup KE_Task Kernel Task
 * @brief Module for managing kernel tasks
 * @{
 *
 * This module implements the functions used for managing tasks.
 *
 * @file ke_task.h
 *
 * @brief This file contains the definition related to kernel task management.
 *
 * Copyright (C) RivieraWaves 2009-2014
 *
 *
 ****************************************************************************************
 */

#ifndef _KE_TASK_H_
#define _KE_TASK_H_

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stdint.h>          // standard integer
#include <stdbool.h>         // standard boolean

#include "../../../arch/compiler/compiler.h"        // compiler defines, INLINE
#include "../../../core_modules/ke/api/ke_msg.h"          // kernel message defines
#include "../../../core_modules/rwip/api/rwip_config.h"     // stack configuration
 
/// Default Message handler code to handle several message type in same handler.
#define KE_MSG_DEFAULT_HANDLER  (0xFFFF)
/// Invalid task
#define KE_TASK_INVALID         (0xFFFF)
/// Used to know if a message is not present in kernel queue
#define KE_MSG_NOT_IN_QUEUE     ((struct co_list_hdr *) 0xFFFFFFFF)

/// Status of ke_task API functions
enum KE_TASK_STATUS
{
    KE_TASK_OK = 0,
    KE_TASK_FAIL,
    KE_TASK_UNKNOWN,
    KE_TASK_CAPA_EXCEEDED,
    KE_TASK_ALREADY_EXISTS,
};


/// Build the first message ID of a task.
#define KE_FIRST_MSG_BLE(task) ((ke_msg_id_t)((task) << 8))
/// Retrieve the task Type from message
#define MSG_T(msg)         ((ke_task_id_t)((msg) >> 8))
/// Retrieve the task ID from message
#define MSG_I(msg)         ((msg) & ((1<<8)-1))

/// Format of a task message handler function
typedef int (*ke_msg_func_t)(ke_msg_id_t const msgid, void const *param,
                             ke_task_id_t const dest_id, ke_task_id_t const src_id);

/// Macro for message handler function declaration or definition
#define KE_MSG_HANDLER(module, fname)   static int module##_##fname##_handler(ke_msg_id_t const msgid,     \
                                                                              void const *param,           \
                                                                              ke_task_id_t const dest_id,  \
                                                                              ke_task_id_t const src_id)

/// Custom message handlers
struct custom_msg_handler 
{
    /// Id of the destination task. This is used because some messages have duplicate handlers like LLD_DATA_IND and LLD_STOP_IND.
    /// Only the type is taken into account and not the index for tasks that have multiple instances
    ke_task_id_t task_id;
    /// Id of the handled message. Can also be KE_MSG_DEFAULT_HANDLER to direct all messages to one handler for the particular task_id
    /// The search is top down so KE_MSG_DEFAULT_HANDLER should be after other handlers for specific messages for the particular task
    ke_msg_id_t id;
    /// Pointer to the handler function for the msgid above.
    ke_msg_func_t func;
};

/// Element of a message handler table.
struct ke_msg_handler
{
    /// Id of the handled message.
    ke_msg_id_t id;
    /// Pointer to the handler function for the msgid above.
    ke_msg_func_t func;
};

/// Element of a state handler table.
struct ke_state_handler
{
    /// Pointer to the message handler table of this state.
    const struct ke_msg_handler *msg_table;
    /// Number of messages handled in this state.
    uint16_t msg_cnt;
};

/// Helps writing the initialization of the state handlers without errors.
#define KE_STATE_HANDLER(hdl) {hdl, sizeof(hdl)/sizeof(struct ke_msg_handler)}

/// Helps writing empty states.
#define KE_STATE_HANDLER_NONE {NULL, 0}

/// Task descriptor grouping all information required by the kernel for the scheduling.
struct ke_task_desc
{
    /// Pointer to the state handler table (one element for each state).
    const struct ke_state_handler* state_handler;
    /// Pointer to the default state handler (element parsed after the current state).
    const struct ke_state_handler* default_handler;
    /// Pointer to the state table (one element for each instance).
    ke_state_t* state;
    /// Maximum number of states in the task.
    uint16_t state_max;
    /// Maximum index of supported instances of the task.
    uint16_t idx_max;
};

/*
 * FUNCTION PROTOTYPES
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Initialize Kernel task module.
 ****************************************************************************************
 */
void ke_task_init(void);

/**
 ****************************************************************************************
 * @brief Create a task.
 *
 * @param[in]  task_type       Task type.
 * @param[in]  p_task_desc     Pointer to task descriptor.
 *
 * @return                     Status
 ****************************************************************************************
 */
uint8_t ke_task_create(uint8_t task_type, struct ke_task_desc const * p_task_desc);

/**
 ****************************************************************************************
 * @brief Delete a task.
 *
 * @param[in]  task_type       Task type.
 *
 * @return                     Status
 ****************************************************************************************
 */
uint8_t ke_task_delete(uint8_t task_type);

/**
 ****************************************************************************************
 * @brief Retrieve the state of a task.
 *
 * @param[in]  id   Task id.
 *
 * @return          Current state of the task
 ****************************************************************************************
 */
ke_state_t ke_state_get(ke_task_id_t const id);

/**
 ****************************************************************************************
 * @brief Set the state of the task identified by its Task Id.
 *
 * In this function we also handle the SAVE service: when a task state changes we
 * try to activate all the messages currently saved in the save queue for the given
 * task identifier.
 *
 * @param[in]  id          Identifier of the task instance whose state is going to be modified
 * @param[in]  state_id    New State
 *
 ****************************************************************************************
 */
void ke_state_set(ke_task_id_t const id, ke_state_t const state_id);

/**
 ****************************************************************************************
 * @brief Generic message handler to consume message without handling it in the task.
 *
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 *
 * @return KE_MSG_CONSUMED
 ****************************************************************************************
 */
int ke_msg_discard(ke_msg_id_t const msgid, void const *param,
                   ke_task_id_t const dest_id, ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Generic message handler to consume message without handling it in the task.
 *
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 *
 * @return KE_MSG_CONSUMED
 ****************************************************************************************
 */
int ke_msg_save(ke_msg_id_t const msgid, void const *param,
                ke_task_id_t const dest_id, ke_task_id_t const src_id);



/**
 ****************************************************************************************
 * @brief This function flushes all messages, currently pending in the kernel for a
 * specific task.
 *
 * @param[in] task The Task Identifier that shall be flushed.
 ****************************************************************************************
 */
void ke_task_msg_flush(ke_task_id_t task);

#endif // _KE_TASK_H_

/// @}
/// @}
/// @}
