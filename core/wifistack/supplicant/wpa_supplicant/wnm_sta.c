/*
 * wpa_supplicant - WNM
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/supp_common.h"
#include "common/ieee802_11_defs.h"
#include "wpa_ctrl.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "supp_scan.h"
#include "bss.h"
#include "wnm_sta.h"

#ifdef CONFIG_WNM_TFS
#define MAX_TFS_IE_LEN  1024
#endif /* CONFIG_WNM_TFS */

#ifdef CONFIG_WNM_BSS_TRANS_MGMT
#define WNM_MAX_NEIGHBOR_REPORT 10
#endif /* CONFIG_WNM_BSS_TRANS_MGMT */

#ifdef CONFIG_WNM_TFS
/* get the TFS IE from driver */
static int ieee80211_11_get_tfs_ie(struct wpa_supplicant *wpa_s, u8 *buf,
				   u16 *buf_len, enum wnm_oper oper)
{
	da16x_wnm_prt("%s: TFS get operation %d\n", __func__, oper);

	return wpa_drv_wnm_oper(wpa_s, oper, wpa_s->bssid, buf, buf_len);
}

/* set the TFS IE to driver */
static int ieee80211_11_set_tfs_ie(struct wpa_supplicant *wpa_s,
				   const u8 *addr, u8 *buf, u16 *buf_len,
				   enum wnm_oper oper)
{
	da16x_wnm_prt("%s: TFS set operation %d\n", __func__, oper);

	return wpa_drv_wnm_oper(wpa_s, oper, addr, buf, buf_len);
}
#endif /* CONFIG_WNM_TFS */

/* MLME-SLEEPMODE.request */
#ifdef CONFIG_WNM_SLEEP_MODE
int ieee802_11_send_wnmsleep_req(struct wpa_supplicant *wpa_s,
				 u8 action, u16 intval, struct wpabuf *tfs_req)
{
	struct _ieee80211_mgmt *mgmt;
	int res;
	size_t len;
	struct wnm_sleep_element *wnmsleep_ie;
	u8 *wnmtfs_ie;
	u8 wnmsleep_ie_len;
	u16 wnmtfs_ie_len;  /* possibly multiple IE(s) */
	enum wnm_oper tfs_oper = action == 0 ? WNM_SLEEP_TFS_REQ_IE_ADD :
		WNM_SLEEP_TFS_REQ_IE_NONE;

	da16x_wnm_prt("[%s] WNM: Request to send WNM-Sleep Mode Request "
		   "action=%s to " MACSTR,
		   action == 0 ? "enter" : "exit\n", __func__, 
		   MAC2STR(wpa_s->bssid));

	/* WNM-Sleep Mode IE */
	wnmsleep_ie_len = sizeof(struct wnm_sleep_element);
	wnmsleep_ie = os_zalloc(sizeof(struct wnm_sleep_element));
	if (wnmsleep_ie == NULL)
		return -1;
	wnmsleep_ie->eid = WLAN_EID_WNMSLEEP;
	wnmsleep_ie->len = wnmsleep_ie_len - 2;
	wnmsleep_ie->action_type = action;
	wnmsleep_ie->status = WNM_STATUS_SLEEP_ACCEPT;
	wnmsleep_ie->intval = host_to_le16(intval);
	da16x_dump("WNM: WNM-Sleep Mode element",
		    (u8 *) wnmsleep_ie, wnmsleep_ie_len);

	/* TFS IE(s) */
	if (tfs_req) {
		wnmtfs_ie_len = wpabuf_len(tfs_req);
		wnmtfs_ie = os_malloc(wnmtfs_ie_len);
		if (wnmtfs_ie == NULL) {
			os_free(wnmsleep_ie);
			return -1;
		}
		os_memcpy(wnmtfs_ie, wpabuf_head(tfs_req), wnmtfs_ie_len);
	} else {
		wnmtfs_ie = os_zalloc(MAX_TFS_IE_LEN);
		if (wnmtfs_ie == NULL) {
			os_free(wnmsleep_ie);
			return -1;
		}
		if (ieee80211_11_get_tfs_ie(wpa_s, wnmtfs_ie, &wnmtfs_ie_len,
					    tfs_oper)) {
			wnmtfs_ie_len = 0;
			os_free(wnmtfs_ie);
			wnmtfs_ie = NULL;
		}
	}
	da16x_dump("WNM: TFS Request element",
		    (u8 *) wnmtfs_ie, wnmtfs_ie_len);

	mgmt = os_zalloc(sizeof(*mgmt) + wnmsleep_ie_len + wnmtfs_ie_len);
	if (mgmt == NULL) {
		da16x_wnm_prt("[%s]  MLME: Failed to allocate buffer for "
			   "WNM-Sleep Request action frame\n", __func__);
		os_free(wnmsleep_ie);
		os_free(wnmtfs_ie);
		return -1;
	}

	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.wnm_sleep_req.action = WNM_SLEEP_MODE_REQ;
	mgmt->u.action.u.wnm_sleep_req.dialogtoken = 1;
	os_memcpy(mgmt->u.action.u.wnm_sleep_req.variable, wnmsleep_ie,
		  wnmsleep_ie_len);
	/* copy TFS IE here */
	if (wnmtfs_ie_len > 0) {
		os_memcpy(mgmt->u.action.u.wnm_sleep_req.variable +
			  wnmsleep_ie_len, wnmtfs_ie, wnmtfs_ie_len);
	}

	len = 1 + sizeof(mgmt->u.action.u.wnm_sleep_req) + wnmsleep_ie_len +
		wnmtfs_ie_len;

	res = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  &mgmt->u.action.category, len, 0);
	if (res < 0)
		da16x_wnm_prt("     [%s] Failed to send WNM-Sleep Request "
			   "(action=%d, intval=%d\n", __func__, action, intval);

	os_free(wnmsleep_ie);
	os_free(wnmtfs_ie);
	os_free(mgmt);

	return res;
}


static void wnm_sleep_mode_enter_success(struct wpa_supplicant *wpa_s,
					 u8 *tfsresp_ie_start,
					 u8 *tfsresp_ie_end)
{
	wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_ENTER_CONFIRM,
			 wpa_s->bssid, NULL, NULL);
	/* remove GTK/IGTK ?? */

	/* set the TFS Resp IE(s) */
	if (tfsresp_ie_start && tfsresp_ie_end &&
	    tfsresp_ie_end - tfsresp_ie_start >= 0) {
		u16 tfsresp_ie_len;
		tfsresp_ie_len = (tfsresp_ie_end + tfsresp_ie_end[1] + 2) -
			tfsresp_ie_start;
		da16x_wnm_prt("     [%s] TFS Resp IE(s) found\n", __func__);
		/* pass the TFS Resp IE(s) to driver for processing */
		if (ieee80211_11_set_tfs_ie(wpa_s, wpa_s->bssid,
					    tfsresp_ie_start,
					    &tfsresp_ie_len,
					    WNM_SLEEP_TFS_RESP_IE_SET))
			da16x_wnm_prt("     [%s] WNM: Fail to set TFS Resp IE\n", __func__);
	}
}


static void wnm_sleep_mode_exit_success(struct wpa_supplicant *wpa_s,
					const u8 *frm, u16 key_len_total)
{
	u8 *ptr, *end;
	u8 gtk_len;

	wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_EXIT_CONFIRM,  wpa_s->bssid,
			 NULL, NULL);

	/* Install GTK/IGTK */

	/* point to key data field */
	ptr = (u8 *) frm + 1 + 2;
	end = ptr + key_len_total;
	da16x_dump("WNM: Key Data", ptr, key_len_total);

	while (ptr + 1 < end) {
		if (ptr + 2 + ptr[1] > end) {
			da16x_wnm_prt("     [%s] WNM: Invalid Key Data element "
				   "length\n", __func__);
			if (end > ptr) {
				da16x_dump("WNM: Remaining data",
					    ptr, end - ptr);
			}
			break;
		}
		if (*ptr == WNM_SLEEP_SUBELEM_GTK) {
			if (ptr[1] < 11 + 5) {
				da16x_wnm_prt("     [%s] WNM: Too short GTK "
					   "subelem\n", __func__);
				break;
			}
			gtk_len = *(ptr + 4);
			if (ptr[1] < 11 + gtk_len ||
			    gtk_len < 5 || gtk_len > 32) {
				da16x_wnm_prt("     [%s] WNM: Invalid GTK "
					   "subelem\n", __func__);
				break;
			}
			wpa_wnmsleep_install_key(
				wpa_s->wpa,
				WNM_SLEEP_SUBELEM_GTK,
				ptr);
			ptr += 13 + gtk_len;
#ifdef CONFIG_IEEE80211W
		} else if (*ptr == WNM_SLEEP_SUBELEM_IGTK) {
			if (ptr[1] < 2 + 6 + WPA_IGTK_LEN) {
				da16x_wnm_prt("     [%s] WNM: Too short IGTK "
					   "subelem\n", __func__);
				break;
			}
			wpa_wnmsleep_install_key(wpa_s->wpa,
						 WNM_SLEEP_SUBELEM_IGTK, ptr);
			ptr += 10 + WPA_IGTK_LEN;
#endif /* CONFIG_IEEE80211W */
		} else
			break; /* skip the loop */
	}
}

static void ieee802_11_rx_wnmsleep_resp(struct wpa_supplicant *wpa_s,
					const u8 *frm, int len)
{
	/*
	 * Action [1] | Dialog Token [1] | Key Data Len [2] | Key Data |
	 * WNM-Sleep Mode IE | TFS Response IE
	 */
	u8 *pos = (u8 *) frm; /* point to payload after the action field */
	u16 key_len_total;
	struct wnm_sleep_element *wnmsleep_ie = NULL;
	/* multiple TFS Resp IE (assuming consecutive) */
	u8 *tfsresp_ie_start = NULL;
	u8 *tfsresp_ie_end = NULL;

	if (len < 3)
		return;
	key_len_total = WPA_GET_LE16(frm + 1);

	da16x_wnm_prt("     [%s] WNM-Sleep Mode Response token=%u key_len_total=%d\n", __func__,
		   frm[0], key_len_total);
	pos += 3 + key_len_total;
	if (pos > frm + len) {
		da16x_wnm_prt("     [%s] WNM: Too short frame for Key Data field\n", __func__);
		return;
	}
	while (pos - frm < len) {
		u8 ie_len = *(pos + 1);
		if (pos + 2 + ie_len > frm + len) {
			da16x_wnm_prt("     [%s] WNM: Invalid IE len %u\n", __func__, ie_len);
			break;
		}
		da16x_dump("WNM: Element", pos, 2 + ie_len);
		if (*pos == WLAN_EID_WNMSLEEP)
			wnmsleep_ie = (struct wnm_sleep_element *) pos;
		else if (*pos == WLAN_EID_TFS_RESP) {
			if (!tfsresp_ie_start)
				tfsresp_ie_start = pos;
			tfsresp_ie_end = pos;
		} else
			da16x_wnm_prt("     [%s] EID %d not recognized\n", __func__, *pos);
		pos += ie_len + 2;
	}

	if (!wnmsleep_ie) {
		da16x_wnm_prt("     [%s] No WNM-Sleep IE found\n", __func__);
		return;
	}

	if (wnmsleep_ie->status == WNM_STATUS_SLEEP_ACCEPT ||
	    wnmsleep_ie->status == WNM_STATUS_SLEEP_EXIT_ACCEPT_GTK_UPDATE) {
		da16x_wnm_prt("     [%s] Successfully recv WNM-Sleep Response "
			   "frame (action=%d, intval=%d)\n", __func__,
			   wnmsleep_ie->action_type, wnmsleep_ie->intval);
		if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_ENTER) {
			wnm_sleep_mode_enter_success(wpa_s, tfsresp_ie_start,
						     tfsresp_ie_end);
		} else if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT) {
			wnm_sleep_mode_exit_success(wpa_s, frm, key_len_total);
		}
	} else {
		da16x_wnm_prt("     [%s] Reject recv WNM-Sleep Response frame "
			   "(action=%d, intval=%d)\n", __func__,
			   wnmsleep_ie->action_type, wnmsleep_ie->intval);
		if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_ENTER)
			wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_ENTER_FAIL,
					 wpa_s->bssid, NULL, NULL);
		else if (wnmsleep_ie->action_type == WNM_SLEEP_MODE_EXIT)
			wpa_drv_wnm_oper(wpa_s, WNM_SLEEP_EXIT_FAIL,
					 wpa_s->bssid, NULL, NULL);
	}
}
#endif /* CONFIG_WNM_SLEEP_MODE */

#ifdef CONFIG_WNM_BSS_TRANS_MGMT
void wnm_deallocate_memory(struct wpa_supplicant *wpa_s)
{
	int i;

	for (i = 0; i < wpa_s->wnm_num_neighbor_report; i++) {
		os_free(wpa_s->wnm_neighbor_report_elements[i].tsf_info);
		os_free(wpa_s->wnm_neighbor_report_elements[i].con_coun_str);
		os_free(wpa_s->wnm_neighbor_report_elements[i].bss_tran_can);
		os_free(wpa_s->wnm_neighbor_report_elements[i].bss_term_dur);
		os_free(wpa_s->wnm_neighbor_report_elements[i].bearing);
		os_free(wpa_s->wnm_neighbor_report_elements[i].meas_pilot);
		os_free(wpa_s->wnm_neighbor_report_elements[i].rrm_cap);
		os_free(wpa_s->wnm_neighbor_report_elements[i].mul_bssid);
	}

	wpa_s->wnm_num_neighbor_report = 0;
	os_free(wpa_s->wnm_neighbor_report_elements);
	wpa_s->wnm_neighbor_report_elements = NULL;
}

static void wnm_parse_neighbor_report_elem(struct neighbor_report *rep,
					   u8 id, u8 elen, const u8 *pos)
{
	switch (id) {
	case WNM_NEIGHBOR_TSF:
		if (elen < 2 + 2) {
			da16x_wnm_prt("     [%s] WNM: Too short TSF\n", __func__);
			break;
		}
		os_free(rep->tsf_info);
		rep->tsf_info = os_zalloc(sizeof(struct tsf_info));
		if (rep->tsf_info == NULL)
			break;
		os_memcpy(rep->tsf_info->tsf_offset, pos, 2);
		os_memcpy(rep->tsf_info->beacon_interval, pos + 2, 2);
		break;
	case WNM_NEIGHBOR_CONDENSED_COUNTRY_STRING:
		if (elen < 2) {
			da16x_wnm_prt("     [%s] WNM: Too short condensed "
				   "country string\n", __func__);
			break;
		}
		os_free(rep->con_coun_str);
		rep->con_coun_str =
			os_zalloc(sizeof(struct condensed_country_string));
		if (rep->con_coun_str == NULL)
			break;
		os_memcpy(rep->con_coun_str->country_string, pos, 2);
		break;
	case WNM_NEIGHBOR_BSS_TRANSITION_CANDIDATE:
		if (elen < 1) {
			da16x_wnm_prt("     [%s] WNM: Too short BSS transition "
				   "candidate\n", __func__);
			break;
		}
		os_free(rep->bss_tran_can);
		rep->bss_tran_can =
			os_zalloc(sizeof(struct bss_transition_candidate));
		if (rep->bss_tran_can == NULL)
			break;
		rep->bss_tran_can->preference = pos[0];
		break;
	case WNM_NEIGHBOR_BSS_TERMINATION_DURATION:
		if (elen < 10) {
			da16x_wnm_prt("     [%s] WNM: Too short BSS termination "
				   "duration\n", __func__);
			break;
		}
		os_free(rep->bss_term_dur);
		rep->bss_term_dur =
			os_zalloc(sizeof(struct bss_termination_duration));
		if (rep->bss_term_dur == NULL)
			break;
		os_memcpy(rep->bss_term_dur->duration, pos, 10);
		break;
	case WNM_NEIGHBOR_BEARING:
		if (elen < 8) {
			da16x_wnm_prt("     [%s] WNM: Too short neighbor "
				   "bearing\n", __func__);
			break;
		}
		os_free(rep->bearing);
		rep->bearing = os_zalloc(sizeof(struct bearing));
		if (rep->bearing == NULL)
			break;
		os_memcpy(rep->bearing->bearing, pos, 8);
		break;
	case WNM_NEIGHBOR_MEASUREMENT_PILOT:
		if (elen < 1) {
			da16x_wnm_prt("     [%s] WNM: Too short measurement "
				   "pilot\n", __func__);
			break;
		}
		os_free(rep->meas_pilot);
		rep->meas_pilot = os_zalloc(sizeof(struct measurement_pilot));
		if (rep->meas_pilot == NULL)
			break;
		rep->meas_pilot->measurement_pilot = pos[0];
		rep->meas_pilot->subelem_len = elen - 1;
		os_memcpy(rep->meas_pilot->subelems, pos + 1, elen - 1);
		break;
	case WNM_NEIGHBOR_RRM_ENABLED_CAPABILITIES:
		if (elen < 5) {
			da16x_wnm_prt("     [%s] WNM: Too short RRM enabled "
				   "capabilities\n", __func__);
			break;
		}
		os_free(rep->rrm_cap);
		rep->rrm_cap =
			os_zalloc(sizeof(struct rrm_enabled_capabilities));
		if (rep->rrm_cap == NULL)
			break;
		os_memcpy(rep->rrm_cap->capabilities, pos, 5);
		break;
	case WNM_NEIGHBOR_MULTIPLE_BSSID:
		if (elen < 1) {
			da16x_wnm_prt("     [%s] WNM: Too short multiple BSSID\n", __func__);
			break;
		}
		os_free(rep->mul_bssid);
		rep->mul_bssid = os_zalloc(sizeof(struct multiple_bssid));
		if (rep->mul_bssid == NULL)
			break;
		rep->mul_bssid->max_bssid_indicator = pos[0];
		rep->mul_bssid->subelem_len = elen - 1;
		os_memcpy(rep->mul_bssid->subelems, pos + 1, elen - 1);
		break;
	}
}


static void wnm_parse_neighbor_report(struct wpa_supplicant *wpa_s,
				      const u8 *pos, u8 len,
				      struct neighbor_report *rep)
{
	u8 left = len;

	if (left < 13) {
		da16x_wnm_prt("     [%s] WNM: Too short neighbor report\n", __func__);
		return;
	}

	os_memcpy(rep->bssid, pos, ETH_ALEN);
	os_memcpy(rep->bssid_information, pos + ETH_ALEN, 4);
	rep->regulatory_class = *(pos + 10);
	rep->channel_number = *(pos + 11);
	rep->phy_type = *(pos + 12);

	pos += 13;
	left -= 13;

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		da16x_wnm_prt("     [%s] WNM: Subelement id=%u len=%u\n", __func__, id, elen);
		left -= 2;
		if (elen > left) {
			da16x_wnm_prt("     [%s] "
				   "WNM: Truncated neighbor report subelement\n", __func__);
			break;
		}
		wnm_parse_neighbor_report_elem(rep, id, elen, pos);
		left -= elen;
		pos += elen;
	}
}


static int compare_scan_neighbor_results(struct wpa_supplicant *wpa_s,
					 struct wpa_scan_results *scan_res,
					 struct neighbor_report *neigh_rep,
					 u8 num_neigh_rep, u8 *bssid_to_connect)
{

	u8 i, j;

	if (scan_res == NULL || num_neigh_rep == 0 || !wpa_s->current_bss)
		return 0;

	da16x_wnm_prt("     [%s] WNM: Current BSS " MACSTR " RSSI %d\n", __func__,
		   MAC2STR(wpa_s->bssid), wpa_s->current_bss->level);

	for (i = 0; i < num_neigh_rep; i++) {
		for (j = 0; j < scan_res->num; j++) {
			/* Check for a better RSSI AP */
			if (os_memcmp(scan_res->res[j]->bssid,
				      neigh_rep[i].bssid, ETH_ALEN) == 0 &&
			    scan_res->res[j]->level >
			    wpa_s->current_bss->level) {
				/* Got a BSSID with better RSSI value */
				os_memcpy(bssid_to_connect, neigh_rep[i].bssid,
					  ETH_ALEN);
				da16x_wnm_prt("     [%s] Found a BSS " MACSTR
					   " with better scan RSSI %d\n", __func__,
					   MAC2STR(scan_res->res[j]->bssid),
					   scan_res->res[j]->level);
				return 1;
			}
			da16x_wnm_prt("     [%s] scan_res[%d] " MACSTR
				   " RSSI %d\n", __func__, j,
				   MAC2STR(scan_res->res[j]->bssid),
				   scan_res->res[j]->level);
		}
	}

	return 0;
}

static void wnm_send_bss_transition_mgmt_resp(
	struct wpa_supplicant *wpa_s, u8 dialog_token,
	enum bss_trans_mgmt_status_code status, u8 delay,
	const u8 *target_bssid)
{
	u8 buf[1000], *pos;
	struct _ieee80211_mgmt *mgmt;
	size_t len;

	da16x_wnm_prt("     [%s] WNM: Send BSS Transition Management Response "
		   "to " MACSTR " dialog_token=%u status=%u delay=%d\n", __func__,
		   MAC2STR(wpa_s->bssid), dialog_token, status, delay);

	mgmt = (struct _ieee80211_mgmt *) buf;
	os_memset(&buf, 0, sizeof(buf));
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.bss_tm_resp.action = WNM_BSS_TRANS_MGMT_RESP;
	mgmt->u.action.u.bss_tm_resp.dialog_token = dialog_token;
	mgmt->u.action.u.bss_tm_resp.status_code = status;
	mgmt->u.action.u.bss_tm_resp.bss_termination_delay = delay;
	pos = mgmt->u.action.u.bss_tm_resp.variable;
	if (target_bssid) {
		os_memcpy(pos, target_bssid, ETH_ALEN);
		pos += ETH_ALEN;
	} else if (status == WNM_BSS_TM_ACCEPT) {
		/*
		 * P802.11-REVmc clarifies that the Target BSSID field is always
		 * present when status code is zero, so use a fake value here if
		 * no BSSID is yet known.
		 */
		os_memset(pos, 0, ETH_ALEN);
		pos += ETH_ALEN;
	}

	len = pos - (u8 *) &mgmt->u.action.category;

	wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
			    wpa_s->own_addr, wpa_s->bssid,
			    &mgmt->u.action.category, len, 0);
}

void wnm_scan_response(struct wpa_supplicant *wpa_s,
		       struct wpa_scan_results *scan_res)
{
	u8 bssid[ETH_ALEN];

	if (scan_res == NULL) {
		da16x_wnm_prt("     [%s] Scan result is NULL\n", __func__);
		goto send_bss_resp_fail;
	}

	/* Compare the Neighbor Report and scan results */
	if (compare_scan_neighbor_results(wpa_s, scan_res,
					  wpa_s->wnm_neighbor_report_elements,
					  wpa_s->wnm_num_neighbor_report,
					  bssid) == 1) {
		/* Associate to the network */
		struct wpa_bss *bss;
		struct wpa_ssid *ssid = wpa_s->current_ssid;

		bss = wpa_bss_get_bssid(wpa_s, bssid);
		if (!bss) {
			da16x_wnm_prt("     [%s] WNM: Target AP not found from "
				   "BSS table\n", __func__);
			goto send_bss_resp_fail;
		}

		/* Send the BSS Management Response - Accept */
		if (wpa_s->wnm_reply) {
			wnm_send_bss_transition_mgmt_resp(wpa_s,
						  wpa_s->wnm_dialog_token,
						  WNM_BSS_TM_ACCEPT,
						  0, bssid);
		}

		wpa_s->reassociate = 1;
		wpa_supplicant_connect(wpa_s, bss, ssid);
		wnm_deallocate_memory(wpa_s);
		return;
	}

	/* Send reject response for all the failures */
send_bss_resp_fail:
	wnm_deallocate_memory(wpa_s);
	if (wpa_s->wnm_reply) {
		wnm_send_bss_transition_mgmt_resp(wpa_s,
						  wpa_s->wnm_dialog_token,
						  WNM_BSS_TM_REJECT_UNSPECIFIED,
						  0, NULL);
	}
	return;
}

static void ieee802_11_rx_bss_trans_mgmt_req(struct wpa_supplicant *wpa_s,
					     const u8 *pos, const u8 *end,
					     int reply)
{
	if (pos + 5 > end)
		return;

	wpa_s->wnm_dialog_token = pos[0];
	wpa_s->wnm_mode = pos[1];
	wpa_s->wnm_dissoc_timer = WPA_GET_LE16(pos + 2);
	wpa_s->wnm_validity_interval = pos[4];
	wpa_s->wnm_reply = reply;

	da16x_wnm_prt("     [%s] WNM: BSS Transition Management Request: "
		   "dialog_token=%u request_mode=0x%x "
		   "disassoc_timer=%u validity_interval=%u\n", __func__,
		   wpa_s->wnm_dialog_token, wpa_s->wnm_mode,
		   wpa_s->wnm_dissoc_timer, wpa_s->wnm_validity_interval);

	pos += 5;

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED) {
		if (pos + 12 > end) {
			da16x_wnm_prt("     [%s] WNM: Too short BSS TM Request\n", __func__);
			return;
		}
		os_memcpy(wpa_s->wnm_bss_termination_duration, pos, 12);
		pos += 12; /* BSS Termination Duration */
	}

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT) {
		char url[256];
		unsigned int beacon_int;

		if (pos + 1 > end || pos + 1 + pos[0] > end) {
			da16x_wnm_prt("     [%s] WNM: Invalid BSS Transition "
				   "Management Request (URL)\n", __func__);
			return;
		}
		os_memcpy(url, pos + 1, pos[0]);
		url[pos[0]] = '\0';
		pos += 1 + pos[0];

		if (wpa_s->current_bss)
			beacon_int = wpa_s->current_bss->beacon_int;
		else
			beacon_int = 100; /* best guess */

		da16x_wnm_prt("     [%s] " ESS_DISASSOC_IMMINENT "%d %u %s",
			wpa_sm_pmf_enabled(wpa_s->wpa),
			wpa_s->wnm_dissoc_timer * beacon_int * 128 / 125, url);
	}

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_DISASSOC_IMMINENT) {
		da16x_wnm_prt("     [%s] WNM: Disassociation Imminent - "
			"Disassociation Timer %u", wpa_s->wnm_dissoc_timer);
		if (wpa_s->wnm_dissoc_timer && !wpa_s->scanning) {
			/* TODO: mark current BSS less preferred for
			 * selection */
			da16x_wnm_prt("     [%s] Trying to find another BSS\n", __func__);
			wpa_supplicant_req_scan(wpa_s, 0, 0);
		}
	}

	if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_PREF_CAND_LIST_INCLUDED) {
		da16x_wnm_prt("     [%s] WNM: Preferred List Available");
		wpa_s->wnm_num_neighbor_report = 0;
		os_free(wpa_s->wnm_neighbor_report_elements);
		wpa_s->wnm_neighbor_report_elements = os_zalloc(
			WNM_MAX_NEIGHBOR_REPORT *
			sizeof(struct neighbor_report));
		if (wpa_s->wnm_neighbor_report_elements == NULL)
			return;

		while (pos + 2 <= end &&
		       wpa_s->wnm_num_neighbor_report < WNM_MAX_NEIGHBOR_REPORT)
		{
			u8 tag = *pos++;
			u8 len = *pos++;

			da16x_wnm_prt("     [%s] WNM: Neighbor report tag %u\n", __func__,
				   tag);
			if (pos + len > end) {
				da16x_wnm_prt("     [%s] WNM: Truncated request\n", __func__);
				return;
			}
			if (tag == WLAN_EID_NEIGHBOR_REPORT) {
				struct neighbor_report *rep;
				rep = &wpa_s->wnm_neighbor_report_elements[
					wpa_s->wnm_num_neighbor_report];
				wnm_parse_neighbor_report(wpa_s, pos, len, rep);
			}

			pos += len;
			wpa_s->wnm_num_neighbor_report++;
		}

		wpa_s->scan_res_handler = wnm_scan_response;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else if (reply) {
		enum bss_trans_mgmt_status_code status;
		if (wpa_s->wnm_mode & WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT)
			status = WNM_BSS_TM_ACCEPT;
		else {
			da16x_wnm_prt("     [%s] WNM: BSS Transition Management "
				"Request did not include candidates\n", __func__);
			status = WNM_BSS_TM_REJECT_UNSPECIFIED;
		}
		wnm_send_bss_transition_mgmt_resp(wpa_s,
						  wpa_s->wnm_dialog_token,
						  status, 0, NULL);
	}
}

int wnm_send_bss_transition_mgmt_query(struct wpa_supplicant *wpa_s,
				       u8 query_reason)
{
	u8 buf[1000], *pos;
	struct _ieee80211_mgmt *mgmt;
	size_t len;
	int ret;

	da16x_wnm_prt("     [%s] WNM: Send BSS Transition Management Query to "
		   MACSTR " query_reason=%u\n", __func__,
		   MAC2STR(wpa_s->bssid), query_reason);

	mgmt = (struct _ieee80211_mgmt *) buf;
	os_memset(&buf, 0, sizeof(buf));
	os_memcpy(mgmt->da, wpa_s->bssid, ETH_ALEN);
	os_memcpy(mgmt->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, wpa_s->bssid, ETH_ALEN);
	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	mgmt->u.action.category = WLAN_ACTION_WNM;
	mgmt->u.action.u.bss_tm_query.action = WNM_BSS_TRANS_MGMT_QUERY;
	mgmt->u.action.u.bss_tm_query.dialog_token = 1;
	mgmt->u.action.u.bss_tm_query.query_reason = query_reason;
	pos = mgmt->u.action.u.bss_tm_query.variable;

	len = pos - (u8 *) &mgmt->u.action.category;

	ret = wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				  wpa_s->own_addr, wpa_s->bssid,
				  &mgmt->u.action.category, len, 0);

	return ret;
}
#endif /* CONFIG_WNM_BSS_TRANS_MGMT */

#ifdef CONFIG_WNM_NOTIFICATION
static void ieee802_11_rx_wnm_notif_req_wfa(struct wpa_supplicant *wpa_s,
					    const u8 *sa, const u8 *data,
					    int len)
{
	const u8 *pos, *end, *next;
	u8 ie, ie_len;

	pos = data;
	end = data + len;

	while (pos + 1 < end) {
		ie = *pos++;
		ie_len = *pos++;
		da16x_wnm_prt("     [%s] WNM: WFA subelement %u len %u\n", __func__,
			   ie, ie_len);
		if (ie_len > end - pos) {
			da16x_wnm_prt("     [%s] WNM: Not enough room for "
				   "subelement\n", __func__);
			break;
		}
		next = pos + ie_len;
		if (ie_len < 4) {
			pos = next;
			continue;
		}
		da16x_wnm_prt("     [%s] WNM: Subelement OUI %06x type %u\n", __func__,
			   WPA_GET_BE24(pos), pos[3]);

		pos = next;
	}
}


static void ieee802_11_rx_wnm_notif_req(struct wpa_supplicant *wpa_s,
					const u8 *sa, const u8 *frm, int len)
{
	const u8 *pos, *end;
	u8 dialog_token, type;

	/* Dialog Token [1] | Type [1] | Subelements */

	if (len < 2 || sa == NULL)
		return;
	end = frm + len;
	pos = frm;
	dialog_token = *pos++;
	type = *pos++;

	da16x_wnm_prt("     [%s] WNM: Received WNM-Notification Request "
		"(dialog_token %u type %u sa " MACSTR ")\n", __func__,
		dialog_token, type, MAC2STR(sa));
	da16x_dump("WNM-Notification Request subelements",
		    pos, end - pos);

	if (wpa_s->wpa_state != WPA_COMPLETED ||
	    os_memcmp(sa, wpa_s->bssid, ETH_ALEN) != 0) {
		da16x_wnm_prt("     [%s] WNM: WNM-Notification frame not "
			"from our AP - ignore it");
		return;
	}

	switch (type) {
	case 1:
		ieee802_11_rx_wnm_notif_req_wfa(wpa_s, sa, pos, end - pos);
		break;
	default:
		da16x_wnm_prt("     [%s] WNM: Ignore unknown "
			"WNM-Notification type %u\n", __func__, type);
		break;
	}
}
#endif /* CONFIG_WNM_NOTIFICATION */

#ifdef CONFIG_WNM_ACTIONS
void ieee802_11_rx_wnm_action(struct wpa_supplicant *wpa_s,
			      const struct _ieee80211_mgmt *mgmt, size_t len)
{
	const u8 *pos, *end;
	u8 act;

	if (len < IEEE80211_HDRLEN + 2)
		return;

	pos = &mgmt->u.action.category;
	pos++;
	act = *pos++;
	end = ((const u8 *) mgmt) + len;

	da16x_wnm_prt("     [%s] WNM: RX action %u from " MACSTR "\n", __func__,
		   act, MAC2STR(mgmt->sa));
	if (wpa_s->wpa_state < WPA_ASSOCIATED ||
	    os_memcmp(mgmt->sa, wpa_s->bssid, ETH_ALEN) != 0) {
		da16x_wnm_prt("     [%s] WNM: Ignore unexpected WNM Action "
			   "frame");
		return;
	}

	switch (act) {
#ifdef CONFIG_WNM_BSS_TRANS_MGMT
	case WNM_BSS_TRANS_MGMT_REQ:
		ieee802_11_rx_bss_trans_mgmt_req(wpa_s, pos, end,
						 !(mgmt->da[0] & 0x01));
		break;
#endif /* CONFIG_WNM_BSS_TRANS_MGMT */
#ifdef CONFIG_WNM_SLEEP_MODE
	case WNM_SLEEP_MODE_RESP:
		ieee802_11_rx_wnmsleep_resp(wpa_s, pos, end - pos);
		break;
#endif /* CONFIG_WNM_SLEEP_MODE */
#ifdef CONFIG_WNM_NOTIFICATION
	case WNM_NOTIFICATION_REQ:
		ieee802_11_rx_wnm_notif_req(wpa_s, mgmt->sa, pos, end - pos);
		break;
#endif /* CONFIG_WNM_NOTIFICATION */
	default:
		da16x_wnm_prt("     [%s] WNM: Unknown request\n", __func__);
		break;
	}
}
#endif /* CONFIG_WNM_ACTIONS */
