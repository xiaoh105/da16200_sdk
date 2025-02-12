/**
 ****************************************************************************************
 *
 * @file disc.h
 *
 * @brief Header file - Device Information Service - Client Role.
 *
 * Copyright (C) RivieraWaves 2009-2015
 *
 *
 ****************************************************************************************
 */

#ifndef DISC_H_
#define DISC_H_

/**
 ****************************************************************************************
 * @addtogroup DIS Device Information Service
 * @ingroup PROFILE
 * @brief Device Information Service
 *****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup DISC Device Information Service Client
 * @ingroup DIS
 * @brief Device Information Service Client
 * @{
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "../../../../../platform/core_modules/rwip/api/rwip_config.h"
#if (BLE_DIS_CLIENT)
#include <stdint.h>
#include <stdbool.h>
#include "../../../../../platform/core_modules/ke/api/ke_task.h"
#include "prf_utils.h"

/*
 * DEFINES
 ****************************************************************************************
 */

///Maximum number of Device Information Service Client task instances
#define DISC_IDX_MAX    (BLE_CONNECTION_MAX)

enum
{
    DISC_MANUFACTURER_NAME_CHAR,
    DISC_MODEL_NB_STR_CHAR,
    DISC_SERIAL_NB_STR_CHAR,
    DISC_HARD_REV_STR_CHAR,
    DISC_FIRM_REV_STR_CHAR,
    DISC_SW_REV_STR_CHAR,
    DISC_SYSTEM_ID_CHAR,
    DISC_IEEE_CHAR,
    DISC_PNP_ID_CHAR,

    DISC_CHAR_MAX,
};

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

/**
 * Structure containing the characteristics handles, value handles and descriptors for
 * the Device Information Service
 */
struct disc_dis_content
{
    /// service info
    struct prf_svc svc;

    /// Characteristic info:
    struct prf_char_inf chars[DISC_CHAR_MAX];
};

/// Environment variable for each Connections
struct disc_cnx_env
{
    ///Current Time Service Characteristics
    struct disc_dis_content dis;
    /// Last char. code requested to read.
    uint8_t  last_char_code;
    /// counter used to check service uniqueness
    uint8_t  nb_svc;
};

/// Device Information Service Client environment variable
struct disc_env_tag
{
    /// profile environment
    prf_env_t prf_env;
    /// Environment variable pointer for each connections
    struct disc_cnx_env* env[DISC_IDX_MAX];
    /// State of different task instances
    ke_state_t state[DISC_IDX_MAX];
};

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

/*
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */


/**
 ****************************************************************************************
 * @brief Retrieve DIS client profile interface
 *
 * @return DIS client profile interface
 ****************************************************************************************
 */
const struct prf_task_cbs* disc_prf_itf_get(void);

/**
 ****************************************************************************************
 * @brief Send Thermometer ATT DB discovery results to HTPC host.
 ****************************************************************************************
 */
void disc_enable_rsp_send(struct disc_env_tag *disc_env, uint8_t conidx, uint8_t status);

#endif //BLE_DIS_CLIENT

/// @} DISC

#endif // DISC_H_
