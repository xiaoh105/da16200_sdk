/**
 ****************************************************************************************
 *
 * @file udp_server_dpm_sample.c
 *
 * @brief Sample app of UDP Server in DPM mode.
 *
 * Copyright (c) 2016-2022 Renesas Electronics. All rights reserved.
 *
 * This software ("Software") is owned by Renesas Electronics.
 *
 * By using this Software you agree that Renesas Electronics retains all
 * intellectual property and proprietary rights in and to this Software and any
 * use, reproduction, disclosure or distribution of the Software without express
 * written permission or a license agreement from Renesas Electronics is
 * strictly prohibited. This Software is solely for use on or in conjunction
 * with Renesas Electronics products.
 *
 * EXCEPT AS OTHERWISE PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, THE
 * SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. EXCEPT AS OTHERWISE
 * PROVIDED IN A LICENSE AGREEMENT BETWEEN THE PARTIES, IN NO EVENT SHALL
 * RENESAS ELECTRONICS SEMICONDUCTOR BE LIABLE FOR ANY DIRECT, SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THE SOFTWARE.
 *
 ****************************************************************************************
 */

#include "sdk_type.h"
#include "sample_defs.h"

#if defined (__UDP_SERVER_DPM_SAMPLE__)			\
		&& defined (__SUPPORT_DPM_MANAGER__)	\
		&& !defined (__LIGHT_DPM_MANAGER__)

#include "da16x_system.h"
#include "common_utils.h"
#include "common_def.h"

#include "user_dpm_manager.h"

//Defalut UDP Server configuration
#define UDP_SERVER_DPM_SAMPLE_DEF_SERVER_PORT			UDP_SVR_TEST_PORT

//NVRAM
#define UDP_SERVER_DPM_SAMPLE_NVRAM_SERVER_PORT_NAME	"UDP_SERVER_PORT"

//Hex Dump
#undef	UDP_SERVER_DPM_SAMPLE_ENABLED_HEXDUMP

//UDP Server inforamtion
typedef struct {
    int port;
} udp_server_dpm_sample_svr_info_t;

//Function prototype
void udp_server_dpm_sample_init_callback();
void udp_server_dpm_sample_wakeup_callback();
void udp_server_dpm_sample_error_callback(UINT error_code, char *comment);
void udp_server_dpm_sample_connect_callback(void *sock, UINT conn_status);
void udp_server_dpm_sample_recv_callback(void *sock, UCHAR *rx_buf, UINT rx_len,
        ULONG rx_ip, ULONG rx_port);
void udp_server_dpm_sample_init_user_config(dpm_user_config_t *user_config);
unsigned char udp_server_dpm_sample_load_server_info();
void udp_server_dpm_sample(ULONG arg);

//Global variable
udp_server_dpm_sample_svr_info_t srv_info = {0x00,};

//Function
static void udp_server_dpm_sample_hex_dump(const char *title, UCHAR *buf, size_t len)
{
#if defined (UDP_SERVER_DPM_SAMPLE_ENABLED_HEXDUMP)
    extern void hex_dump(UCHAR * data, UINT length);

    PRINTF("%s(%ld)\n", title, len);

    hex_dump(buf, len);
#else
    DA16X_UNUSED_ARG(title);
    DA16X_UNUSED_ARG(buf);
    DA16X_UNUSED_ARG(len);
#endif // (UDP_SERVER_DPM_SAMPLE_ENABLED_HEXDUMP)

    return ;
}

void udp_server_dpm_sample_init_callback()
{
    PRINTF(GREEN_COLOR " [%s] Boot initialization\n" CLEAR_COLOR, __func__);

    dpm_mng_job_done(); //Done opertaion
}

void udp_server_dpm_sample_wakeup_callback()
{
    PRINTF(GREEN_COLOR " [%s] DPM Wakeup\n" CLEAR_COLOR, __func__);

    dpm_mng_job_done(); //Done opertaion
}

void udp_server_dpm_sample_external_callback()
{
    PRINTF(GREEN_COLOR " [%s] External DPM Wakeup\n" CLEAR_COLOR, __func__);

    dpm_mng_job_done(); //Done opertaion
}

void udp_server_dpm_sample_error_callback(UINT error_code, char *comment)
{
    PRINTF(RED_COLOR " [%s] Error Callback(%s - 0x%0x) \n" CLEAR_COLOR,
           __func__, comment, error_code);
}

void udp_server_dpm_sample_connect_callback(void *sock, UINT conn_status)
{
    DA16X_UNUSED_ARG(sock);

    PRINTF(GREEN_COLOR " [%s] UDP Connection Result(0x%x) \n" CLEAR_COLOR,
           __func__, conn_status);

    dpm_mng_job_done(); //Done opertaion
}

void udp_server_dpm_sample_recv_callback(void *sock, UCHAR *rx_buf, UINT rx_len,
        ULONG rx_ip, ULONG rx_port)
{
    DA16X_UNUSED_ARG(sock);

    int status = pdPASS;

    //Display received packet
    PRINTF(" =====> Received Packet(%ld) \n", rx_len);

    udp_server_dpm_sample_hex_dump("Recevied Packet", rx_buf, rx_len);

    //Echo message
    status = dpm_mng_send_to_session(SESSION1, rx_ip, rx_port, (char *)rx_buf, rx_len);
    if (status) {
        PRINTF(RED_COLOR " [%s] Fail send data(session%d,0x%x) \n" CLEAR_COLOR,
               __func__, SESSION1, status);
    } else {
        //Display sent packet
        PRINTF(" <===== Sent Packet(%ld) \n", rx_len);

        udp_server_dpm_sample_hex_dump("Sent Packet", rx_buf, rx_len);
    }

    dpm_mng_job_done(); //Done opertaion
}

void udp_server_dpm_sample_init_user_config(dpm_user_config_t *user_config)
{
    const int session_idx = 0;

    if (!user_config) {
        PRINTF(" [%s] Invalid parameter\n", __func__);
        return ;
    }

    //Set Boot init callback
    user_config->bootInitCallback = udp_server_dpm_sample_init_callback;

    //Set DPM wakkup init callback
    user_config->wakeupInitCallback = udp_server_dpm_sample_wakeup_callback;

    //Set External wakeup callback
    user_config->externWakeupCallback = udp_server_dpm_sample_external_callback;

    //Set Error callback
    user_config->errorCallback = udp_server_dpm_sample_error_callback;

    //Set session type(UDP Server)
    user_config->sessionConfig[session_idx].sessionType = REG_TYPE_UDP_SERVER;

    //Set local port
    user_config->sessionConfig[session_idx].sessionMyPort =
        UDP_SERVER_DPM_SAMPLE_DEF_SERVER_PORT;

    //Set Connection callback
    user_config->sessionConfig[session_idx].sessionConnectCallback =
        udp_server_dpm_sample_connect_callback;

    //Set Recv callback
    user_config->sessionConfig[session_idx].sessionRecvCallback =
        udp_server_dpm_sample_recv_callback;

    //Set secure mode
    user_config->sessionConfig[session_idx].supportSecure = pdFALSE;

    //Set user configuration
    user_config->ptrDataFromRetentionMemory = (UCHAR *)&srv_info;
    user_config->sizeOfRetentionMemory = sizeof(udp_server_dpm_sample_svr_info_t);

    return ;
}

unsigned char udp_server_dpm_sample_load_server_info()
{
    int srv_port = 0;

    udp_server_dpm_sample_svr_info_t *srv_info_ptr = &srv_info;

    memset(srv_info_ptr, 0x00, sizeof(udp_server_dpm_sample_svr_info_t));

    //Read server port in nvram
    if (read_nvram_int(UDP_SERVER_DPM_SAMPLE_NVRAM_SERVER_PORT_NAME, &srv_port)) {
        srv_port = UDP_SERVER_DPM_SAMPLE_DEF_SERVER_PORT;
    }

    //Set server port
    srv_info_ptr->port = srv_port;

    PRINTF(" [%s] UDP Server Information(%d)\n", __func__, srv_info_ptr->port);

    return pdPASS;
}

void udp_server_dpm_sample(ULONG arg)
{
    DA16X_UNUSED_ARG(arg);

    unsigned char status = pdPASS;

    PRINTF(" [%s] Start UDP Server Sample\n", __func__);

    //Load server information
    status = udp_server_dpm_sample_load_server_info();
    if (status == pdFAIL) {
        PRINTF(" [%s] Failed to load UDP Server information(0x%02x)\n",
               __func__, status);

        vTaskDelete(NULL);
        return ;
    }

    //Register user configuration
    dpm_mng_regist_config_cb(udp_server_dpm_sample_init_user_config);

    //Start UDP Server Sample
    dpm_mng_start();

    vTaskDelete(NULL);

    return ;
}

#endif //  (__UDP_SERVER_DPM_SAMPLE__) && (__SUPPORT_DPM_MANAGER__) && !(__LIGHT_DPM_MANAGER__)

/* EOF */
