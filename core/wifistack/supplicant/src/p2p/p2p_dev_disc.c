/*
 * Wi-Fi Direct - P2P Device Discoverability procedure
 * Copyright (c) 2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#ifdef	CONFIG_P2P

#include "common/ieee802_11_defs.h"
#include "p2p_i.h"
#include "supp_p2p.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"

static struct wpabuf * p2p_build_dev_disc_req(struct p2p_data *p2p,
					      struct p2p_device *go,
					      const u8 *dev_id)
{
	struct wpabuf *buf;
	u8 *len;

	buf = wpabuf_alloc(100);
	if (buf == NULL)
		return NULL;

	go->dialog_token++;
	if (go->dialog_token == 0)
		go->dialog_token = 1;
	p2p_buf_add_public_action_hdr(buf, P2P_DEV_DISC_REQ, go->dialog_token);

	len = p2p_buf_add_ie_hdr(buf);
	p2p_buf_add_device_id(buf, dev_id);
	p2p_buf_add_group_id(buf, go->info.p2p_device_addr, go->oper_ssid,
			     go->oper_ssid_len);
	p2p_buf_update_ie_hdr(buf, len);

	return buf;
}


void p2p_dev_disc_req_cb(struct p2p_data *p2p, int success)
{
	da16x_p2p_prt("[%s] Device Discoverability Request TX callback: "
			"success=%d\n", __func__, success);

	if (!success) {
		/*
		 * Use P2P find, if needed, to find the other device or to
		 * retry device discoverability.
		 */
		p2p_set_state(p2p, P2P_CONNECT);
		p2p_set_timeout(p2p, 0, 100000);
		return;
	}

	da16x_p2p_prt("[%s] GO acknowledged Device Discoverability Request - "
		"wait for response\n", __func__);
	/*
	 * TODO: is the remain-on-channel from Action frame TX long enough for
	 * most cases or should we try to increase its duration and/or start
	 * another remain-on-channel if needed once the previous one expires?
	 */
}


int p2p_send_dev_disc_req(struct p2p_data *p2p, struct p2p_device *dev,
			  int injector)
{
	struct p2p_device *go;
	struct wpabuf *req;

	go = p2p_get_device(p2p, dev->member_in_go_dev);
	if (go == NULL || dev->oper_freq <= 0) {
		da16x_p2p_prt("[%s] Could not find peer entry for GO and "
			"frequency to send Device Discoverability Request\n",
			__func__);
		return -1;
	}

	if (injector) {
		struct p2p_device *go_tmp = os_malloc(sizeof
						      (struct p2p_device));
		if(go_tmp == NULL) return -1;
		
		os_memcpy(go_tmp, go, sizeof(struct p2p_device));
		go = go_tmp;	
	
		os_memcpy(dev->info.p2p_device_addr, dev->member_in_go_iface,
			  ETH_ALEN);
		
		os_free(go_tmp);
	}

	req = p2p_build_dev_disc_req(p2p, go, dev->info.p2p_device_addr);
	if (req == NULL)
		return -1;

	da16x_p2p_prt("[%s] Sending Device Discoverability Request to GO " MACSTR
		" for client " MACSTR "\n",
		__func__,
		MAC2STR(go->info.p2p_device_addr),
		MAC2STR(dev->info.p2p_device_addr));

	p2p->pending_client_disc_go = go;
	os_memcpy(p2p->pending_client_disc_addr, dev->info.p2p_device_addr,
		  ETH_ALEN);
	p2p->pending_action_state = P2P_PENDING_DEV_DISC_REQUEST;
	if (p2p_send_action(p2p, dev->oper_freq, go->info.p2p_device_addr,
			    p2p->cfg->dev_addr, go->info.p2p_device_addr,
			    wpabuf_head(req), wpabuf_len(req), 1000) < 0) {
		da16x_p2p_prt("[%s] Failed to send Action frame\n", __func__);
		wpabuf_free(req);
		/* TODO: how to recover from failure? */
		return -1;
	}

	wpabuf_free(req);
	
	if (injector)
		os_memcpy(dev->info.p2p_device_addr, go->info.p2p_device_addr,
			  ETH_ALEN);

	return 0;
}


static struct wpabuf * p2p_build_dev_disc_resp(u8 dialog_token, u8 status)
{
	struct wpabuf *buf;
	u8 *len;

	buf = wpabuf_alloc(100);
	if (buf == NULL)
		return NULL;

	p2p_buf_add_public_action_hdr(buf, P2P_DEV_DISC_RESP, dialog_token);

	len = p2p_buf_add_ie_hdr(buf);
	p2p_buf_add_status(buf, status);
	p2p_buf_update_ie_hdr(buf, len);

	return buf;
}


void p2p_dev_disc_resp_cb(struct p2p_data *p2p, int success)
{
	da16x_p2p_prt("[%s] Device Discoverability Response TX callback: "
		"success=%d\n", __func__, success);

	p2p->cfg->send_action_done(p2p->cfg->cb_ctx);
}


static void p2p_send_dev_disc_resp(struct p2p_data *p2p, u8 dialog_token,
				   const u8 *addr, int freq, u8 status)
{
	struct wpabuf *resp;

	resp = p2p_build_dev_disc_resp(dialog_token, status);
	if (resp == NULL)
		return;

	da16x_p2p_prt("[%s] Sending Device Discoverability Response to " MACSTR
		" (status %u freq %d)\n",
		__func__,
		MAC2STR(addr), status, freq);

	p2p->pending_action_state = P2P_PENDING_DEV_DISC_RESPONSE;
	if (p2p_send_action(p2p, freq, addr, p2p->cfg->dev_addr,
			    p2p->cfg->dev_addr,
			    wpabuf_head(resp), wpabuf_len(resp), 200) < 0) {
		da16x_p2p_prt("[%s] Failed to send Action frame\n",
			__func__);
	}

	wpabuf_free(resp);
}


void p2p_process_dev_disc_req(struct p2p_data *p2p, const u8 *sa,
			      const u8 *data, size_t len, int rx_freq)
{
	struct p2p_message msg;
	size_t g;

	da16x_p2p_prt("[%s] Received Device Discoverability Request from " MACSTR
		" (freq=%d)\n", __func__, MAC2STR(sa), rx_freq);

	if (p2p_parse(data, len, &msg))
		return;

	if (msg.dialog_token == 0) {
		da16x_p2p_prt("[%s] Invalid Dialog Token 0 (must be nonzero) "
			"in Device Discoverability Request\n", __func__);
		p2p_send_dev_disc_resp(p2p, msg.dialog_token, sa, rx_freq,
				       P2P_SC_FAIL_INVALID_PARAMS);
		p2p_parse_free(&msg);
		return;
	}

	if (msg.device_id == NULL) {
		da16x_p2p_prt("[%s] P2P Device ID attribute missing from "
			"Device Discoverability Request\n", __func__);
		p2p_send_dev_disc_resp(p2p, msg.dialog_token, sa, rx_freq,
				       P2P_SC_FAIL_INVALID_PARAMS);
		p2p_parse_free(&msg);
		return;
	}

	for (g = 0; g < p2p->num_groups; g++) {
		if (p2p_group_go_discover(p2p->groups[g], msg.device_id, sa,
					  rx_freq) == 0) {
			da16x_p2p_prt("[%s] Scheduled GO Discoverability "
				"Request for the target device\n", __func__);
			/*
			 * P2P group code will use a callback to indicate TX
			 * status, so that we can reply to the request once the
			 * target client has acknowledged the request or it has
			 * timed out.
			 */
			p2p->pending_dev_disc_dialog_token = msg.dialog_token;
			os_memcpy(p2p->pending_dev_disc_addr, sa, ETH_ALEN);
			p2p->pending_dev_disc_freq = rx_freq;
			p2p_parse_free(&msg);
			return;
		}
	}

	da16x_p2p_prt("[%s] Requested client was not found in any group or "
			"did not support client discoverability\n",
			__func__);
	p2p_send_dev_disc_resp(p2p, msg.dialog_token, sa, rx_freq,
			       P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE);
	p2p_parse_free(&msg);
}


void p2p_process_dev_disc_resp(struct p2p_data *p2p, const u8 *sa,
			       const u8 *data, size_t len)
{
	struct p2p_message msg;
	struct p2p_device *go;
	u8 status;

	da16x_p2p_prt("[%s] Received Device Discoverability Response from "
			MACSTR "\n",
			__func__, MAC2STR(sa));

	go = p2p->pending_client_disc_go;
	if (go == NULL ||
	    os_memcmp(sa, go->info.p2p_device_addr, ETH_ALEN) != 0) {
		da16x_p2p_prt("[%s] Ignore unexpected Device Discoverability "
			"Response\n", __func__);

		return;
	}

	if (p2p_parse(data, len, &msg))
		return;

	if (msg.status == NULL) {
		p2p_parse_free(&msg);
		return;
	}

	if (msg.dialog_token != go->dialog_token) {
		da16x_p2p_prt("[%s] Ignore Device Discoverability Response "
			"with unexpected dialog token %u (expected %u)\n",
			__func__, msg.dialog_token, go->dialog_token);

		p2p_parse_free(&msg);
		return;
	}

	status = *msg.status;
	p2p_parse_free(&msg);

	da16x_p2p_prt("[%s] Device Discoverability Response status %u\n",
			__func__, status);

	if (p2p->go_neg_peer == NULL ||
	    os_memcmp(p2p->pending_client_disc_addr,
		      p2p->go_neg_peer->info.p2p_device_addr, ETH_ALEN) != 0 ||
	    os_memcmp(p2p->go_neg_peer->member_in_go_dev,
		      go->info.p2p_device_addr, ETH_ALEN) != 0) {
		da16x_p2p_prt("[%s] No pending operation with the client "
			"discoverability peer anymore\n", __func__);
		return;
	}

	if (status == 0) {
		/*
		 * Peer is expected to be awake for at least 100 TU; try to
		 * connect immediately.
		 */
		da16x_p2p_prt("[%s] Client discoverability request succeeded\n",
			__func__);

		if (p2p->state == P2P_CONNECT) {
			/*
			 * Change state to force the timeout to start in
			 * P2P_CONNECT again without going through the short
			 * Listen state.
			 */
			p2p_set_state(p2p, P2P_CONNECT_LISTEN);
			p2p->cfg->send_action_done(p2p->cfg->cb_ctx);
		}
		p2p_set_timeout(p2p, 0, 0);
	} else {
		/*
		 * Client discoverability request failed; try to connect from
		 * timeout.
		 */
		da16x_p2p_prt("[%s] Client discoverability request failed\n",
				__func__);

		p2p_set_timeout(p2p, 0, 500000);
	}

}


void p2p_go_disc_req_cb(struct p2p_data *p2p, int success)
{
	da16x_p2p_prt("[%s] GO Discoverability Request TX callback: "
		"success=%d\n", __func__, success);

	p2p->cfg->send_action_done(p2p->cfg->cb_ctx);

	if (p2p->pending_dev_disc_dialog_token == 0) {
		da16x_p2p_prt("[%s] No pending Device Discoverability Request\n",
				__func__);
		return;
	}

	p2p_send_dev_disc_resp(p2p, p2p->pending_dev_disc_dialog_token,
			       p2p->pending_dev_disc_addr,
			       p2p->pending_dev_disc_freq,
			       success ? P2P_SC_SUCCESS :
			       P2P_SC_FAIL_UNABLE_TO_ACCOMMODATE);

	p2p->pending_dev_disc_dialog_token = 0;
}


void p2p_process_go_disc_req(struct p2p_data *p2p, const u8 *da, const u8 *sa,
			     const u8 *data, size_t len, int rx_freq)
{
	unsigned int tu;
	struct wpabuf *ies;

	da16x_p2p_prt("[%s] Received GO Discoverability Request - "
			"remain awake for 100 TU\n", __func__);

	ies = p2p_build_probe_resp_ies(p2p);
	if (ies == NULL)
		return;

	/* Remain awake 100 TU on operating channel */
	p2p->pending_client_disc_freq = rx_freq;
	tu = 100;
	if (p2p->cfg->start_listen(p2p->cfg->cb_ctx, rx_freq, 1024 * tu / 1000,
		    ies) < 0) {
		da16x_p2p_prt("[%s] Failed to start listen mode for client "
			"discoverability\n", __func__);
	}
	wpabuf_free(ies);
}

#endif	/* CONFIG_P2P */

/* EOF */
