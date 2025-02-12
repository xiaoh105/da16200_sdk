/*
 * hostapd - WNM
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "supp_def.h"

#include "utils/supp_common.h"
#include "supp_eloop.h"
#include "common/ieee802_11_defs.h"
#include "ap/hostapd.h"
#include "ap/sta_info.h"
#include "ap/ap_config.h"
#include "ap/ap_drv_ops.h"
#include "ap/wpa_auth.h"
#include "wnm_ap.h"

#ifdef CONFIG_WNM_TFS
#define MAX_TFS_IE_LEN  1024


/* get the TFS IE from driver */
static int ieee80211_11_get_tfs_ie(struct hostapd_data *hapd, const u8 *addr,
				   u8 *buf, u16 *buf_len, enum wnm_oper oper)
{
	da16x_wnm_prt("     [%s]  TFS get operation %d\n", __func__, oper);

	return hostapd_drv_wnm_oper(hapd, oper, addr, buf, buf_len);
}


/* set the TFS IE to driver */
static int ieee80211_11_set_tfs_ie(struct hostapd_data *hapd, const u8 *addr,
				   u8 *buf, u16 *buf_len, enum wnm_oper oper)
{
	da16x_wnm_prt("     [%s]  TFS set operation %d\n", __func__, oper);

	return hostapd_drv_wnm_oper(hapd, oper, addr, buf, buf_len);
}
#endif /* CONFIG_WNM_TFS */

#ifdef CONFIG_WNM_SLEEP_MODE
/* MLME-SLEEPMODE.response */
static int ieee802_11_send_wnmsleep_resp(struct hostapd_data *hapd,
					 const u8 *addr, u8 dialog_token,
					 u8 action_type, u16 intval)
{
	struct _ieee80211_mgmt *mgmt;
	int res;
	size_t len;
	size_t gtk_elem_len = 0;
	size_t igtk_elem_len = 0;
	struct wnm_sleep_element wnmsleep_ie;
	u8 *wnmtfs_ie;
	u8 wnmsleep_ie_len;
	u16 wnmtfs_ie_len;
	u8 *pos;
	struct sta_info *sta;
	enum wnm_oper tfs_oper = action_type == WNM_SLEEP_MODE_ENTER ?
		WNM_SLEEP_TFS_RESP_IE_ADD : WNM_SLEEP_TFS_RESP_IE_NONE;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		da16x_wnm_prt("     [%s]  station not found\n", __func__);
		return -EINVAL;
	}

	/* WNM-Sleep Mode IE */
	os_memset(&wnmsleep_ie, 0, sizeof(struct wnm_sleep_element));
	wnmsleep_ie_len = sizeof(struct wnm_sleep_element);
	wnmsleep_ie.eid = WLAN_EID_WNMSLEEP;
	wnmsleep_ie.len = wnmsleep_ie_len - 2;
	wnmsleep_ie.action_type = action_type;
	wnmsleep_ie.status = WNM_STATUS_SLEEP_ACCEPT;
	wnmsleep_ie.intval = host_to_le16(intval);

	/* TFS IE(s) */
	wnmtfs_ie = os_zalloc(MAX_TFS_IE_LEN);
	if (wnmtfs_ie == NULL)
		return -1;
	if (ieee80211_11_get_tfs_ie(hapd, addr, wnmtfs_ie, &wnmtfs_ie_len,
				    tfs_oper)) {
		wnmtfs_ie_len = 0;
		os_free(wnmtfs_ie);
		wnmtfs_ie = NULL;
	}

#define MAX_GTK_SUBELEM_LEN 45
#define MAX_IGTK_SUBELEM_LEN 26
	mgmt = os_zalloc(sizeof(*mgmt) + wnmsleep_ie_len +
			 MAX_GTK_SUBELEM_LEN + MAX_IGTK_SUBELEM_LEN);
	if (mgmt == NULL) {
		da16x_wnm_prt("     [%s] MLME: Failed to allocate buffer for "
			   "WNM-Sleep Response action frame\n", __func__);
		return -1;
	}
	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.wnm_sleep_resp.action = WNM_SLEEP_MODE_RESP;
	mgmt->u.action.u.wnm_sleep_resp.dialogtoken = dialog_token;
	pos = (u8 *)mgmt->u.action.u.wnm_sleep_resp.variable;
	/* add key data if MFP is enabled */
	if (!wpa_auth_uses_mfp(sta->wpa_sm) ||
	    action_type != WNM_SLEEP_MODE_EXIT) {
		mgmt->u.action.u.wnm_sleep_resp.keydata_len = 0;
	} else {
		gtk_elem_len = wpa_wnmsleep_gtk_subelem(sta->wpa_sm, pos);
		pos += gtk_elem_len;
		da16x_wnm_prt("     [%s] Pass 4, gtk_len = %d\n", __func__,
			   (int) gtk_elem_len);
#ifdef CONFIG_IEEE80211W
		res = wpa_wnmsleep_igtk_subelem(sta->wpa_sm, pos);
		if (res < 0) {
			os_free(wnmtfs_ie);
			os_free(mgmt);
			return -1;
		}
		igtk_elem_len = res;
		pos += igtk_elem_len;
		da16x_wnm_prt("     [%s] Pass 4 igtk_len = %d\n", __func__,
			   (int) igtk_elem_len);
#endif /* CONFIG_IEEE80211W */

		WPA_PUT_LE16((u8 *)
			     &mgmt->u.action.u.wnm_sleep_resp.keydata_len,
			     gtk_elem_len + igtk_elem_len);
	}
	os_memcpy(pos, &wnmsleep_ie, wnmsleep_ie_len);
	/* copy TFS IE here */
	pos += wnmsleep_ie_len;
	if (wnmtfs_ie)
		os_memcpy(pos, wnmtfs_ie, wnmtfs_ie_len);

	len = 1 + sizeof(mgmt->u.action.u.wnm_sleep_resp) + gtk_elem_len +
		igtk_elem_len + wnmsleep_ie_len + wnmtfs_ie_len;

	/* In driver, response frame should be forced to sent when STA is in
	 * PS mode */
	res = hostapd_drv_send_action(hapd, hapd->iface->freq, 0,
				      mgmt->da, &mgmt->u.action.category, len);

	if (!res) {
		da16x_wnm_prt("     [%s] Successfully send WNM-Sleep Response "
			   "frame\n", __func__);

		/* when entering wnmsleep
		 * 1. pause the node in driver
		 * 2. mark the node so that AP won't update GTK/IGTK during
		 * WNM Sleep
		 */
		if (wnmsleep_ie.status == WNM_STATUS_SLEEP_ACCEPT &&
		    wnmsleep_ie.action_type == WNM_SLEEP_MODE_ENTER) {
			sta->flags |= WLAN_STA_WNM_SLEEP_MODE;
			hostapd_drv_wnm_oper(hapd, WNM_SLEEP_ENTER_CONFIRM,
					     addr, NULL, NULL);
			wpa_set_wnmsleep(sta->wpa_sm, 1);
		}
		/* when exiting wnmsleep
		 * 1. unmark the node
		 * 2. start GTK/IGTK update if MFP is not used
		 * 3. unpause the node in driver
		 */
		if ((wnmsleep_ie.status == WNM_STATUS_SLEEP_ACCEPT ||
		     wnmsleep_ie.status ==
		     WNM_STATUS_SLEEP_EXIT_ACCEPT_GTK_UPDATE) &&
		    wnmsleep_ie.action_type == WNM_SLEEP_MODE_EXIT) {
			sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;
			wpa_set_wnmsleep(sta->wpa_sm, 0);
			hostapd_drv_wnm_oper(hapd, WNM_SLEEP_EXIT_CONFIRM,
					     addr, NULL, NULL);
			if (!wpa_auth_uses_mfp(sta->wpa_sm))
				wpa_wnmsleep_rekey_gtk(sta->wpa_sm);
		}
	} else
		da16x_wnm_prt("     [%s] Fail to send WNM-Sleep Response frame\n", __func__);

#undef MAX_GTK_SUBELEM_LEN
#undef MAX_IGTK_SUBELEM_LEN
	os_free(wnmtfs_ie);
	os_free(mgmt);
	return res;
}


static void ieee802_11_rx_wnmsleep_req(struct hostapd_data *hapd,
				       const u8 *addr, const u8 *frm, int len)
{
	/* Dialog Token [1] | WNM-Sleep Mode IE | TFS Response IE */
	const u8 *pos = frm;
	u8 dialog_token;
	struct wnm_sleep_element *wnmsleep_ie = NULL;
	/* multiple TFS Req IE (assuming consecutive) */
	u8 *tfsreq_ie_start = NULL;
	u8 *tfsreq_ie_end = NULL;
	u16 tfsreq_ie_len = 0;

	dialog_token = *pos++;
	while (pos + 1 < frm + len) {
		u8 ie_len = pos[1];
		if (pos + 2 + ie_len > frm + len)
			break;
		if (*pos == WLAN_EID_WNMSLEEP)
			wnmsleep_ie = (struct wnm_sleep_element *) pos;
		else if (*pos == WLAN_EID_TFS_REQ) {
			if (!tfsreq_ie_start)
				tfsreq_ie_start = (u8 *) pos;
			tfsreq_ie_end = (u8 *) pos;
		} else
			da16x_wnm_prt("     [%s] WNM: EID %d not recognized\n", __func__,
				   *pos);
		pos += ie_len + 2;
	}

	if (!wnmsleep_ie) {
		da16x_wnm_prt("     [%s] No WNM-Sleep IE found\n", __func__);
		return;
	}

	if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_ENTER &&
	    tfsreq_ie_start && tfsreq_ie_end &&
	    tfsreq_ie_end - tfsreq_ie_start >= 0) {
		tfsreq_ie_len = (tfsreq_ie_end + tfsreq_ie_end[1] + 2) -
			tfsreq_ie_start;
		da16x_wnm_prt("     [%s] TFS Req IE(s) found\n", __func__);
		/* pass the TFS Req IE(s) to driver for processing */
		if (ieee80211_11_set_tfs_ie(hapd, addr, tfsreq_ie_start,
					    &tfsreq_ie_len,
					    WNM_SLEEP_TFS_REQ_IE_SET))
			da16x_wnm_prt("     [%s] Fail to set TFS Req IE\n", __func__);
	}

	ieee802_11_send_wnmsleep_resp(hapd, addr, dialog_token,
				      wnmsleep_ie->action_type,
				      le_to_host16(wnmsleep_ie->intval));

	if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT) {
		/* clear the tfs after sending the resp frame */
		ieee80211_11_set_tfs_ie(hapd, addr, tfsreq_ie_start,
					&tfsreq_ie_len, WNM_SLEEP_TFS_IE_DEL);
	}
}
#endif /* CONFIG_WNM_SLEEP_MODE  */

#ifdef CONFIG_WNM_BSS_TRANS_MGMT
static int ieee802_11_send_bss_trans_mgmt_request(struct hostapd_data *hapd,
						  const u8 *addr,
						  u8 dialog_token,
						  const char *url)
{
	struct _ieee80211_mgmt *mgmt;
	size_t url_len, len;
	u8 *pos;
	int res;

	if (url)
		url_len = os_strlen(url);
	else
		url_len = 0;

	mgmt = os_zalloc(sizeof(*mgmt) + (url_len ? 1 + url_len : 0));
	if (mgmt == NULL)
		return -1;
	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.bss_tm_req.action = WNM_BSS_TRANS_MGMT_REQ;
	mgmt->u.action.u.bss_tm_req.dialog_token = dialog_token;
	mgmt->u.action.u.bss_tm_req.req_mode = 0;
	mgmt->u.action.u.bss_tm_req.disassoc_timer = host_to_le16(0);
	mgmt->u.action.u.bss_tm_req.validity_interval = 1;
	pos = mgmt->u.action.u.bss_tm_req.variable;
	if (url) {
		*pos++ += url_len;
		os_memcpy(pos, url, url_len);
		pos += url_len;
	}

	da16x_wnm_prt("     [%s] WNM: Send BSS Transition Management Request to "
		   MACSTR " dialog_token=%u req_mode=0x%x disassoc_timer=%u "
		   "validity_interval=%u\n", __func__,
		   MAC2STR(addr), dialog_token,
		   mgmt->u.action.u.bss_tm_req.req_mode,
		   le_to_host16(mgmt->u.action.u.bss_tm_req.disassoc_timer),
		   mgmt->u.action.u.bss_tm_req.validity_interval);

	len = pos - &mgmt->u.action.category;
	res = hostapd_drv_send_action(hapd, hapd->iface->freq, 0,
				      mgmt->da, &mgmt->u.action.category, len);
	os_free(mgmt);
	return res;
}


static void ieee802_11_rx_bss_trans_mgmt_query(struct hostapd_data *hapd,
					       const u8 *addr, const u8 *frm,
					       size_t len)
{
	u8 dialog_token, reason;
	const u8 *pos, *end;

	if (len < 2) {
		da16x_wnm_prt("     [%s] WNM: Ignore too short BSS Transition Management Query from "
			   MACSTR "\n", __func__, MAC2STR(addr));
		return;
	}

	pos = frm;
	end = pos + len;
	dialog_token = *pos++;
	reason = *pos++;

	da16x_wnm_prt("     [%s] WNM: BSS Transition Management Query from "
		   MACSTR " dialog_token=%u reason=%u\n", __func__,
		   MAC2STR(addr), dialog_token, reason);

	wpa_hexdump(MSG_DEBUG, "WNM: BSS Transition Candidate List Entries",
		    pos, end - pos);

	ieee802_11_send_bss_trans_mgmt_request(hapd, addr, dialog_token, NULL);
}


static void ieee802_11_rx_bss_trans_mgmt_resp(struct hostapd_data *hapd,
					      const u8 *addr, const u8 *frm,
					      size_t len)
{
	u8 dialog_token, status_code, bss_termination_delay;
	const u8 *pos, *end;

	if (len < 3) {
		da16x_wnm_prt("     [%s] WNM: Ignore too short BSS Transition Management Response from "
			   MACSTR "\n", __func__, MAC2STR(addr));
		return;
	}

	pos = frm;
	end = pos + len;
	dialog_token = *pos++;
	status_code = *pos++;
	bss_termination_delay = *pos++;

	da16x_wnm_prt("     [%s] WNM: BSS Transition Management Response from "
		   MACSTR " dialog_token=%u status_code=%u "
		   "bss_termination_delay=%u\n", __func__, MAC2STR(addr), dialog_token,
		   status_code, bss_termination_delay);

	if (status_code == WNM_BSS_TM_ACCEPT) {
		if (end - pos < ETH_ALEN) {
			da16x_wnm_prt("     [%s] WNM: not enough room for Target BSSID field\n", __func__);
			return;
		}
		da16x_wnm_prt("     [%s] WNM: Target BSSID: " MACSTR "\n", __func__,
			   MAC2STR(pos));
		pos += ETH_ALEN;
	}

	wpa_hexdump(MSG_DEBUG, "WNM: BSS Transition Candidate List Entries",
		    pos, end - pos);
}
#endif /* CONFIG_WNM_BSS_TRANS_MGMT */

#ifdef CONFIG_WNM_ACTIONS
int ieee802_11_rx_wnm_action_ap(struct hostapd_data *hapd,
				const struct _ieee80211_mgmt *mgmt, size_t len)
{
	u8 action;
	const u8 *payload;
	size_t plen;

	if (len < IEEE80211_HDRLEN + 2)
		return -1;

	payload = &mgmt->u.action.category;
	payload++;
	action = *payload++;
	plen = (((const u8 *) mgmt) + len) - payload;

	switch (action) {
#ifdef CONFIG_WNM_BSS_TRANS_MGMT
	case WNM_BSS_TRANS_MGMT_QUERY:
		ieee802_11_rx_bss_trans_mgmt_query(hapd, mgmt->sa, payload,
						   plen);
		return 0;
	case WNM_BSS_TRANS_MGMT_RESP:
		ieee802_11_rx_bss_trans_mgmt_resp(hapd, mgmt->sa, payload,
						  plen);
		return 0;
#endif /* CONFIG_WNM_BSS_TRANS_MGMT */
#ifdef CONFIG_WNM_SLEEP_MODE
	case WNM_SLEEP_MODE_REQ:
		ieee802_11_rx_wnmsleep_req(hapd, mgmt->sa, payload, plen);
		return 0;
#endif /* CONFIG_WNM_SLEEP_MODE */
	}

	da16x_wnm_prt("     [%s] WNM: Unsupported WNM Action %u from " MACSTR "\n", __func__,
		   action, MAC2STR(mgmt->sa));
	return -1;
}
#endif /* CONFIG_WNM_ACTIONS */

#ifdef CONFIG_WNM_BSS_TRANS_MGMT
int wnm_send_disassoc_imminent(struct hostapd_data *hapd,
			       struct sta_info *sta, int disassoc_timer)
{
	u8 buf[1000], *pos;
	struct _ieee80211_mgmt *mgmt;

	os_memset(buf, 0, sizeof(buf));
	mgmt = (struct _ieee80211_mgmt *) buf;
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memcpy(mgmt->da, sta->addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.bss_tm_req.action = WNM_BSS_TRANS_MGMT_REQ;
	mgmt->u.action.u.bss_tm_req.dialog_token = 1;
	mgmt->u.action.u.bss_tm_req.req_mode =
		WNM_BSS_TM_REQ_DISASSOC_IMMINENT;
	mgmt->u.action.u.bss_tm_req.disassoc_timer =
		host_to_le16(disassoc_timer);
	mgmt->u.action.u.bss_tm_req.validity_interval = 0;

	pos = mgmt->u.action.u.bss_tm_req.variable;

	da16x_wnm_prt("     [%s] WNM: Send BSS Transition Management Request "
		"frame to indicate imminent disassociation (disassoc_timer=%d) to "
		   MACSTR "\n", __func__, disassoc_timer, MAC2STR(sta->addr));
	if (hostapd_drv_send_mlme(hapd, buf, pos - buf, 0) < 0) {
		da16x_wnm_prt("     [%s] Failed to send BSS Transition "
			   "Management Request frame\n", __func__);
		return -1;
	}

	return 0;
}


int wnm_send_ess_disassoc_imminent(struct hostapd_data *hapd,
				   struct sta_info *sta, const char *url,
				   int disassoc_timer)
{
	u8 buf[1000], *pos;
	struct _ieee80211_mgmt *mgmt;
	size_t url_len;

	os_memset(buf, 0, sizeof(buf));
	mgmt = (struct _ieee80211_mgmt *) buf;
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memcpy(mgmt->da, sta->addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.bss_tm_req.action = WNM_BSS_TRANS_MGMT_REQ;
	mgmt->u.action.u.bss_tm_req.dialog_token = 1;
	mgmt->u.action.u.bss_tm_req.req_mode =
		WNM_BSS_TM_REQ_DISASSOC_IMMINENT |
		WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT;
	mgmt->u.action.u.bss_tm_req.disassoc_timer =
		host_to_le16(disassoc_timer);
	mgmt->u.action.u.bss_tm_req.validity_interval = 0x01;

	pos = mgmt->u.action.u.bss_tm_req.variable;

	/* Session Information URL */
	url_len = os_strlen(url);
	if (url_len > 255)
		return -1;
	*pos++ = url_len;
	os_memcpy(pos, url, url_len);
	pos += url_len;

	if (hostapd_drv_send_mlme(hapd, buf, pos - buf, 0) < 0) {
		da16x_wnm_prt("     [%s] Failed to send BSS Transition "
			   "Management Request frame\n", __func__);
		return -1;
	}

	/* send disassociation frame after time-out */
	if (disassoc_timer) {
		int timeout, beacon_int;

		/*
		 * Prevent STA from reconnecting using cached PMKSA to force
		 * full authentication with the authentication server (which may
		 * decide to reject the connection),
		 */
		wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);

		beacon_int = hapd->iconf->beacon_int;
		if (beacon_int < 1)
			beacon_int = 100; /* best guess */
		/* Calculate timeout in ms based on beacon_int in TU */
		timeout = disassoc_timer * beacon_int * 128 / 125;
		da16x_wnm_prt("     [%s] Disassociation timer for " MACSTR
			   " set to %d ms\n", __func__, MAC2STR(sta->addr), timeout);

		sta->timeout_next = STA_DISASSOC_FROM_CLI;
		da16x_eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		da16x_eloop_register_timeout(timeout / 1000,
				       timeout % 1000 * 1000,
				       ap_handle_timer, hapd, sta);
	}
#endif /* CONFIG_WNM_BSS_TRANS_MGMT */
