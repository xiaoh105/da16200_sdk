/*
 * Authentication server setup
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#ifdef	CONFIG_AP

#include "crypto/tls.h"
#include "eap_server/eap.h"
#ifdef	EAP_SERVER_SIM
#include "eap_server/eap_sim_db.h"
#endif	/* EAP_SERVER_SIM */
#include "eapol_auth/eapol_auth_sm.h"

#ifdef	CONFIG_RADIUS
#include "radius/radius_server.h"
#endif	/* CONFIG_RADIUS */

#include "hostapd.h"
#include "ap_config.h"
#include "sta_info.h"
#include "authsrv.h"


#if defined(EAP_SERVER_SIM) || defined(EAP_SERVER_AKA)
#define EAP_SIM_DB
#endif /* EAP_SERVER_SIM || EAP_SERVER_AKA */


#ifdef EAP_SIM_DB
static int hostapd_sim_db_cb_sta(struct hostapd_data *hapd,
				 struct sta_info *sta, void *ctx)
{
	if (eapol_auth_eap_pending_cb(sta->eapol_sm, ctx) == 0)
		return 1;
	return 0;
}


static void hostapd_sim_db_cb(void *ctx, void *session_ctx)
{
	struct hostapd_data *hapd = ctx;
	if (ap_for_each_sta(hapd, hostapd_sim_db_cb_sta, session_ctx) == 0) {
#ifdef RADIUS_SERVER
		radius_server_eap_pending_cb(hapd->radius_srv, session_ctx);
#endif /* RADIUS_SERVER */
	}
}
#endif /* EAP_SIM_DB */


#ifdef RADIUS_SERVER

static int hostapd_radius_get_eap_user(void *ctx, const u8 *identity,
				       size_t identity_len, int phase2,
				       struct eap_user *user)
{
	const struct hostapd_eap_user *eap_user;
	int i;

	eap_user = hostapd_get_eap_user(ctx, identity, identity_len, phase2);
	if (eap_user == NULL)
		return -1;

	if (user == NULL)
		return 0;

	os_memset(user, 0, sizeof(*user));
	for (i = 0; i < EAP_MAX_METHODS; i++) {
		user->methods[i].vendor = eap_user->methods[i].vendor;
		user->methods[i].method = eap_user->methods[i].method;
	}

	if (eap_user->password) {
		user->password = os_malloc(eap_user->password_len);
		if (user->password == NULL)
			return -1;
		os_memcpy(user->password, eap_user->password,
			  eap_user->password_len);
		user->password_len = eap_user->password_len;
		user->password_hash = eap_user->password_hash;
	}
	user->force_version = eap_user->force_version;
	user->macacl = eap_user->macacl;
	user->ttls_auth = eap_user->ttls_auth;
	user->remediation = eap_user->remediation;
	user->accept_attr = eap_user->accept_attr;

	return 0;
}


static int hostapd_setup_radius_srv(struct hostapd_data *hapd)
{
	struct radius_server_conf srv;
	struct hostapd_bss_config *conf = hapd->conf;
	os_memset(&srv, 0, sizeof(srv));
	srv.client_file = conf->radius_server_clients;
	srv.auth_port = conf->radius_server_auth_port;
	srv.acct_port = conf->radius_server_acct_port;
	srv.conf_ctx = hapd;
	srv.eap_sim_db_priv = hapd->eap_sim_db_priv;
	srv.ssl_ctx = hapd->ssl_ctx;
	srv.msg_ctx = hapd->msg_ctx;
	srv.pac_opaque_encr_key = conf->pac_opaque_encr_key;
	srv.eap_fast_a_id = conf->eap_fast_a_id;
	srv.eap_fast_a_id_len = conf->eap_fast_a_id_len;
	srv.eap_fast_a_id_info = conf->eap_fast_a_id_info;
	srv.eap_fast_prov = conf->eap_fast_prov;
	srv.pac_key_lifetime = conf->pac_key_lifetime;
	srv.pac_key_refresh_time = conf->pac_key_refresh_time;
	srv.eap_sim_aka_result_ind = conf->eap_sim_aka_result_ind;
	srv.tnc = conf->tnc;
	srv.wps = hapd->wps;
	srv.ipv6 = conf->radius_server_ipv6;
	srv.get_eap_user = hostapd_radius_get_eap_user;
	srv.eap_req_id_text = conf->eap_req_id_text;
	srv.eap_req_id_text_len = conf->eap_req_id_text_len;
	srv.pwd_group = conf->pwd_group;
	srv.server_id = conf->server_id ? conf->server_id : "hostapd";
	srv.sqlite_file = conf->eap_user_sqlite;
#ifdef CONFIG_RADIUS_TEST
	srv.dump_msk_file = conf->dump_msk_file;
#endif /* CONFIG_RADIUS_TEST */

	hapd->radius_srv = radius_server_init(&srv);
	if (hapd->radius_srv == NULL) {
		da16x_ap_prt("[%s] RADIUS server initialization failed.\n",
				__func__);
		return -1;
	}

	return 0;
}

#endif /* RADIUS_SERVER */


int authsrv_init(struct hostapd_data *hapd)
{
#ifdef EAP_TLS_FUNCS
	if (hapd->conf->eap_server &&
	    (hapd->conf->ca_cert || hapd->conf->server_cert ||
	     hapd->conf->private_key || hapd->conf->dh_file)) {
		struct tls_connection_params params;

		hapd->ssl_ctx = tls_init(NULL);
		if (hapd->ssl_ctx == NULL) {
			da16x_ap_prt("[%s] Failed to initialize TLS\n",
						__func__);
			authsrv_deinit(hapd);
			return -1;
		}

		os_memset(&params, 0, sizeof(params));
		params.ca_cert = hapd->conf->ca_cert;
		params.client_cert = hapd->conf->server_cert;
		params.private_key = hapd->conf->private_key;
		params.private_key_passwd = hapd->conf->private_key_passwd;
		params.dh_file = hapd->conf->dh_file;
		params.ocsp_stapling_response =
			hapd->conf->ocsp_stapling_response;

		if (tls_global_set_params(hapd->ssl_ctx, &params)) {
			da16x_ap_prt("[%s] Failed to set TLS parameters\n",
					__func__);
			authsrv_deinit(hapd);
			return -1;
		}

		if (tls_global_set_verify(hapd->ssl_ctx,
					  hapd->conf->check_crl)) {
			da16x_ap_prt("[%s] Failed to enable check_crl\n",
					__func__);
			authsrv_deinit(hapd);
			return -1;
		}
	}
#endif /* EAP_TLS_FUNCS */

#ifdef EAP_SIM_DB
	if (hapd->conf->eap_sim_db) {
		hapd->eap_sim_db_priv =
			eap_sim_db_init(hapd->conf->eap_sim_db,
					hostapd_sim_db_cb, hapd);
		if (hapd->eap_sim_db_priv == NULL) {
			da16x_ap_prt("[%s] Failed to initialize EAP-SIM "
				   "database interface\n", __func__);
			authsrv_deinit(hapd);
			return -1;
		}
	}
#endif /* EAP_SIM_DB */

#ifdef RADIUS_SERVER
	if (hapd->conf->radius_server_clients &&
	    hostapd_setup_radius_srv(hapd))
		return -1;
#endif /* RADIUS_SERVER */

	return 0;
}


void authsrv_deinit(struct hostapd_data *hapd)
{
#ifdef RADIUS_SERVER
	radius_server_deinit(hapd->radius_srv);
	hapd->radius_srv = NULL;
#endif /* RADIUS_SERVER */

#ifdef EAP_TLS_FUNCS
	if (hapd->ssl_ctx) {
		tls_deinit(hapd->ssl_ctx);
		hapd->ssl_ctx = NULL;
	}
#endif /* EAP_TLS_FUNCS */

#ifdef EAP_SIM_DB
	if (hapd->eap_sim_db_priv) {
		eap_sim_db_deinit(hapd->eap_sim_db_priv);
		hapd->eap_sim_db_priv = NULL;
	}
#endif /* EAP_SIM_DB */
}

#endif	/* CONFIG_AP */

/* EOF */
