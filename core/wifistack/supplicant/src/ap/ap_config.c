/*
 * hostapd / Configuration helper functions
 * Copyright (c) 2003-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#ifdef	CONFIG_AP

#include "utils/supp_common.h"
#include "crypto/sha1.h"
#include "crypto/tls.h"
#ifdef	CONFIG_RADIUS
#include "radius/radius_client.h"
#endif	/* CONFIG_RADIUS */
#include "common/ieee802_11_defs.h"
#include "common/eapol_common.h"
#if 0
#include "common/dhcp.h"
#endif /* 0 */
#include "eap_common/eap_wsc_common.h"
#include "eap_server/eap.h"
#include "wpa_auth.h"
#include "sta_info.h"
#include "ap_config.h"
#include "supp_config.h"
#include "wpa_supplicant_i.h"

extern int get_run_mode(void);
extern int i3ed11_freq_to_ch(int freq);

#ifdef	CONFIG_AP_VLAN
static void hostapd_config_free_vlan(struct hostapd_bss_config *bss)
{
	struct hostapd_vlan *vlan, *prev;

	vlan = bss->vlan;
	prev = NULL;
	while (vlan) {
		prev = vlan;
		vlan = vlan->next;
		os_free(prev);
	}

	bss->vlan = NULL;
}
#endif	/* CONFIG_AP_VLAN */

#ifndef DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES
#define DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES 0
#endif /* DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES */

void hostapd_config_defaults_bss(struct hostapd_bss_config *bss)
{
	dl_list_init(&bss->anqp_elem);

#ifdef	CONFIG_AP_SYSLOG
	bss->logger_syslog_level = HOSTAPD_LEVEL_INFO;
	bss->logger_stdout_level = HOSTAPD_LEVEL_INFO;
	bss->logger_syslog = (unsigned int) -1;
	bss->logger_stdout = (unsigned int) -1;
#endif	/* CONFIG_AP_SYSLOG */

	bss->auth_algs = WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED;

#ifdef	CONFIG_AP_WEP_KEY
	bss->wep_rekeying_period = 300;
	/* use key0 in individual key and key1 in broadcast key */
	bss->broadcast_key_idx_min = 1;
	bss->broadcast_key_idx_max = 2;
#endif	/* CONFIG_AP_WEP_KEY */
	bss->eap_reauth_period = 3600;

	bss->wpa_group_rekey = 600;
	bss->wpa_gmk_rekey = 86400;
	bss->wpa_group_update_count = 4;
	bss->wpa_pairwise_update_count = 4;
	bss->wpa_disable_eapol_key_retries =
		DEFAULT_WPA_DISABLE_EAPOL_KEY_RETRIES;
	bss->wpa_key_mgmt = WPA_KEY_MGMT_PSK;
	bss->wpa_pairwise = WPA_CIPHER_TKIP;
	bss->wpa_group = WPA_CIPHER_TKIP;
	bss->rsn_pairwise = 0;

#ifdef	CONFIG_CONCURRENT
	if (   get_run_mode() == STA_SOFT_AP_CONCURRENT_MODE
#ifdef	CONFIG_P2P
		|| get_run_mode() == STA_P2P_CONCURRENT_MODE
#endif	/* CONFIG_P2P */
#if 0 //def CONFIG_MESH
	    || get_run_mode() == STA_MESH_PORTAL_MODE
#endif /* CONFIG_MESH */
	    ) {
		bss->max_num_sta = DEFAULT_CONCURRENT_MAX_NUM_STA;
	} else
#endif	/* CONFIG_CONCURRENT */
#ifdef CONFIG_MESH
	if(get_run_mode() == MESH_POINT_MODE)
		bss->max_num_sta = DEFAULT_POINT_MAX_PEER_LINKS;
	else if(get_run_mode() == STA_MESH_PORTAL_MODE)
		bss->max_num_sta = DEFAULT_POTAL_MAX_PEER_LINKS;
	else	
#endif
		bss->max_num_sta = DEFAULT_MAX_NUM_STA; // MAX_STA_COUNT;

	bss->dtim_period = DTIM_PERIOD_DEFAULT;

#ifdef	CONFIG_RADIUS
	bss->radius_server_auth_port = 1812;
#endif	/* CONFIG_RADIUS */
#ifdef	UNUSED_CODE
	bss->eap_sim_db_timeout = 1;
#endif	/* UNUSED_CODE */
	bss->ap_max_inactivity = AP_MAX_INACTIVITY;
	bss->eapol_version = EAPOL_VERSION;

	bss->max_listen_interval = 65535;

	bss->pwd_group = 19; /* ECC: GF(p=256) */

#ifdef CONFIG_IEEE80211W
	bss->assoc_sa_query_max_timeout = 1000;
	bss->assoc_sa_query_retry_timeout = 201;
	bss->group_mgmt_cipher = WPA_CIPHER_AES_128_CMAC;
#endif /* CONFIG_IEEE80211W */
#ifdef EAP_SERVER_FAST
	 /* both anonymous and authenticated provisioning */
	bss->eap_fast_prov = 3;
	bss->pac_key_lifetime = 7 * 24 * 60 * 60;
	bss->pac_key_refresh_time = 1 * 24 * 60 * 60;
#endif /* EAP_SERVER_FAST */

#ifdef	CONFIG_AP_WMM
	/* Set to -1 as defaults depends on HT in setup */
	bss->wmm_enabled = -1;
#endif	/* CONFIG_AP_WMM */

#ifdef CONFIG_IEEE80211R_AP
	bss->ft_over_ds = 1;
	bss->rkh_pos_timeout = 86400;
	bss->rkh_neg_timeout = 60;
	bss->rkh_pull_timeout = 1000;
	bss->rkh_pull_retries = 4;
	bss->r0_key_lifetime = 1209600;
#endif /* CONFIG_IEEE80211R_AP */

#ifdef	CONFIG_RADIUS
	bss->radius_das_time_window = 300;
#endif	/* CONFIG_RADIUS */

#ifdef CONFIG_SAE
	bss->sae_anti_clogging_threshold = 1; //5;
	bss->sae_sync = 5;
#endif	// CONFIG_SAE

#ifdef CONFIG_GAS
	bss->gas_frag_limit = 1400;
#endif /* CONFIG_GAS */

#ifdef CONFIG_FILS
	dl_list_init(&bss->fils_realms);
	bss->fils_hlp_wait_time = 30;
	bss->dhcp_server_port = DHCP_SERVER_PORT;
	bss->dhcp_relay_port = DHCP_SERVER_PORT;
#endif /* CONFIG_FILS */

#ifdef CONFIG_M2U_BC_DEAUTH
	bss->broadcast_deauth = 1;
#endif /* CONFIG_M2U_BC_DEAUTH */

#ifdef CONFIG_MBO
	bss->mbo_cell_data_conn_pref = -1;
#endif /* CONFIG_MBO */

	/* Disable TLS v1.3 by default for now to avoid interoperability issue.
	 * This can be enabled by default once the implementation has been fully
	 * completed and tested with other implementations. */
	bss->tls_flags = TLS_CONN_DISABLE_TLSv1_3;

	bss->set_greenfield = 0;	//greenfield,disable

#ifdef CONFIG_OWE_AP
#ifdef TEST_OWE_GROUP_FIXED // TEST
	bss->owe_groups = os_calloc(2, sizeof(int));
	bss->owe_groups[0] = 20;
	bss->owe_groups[1] = 21;
#else
	bss->owe_groups = os_calloc(3,sizeof(int));
	bss->owe_groups[0] = 19;
	bss->owe_groups[1] = 20;
	bss->owe_groups[2] = 21;
#endif /* TEST_OWE_GROUP_FIXED */
#endif /* CONFIG_OWE_AP */
}


struct hostapd_config * hostapd_config_defaults(void)
{
#define ecw2cw(ecw) ((1 << (ecw)) - 1)

	struct hostapd_config *conf;
	struct hostapd_bss_config *bss;
	const int aCWmin = 4, aCWmax = 10;
	const struct hostapd_wmm_ac_params ac_bk =
		{ aCWmin, aCWmax, 7, 0, 0 }; /* background traffic */
	const struct hostapd_wmm_ac_params ac_be =
		{ aCWmin, aCWmax, 3, 0, 0 }; /* best effort traffic */
	const struct hostapd_wmm_ac_params ac_vi = /* video traffic */
		{ aCWmin - 1, aCWmin, 2, 3008 / 32, 0 };
	const struct hostapd_wmm_ac_params ac_vo = /* voice traffic */
		{ aCWmin - 2, aCWmin - 1, 2, 1504 / 32, 0 };
	const struct hostapd_tx_queue_params txq_bk =
		{ 7, ecw2cw(aCWmin), ecw2cw(aCWmax), 0 };
	const struct hostapd_tx_queue_params txq_be =
		{ 3, ecw2cw(aCWmin), 4 * (ecw2cw(aCWmin) + 1) - 1, 0};
	const struct hostapd_tx_queue_params txq_vi =
		{ 1, (ecw2cw(aCWmin) + 1) / 2 - 1, ecw2cw(aCWmin), 30};
	const struct hostapd_tx_queue_params txq_vo =
		{ 1, (ecw2cw(aCWmin) + 1) / 4 - 1,
		  (ecw2cw(aCWmin) + 1) / 2 - 1, 15};

#undef ecw2cw

	conf = os_zalloc(sizeof(*conf));
	bss = os_zalloc(sizeof(*bss));
	if (conf == NULL || bss == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory for "
			   "configuration data.");
		os_free(conf);
		os_free(bss);
		return NULL;
	}
	conf->bss = os_calloc(1, sizeof(struct hostapd_bss_config *));
	if (conf->bss == NULL) {
		os_free(conf);
		os_free(bss);
		return NULL;
	}
	conf->bss[0] = bss;

#ifdef	CONFIG_RADIUS
	bss->radius = os_zalloc(sizeof(*bss->radius));
	if (bss->radius == NULL) {
		os_free(conf->bss);
		os_free(conf);
		os_free(bss);
		return NULL;
	}
#endif	/* CONFIG_RADIUS */

	hostapd_config_defaults_bss(bss);

	strcpy(conf->country, DEFAULT_AP_COUNTRY);
#if 1
	conf->channel = i3ed11_freq_to_ch(FREQUENCE_DEFAULT);
#else
//	conf->channel = DEFAULT_AP_CHANNEL;
#endif /* 1 */

	conf->num_bss = 1;

	conf->beacon_int = 100;
	conf->rts_threshold = -1; /* use driver default: 2347 */
	conf->fragm_threshold = -1; /* user driver default: 2346 */
	conf->send_probe_response = 1;
	/* Set to invalid value means do not add Power Constraint IE */
	conf->local_pwr_constraint = -1;

	conf->wmm_ac_params[0] = ac_be;
	conf->wmm_ac_params[1] = ac_bk;
	conf->wmm_ac_params[2] = ac_vi;
	conf->wmm_ac_params[3] = ac_vo;

	conf->tx_queue[0] = txq_vo;
	conf->tx_queue[1] = txq_vi;
	conf->tx_queue[2] = txq_be;
	conf->tx_queue[3] = txq_bk;

	conf->ht_capab = HT_CAP_INFO_SMPS_DISABLED;

	conf->ap_table_max_size = 255;
	conf->ap_table_expiration_time = 60;
#ifdef CONFIG_TESTING_OPTIONS
	conf->track_sta_max_age = 180;

	conf->ignore_probe_probability = 0.0;
	conf->ignore_auth_probability = 0.0;
	conf->ignore_assoc_probability = 0.0;
	conf->ignore_reassoc_probability = 0.0;
	conf->corrupt_gtk_rekey_mic_probability = 0.0;
	conf->ecsa_ie_only = 0;
#endif /* CONFIG_TESTING_OPTIONS */

	conf->acs = 0;
	conf->acs_ch_list.num = 0;
#ifdef CONFIG_ACS
	conf->acs_num_scans = 1; //5; /* by Jin */
#endif /* CONFIG_ACS */

	/* The third octet of the country string uses an ASCII space character
	 * by default to indicate that the regulations encompass all
	 * environments for the current frequency band in the country. */
	conf->country[2] = ' ';

	return conf;
}


int hostapd_mac_comp(const void *a, const void *b)
{
	return os_memcmp(a, b, sizeof(macaddr));
}

#if 0	/* by Bright */
static int hostapd_config_read_wpa_psk(const char *fname,
				       struct hostapd_ssid *ssid)
{
	FILE *f;
	char buf[128], *pos;
	int line = 0, ret = 0, len, ok;
	u8 addr[ETH_ALEN];
	struct hostapd_wpa_psk *psk;

	if (!fname)
		return 0;

	f = fopen(fname, "r");
	if (!f) {
		wpa_printf(MSG_ERROR, "WPA PSK file '%s' not found.", fname);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f)) {
		line++;

		if (buf[0] == '#')
			continue;
		pos = buf;
		while (*pos != '\0') {
			if (*pos == '\n') {
				*pos = '\0';
				break;
			}
			pos++;
		}
		if (buf[0] == '\0')
			continue;

		if (hwaddr_aton(buf, addr)) {
			wpa_printf(MSG_ERROR, "Invalid MAC address '%s' on "
				   "line %d in '%s'", buf, line, fname);
			ret = -1;
			break;
		}

		psk = os_zalloc(sizeof(*psk));
		if (psk == NULL) {
			wpa_printf(MSG_ERROR, "WPA PSK allocation failed");
			ret = -1;
			break;
		}
		if (is_zero_ether_addr(addr))
			psk->group = 1;
		else
			os_memcpy(psk->addr, addr, ETH_ALEN);

		pos = buf + 17;
		if (*pos == '\0') {
			wpa_printf(MSG_ERROR, "No PSK on line %d in '%s'",
				   line, fname);
			os_free(psk);
			ret = -1;
			break;
		}
		pos++;

		ok = 0;
		len = os_strlen(pos);
		if (len == 64 && hexstr2bin(pos, psk->psk, PMK_LEN) == 0)
			ok = 1;
		else if (len >= 8 && len < 64) {
			pbkdf2_sha1(pos, ssid->ssid, ssid->ssid_len,
				    4096, psk->psk, PMK_LEN);
			ok = 1;
		}
		if (!ok) {
			wpa_printf(MSG_ERROR, "Invalid PSK '%s' on line %d in "
				   "'%s'", pos, line, fname);
			os_free(psk);
			ret = -1;
			break;
		}

		psk->next = ssid->wpa_psk;
		ssid->wpa_psk = psk;
	}

	fclose(f);

	return ret;
}
#endif	/* 0 */


static int hostapd_derive_psk(struct hostapd_ssid *ssid)
{
	ssid->wpa_psk = os_zalloc(sizeof(struct hostapd_wpa_psk));
	if (ssid->wpa_psk == NULL) {
		wpa_printf(MSG_ERROR, "Unable to alloc space for PSK");
		return -1;
	}
	wpa_hexdump_ascii(MSG_DEBUG, "SSID",
			  (u8 *) ssid->ssid, ssid->ssid_len);
	wpa_hexdump_ascii_key(MSG_DEBUG, "PSK (ASCII passphrase)",
			      (u8 *) ssid->wpa_passphrase,
			      os_strlen(ssid->wpa_passphrase));

	pbkdf2_sha1(ssid->wpa_passphrase,
		    ssid->ssid, ssid->ssid_len,
		    4096, ssid->wpa_psk->psk, PMK_LEN);

	wpa_hexdump_key(MSG_DEBUG, "PSK (from passphrase)",
			ssid->wpa_psk->psk, PMK_LEN);
	return 0;
}


int hostapd_setup_wpa_psk(struct hostapd_bss_config *conf)
{
	struct hostapd_ssid *ssid = &conf->ssid;

	if (ssid->wpa_passphrase != NULL) {
		if (ssid->wpa_psk != NULL) {
			wpa_printf_dbg(MSG_DEBUG, "Using pre-configured WPA PSK "
				   "instead of passphrase");
		} else {
			wpa_printf_dbg(MSG_DEBUG, "Deriving WPA PSK based on "
				   "passphrase");
			if (hostapd_derive_psk(ssid) < 0)
				return -1;
		}
		ssid->wpa_psk->group = 1;
	}
#if 0	/* by Bright */
	return hostapd_config_read_wpa_psk(ssid->wpa_psk_file, &conf->ssid);
#else
	return 0;
#endif
}


#ifdef	CONFIG_RADIUS
static void hostapd_config_free_radius(struct hostapd_radius_server *servers,
				       int num_servers)
{
	int i;

	for (i = 0; i < num_servers; i++) {
		os_free(servers[i].shared_secret);
	}
	os_free(servers);
}


struct hostapd_radius_attr *
hostapd_config_get_radius_attr(struct hostapd_radius_attr *attr, u8 type)
{
	for (; attr; attr = attr->next) {
		if (attr->type == type)
			return attr;
	}
	return NULL;
}


static void hostapd_config_free_radius_attr(struct hostapd_radius_attr *attr)
{
	struct hostapd_radius_attr *prev;

	while (attr) {
		prev = attr;
		attr = attr->next;
		wpabuf_free(prev->val);
		os_free(prev);
	}
}
#endif	/* CONFIG_RADIUS */


void hostapd_config_free_eap_user(struct hostapd_eap_user *user)
{
#ifdef	CONFIG_RADIUS
	hostapd_config_free_radius_attr(user->accept_attr);
#endif	/* CONFIG_RADIUS */
	os_free(user->identity);
#ifdef	CONFIG_SUPP27_BIN_CLR_FREE
	bin_clear_free(user->password, user->password_len);
	bin_clear_free(user->salt, user->salt_len);
#else
	os_free(user->password);
	os_free(user->salt);
#endif	/* CONFIG_SUPP27_BIN_CLR_FREE */
	os_free(user);
}


void hostapd_config_free_eap_users(struct hostapd_eap_user *user)
{
	struct hostapd_eap_user *prev_user;

	while (user) {
		prev_user = user;
		user = user->next;
		hostapd_config_free_eap_user(prev_user);
	}
}

#ifdef	CONFIG_AP_WEP_KEY
static void hostapd_config_free_wep(struct hostapd_wep_keys *keys)
{
	int i;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
#ifdef	CONFIG_SUPP27_BIN_CLR_FREE
		bin_clear_free(keys->key[i], keys->len[i]);
#else
		os_free(keys->key[i]);
#endif	/* CONFIG_SUPP27_BIN_CLR_FREE */
		keys->key[i] = NULL;
	}
}
#endif	/* CONFIG_AP_WEP_KEY */


void hostapd_config_clear_wpa_psk(struct hostapd_wpa_psk **l)
{
	struct hostapd_wpa_psk *psk, *tmp;

	for (psk = *l; psk;) {
		tmp = psk;
		psk = psk->next;
#ifdef	CONFIG_SUPP27_BIN_CLR_FREE
		bin_clear_free(tmp, sizeof(*tmp));
#else
		os_free(tmp);
#endif	/* CONFIG_SUPP27_BIN_CLR_FREE */
	}
	*l = NULL;
}


static void hostapd_config_free_anqp_elem(struct hostapd_bss_config *conf)
{
	struct anqp_element *elem;

	while ((elem = dl_list_first(&conf->anqp_elem, struct anqp_element,
				     list))) {
		dl_list_del(&elem->list);
		wpabuf_free(elem->payload);
		os_free(elem);
	}
}


#ifdef CONFIG_FILS
static void hostapd_config_free_fils_realms(struct hostapd_bss_config *conf)
{

	struct fils_realm *realm;

	while ((realm = dl_list_first(&conf->fils_realms, struct fils_realm,
				      list))) {
		dl_list_del(&realm->list);
		os_free(realm);
	}
}
#endif /* CONFIG_FILS */


#ifdef CONFIG_SAE
static void hostapd_config_free_sae_passwords(struct hostapd_bss_config *conf)
{
	struct sae_password_entry *pw, *tmp;

	pw = conf->sae_passwords;
	conf->sae_passwords = NULL;
	while (pw) {
		tmp = pw;
		pw = pw->next;
#ifdef	CONFIG_SUPP27_STR_CLR_FREE
		str_clear_free(tmp->password);
#else
		os_free(tmp->password);
#endif	/* CONFIG_SUPP27_STR_CLR_FREE */
		os_free(tmp->identifier);
		os_free(tmp);
	}
}
#endif	// CONFIG_SAE


void hostapd_config_free_bss(struct hostapd_bss_config *conf)
{
	if (conf == NULL)
		return;

	hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);

#ifdef	CONFIG_SUPP27_STR_CLR_FREE
	str_clear_free(conf->ssid.wpa_passphrase);
#else
	os_free(conf->ssid.wpa_passphrase);
#endif	/* CONFIG_SUPP27_STR_CLR_FREE */
	os_free(conf->ssid.wpa_psk_file);

#ifdef	CONFIG_AP_WEP_KEY
	hostapd_config_free_wep(&conf->ssid.wep);
#endif	/* CONFIG_AP_WEP_KEY */

#ifdef CONFIG_FULL_DYNAMIC_VLAN
	os_free(conf->ssid.vlan_tagged_interface);
#endif /* CONFIG_FULL_DYNAMIC_VLAN */

	hostapd_config_free_eap_users(conf->eap_user);
	os_free(conf->eap_user_sqlite);

	os_free(conf->eap_req_id_text);
#ifdef	CONFIG_ERP
	os_free(conf->erp_domain);
#endif	/* CONFIG_ERP */
#ifdef	CONFIG_ACL
	os_free(conf->accept_mac);
	os_free(conf->deny_mac);
#endif	/* CONFIG_ACL */
	os_free(conf->nas_identifier);

#ifdef	CONFIG_RADIUS
	if (conf->radius) {
		hostapd_config_free_radius(conf->radius->auth_servers,
					   conf->radius->num_auth_servers);
		hostapd_config_free_radius(conf->radius->acct_servers,
					   conf->radius->num_acct_servers);
	}
	hostapd_config_free_radius_attr(conf->radius_auth_req_attr);
	hostapd_config_free_radius_attr(conf->radius_acct_req_attr);
#endif	/* CONFIG_RADIUS */

#ifdef	CONFIG_AP_PRE_AUTH
	os_free(conf->rsn_preauth_interfaces);
#endif	/* CONFIG_AP_PRE_AUTH */
#ifdef	CONFIG_CTRL_IFACE
	os_free(conf->ctrl_interface);
#endif	/* CONFIG_CTRL_IFACE */
	os_free(conf->ca_cert);
	os_free(conf->server_cert);
	os_free(conf->private_key);
	os_free(conf->private_key_passwd);
	os_free(conf->ocsp_stapling_response);
	os_free(conf->ocsp_stapling_response_multi);
	os_free(conf->dh_file);
#ifdef CONFIG_OPENSSL_MOD
	os_free(conf->openssl_ciphers);
#endif /* CONFIG_OPENSSL_MOD */
	os_free(conf->pac_opaque_encr_key);
	os_free(conf->eap_fast_a_id);
	os_free(conf->eap_fast_a_id_info);
#ifdef	UNUSED_CODE
	os_free(conf->eap_sim_db);
#endif	/* UNUSED_CODE */
#ifdef	CONFIG_RADIUS
	os_free(conf->radius_server_clients);
	os_free(conf->radius);
	os_free(conf->radius_das_shared_secret);
#endif	/* CONFIG_RADIUS */
#ifdef	CONFIG_AP_VLAN
	hostapd_config_free_vlan(conf);
#endif	/* CONFIG_AP_VLAN */
#ifdef	CONFIG_TIME_ADV
	os_free(conf->time_zone);
#endif	/* CONFIG_TIME_ADV */

#ifdef CONFIG_IEEE80211R_AP
	{
		struct ft_remote_r0kh *r0kh, *r0kh_prev;
		struct ft_remote_r1kh *r1kh, *r1kh_prev;

		r0kh = conf->r0kh_list;
		conf->r0kh_list = NULL;
		while (r0kh) {
			r0kh_prev = r0kh;
			r0kh = r0kh->next;
			os_free(r0kh_prev);
		}

		r1kh = conf->r1kh_list;
		conf->r1kh_list = NULL;
		while (r1kh) {
			r1kh_prev = r1kh;
			r1kh = r1kh->next;
			os_free(r1kh_prev);
		}
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_WPS
	os_free(conf->wps_pin_requests);
	os_free(conf->device_name);
	os_free(conf->manufacturer);
	os_free(conf->model_name);
	os_free(conf->model_number);
	os_free(conf->serial_number);
	os_free(conf->config_methods);
	os_free(conf->ap_pin);
	os_free(conf->extra_cred);
	os_free(conf->ap_settings);
	os_free(conf->upnp_iface);
	os_free(conf->friendly_name);
	os_free(conf->manufacturer_url);
	os_free(conf->model_description);
	os_free(conf->model_url);
	os_free(conf->upc);
	{
		unsigned int i;

		for (i = 0; i < MAX_WPS_VENDOR_EXTENSIONS; i++)
			wpabuf_free(conf->wps_vendor_ext[i]);
	}
#ifdef	CONFIG_P2P_UNUSED_CMD
	wpabuf_free(conf->wps_nfc_dh_pubkey);
	wpabuf_free(conf->wps_nfc_dh_privkey);
	wpabuf_free(conf->wps_nfc_dev_pw);
#endif	/* CONFIG_P2P_UNUSED_CMD */
#endif /* CONFIG_WPS */

#ifdef	CONFIG_ROAM_CONSORTIUM
	os_free(conf->roaming_consortium);
#endif	/* CONFIG_ROAM_CONSORTIUM */
#ifdef	CONFIG_VENUE_NAME
	os_free(conf->venue_name);
	os_free(conf->venue_url);
#endif	/* CONFIG_VENUE_NAME */
#ifdef	CONFIG_NAI_REALM
	os_free(conf->nai_realm_data);
#endif	/* CONFIG_VENUE_NAME */
#ifdef	CONFIG_NETWORK_AUTH_TYPE
	os_free(conf->network_auth_type);
#endif	/* CONFIG_NETWORK_AUTH_TYPE */
#ifdef	CONFIG_3GPP
	os_free(conf->anqp_3gpp_cell_net);
#endif	/* CONFIG_3GPP */
#ifdef	CONFIG_DOMAIN_NAME
	os_free(conf->domain_name);
#endif	/* CONFIG_DOMAIN_NAME */
	hostapd_config_free_anqp_elem(conf);

#ifdef CONFIG_RADIUS_TEST
	os_free(conf->dump_msk_file);
#endif /* CONFIG_RADIUS_TEST */

#ifdef CONFIG_HS20
	os_free(conf->hs20_oper_friendly_name);
	os_free(conf->hs20_wan_metrics);
	os_free(conf->hs20_connection_capability);
	os_free(conf->hs20_operating_class);
	os_free(conf->hs20_icons);
	if (conf->hs20_osu_providers) {
		size_t i;
		for (i = 0; i < conf->hs20_osu_providers_count; i++) {
			struct hs20_osu_provider *p;
			size_t j;
			p = &conf->hs20_osu_providers[i];
			os_free(p->friendly_name);
			os_free(p->server_uri);
			os_free(p->method_list);
			for (j = 0; j < p->icons_count; j++)
				os_free(p->icons[j]);
			os_free(p->icons);
			os_free(p->osu_nai);
			os_free(p->service_desc);
		}
		os_free(conf->hs20_osu_providers);
	}
	if (conf->hs20_operator_icon) {
		size_t i;

		for (i = 0; i < conf->hs20_operator_icon_count; i++)
			os_free(conf->hs20_operator_icon[i]);
		os_free(conf->hs20_operator_icon);
	}
	os_free(conf->subscr_remediation_url);
	os_free(conf->t_c_filename);
	os_free(conf->t_c_server_url);
#endif /* CONFIG_HS20 */

#ifdef	CONFIG_AP_VENDOR_ELEM
	wpabuf_free(conf->vendor_elements);
#endif	/* CONFIG_AP_VENDOR_ELEM */
	wpabuf_free(conf->assocresp_elements);

#ifdef CONFIG_SAE
	os_free(conf->sae_groups);
#endif	// CONFIG_SAE
#ifdef CONFIG_OWE_AP
	os_free(conf->owe_groups);
#endif /* CONFIG_OWE_AP */

#ifdef CONFIG_WOWLAN
	os_free(conf->wowlan_triggers);
#endif	// CONFIG_WOWLAN

	os_free(conf->server_id);

#ifdef CONFIG_TESTING_OPTIONS
	wpabuf_free(conf->own_ie_override);
	wpabuf_free(conf->sae_commit_override);
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_NO_PROBE_RESP_IF_SEEN_ON
	os_free(conf->no_probe_resp_if_seen_on);
#endif /* CONFIG_NO_PROBE_RESP_IF_SEEN_ON */
#ifdef CONFIG_NO_AUTH_IF_SEEN_ON
	os_free(conf->no_auth_if_seen_on);
#endif /* CONFIG_NO_AUTH_IF_SEEN_ON */

#ifdef CONFIG_FILS
	hostapd_config_free_fils_realms(conf);
#endif /* CONFIG_FILS */

#ifdef CONFIG_DPP
	os_free(conf->dpp_connector);
	wpabuf_free(conf->dpp_netaccesskey);
	wpabuf_free(conf->dpp_csign);
#endif /* CONFIG_DPP */

#ifdef CONFIG_SAE
	hostapd_config_free_sae_passwords(conf);
#endif	// CONFIG_SAE

	os_free(conf);
}


/**
 * hostapd_config_free - Free hostapd configuration
 * @conf: Configuration data from hostapd_config_read().
 */
void hostapd_config_free(struct hostapd_config *conf)
{
	size_t i;

	if (conf == NULL)
		return;

	for (i = 0; i < conf->num_bss; i++)
		hostapd_config_free_bss(conf->bss[i]);
	os_free(conf->bss);
	os_free(conf->supported_rates);
	os_free(conf->basic_rates);
	os_free(conf->acs_ch_list.range);
	os_free(conf->driver_params);
#ifdef CONFIG_ACS
	os_free(conf->acs_chan_bias);
#endif /* CONFIG_ACS */
	wpabuf_free(conf->lci);
	wpabuf_free(conf->civic);

	os_free(conf);
}


/**
 * hostapd_maclist_found - Find a MAC address from a list
 * @list: MAC address list
 * @num_entries: Number of addresses in the list
 * @addr: Address to search for
 * @vlan_id: Buffer for returning VLAN ID or %NULL if not needed
 * Returns: 1 if address is in the list or 0 if not.
 *
 * Perform a binary search for given MAC address from a pre-sorted list.
 */
int hostapd_maclist_found(struct mac_acl_entry *list, int num_entries,
			  const u8 *addr
#ifdef CONFIG_AP_VLAN
			  , struct vlan_description *vlan_id
#endif /* CONFIG_VLAN */
			  )
{
	int start, end, middle, res;

	start = 0;
	end = num_entries - 1;

	while (start <= end) {
		middle = (start + end) / 2;
		res = os_memcmp(list[middle].addr, addr, ETH_ALEN);
		if (res == 0) {
#ifdef CONFIG_AP_VLAN
			if (vlan_id)
				*vlan_id = list[middle].vlan_id;
#endif /* CONFIG_AP_VLAN */
			return 1;
		}
		if (res < 0)
			start = middle + 1;
		else
			end = middle - 1;
	}

	return 0;
}

#if 1	/* by Bright : Code sync with Supp 2.4 */
int hostapd_maclist_found_none_sorted(struct mac_acl_entry *list,
			int num_entries, const u8 *addr
#ifdef	CONFIG_AP_VLAN
			, int *vlan_id
#endif	/* CONFIG_AP_VLAN */
			)
{
	int i = 0;
	int res;


	while (i < NUM_MAX_ACL) {
		res = os_memcmp(list[i].addr, addr, ETH_ALEN);
		if (res == 0) {
#ifdef	CONFIG_AP_VLAN
			if (vlan_id)
				*vlan_id = list[i].vlan_id;
#endif	/* CONFIG_AP_VLAN */
			return 1;
		}
		i++;
	}
	return 0;
}

#endif	/* 0 */

int hostapd_rate_found(int *list, int rate)
{
	int i;

	if (list == NULL)
		return 0;

	for (i = 0; list[i] >= 0; i++)
		if (list[i] == rate)
			return 1;

	return 0;
}

#ifdef	CONFIG_AP_VLAN
int hostapd_vlan_valid(struct hostapd_vlan *vlan,
		       struct vlan_description *vlan_desc)
{
	struct hostapd_vlan *v = vlan;
	int i;

	if (!vlan_desc->notempty || vlan_desc->untagged < 0 ||
	    vlan_desc->untagged > MAX_VLAN_ID)
		return 0;
	for (i = 0; i < MAX_NUM_TAGGED_VLAN; i++) {
		if (vlan_desc->tagged[i] < 0 ||
		    vlan_desc->tagged[i] > MAX_VLAN_ID)
			return 0;
	}
	if (!vlan_desc->untagged && !vlan_desc->tagged[0])
		return 0;

	while (v) {
		if (!vlan_compare(&v->vlan_desc, vlan_desc) ||
		    v->vlan_id == VLAN_ID_WILDCARD)
			return 1;
		v = v->next;
	}
	return 0;
}


const char * hostapd_get_vlan_id_ifname(struct hostapd_vlan *vlan, int vlan_id)
{
	struct hostapd_vlan *v = vlan;
	while (v) {
		if (v->vlan_id == vlan_id)
			return v->ifname;
		v = v->next;
	}
	return NULL;
}
#endif	/* CONFIG_AP_VLAN */

const u8 * hostapd_get_psk(const struct hostapd_bss_config *conf,
			   const u8 *addr, const u8 *p2p_dev_addr,
			   const u8 *prev_psk)
{
	struct hostapd_wpa_psk *psk;
	int next_ok = prev_psk == NULL;

#ifdef CONFIG_P2P
	if (p2p_dev_addr && !is_zero_ether_addr(p2p_dev_addr)) {
		wpa_printf_dbg(MSG_DEBUG, "Searching a PSK for " MACSTR
			   " p2p_dev_addr=" MACSTR " prev_psk=%p",
			   MAC2STR(addr), MAC2STR(p2p_dev_addr), prev_psk);
		addr = NULL; /* Use P2P Device Address for matching */
	} else
#endif /* CONFIG_P2P */
	{
		wpa_printf_dbg(MSG_DEBUG, "Searching a PSK for " MACSTR
			   " prev_psk=%p",
			   MAC2STR(addr), prev_psk);
	}

	for (psk = conf->ssid.wpa_psk; psk != NULL; psk = psk->next) {
		if (   next_ok
			&& (   psk->group
				|| (addr && os_memcmp(psk->addr, addr, ETH_ALEN) == 0)
#ifdef CONFIG_P2P
				|| (   !addr
					&& p2p_dev_addr
					&& os_memcmp(psk->p2p_dev_addr, p2p_dev_addr, ETH_ALEN) == 0)
#endif /* CONFIG_P2P */
				)
			)
			return psk->psk;

		if (psk->psk == prev_psk)
			next_ok = 1;
	}

	return NULL;
}


static int hostapd_config_check_bss(struct hostapd_bss_config *bss,
				    struct hostapd_config *conf,
				    int full_config)
{
#ifdef	CONFIG_RADIUS
	if (full_config && bss->ieee802_1x && !bss->eap_server &&
	    !bss->radius->auth_servers) {
#else
	if (full_config && bss->ieee802_1x && !bss->eap_server) {
#endif	/* CONFIG_RADIUS */
		wpa_printf(MSG_ERROR, "Invalid IEEE 802.1X configuration (no "
			   "EAP authenticator configured).");
		return -1;
	}

#ifdef	CONFIG_AP_WEP_KEY
	if (bss->wpa) {
		int wep, i;

		wep = bss->default_wep_key_len > 0 || bss->individual_wep_key_len > 0;
		for (i = 0; i < NUM_WEP_KEYS; i++) {
			if (bss->ssid.wep.keys_set) {
				wep = 1;
				break;
			}
		}

		if (wep) {
			wpa_printf(MSG_ERROR, "WEP configuration in a WPA network is not supported");
			return -1;
		}
	}
#endif	/* CONFIG_AP_WEP_KEY */

#ifdef	CONFIG_RADIUS
	if (full_config && bss->wpa &&
	    bss->wpa_psk_radius != PSK_RADIUS_IGNORED &&
	    bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH) {
		wpa_printf(MSG_ERROR, "WPA-PSK using RADIUS enabled, but no "
			   "RADIUS checking (macaddr_acl=2) enabled.");
		return -1;
	}
#endif	/* CONFIG_RADIUS */

#ifdef	CONFIG_RADIUS
	if (full_config && bss->wpa && (bss->wpa_key_mgmt & WPA_KEY_MGMT_PSK) &&
	    bss->ssid.wpa_psk == NULL && bss->ssid.wpa_passphrase == NULL &&
	    bss->ssid.wpa_psk_file == NULL &&
	    (bss->wpa_psk_radius != PSK_RADIUS_REQUIRED ||
	     bss->macaddr_acl != USE_EXTERNAL_RADIUS_AUTH)) {
#else
	if (full_config && bss->wpa && (bss->wpa_key_mgmt & WPA_KEY_MGMT_PSK) &&
	    bss->ssid.wpa_psk == NULL && bss->ssid.wpa_passphrase == NULL &&
	    bss->ssid.wpa_psk_file == NULL) {
#endif	/* CONFIG_RADIUS */
		wpa_printf(MSG_ERROR, "WPA-PSK enabled, but PSK or passphrase "
			   "is not configured.");
		return -1;
	}

	if (full_config && !is_zero_ether_addr(bss->bssid)) {
		size_t i;

		for (i = 0; i < conf->num_bss; i++) {
			if (conf->bss[i] != bss &&
			    (hostapd_mac_comp(conf->bss[i]->bssid,
					      bss->bssid) == 0)) {
				wpa_printf(MSG_ERROR, "Duplicate BSSID " MACSTR
					   " on interface '%s' and '%s'.",
					   MAC2STR(bss->bssid),
					   conf->bss[i]->iface, bss->iface);
				return -1;
			}
		}
	}

#ifdef CONFIG_IEEE80211R_AP
	if (full_config && wpa_key_mgmt_ft(bss->wpa_key_mgmt) &&
	    (bss->nas_identifier == NULL ||
	     os_strlen(bss->nas_identifier) < 1 ||
	     os_strlen(bss->nas_identifier) > FT_R0KH_ID_MAX_LEN)) {
		wpa_printf(MSG_ERROR, "FT (IEEE 802.11r) requires "
			   "nas_identifier to be configured as a 1..48 octet "
			   "string");
		return -1;
	}
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_IEEE80211N
	if (full_config && conf->ieee80211n &&
	    conf->hw_mode == HOSTAPD_MODE_IEEE80211B) {
		bss->disable_11n = 1;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) in 11b mode is not "
			   "allowed, disabling HT capabilities");
	}

	if (full_config && conf->ieee80211n &&
	    bss->ssid.security_policy == SECURITY_STATIC_WEP) {
		bss->disable_11n = 1;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) with WEP is not "
			   "allowed, disabling HT capabilities");
	}

	if (full_config && conf->ieee80211n && bss->wpa &&
	    !(bss->wpa_pairwise & WPA_CIPHER_CCMP) &&
	    !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				   WPA_CIPHER_CCMP_256 | WPA_CIPHER_GCMP_256)))
	{
		bss->disable_11n = 1;
		wpa_printf(MSG_ERROR, "HT (IEEE 802.11n) with WPA/WPA2 "
			   "requires CCMP/GCMP to be enabled, disabling HT "
			   "capabilities");
	}
#endif /* CONFIG_IEEE80211N */

#ifdef CONFIG_IEEE80211AC
	if (full_config && conf->ieee80211ac &&
	    bss->ssid.security_policy == SECURITY_STATIC_WEP) {
		bss->disable_11ac = 1;
		wpa_printf(MSG_ERROR,
			   "VHT (IEEE 802.11ac) with WEP is not allowed, disabling VHT capabilities");
	}

	if (full_config && conf->ieee80211ac && bss->wpa &&
	    !(bss->wpa_pairwise & WPA_CIPHER_CCMP) &&
	    !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				   WPA_CIPHER_CCMP_256 | WPA_CIPHER_GCMP_256)))
	{
		bss->disable_11ac = 1;
		wpa_printf(MSG_ERROR,
			   "VHT (IEEE 802.11ac) with WPA/WPA2 requires CCMP/GCMP to be enabled, disabling VHT capabilities");
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_WPS
	if (full_config && bss->wps_state && bss->ignore_broadcast_ssid) {
		wpa_printf(MSG_INFO, "WPS: ignore_broadcast_ssid "
			   "configuration forced WPS to be disabled");
		bss->wps_state = 0;
	}

#ifdef	CONFIG_AP_WEP_KEY
	if (full_config && bss->wps_state &&
	    bss->ssid.wep.keys_set && bss->wpa == 0) {
		wpa_printf(MSG_INFO, "WPS: WEP configuration forced WPS to be "
			   "disabled");
		bss->wps_state = 0;
	}
#endif	/* CONFIG_AP_WEP_KEY */

	if (full_config && bss->wps_state && bss->wpa &&
	    (!(bss->wpa & 2) ||
	     !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				    WPA_CIPHER_CCMP_256 |
				    WPA_CIPHER_GCMP_256)))) {
		wpa_printf(MSG_INFO, "WPS: WPA/TKIP configuration without "
			   "WPA2/CCMP/GCMP forced WPS to be disabled");
		bss->wps_state = 0;
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20
	if (full_config && bss->hs20 &&
	    (!(bss->wpa & 2) ||
	     !(bss->rsn_pairwise & (WPA_CIPHER_CCMP | WPA_CIPHER_GCMP |
				    WPA_CIPHER_CCMP_256 |
				    WPA_CIPHER_GCMP_256)))) {
		wpa_printf(MSG_ERROR, "HS 2.0: WPA2-Enterprise/CCMP "
			   "configuration is required for Hotspot 2.0 "
			   "functionality");
		return -1;
	}
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MBO
	if (full_config && bss->mbo_enabled && (bss->wpa & 2) &&
	    bss->ieee80211w == NO_MGMT_FRAME_PROTECTION) {
		wpa_printf(MSG_ERROR,
			   "MBO: PMF needs to be enabled whenever using WPA2 with MBO");
		return -1;
	}
#endif /* CONFIG_MBO */

	return 0;
}


static int hostapd_config_check_cw(struct hostapd_config *conf, int queue)
{
	int tx_cwmin = conf->tx_queue[queue].cwmin;
	int tx_cwmax = conf->tx_queue[queue].cwmax;
	int ac_cwmin = conf->wmm_ac_params[queue].cwmin;
	int ac_cwmax = conf->wmm_ac_params[queue].cwmax;

	if (tx_cwmin > tx_cwmax) {
		wpa_printf(MSG_ERROR,
			   "Invalid TX queue cwMin/cwMax values. cwMin(%d) greater than cwMax(%d)",
			   tx_cwmin, tx_cwmax);
		return -1;
	}
	if (ac_cwmin > ac_cwmax) {
		wpa_printf(MSG_ERROR,
			   "Invalid WMM AC cwMin/cwMax values. cwMin(%d) greater than cwMax(%d)",
			   ac_cwmin, ac_cwmax);
		return -1;
	}
	return 0;
}


int hostapd_config_check(struct hostapd_config *conf, int full_config)
{
	size_t i;

#ifdef	CONFIG_IEEE80211D
	if (full_config && conf->ieee80211d &&
	    (!conf->country[0] || !conf->country[1])) {
		wpa_printf(MSG_ERROR, "Cannot enable IEEE 802.11d without "
			   "setting the country_code");
		return -1;
	}
#endif	/* CONFIG_IEEE80211D */

#ifdef	CONFIG_IEEE80211H
	if (full_config && conf->ieee80211h && !conf->ieee80211d) {
		wpa_printf(MSG_ERROR, "Cannot enable IEEE 802.11h without "
			   "IEEE 802.11d enabled");
		return -1;
	}
#endif	/* CONFIG_IEEE80211H */

	if (full_config && conf->local_pwr_constraint != -1
#ifdef	CONFIG_IEEE80211D
	    && !conf->ieee80211d
#endif	/* CONFIG_IEEE80211D */
	) {
		wpa_printf(MSG_ERROR, "Cannot add Power Constraint element without Country element");
		return -1;
	}

	if (full_config && conf->spectrum_mgmt_required &&
	    conf->local_pwr_constraint == -1) {
		wpa_printf(MSG_ERROR, "Cannot set Spectrum Management bit without Country and Power Constraint elements");
		return -1;
	}

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		if (hostapd_config_check_cw(conf, i))
			return -1;
	}

	for (i = 0; i < conf->num_bss; i++) {
		if (hostapd_config_check_bss(conf->bss[i], conf, full_config))
			return -1;
	}

	return 0;
}


void hostapd_set_security_params(struct hostapd_bss_config *bss,
				 int full_config)
{
#ifdef CONFIG_AP_WEP_KEY
	if (bss->individual_wep_key_len == 0) {
		/* individual keys are not use; can use key idx0 for
		 * broadcast keys */
		bss->broadcast_key_idx_min = 0;
	}
#endif /* CONFIG_AP_WEP_KEY */

	if ((bss->wpa & 2) && bss->rsn_pairwise == 0)
		bss->rsn_pairwise = bss->wpa_pairwise;
	if (bss->group_cipher)
		bss->wpa_group = bss->group_cipher;
	else
		bss->wpa_group = wpa_select_ap_group_cipher(bss->wpa,
							    bss->wpa_pairwise,
							    bss->rsn_pairwise);
	if (!bss->wpa_group_rekey_set)
		bss->wpa_group_rekey = bss->wpa_group == WPA_CIPHER_TKIP ?
			600 : 86400;

#ifdef CONFIG_RADIUS
	if (full_config) {
		bss->radius->auth_server = bss->radius->auth_servers;
		bss->radius->acct_server = bss->radius->acct_servers;
	}
#endif /* CONFIG_RADIUS */

	if (bss->wpa && bss->ieee802_1x) {
		bss->ssid.security_policy = SECURITY_WPA;
	} else if (bss->wpa) {
		bss->ssid.security_policy = SECURITY_WPA_PSK;
	} else if (bss->ieee802_1x) {
		int cipher = WPA_CIPHER_NONE;
		bss->ssid.security_policy = SECURITY_IEEE_802_1X;

#ifdef CONFIG_AP_WEP_KEY
		bss->ssid.wep.default_len = bss->default_wep_key_len;
		if (full_config && bss->default_wep_key_len) {
			cipher = bss->default_wep_key_len >= 13 ?
				WPA_CIPHER_WEP104 : WPA_CIPHER_WEP40;
		} else if (full_config && bss->ssid.wep.keys_set) {
			if (bss->ssid.wep.len[0] >= 13)
				cipher = WPA_CIPHER_WEP104;
			else
				cipher = WPA_CIPHER_WEP40;
		}
#endif /* CONFIG_AP_WEP_KEY */

		bss->wpa_group = cipher;
		bss->wpa_pairwise = cipher;
		bss->rsn_pairwise = cipher;
		if (full_config)
			bss->wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X_NO_WPA;
#ifdef CONFIG_AP_WEP_KEY
	} else if (bss->ssid.wep.keys_set) {
		int cipher = WPA_CIPHER_WEP40;
		if (bss->ssid.wep.len[0] >= 13)
			cipher = WPA_CIPHER_WEP104;
		bss->ssid.security_policy = SECURITY_STATIC_WEP;
		bss->wpa_group = cipher;
		bss->wpa_pairwise = cipher;
		bss->rsn_pairwise = cipher;
		if (full_config)
			bss->wpa_key_mgmt = WPA_KEY_MGMT_NONE;
#endif /* CONFIG_AP_WEP_KEY */
	} else if (bss->osen) {
		bss->ssid.security_policy = SECURITY_OSEN;
		bss->wpa_group = WPA_CIPHER_CCMP;
		bss->wpa_pairwise = 0;
		bss->rsn_pairwise = WPA_CIPHER_CCMP;
	} else {
		bss->ssid.security_policy = SECURITY_PLAINTEXT;
		if (full_config) {
			bss->wpa_group = WPA_CIPHER_NONE;
			bss->wpa_pairwise = WPA_CIPHER_NONE;
			bss->rsn_pairwise = WPA_CIPHER_NONE;
			bss->wpa_key_mgmt = WPA_KEY_MGMT_NONE;
		}
	}
}

#endif	/* CONFIG_AP */

/* EOF */
