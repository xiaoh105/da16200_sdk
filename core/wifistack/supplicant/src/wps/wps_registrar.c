/*
 * Wi-Fi Protected Setup - Registrar
 * Copyright (c) 2008-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifdef	CONFIG_WPS

#include "utils/supp_common.h"
#include "utils/base64.h"
#include "utils/supp_eloop.h"
#include "utils/uuid.h"
#include "utils/list.h"
#include "crypto.h"
#include "crypto/sha256.h"
#include "crypto/random.h"
#include "common/ieee802_11_defs.h"
#include "wps_i.h"
#include "wps_dev_attr.h"

#ifndef CONFIG_WPS_STRICT
#define WPS_WORKAROUNDS
#endif /* CONFIG_WPS_STRICT */

struct wps_uuid_pin {
	struct dl_list list;
	u8 uuid[WPS_UUID_LEN];
	int wildcard_uuid;
	u8 *pin;
	size_t pin_len;
#define PIN_LOCKED BIT(0)
#define PIN_EXPIRES BIT(1)
	int flags;
	struct os_reltime expiration;
	u8 enrollee_addr[ETH_ALEN];
};


extern int hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,	\
		const u8 *addr[], const size_t *len, u8 *mac);


static void wps_free_pin(struct wps_uuid_pin *pin)
{
	os_free(pin->pin);
	os_free(pin);
}


static void wps_remove_pin(struct wps_uuid_pin *pin)
{
	dl_list_del(&pin->list);
	wps_free_pin(pin);
}


static void wps_free_pins(struct dl_list *pins)
{
	struct wps_uuid_pin *pin, *prev;
	dl_list_for_each_safe(pin, prev, pins, struct wps_uuid_pin, list)
		wps_remove_pin(pin);
}


struct wps_pbc_session {
	struct wps_pbc_session *next;
	u8 addr[ETH_ALEN];
	u8 uuid_e[WPS_UUID_LEN];
	struct os_reltime timestamp;
};


static void wps_free_pbc_sessions(struct wps_pbc_session *pbc)
{
	struct wps_pbc_session *prev;

	while (pbc) {
		prev = pbc;
		pbc = pbc->next;
		os_free(prev);
	}
}


struct wps_registrar_device {
	struct wps_registrar_device *next;
	struct wps_device_data dev;
	u8 uuid[WPS_UUID_LEN];
};


struct wps_registrar {
	struct wps_context *wps;

	int pbc;
	int selected_registrar;

	int (*new_psk_cb)(void *ctx, const u8 *mac_addr, const u8 *p2p_dev_addr,
			  const u8 *psk, size_t psk_len);
	int (*set_ie_cb)(void *ctx, struct wpabuf *beacon_ie,
			 struct wpabuf *probe_resp_ie);
	void (*pin_needed_cb)(void *ctx, const u8 *uuid_e,
			      const struct wps_device_data *dev);
	void (*reg_success_cb)(void *ctx, const u8 *mac_addr,
			       const u8 *uuid_e, const u8 *dev_pw,
			       size_t dev_pw_len);
	void (*set_sel_reg_cb)(void *ctx, int sel_reg, u16 dev_passwd_id,
			       u16 sel_reg_config_methods);
	void (*enrollee_seen_cb)(void *ctx, const u8 *addr, const u8 *uuid_e,
				 const u8 *pri_dev_type, u16 config_methods,
				 u16 dev_password_id, u8 request_type,
				 const char *dev_name);
	void *cb_ctx;

	struct dl_list pins;
	struct wps_pbc_session *pbc_sessions;

	int skip_cred_build;
	struct wpabuf *extra_cred;
	int disable_auto_conf;
	int sel_reg_union;
	int sel_reg_dev_password_id_override;
	int sel_reg_config_methods_override;
	int static_wep_only;
#ifdef	CONFIG_SUPP27_WPS_DUALBAND
	int dualband;
#endif	/* CONFIG_SUPP27_WPS_DUALBAND */
	int force_per_enrollee_psk;

	struct wps_registrar_device *devices;

	int force_pbc_overlap;

	u8 authorized_macs[WPS_MAX_AUTHORIZED_MACS][ETH_ALEN];
	u8 authorized_macs_union[WPS_MAX_AUTHORIZED_MACS][ETH_ALEN];

#ifdef CONFIG_P2P
	u8 p2p_dev_addr[ETH_ALEN];
#endif /* CONFIG_P2P */

	u8 pbc_ignore_uuid[WPS_UUID_LEN];
#ifdef WPS_WORKAROUNDS
	struct os_reltime pbc_ignore_start;
#endif /* WPS_WORKAROUNDS */
};


static int wps_set_ie(struct wps_registrar *reg);
static void wps_registrar_pbc_timeout(void *eloop_ctx, void *timeout_ctx);
static void wps_registrar_set_selected_timeout(void *eloop_ctx,
					       void *timeout_ctx);
static void wps_registrar_remove_pin(struct wps_registrar *reg,
				     struct wps_uuid_pin *pin);


static void wps_registrar_add_authorized_mac(struct wps_registrar *reg,
					     const u8 *addr)
{
	int i;
	da16x_wps_prt("[%s] WPS: Add authorized MAC " MACSTR "\n", __func__,
		   MAC2STR(addr));
	for (i = 0; i < WPS_MAX_AUTHORIZED_MACS; i++)
		if (os_memcmp(reg->authorized_macs[i], addr, ETH_ALEN) == 0) {
			da16x_wps_prt("[%s] WPS: Authorized MAC was "
				   "already in the list\n", __func__);
			return; /* already in list */
		}
	for (i = WPS_MAX_AUTHORIZED_MACS - 1; i > 0; i--)
		os_memcpy(reg->authorized_macs[i], reg->authorized_macs[i - 1],
			  ETH_ALEN);
	os_memcpy(reg->authorized_macs[0], addr, ETH_ALEN);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Authorized MACs",
		    (u8 *) reg->authorized_macs, sizeof(reg->authorized_macs));
#endif
}


static void wps_registrar_remove_authorized_mac(struct wps_registrar *reg,
						const u8 *addr)
{
	int i;
	da16x_wps_prt("[%s] WPS: Remove authorized MAC " MACSTR "\n", __func__,
		   MAC2STR(addr));
	for (i = 0; i < WPS_MAX_AUTHORIZED_MACS; i++) {
		if (os_memcmp(reg->authorized_macs, addr, ETH_ALEN) == 0)
			break;
	}
	if (i == WPS_MAX_AUTHORIZED_MACS) {
		da16x_wps_prt("[%s] WPS: Authorized MAC was not in the "
			   "list\n", __func__);
		return; /* not in the list */
	}
	for (; i + 1 < WPS_MAX_AUTHORIZED_MACS; i++)
		os_memcpy(reg->authorized_macs[i], reg->authorized_macs[i + 1],
			  ETH_ALEN);
	os_memset(reg->authorized_macs[WPS_MAX_AUTHORIZED_MACS - 1], 0,
		  ETH_ALEN);
	
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Authorized MACs",
		    (u8 *) reg->authorized_macs, sizeof(reg->authorized_macs));
#endif
}


static void wps_free_devices(struct wps_registrar_device *dev)
{
	struct wps_registrar_device *prev;

	while (dev) {
		prev = dev;
		dev = dev->next;
		wps_device_data_free(&prev->dev);
		os_free(prev);
	}
}


static struct wps_registrar_device * wps_device_get(struct wps_registrar *reg,
						    const u8 *addr)
{
	struct wps_registrar_device *dev;

	for (dev = reg->devices; dev; dev = dev->next) {
		if (os_memcmp(dev->dev.mac_addr, addr, ETH_ALEN) == 0)
			return dev;
	}
	return NULL;
}


static void wps_device_clone_data(struct wps_device_data *dst,
				  struct wps_device_data *src)
{
	os_memcpy(dst->mac_addr, src->mac_addr, ETH_ALEN);
	os_memcpy(dst->pri_dev_type, src->pri_dev_type, WPS_DEV_TYPE_LEN);

#define WPS_STRDUP(n) \
	os_free(dst->n); \
	dst->n = src->n ? os_strdup(src->n) : NULL

	WPS_STRDUP(device_name);
	WPS_STRDUP(manufacturer);
	WPS_STRDUP(model_name);
	WPS_STRDUP(model_number);
	WPS_STRDUP(serial_number);
#undef WPS_STRDUP
}


int wps_device_store(struct wps_registrar *reg,
		     struct wps_device_data *dev, const u8 *uuid)
{
	struct wps_registrar_device *d;

	d = wps_device_get(reg, dev->mac_addr);
	if (d == NULL) {
		d = os_zalloc(sizeof(*d));
		if (d == NULL)
			return -1;
		d->next = reg->devices;
		reg->devices = d;
	}

	wps_device_clone_data(&d->dev, dev);
	os_memcpy(d->uuid, uuid, WPS_UUID_LEN);

	return 0;
}


static void wps_registrar_add_pbc_session(struct wps_registrar *reg,
					  const u8 *addr, const u8 *uuid_e)
{
	struct wps_pbc_session *pbc, *prev = NULL;
	struct os_reltime now;

	os_get_reltime(&now);

	pbc = reg->pbc_sessions;
	while (pbc) {
		if (os_memcmp(pbc->addr, addr, ETH_ALEN) == 0 &&
		    os_memcmp(pbc->uuid_e, uuid_e, WPS_UUID_LEN) == 0) {
			if (prev)
				prev->next = pbc->next;
			else
				reg->pbc_sessions = pbc->next;
			break;
		}
		prev = pbc;
		pbc = pbc->next;
	}

	if (!pbc) {
		pbc = os_zalloc(sizeof(*pbc));
		if (pbc == NULL)
			return;
		os_memcpy(pbc->addr, addr, ETH_ALEN);
		if (uuid_e)
			os_memcpy(pbc->uuid_e, uuid_e, WPS_UUID_LEN);
	}

	pbc->next = reg->pbc_sessions;
	reg->pbc_sessions = pbc;
	pbc->timestamp = now;

	/* remove entries that have timed out */
	prev = pbc;
	pbc = pbc->next;

	while (pbc) {
		if (os_reltime_expired(&now, &pbc->timestamp,
				       WPS_PBC_WALK_TIME)) {
			prev->next = NULL;
			wps_free_pbc_sessions(pbc);
			break;
		}
		prev = pbc;
		pbc = pbc->next;
	}
}


static void wps_registrar_remove_pbc_session(struct wps_registrar *reg,
					     const u8 *uuid_e,
					     const u8 *p2p_dev_addr)
{
	struct wps_pbc_session *pbc, *prev = NULL, *tmp;

	pbc = reg->pbc_sessions;
	while (pbc) {
		if (os_memcmp(pbc->uuid_e, uuid_e, WPS_UUID_LEN) == 0
#ifdef CONFIG_P2P
			|| (p2p_dev_addr && !is_zero_ether_addr(reg->p2p_dev_addr) &&
		     os_memcmp(reg->p2p_dev_addr, p2p_dev_addr, ETH_ALEN) == 0)
#endif /* CONFIG_P2P */
			)
		{
			if (prev)
				prev->next = pbc->next;
			else
				reg->pbc_sessions = pbc->next;
			tmp = pbc;
			pbc = pbc->next;
			da16x_wps_prt("[%s] WPS: Removing PBC session for "
				   "addr=" MACSTR "\n", __func__, MAC2STR(tmp->addr));
#ifdef ENABLE_WPS_DBG
			da16x_dump("WPS: Removed UUID-E",
				    tmp->uuid_e, WPS_UUID_LEN);
#endif
			os_free(tmp);
			continue;
		}
		prev = pbc;
		pbc = pbc->next;
	}
}


int wps_registrar_pbc_overlap(struct wps_registrar *reg,
			      const u8 *addr, const u8 *uuid_e)
{
	int count = 0;
	struct wps_pbc_session *pbc;
	struct wps_pbc_session *first = NULL;
	struct os_reltime now;

	os_get_reltime(&now);

	da16x_wps_prt("[%s] WPS: Checking active PBC sessions "
			"for overlap\n", __func__);

	if (uuid_e) {
		da16x_wps_prt("[%s] WPS: Add one for the "
				"requested UUID\n", __func__);
#ifdef ENABLE_WPS_DBG
		da16x_dump("WPS: Requested UUID",
			    uuid_e, WPS_UUID_LEN);
#endif
		count++;
	}

	for (pbc = reg->pbc_sessions; pbc; pbc = pbc->next) {
		da16x_wps_prt("[%s] WPS: Consider PBC session with "
				MACSTR "\n", __func__,
			   	MAC2STR(pbc->addr));
#ifdef ENABLE_WPS_DBG
		da16x_dump("WPS: UUID-E",
			    pbc->uuid_e, WPS_UUID_LEN);
#endif
		if (os_reltime_expired(&now, &pbc->timestamp,
				       WPS_PBC_WALK_TIME)) {
			da16x_wps_prt("[%s] WPS: PBC walk time has "
					"expired\n", __func__);
			break;
		}
		if (first &&
		    os_memcmp(pbc->uuid_e, first->uuid_e, WPS_UUID_LEN) == 0) {
			da16x_wps_prt("[%s] WPS: Same Enrollee\n", __func__);
			continue; /* same Enrollee */
		}
		if (uuid_e == NULL ||
		    os_memcmp(uuid_e, pbc->uuid_e, WPS_UUID_LEN)) {
			da16x_wps_prt("[%s] WPS: New Enrollee\n", __func__);
			count++;
		}
		if (first == NULL)
			first = pbc;
	}

	da16x_wps_prt("[%s] WPS: %u active PBC session(s) found\n\n", __func__, count);

	return count > 1 ? 1 : 0;
}


static int wps_build_wps_state(struct wps_context *wps, struct wpabuf *msg)
{
	da16x_wps_prt("[%s] WPS:  * Wi-Fi Protected Setup State (%d)\n\n", __func__,
		   wps->wps_state);
	wpabuf_put_be16(msg, ATTR_WPS_STATE);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, wps->wps_state);
	return 0;
}


#ifdef CONFIG_WPS_UPNP
static void wps_registrar_free_pending_m2(struct wps_context *wps)
{
	struct upnp_pending_message *p, *p2, *prev = NULL;
	p = wps->upnp_msgs;
	while (p) {
		if (p->type == WPS_M2 || p->type == WPS_M2D) {
			if (prev == NULL)
				wps->upnp_msgs = p->next;
			else
				prev->next = p->next;
			da16x_wps_prt("[%s] WPS UPnP: Drop pending M2/M2D\n", __func__);
			p2 = p;
			p = p->next;
			wpabuf_free(p2->msg);
			os_free(p2);
			continue;
		}
		prev = p;
		p = p->next;
	}
}
#endif /* CONFIG_WPS_UPNP */


#ifdef	CONFIG_WPS_AP
static int wps_build_ap_setup_locked(struct wps_context *wps,
				     struct wpabuf *msg)
{
	if (wps->ap_setup_locked && wps->ap_setup_locked != 2) {
		da16x_wps_prt("[%s] WPS:  * AP Setup Locked\n", __func__);
		wpabuf_put_be16(msg, ATTR_AP_SETUP_LOCKED);
		wpabuf_put_be16(msg, 1);
		wpabuf_put_u8(msg, 1);
	}
	return 0;
}
#endif	/* CONFIG_WPS_AP */


static int wps_build_selected_registrar(struct wps_registrar *reg,
					struct wpabuf *msg)
{
	if (!reg->sel_reg_union)
		return 0;
	da16x_wps_prt("[%s] WPS:  * Selected Registrar\n", __func__);
	wpabuf_put_be16(msg, ATTR_SELECTED_REGISTRAR);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, 1);
	return 0;
}


static int wps_build_sel_reg_dev_password_id(struct wps_registrar *reg,
					     struct wpabuf *msg)
{
	u16 id = reg->pbc ? DEV_PW_PUSHBUTTON : DEV_PW_DEFAULT;
	if (!reg->sel_reg_union)
		return 0;
	if (reg->sel_reg_dev_password_id_override >= 0)
		id = reg->sel_reg_dev_password_id_override;
	da16x_wps_prt("[%s] WPS:  * Device Password ID (%d)\n\n", __func__, id);
	wpabuf_put_be16(msg, ATTR_DEV_PASSWORD_ID);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, id);
	return 0;
}


static int wps_build_sel_pbc_reg_uuid_e(struct wps_registrar *reg,
					struct wpabuf *msg)
{
	u16 id = reg->pbc ? DEV_PW_PUSHBUTTON : DEV_PW_DEFAULT;
	if (!reg->sel_reg_union)
		return 0;
	if (reg->sel_reg_dev_password_id_override >= 0)
		id = reg->sel_reg_dev_password_id_override;
	if (id != DEV_PW_PUSHBUTTON
#ifdef	CONFIG_SUPP27_WPS_DUALBAND
		|| !reg->dualband
#endif	/* CONFIG_SUPP27_WPS_DUALBAND */
		)
		return 0;
	return wps_build_uuid_e(msg, reg->wps->uuid);
}


static void wps_set_pushbutton(u16 *methods, u16 conf_methods)
{
	*methods |= WPS_CONFIG_PUSHBUTTON;
	if ((conf_methods & WPS_CONFIG_VIRT_PUSHBUTTON) ==
	    WPS_CONFIG_VIRT_PUSHBUTTON)
		*methods |= WPS_CONFIG_VIRT_PUSHBUTTON;
	if ((conf_methods & WPS_CONFIG_PHY_PUSHBUTTON) ==
	    WPS_CONFIG_PHY_PUSHBUTTON)
		*methods |= WPS_CONFIG_PHY_PUSHBUTTON;
	if ((*methods & WPS_CONFIG_VIRT_PUSHBUTTON) !=
	    WPS_CONFIG_VIRT_PUSHBUTTON &&
	    (*methods & WPS_CONFIG_PHY_PUSHBUTTON) !=
	    WPS_CONFIG_PHY_PUSHBUTTON) {
		/*
		 * Required to include virtual/physical flag, but we were not
		 * configured with push button type, so have to default to one
		 * of them.
		 */
		*methods |= WPS_CONFIG_PHY_PUSHBUTTON;
	}
}


static int wps_build_sel_reg_config_methods(struct wps_registrar *reg,
					    struct wpabuf *msg)
{
	u16 methods;
	if (!reg->sel_reg_union)
		return 0;
	methods = reg->wps->config_methods;
	methods &= ~WPS_CONFIG_PUSHBUTTON;
	methods &= ~(WPS_CONFIG_VIRT_PUSHBUTTON |
		     WPS_CONFIG_PHY_PUSHBUTTON);
	if (reg->pbc)
		wps_set_pushbutton(&methods, reg->wps->config_methods);
	if (reg->sel_reg_config_methods_override >= 0)
		methods = reg->sel_reg_config_methods_override;
	da16x_wps_prt("[%s] WPS:  * Selected Registrar Config Methods (%x)\n\n", __func__,
		   methods);
	wpabuf_put_be16(msg, ATTR_SELECTED_REGISTRAR_CONFIG_METHODS);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, methods);
	return 0;
}


static int wps_build_probe_config_methods(struct wps_registrar *reg,
					  struct wpabuf *msg)
{
	u16 methods;
	/*
	 * These are the methods that the AP supports as an Enrollee for adding
	 * external Registrars.
	 */
	methods = reg->wps->config_methods & ~WPS_CONFIG_PUSHBUTTON;
	methods &= ~(WPS_CONFIG_VIRT_PUSHBUTTON |
		     WPS_CONFIG_PHY_PUSHBUTTON);
	da16x_wps_prt("[%s] WPS:  * Config Methods (%x)\n\n", __func__, methods);
	wpabuf_put_be16(msg, ATTR_CONFIG_METHODS);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, methods);
	return 0;
}


static int wps_build_config_methods_r(struct wps_registrar *reg,
				      struct wpabuf *msg)
{
	return wps_build_config_methods(msg, reg->wps->config_methods);
}


const u8 * wps_authorized_macs(struct wps_registrar *reg, size_t *count)
{
	*count = 0;

	while (*count < WPS_MAX_AUTHORIZED_MACS) {
		if (is_zero_ether_addr(reg->authorized_macs_union[*count]))
			break;
		(*count)++;
	}

	return (const u8 *) reg->authorized_macs_union;
}


/**
 * wps_registrar_deinit - Deinitialize WPS Registrar data
 * @reg: Registrar data from wps_registrar_init()
 */
void wps_registrar_deinit(struct wps_registrar *reg)
{
	if (reg == NULL)
		return;
	da16x_eloop_cancel_timeout(wps_registrar_pbc_timeout, reg, NULL);
	da16x_eloop_cancel_timeout(wps_registrar_set_selected_timeout, reg, NULL);
	wps_free_pins(&reg->pins);
	wps_free_pbc_sessions(reg->pbc_sessions);
	wpabuf_free(reg->extra_cred);
	wps_free_devices(reg->devices);
	os_free(reg);
}

/**
 * wps_registrar_init - Initialize WPS Registrar data
 * @wps: Pointer to longterm WPS context
 * @cfg: Registrar configuration
 * Returns: Pointer to allocated Registrar data or %NULL on failure
 *
 * This function is used to initialize WPS Registrar functionality. It can be
 * used for a single Registrar run (e.g., when run in a supplicant) or multiple
 * runs (e.g., when run as an internal Registrar in an AP). Caller is
 * responsible for freeing the returned data with wps_registrar_deinit() when
 * Registrar functionality is not needed anymore.
 */
struct wps_registrar *
wps_registrar_init(struct wps_context *wps,
		   const struct wps_registrar_config *cfg)
{
	struct wps_registrar *reg = os_zalloc(sizeof(*reg));
	if (reg == NULL)
		return NULL;

	dl_list_init(&reg->pins);
	reg->wps = wps;
	reg->new_psk_cb = cfg->new_psk_cb;
	reg->set_ie_cb = cfg->set_ie_cb;
	reg->pin_needed_cb = cfg->pin_needed_cb;
	reg->reg_success_cb = cfg->reg_success_cb;
	reg->set_sel_reg_cb = cfg->set_sel_reg_cb;
	reg->enrollee_seen_cb = cfg->enrollee_seen_cb;
	reg->cb_ctx = cfg->cb_ctx;
	reg->skip_cred_build = cfg->skip_cred_build;
	if (cfg->extra_cred) {
		reg->extra_cred = wpabuf_alloc_copy(cfg->extra_cred,
						    cfg->extra_cred_len);
		if (reg->extra_cred == NULL) {
			os_free(reg);
			return NULL;
		}
	}
	reg->disable_auto_conf = cfg->disable_auto_conf;
	reg->sel_reg_dev_password_id_override = -1;
	reg->sel_reg_config_methods_override = -1;
	reg->static_wep_only = cfg->static_wep_only;
#ifdef	CONFIG_SUPP27_WPS_DUALBAND
	reg->dualband = cfg->dualband;
#endif	/* CONFIG_SUPP27_WPS_DUALBAND */
	reg->force_per_enrollee_psk = cfg->force_per_enrollee_psk;

	if (wps_set_ie(reg)) {
		wps_registrar_deinit(reg);
		return NULL;
	}

	return reg;
}


static void wps_registrar_invalidate_unused(struct wps_registrar *reg)
{
	struct wps_uuid_pin *pin;

	dl_list_for_each(pin, &reg->pins, struct wps_uuid_pin, list) {
		if (pin->wildcard_uuid == 1 && !(pin->flags & PIN_LOCKED)) {
			da16x_wps_prt("[%s] WPS: Invalidate previously "
				   "configured wildcard PIN\n", __func__);
			wps_registrar_remove_pin(reg, pin);
			break;
		}
	}
}


/**
 * wps_registrar_add_pin - Configure a new PIN for Registrar
 * @reg: Registrar data from wps_registrar_init()
 * @addr: Enrollee MAC address or %NULL if not known
 * @uuid: UUID-E or %NULL for wildcard (any UUID)
 * @pin: PIN (Device Password)
 * @pin_len: Length of pin in octets
 * @timeout: Time (in seconds) when the PIN will be invalidated; 0 = no timeout
 * Returns: 0 on success, -1 on failure
 */
int wps_registrar_add_pin(struct wps_registrar *reg, const u8 *addr,
			  const u8 *uuid, const u8 *pin, size_t pin_len,
			  int timeout)
{
	struct wps_uuid_pin *p;

	p = os_zalloc(sizeof(*p));
	if (p == NULL)
		return -1;
	if (addr)
		os_memcpy(p->enrollee_addr, addr, ETH_ALEN);
	if (uuid == NULL)
		p->wildcard_uuid = 1;
	else
		os_memcpy(p->uuid, uuid, WPS_UUID_LEN);
	p->pin = os_memdup(pin, pin_len);
	if (p->pin == NULL) {
		os_free(p);
		return -1;
	}
	p->pin_len = pin_len;

	if (timeout) {
		p->flags |= PIN_EXPIRES;
		os_get_reltime(&p->expiration);
		p->expiration.sec += timeout;
	}

	if (p->wildcard_uuid)
		wps_registrar_invalidate_unused(reg);

	dl_list_add(&reg->pins, &p->list);

	da16x_wps_prt("[%s] WPS: A new PIN configured (timeout=%d)\n\n", __func__,
		   timeout);
	
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: UUID", uuid, WPS_UUID_LEN);
	da16x_ascii_dump("WPS: PIN", pin, pin_len);
#endif

	reg->selected_registrar = 1;
	reg->pbc = 0;
	if (addr)
		wps_registrar_add_authorized_mac(reg, addr);
	else
		wps_registrar_add_authorized_mac(
			reg, (u8 *) "\xff\xff\xff\xff\xff\xff");
	wps_registrar_selected_registrar_changed(reg, 0);
	da16x_eloop_cancel_timeout(wps_registrar_set_selected_timeout, reg, NULL);
	da16x_eloop_register_timeout(WPS_PBC_WALK_TIME, 0,
			       wps_registrar_set_selected_timeout,
			       reg, NULL);

	return 0;
}


static void wps_registrar_remove_pin(struct wps_registrar *reg,
				     struct wps_uuid_pin *pin)
{
	u8 *addr;
	u8 bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (is_zero_ether_addr(pin->enrollee_addr))
		addr = bcast;
	else
		addr = pin->enrollee_addr;
	wps_registrar_remove_authorized_mac(reg, addr);
	wps_remove_pin(pin);
	wps_registrar_selected_registrar_changed(reg, 0);
}


static void wps_registrar_expire_pins(struct wps_registrar *reg)
{
	struct wps_uuid_pin *pin, *prev;
	struct os_reltime now;

	os_get_reltime(&now);
	dl_list_for_each_safe(pin, prev, &reg->pins, struct wps_uuid_pin, list)
	{
		if ((pin->flags & PIN_EXPIRES) &&
		    os_reltime_before(&pin->expiration, &now)) {
#ifdef ENABLE_WPS_DBG
			da16x_dump("WPS: Expired PIN for UUID",
				    pin->uuid, WPS_UUID_LEN);
#endif
			wps_registrar_remove_pin(reg, pin);
		}
	}
}


/**
 * wps_registrar_invalidate_wildcard_pin - Invalidate a wildcard PIN
 * @reg: Registrar data from wps_registrar_init()
 * @dev_pw: PIN to search for or %NULL to match any
 * @dev_pw_len: Length of dev_pw in octets
 * Returns: 0 on success, -1 if not wildcard PIN is enabled
 */
static int wps_registrar_invalidate_wildcard_pin(struct wps_registrar *reg,
						 const u8 *dev_pw,
						 size_t dev_pw_len)
{
	struct wps_uuid_pin *pin, *prev;

	dl_list_for_each_safe(pin, prev, &reg->pins, struct wps_uuid_pin, list)
	{
		if (dev_pw && pin->pin &&
		    (dev_pw_len != pin->pin_len ||
		     os_memcmp(dev_pw, pin->pin, dev_pw_len) != 0))
			continue; /* different PIN */
		if (pin->wildcard_uuid) {
#ifdef ENABLE_WPS_DBG
			da16x_dump("WPS: Invalidated PIN for UUID",
				    pin->uuid, WPS_UUID_LEN);
#endif
			wps_registrar_remove_pin(reg, pin);
			return 0;
		}
	}

	return -1;
}


/**
 * wps_registrar_invalidate_pin - Invalidate a PIN for a specific UUID-E
 * @reg: Registrar data from wps_registrar_init()
 * @uuid: UUID-E
 * Returns: 0 on success, -1 on failure (e.g., PIN not found)
 */
int wps_registrar_invalidate_pin(struct wps_registrar *reg, const u8 *uuid)
{
	struct wps_uuid_pin *pin, *prev;

	dl_list_for_each_safe(pin, prev, &reg->pins, struct wps_uuid_pin, list)
	{
		if (os_memcmp(pin->uuid, uuid, WPS_UUID_LEN) == 0) {
#ifdef ENABLE_WPS_DBG
			da16x_dump("WPS: Invalidated PIN for UUID",
				    pin->uuid, WPS_UUID_LEN);
#endif
			wps_registrar_remove_pin(reg, pin);
			return 0;
		}
	}

	return -1;
}


static const u8 * wps_registrar_get_pin(struct wps_registrar *reg,
					const u8 *uuid, size_t *pin_len)
{
	struct wps_uuid_pin *pin, *found = NULL;
	int wildcard = 0;

	wps_registrar_expire_pins(reg);

	dl_list_for_each(pin, &reg->pins, struct wps_uuid_pin, list) {
		if (!pin->wildcard_uuid &&
		    os_memcmp(pin->uuid, uuid, WPS_UUID_LEN) == 0) {
			found = pin;
			break;
		}
	}

	if (!found) {
		/* Check for wildcard UUIDs since none of the UUID-specific
		 * PINs matched */
		dl_list_for_each(pin, &reg->pins, struct wps_uuid_pin, list) {
			if (pin->wildcard_uuid == 1 ||
			    pin->wildcard_uuid == 2) {
				da16x_wps_prt("[%s] WPS: Found a wildcard "
					   "PIN. Assigned it for this UUID-E\n", __func__);
				wildcard = 1;
				os_memcpy(pin->uuid, uuid, WPS_UUID_LEN);
				found = pin;
				break;
			}
		}
	}

	if (!found)
		return NULL;

	/*
	 * Lock the PIN to avoid attacks based on concurrent re-use of the PIN
	 * that could otherwise avoid PIN invalidations.
	 */
	if (found->flags & PIN_LOCKED) {
		da16x_wps_prt("[%s] WPS: Selected PIN locked - do not "
			   "allow concurrent re-use\n", __func__);
		return NULL;
	}
	*pin_len = found->pin_len;
	found->flags |= PIN_LOCKED;
	if (wildcard)
		found->wildcard_uuid++;
	return found->pin;
}


/**
 * wps_registrar_unlock_pin - Unlock a PIN for a specific UUID-E
 * @reg: Registrar data from wps_registrar_init()
 * @uuid: UUID-E
 * Returns: 0 on success, -1 on failure
 *
 * PINs are locked to enforce only one concurrent use. This function unlocks a
 * PIN to allow it to be used again. If the specified PIN was configured using
 * a wildcard UUID, it will be removed instead of allowing multiple uses.
 */
int wps_registrar_unlock_pin(struct wps_registrar *reg, const u8 *uuid)
{
	struct wps_uuid_pin *pin;

	dl_list_for_each(pin, &reg->pins, struct wps_uuid_pin, list) {
		if (os_memcmp(pin->uuid, uuid, WPS_UUID_LEN) == 0) {
			if (pin->wildcard_uuid == 3) {
				da16x_wps_prt("[%s] WPS: Invalidating used "
					   "wildcard PIN\n", __func__);
				return wps_registrar_invalidate_pin(reg, uuid);
			}
			pin->flags &= ~PIN_LOCKED;
			return 0;
		}
	}

	return -1;
}


static void wps_registrar_stop_pbc(struct wps_registrar *reg)
{
	reg->selected_registrar = 0;
	reg->pbc = 0;
#ifdef CONFIG_P2P
	os_memset(reg->p2p_dev_addr, 0, ETH_ALEN);
#endif /* CONFIG_P2P */
	wps_registrar_remove_authorized_mac(reg, (u8 *) "\xff\xff\xff\xff\xff\xff");
	wps_registrar_selected_registrar_changed(reg, 0);
}


static void wps_registrar_pbc_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wps_registrar *reg = eloop_ctx;

	da16x_wps_prt("[%s] WPS: PBC timed out - disable PBC mode\n", __func__);
	wps_pbc_timeout_event(reg->wps);
	wps_registrar_stop_pbc(reg);
}


/**
 * wps_registrar_button_pushed - Notify Registrar that AP button was pushed
 * @reg: Registrar data from wps_registrar_init()
 * @p2p_dev_addr: Limit allowed PBC devices to the specified P2P device, %NULL
 *	indicates no such filtering
 * Returns: 0 on success, -1 on failure, -2 on session overlap
 *
 * This function is called on an AP when a push button is pushed to activate
 * PBC mode. The PBC mode will be stopped after walk time (2 minutes) timeout
 * or when a PBC registration is completed. If more than one Enrollee in active
 * PBC mode has been detected during the monitor time (previous 2 minutes), the
 * PBC mode is not activated and -2 is returned to indicate session overlap.
 * This is skipped if a specific Enrollee is selected.
 */
int wps_registrar_button_pushed(struct wps_registrar *reg,
				const u8 *p2p_dev_addr)
{
	if (p2p_dev_addr == NULL &&
	    wps_registrar_pbc_overlap(reg, NULL, NULL)) {
		da16x_wps_prt("[%s] WPS: PBC overlap - do not start PBC "
			   "mode\n", __func__);
		wps_pbc_overlap_event(reg->wps);
		return -2;
	}
	da16x_wps_prt("[%s] WPS: Button pushed - PBC mode started\n", __func__);
	reg->force_pbc_overlap = 0;
	reg->selected_registrar = 1;
	reg->pbc = 1;
#ifdef CONFIG_P2P
	if (p2p_dev_addr)
		os_memcpy(reg->p2p_dev_addr, p2p_dev_addr, ETH_ALEN);
	else
		os_memset(reg->p2p_dev_addr, 0, ETH_ALEN);
#endif /* CONFIG_P2P */
	wps_registrar_add_authorized_mac(reg,
					 (u8 *) "\xff\xff\xff\xff\xff\xff");
	wps_registrar_selected_registrar_changed(reg, 0);

	wps_pbc_active_event(reg->wps);
	da16x_eloop_cancel_timeout(wps_registrar_set_selected_timeout, reg, NULL);
	da16x_eloop_cancel_timeout(wps_registrar_pbc_timeout, reg, NULL);
	da16x_eloop_register_timeout(WPS_PBC_WALK_TIME, 0, wps_registrar_pbc_timeout,
			       reg, NULL);
	return 0;
}


static void wps_registrar_pbc_completed(struct wps_registrar *reg)
{
	da16x_wps_prt("[%s] WPS: PBC completed - stopping PBC mode\n", __func__);
	da16x_eloop_cancel_timeout(wps_registrar_pbc_timeout, reg, NULL);
	wps_registrar_stop_pbc(reg);
	wps_pbc_disable_event(reg->wps);
#if 1	/* by Shingu 20160630 (WPS Optimize) */
	wps_free_pbc_sessions(reg->pbc_sessions);
#endif	/* 1 */
}


static void wps_registrar_pin_completed(struct wps_registrar *reg)
{
	da16x_wps_prt("[%s] WPS: PIN completed using internal Registrar\n", __func__);
	da16x_eloop_cancel_timeout(wps_registrar_set_selected_timeout, reg, NULL);
	reg->selected_registrar = 0;
	wps_registrar_selected_registrar_changed(reg, 0);
#if 1	/* by Shingu 20160630 (WPS Optimize) */
	wps_free_pins(&reg->pins);
#endif	/* 1 */
}


void wps_registrar_complete(struct wps_registrar *registrar, const u8 *uuid_e,
			    const u8 *dev_pw, size_t dev_pw_len)
{
	if (registrar->pbc) {
		wps_registrar_remove_pbc_session(registrar,
						 uuid_e, NULL);
		wps_registrar_pbc_completed(registrar);
#ifdef WPS_WORKAROUNDS
		os_get_reltime(&registrar->pbc_ignore_start);
#endif /* WPS_WORKAROUNDS */
		os_memcpy(registrar->pbc_ignore_uuid, uuid_e, WPS_UUID_LEN);
	} else {
		wps_registrar_pin_completed(registrar);
	}

	if (dev_pw &&
	    wps_registrar_invalidate_wildcard_pin(registrar, dev_pw,
						  dev_pw_len) == 0) {
		da16x_dump("WPS: Invalidated wildcard PIN",
				dev_pw, dev_pw_len);
	}
}


int wps_registrar_wps_cancel(struct wps_registrar *reg)
{
	if (reg->pbc) {
		da16x_wps_prt("[%s] WPS: PBC is set - cancelling it\n", __func__);
		wps_registrar_pbc_timeout(reg, NULL);
		da16x_eloop_cancel_timeout(wps_registrar_pbc_timeout, reg, NULL);
		return 1;
	} else if (reg->selected_registrar) {
		/* PIN Method */
		da16x_wps_prt("[%s] WPS: PIN is set - cancelling it\n", __func__);
		wps_registrar_pin_completed(reg);
		wps_registrar_invalidate_wildcard_pin(reg, NULL, 0);
		return 1;
	}
	return 0;
}


/**
 * wps_registrar_probe_req_rx - Notify Registrar of Probe Request
 * @reg: Registrar data from wps_registrar_init()
 * @addr: MAC address of the Probe Request sender
 * @wps_data: WPS IE contents
 *
 * This function is called on an AP when a Probe Request with WPS IE is
 * received. This is used to track PBC mode use and to detect possible overlap
 * situation with other WPS APs.
 */
void wps_registrar_probe_req_rx(struct wps_registrar *reg, const u8 *addr,
				const struct wpabuf *wps_data,
				int p2p_wildcard)
{
	struct wps_parse_attr attr;
	int skip_add = 0;

#ifdef ENABLE_WPS_DBG
	da16x_buf_dump("WPS: Probe Request with WPS data received",
			wps_data);
#endif

	if (wps_parse_msg(wps_data, &attr) < 0)
		return;

	if (attr.config_methods == NULL) {
		da16x_wps_prt("[%s] WPS: No Config Methods attribute in "
			   "Probe Request\n", __func__);
		return;
	}

	if (attr.dev_password_id == NULL) {
		da16x_wps_prt("[%s] WPS: No Device Password Id attribute "
			   "in Probe Request\n", __func__);
		return;
	}

	if (reg->enrollee_seen_cb && attr.uuid_e &&
	    attr.primary_dev_type && attr.request_type && !p2p_wildcard) {
		char *dev_name = NULL;
		if (attr.dev_name) {
			dev_name = os_zalloc(attr.dev_name_len + 1);
			if (dev_name) {
				os_memcpy(dev_name, attr.dev_name,
					  attr.dev_name_len);
			}
		}
		reg->enrollee_seen_cb(reg->cb_ctx, addr, attr.uuid_e,
				      attr.primary_dev_type,
				      WPA_GET_BE16(attr.config_methods),
				      WPA_GET_BE16(attr.dev_password_id),
				      *attr.request_type, dev_name);
		os_free(dev_name);
	}

	if (WPA_GET_BE16(attr.dev_password_id) != DEV_PW_PUSHBUTTON)
		return; /* Not PBC */

	da16x_wps_prt("[%s] WPS: Probe Request for PBC received from "
		   MACSTR "\n", __func__, MAC2STR(addr));
	if (attr.uuid_e == NULL) {
		da16x_wps_prt("[%s] WPS: Invalid Probe Request WPS IE: No "
			   "UUID-E included\n", __func__);
		return;
	}
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: UUID-E from Probe Request", attr.uuid_e,
		    WPS_UUID_LEN);
#endif

#ifdef WPS_WORKAROUNDS
	if (reg->pbc_ignore_start.sec &&
	    os_memcmp(attr.uuid_e, reg->pbc_ignore_uuid, WPS_UUID_LEN) == 0) {
		struct os_reltime now, dur;
		os_get_reltime(&now);
		os_reltime_sub(&now, &reg->pbc_ignore_start, &dur);
		if (dur.sec >= 0 && dur.sec < 5) {
			da16x_wps_prt("[%s] WPS: Ignore PBC activation "
				   "based on Probe Request from the Enrollee "
				   "that just completed PBC provisioning\n", __func__);
			skip_add = 1;
		} else
			reg->pbc_ignore_start.sec = 0;
	}
#endif /* WPS_WORKAROUNDS */

	if (!skip_add)
		wps_registrar_add_pbc_session(reg, addr, attr.uuid_e);
	if (wps_registrar_pbc_overlap(reg, addr, attr.uuid_e)) {
		da16x_wps_prt("[%s] WPS: PBC session overlap detected\n", __func__);
		reg->force_pbc_overlap = 1;
		wps_pbc_overlap_event(reg->wps);
	}
}


int wps_cb_new_psk(struct wps_registrar *reg, const u8 *mac_addr,
		   const u8 *p2p_dev_addr, const u8 *psk, size_t psk_len)
{
	if (reg->new_psk_cb == NULL)
		return 0;

	return reg->new_psk_cb(reg->cb_ctx, mac_addr, p2p_dev_addr, psk,
			       psk_len);
}


static void wps_cb_pin_needed(struct wps_registrar *reg, const u8 *uuid_e,
			      const struct wps_device_data *dev)
{
	if (reg->pin_needed_cb == NULL)
		return;

	reg->pin_needed_cb(reg->cb_ctx, uuid_e, dev);
}


static void wps_cb_reg_success(struct wps_registrar *reg, const u8 *mac_addr,
			       const u8 *uuid_e, const u8 *dev_pw,
			       size_t dev_pw_len)
{
	if (reg->reg_success_cb == NULL)
		return;

	reg->reg_success_cb(reg->cb_ctx, mac_addr, uuid_e, dev_pw, dev_pw_len);
}


static int wps_cb_set_ie(struct wps_registrar *reg, struct wpabuf *beacon_ie,
			 struct wpabuf *probe_resp_ie)
{
	return reg->set_ie_cb(reg->cb_ctx, beacon_ie, probe_resp_ie);
}


static void wps_cb_set_sel_reg(struct wps_registrar *reg)
{
	u16 methods = 0;
	if (reg->set_sel_reg_cb == NULL)
		return;

	if (reg->selected_registrar) {
		methods = reg->wps->config_methods & ~WPS_CONFIG_PUSHBUTTON;
		methods &= ~(WPS_CONFIG_VIRT_PUSHBUTTON |
			     WPS_CONFIG_PHY_PUSHBUTTON);
		if (reg->pbc)
			wps_set_pushbutton(&methods, reg->wps->config_methods);
	}

	da16x_wps_prt("[%s] WPS: wps_cb_set_sel_reg: sel_reg=%d "
		   "config_methods=0x%x pbc=%d methods=0x%x\n\n", __func__,
		   reg->selected_registrar, reg->wps->config_methods,
		   reg->pbc, methods);

	reg->set_sel_reg_cb(reg->cb_ctx, reg->selected_registrar,
			    reg->pbc ? DEV_PW_PUSHBUTTON : DEV_PW_DEFAULT,
			    methods);
}


static int wps_set_ie(struct wps_registrar *reg)
{
	struct wpabuf *beacon;
	struct wpabuf *probe;
	const u8 *auth_macs;
	size_t count;
	size_t vendor_len = 0;
	int i;

	if (reg->set_ie_cb == NULL)
		return 0;

	for (i = 0; i < MAX_WPS_VENDOR_EXTENSIONS; i++) {
		if (reg->wps->dev.vendor_ext[i]) {
			vendor_len += 2 + 2;
			vendor_len += wpabuf_len(reg->wps->dev.vendor_ext[i]);
		}
	}

	beacon = wpabuf_alloc(400 + vendor_len);
	if (beacon == NULL)
		return -1;
	probe = wpabuf_alloc(500 + vendor_len);
	if (probe == NULL) {
		wpabuf_free(beacon);
		return -1;
	}

	auth_macs = wps_authorized_macs(reg, &count);

	da16x_wps_prt("[%s] WPS: Build Beacon IEs\n", __func__);

	if (wps_build_version(beacon) ||
	    wps_build_wps_state(reg->wps, beacon) ||
#ifdef	CONFIG_WPS_AP
	    wps_build_ap_setup_locked(reg->wps, beacon) ||
#endif	/* CONFIG_WPS_AP */
	    wps_build_selected_registrar(reg, beacon) ||
	    wps_build_sel_reg_dev_password_id(reg, beacon) ||
	    wps_build_sel_reg_config_methods(reg, beacon) ||
	    wps_build_sel_pbc_reg_uuid_e(reg, beacon) ||
	    (
#ifdef	CONFIG_SUPP27_WPS_DUALBAND
			reg->dualband &&
#endif	/* CONFIG_SUPP27_WPS_DUALBAND */
			wps_build_rf_bands(&reg->wps->dev, beacon, 0)
		) ||
	    wps_build_wfa_ext(beacon, 0, auth_macs, count) ||
	    wps_build_vendor_ext(&reg->wps->dev, beacon)) {
		wpabuf_free(beacon);
		wpabuf_free(probe);
		return -1;
	}

#ifdef CONFIG_P2P
	if (wps_build_dev_name(&reg->wps->dev, beacon) ||
	    wps_build_primary_dev_type(&reg->wps->dev, beacon)) {
		wpabuf_free(beacon);
		wpabuf_free(probe);
		return -1;
	}
#endif /* CONFIG_P2P */

	da16x_wps_prt("[%s] WPS: Build Probe Response IEs\n", __func__);

	if (wps_build_version(probe) ||
	    wps_build_wps_state(reg->wps, probe) ||
#ifdef	CONFIG_WPS_AP
	    wps_build_ap_setup_locked(reg->wps, probe) ||
#endif	/* CONFIG_WPS_AP */
	    wps_build_selected_registrar(reg, probe) ||
	    wps_build_sel_reg_dev_password_id(reg, probe) ||
	    wps_build_sel_reg_config_methods(reg, probe) ||
	    wps_build_resp_type(probe, reg->wps->ap ? WPS_RESP_AP :
				WPS_RESP_REGISTRAR) ||
	    wps_build_uuid_e(probe, reg->wps->uuid) ||
	    wps_build_device_attrs(&reg->wps->dev, probe) ||
	    wps_build_probe_config_methods(reg, probe) ||
	    (
#ifdef	CONFIG_SUPP27_WPS_DUALBAND
			reg->dualband &&
#endif	/* CONFIG_SUPP27_WPS_DUALBAND */
			wps_build_rf_bands(&reg->wps->dev, probe, 0)
		) ||
	    wps_build_wfa_ext(probe, 0, auth_macs, count) ||
	    wps_build_vendor_ext(&reg->wps->dev, probe)) {
		wpabuf_free(beacon);
		wpabuf_free(probe);
		return -1;
	}

	beacon = wps_ie_encapsulate(beacon);
	probe = wps_ie_encapsulate(probe);

	if (!beacon || !probe) {
		wpabuf_free(beacon);
		wpabuf_free(probe);
		return -1;
	}

	if (reg->static_wep_only) {
		/*
		 * Windows XP and Vista clients can get confused about
		 * EAP-Identity/Request when they probe the network with
		 * EAPOL-Start. In such a case, they may assume the network is
		 * using IEEE 802.1X and prompt user for a certificate while
		 * the correct (non-WPS) behavior would be to ask for the
		 * static WEP key. As a workaround, use Microsoft Provisioning
		 * IE to advertise that legacy 802.1X is not supported.
		 */
		const u8 ms_wps[7] = {
			WLAN_EID_VENDOR_SPECIFIC, 5,
			/* Microsoft Provisioning IE (00:50:f2:5) */
			0x00, 0x50, 0xf2, 5,
			0x00 /* no legacy 802.1X or MS WPS */
		};
		da16x_wps_prt("[%s] WPS: Add Microsoft Provisioning IE "
			   "into Beacon/Probe Response frames\n", __func__);
		wpabuf_put_data(beacon, ms_wps, sizeof(ms_wps));
		wpabuf_put_data(probe, ms_wps, sizeof(ms_wps));
	}

	return wps_cb_set_ie(reg, beacon, probe);
}


static int wps_get_dev_password(struct wps_data *wps)
{
	const u8 *pin;
	size_t pin_len = 0;

	os_free(wps->dev_password);
	wps->dev_password = NULL;

	if (wps->pbc) {
		da16x_wps_prt("[%s] WPS: Use default PIN for PBC\n", __func__);
		pin = (const u8 *) "00000000";
		pin_len = 8;
	} else {
		pin = wps_registrar_get_pin(wps->wps->registrar, wps->uuid_e,
					    &pin_len);
		if (pin && wps->dev_pw_id >= 0x10) {
			da16x_wps_prt("[%s] WPS: No match for OOB Device "
				   "Password ID, but PIN found\n", __func__);
			/*
			 * See whether Enrollee is willing to use PIN instead.
			 */
			wps->dev_pw_id = DEV_PW_DEFAULT;
		}
	}
	if (pin == NULL) {
		da16x_wps_prt("[%s] WPS: No Device Password available for "
			   "the Enrollee (context %p registrar %p)\n\n", __func__,
			   wps->wps, wps->wps->registrar);
		wps_cb_pin_needed(wps->wps->registrar, wps->uuid_e,
				  &wps->peer_dev);
		return -1;
	}

	wps->dev_password = os_memdup(pin, pin_len);
	if (wps->dev_password == NULL)
		return -1;
	wps->dev_password_len = pin_len;

	return 0;
}


static int wps_build_uuid_r(struct wps_data *wps, struct wpabuf *msg)
{
	da16x_wps_prt("[%s] WPS:  * UUID-R\n", __func__);
	wpabuf_put_be16(msg, ATTR_UUID_R);
	wpabuf_put_be16(msg, WPS_UUID_LEN);
	wpabuf_put_data(msg, wps->uuid_r, WPS_UUID_LEN);
	return 0;
}


static int wps_build_r_hash(struct wps_data *wps, struct wpabuf *msg)
{
	u8 *hash;
	const u8 *addr[4];
	size_t len[4];

	if (random_get_bytes(wps->snonce, 2 * WPS_SECRET_NONCE_LEN) < 0)
		return -1;
	
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: R-S1", wps->snonce, WPS_SECRET_NONCE_LEN);
	da16x_dump("WPS: R-S2",
		    wps->snonce + WPS_SECRET_NONCE_LEN, WPS_SECRET_NONCE_LEN);
#endif

	if (wps->dh_pubkey_e == NULL || wps->dh_pubkey_r == NULL) {
		da16x_wps_prt("[%s] WPS: DH public keys not available for "
			   "R-Hash derivation\n", __func__);
		return -1;
	}

	da16x_wps_prt("[%s] WPS:  * R-Hash1\n", __func__);
	wpabuf_put_be16(msg, ATTR_R_HASH1);
	wpabuf_put_be16(msg, SHA256_MAC_LEN);
	hash = wpabuf_put(msg, SHA256_MAC_LEN);
	/* R-Hash1 = HMAC_AuthKey(R-S1 || PSK1 || PK_E || PK_R) */
	addr[0] = wps->snonce;
	len[0] = WPS_SECRET_NONCE_LEN;
	addr[1] = wps->psk1;
	len[1] = WPS_PSK_LEN;
	addr[2] = wpabuf_head(wps->dh_pubkey_e);
	len[2] = wpabuf_len(wps->dh_pubkey_e);
	addr[3] = wpabuf_head(wps->dh_pubkey_r);
	len[3] = wpabuf_len(wps->dh_pubkey_r);
	hmac_sha256_vector(wps->authkey, WPS_AUTHKEY_LEN, 4, addr, len, hash);
	
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: R-Hash1", hash, SHA256_MAC_LEN);
#endif

	da16x_wps_prt("[%s] WPS:  * R-Hash2\n", __func__);
	wpabuf_put_be16(msg, ATTR_R_HASH2);
	wpabuf_put_be16(msg, SHA256_MAC_LEN);
	hash = wpabuf_put(msg, SHA256_MAC_LEN);
	/* R-Hash2 = HMAC_AuthKey(R-S2 || PSK2 || PK_E || PK_R) */
	addr[0] = wps->snonce + WPS_SECRET_NONCE_LEN;
	addr[1] = wps->psk2;
	hmac_sha256_vector(wps->authkey, WPS_AUTHKEY_LEN, 4, addr, len, hash);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: R-Hash2", hash, SHA256_MAC_LEN);
#endif

	return 0;
}


static int wps_build_r_snonce1(struct wps_data *wps, struct wpabuf *msg)
{
	da16x_wps_prt("[%s] WPS:  * R-SNonce1\n", __func__);
	wpabuf_put_be16(msg, ATTR_R_SNONCE1);
	wpabuf_put_be16(msg, WPS_SECRET_NONCE_LEN);
	wpabuf_put_data(msg, wps->snonce, WPS_SECRET_NONCE_LEN);
	return 0;
}


static int wps_build_r_snonce2(struct wps_data *wps, struct wpabuf *msg)
{
	da16x_wps_prt("[%s] WPS:  * R-SNonce2\n", __func__);
	wpabuf_put_be16(msg, ATTR_R_SNONCE2);
	wpabuf_put_be16(msg, WPS_SECRET_NONCE_LEN);
	wpabuf_put_data(msg, wps->snonce + WPS_SECRET_NONCE_LEN,
			WPS_SECRET_NONCE_LEN);
	return 0;
}


static int wps_build_cred_network_idx(struct wpabuf *msg,
				      const struct wps_credential *cred)
{
	da16x_wps_prt("[%s] WPS:  * Network Index (1)\n", __func__);
	wpabuf_put_be16(msg, ATTR_NETWORK_INDEX);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, 1);
	return 0;
}


static int wps_build_cred_ssid(struct wpabuf *msg,
			       const struct wps_credential *cred)
{
	da16x_wps_prt("[%s] WPS:  * SSID\n", __func__);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: SSID for Credential",
			  cred->ssid, cred->ssid_len);
#endif
	wpabuf_put_be16(msg, ATTR_SSID);
	wpabuf_put_be16(msg, cred->ssid_len);
	wpabuf_put_data(msg, cred->ssid, cred->ssid_len);
	return 0;
}


static int wps_build_cred_auth_type(struct wpabuf *msg,
				    const struct wps_credential *cred)
{
	da16x_wps_prt("[%s] WPS:  * Authentication Type (0x%x)\n\n", __func__,
		   cred->auth_type);
	wpabuf_put_be16(msg, ATTR_AUTH_TYPE);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, cred->auth_type);
	return 0;
}


static int wps_build_cred_encr_type(struct wpabuf *msg,
				    const struct wps_credential *cred)
{
	da16x_wps_prt("[%s] WPS:  * Encryption Type (0x%x)\n\n", __func__,
		   cred->encr_type);
	wpabuf_put_be16(msg, ATTR_ENCR_TYPE);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, cred->encr_type);
	return 0;
}


static int wps_build_cred_network_key(struct wpabuf *msg,
				      const struct wps_credential *cred)
{
	da16x_wps_prt("[%s] WPS:  * Network Key (len=%d)\n\n", __func__,
		   (int) cred->key_len);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Network Key",
			cred->key, cred->key_len);
#endif
	wpabuf_put_be16(msg, ATTR_NETWORK_KEY);
	wpabuf_put_be16(msg, cred->key_len);
	wpabuf_put_data(msg, cred->key, cred->key_len);
	return 0;
}


static int wps_build_credential(struct wpabuf *msg,
				const struct wps_credential *cred)
{
	if (wps_build_cred_network_idx(msg, cred) ||
	    wps_build_cred_ssid(msg, cred) ||
	    wps_build_cred_auth_type(msg, cred) ||
	    wps_build_cred_encr_type(msg, cred) ||
	    wps_build_cred_network_key(msg, cred) ||
	    wps_build_mac_addr(msg, cred->mac_addr))
		return -1;
	return 0;
}


int wps_build_credential_wrap(struct wpabuf *msg,
			      const struct wps_credential *cred)
{
	struct wpabuf *wbuf;
	wbuf = wpabuf_alloc(200);
	if (wbuf == NULL)
		return -1;
	if (wps_build_credential(wbuf, cred)) {
		wpabuf_free(wbuf);
		return -1;
	}
	wpabuf_put_be16(msg, ATTR_CRED);
	wpabuf_put_be16(msg, wpabuf_len(wbuf));
	wpabuf_put_buf(msg, wbuf);
	wpabuf_free(wbuf);
	return 0;
}


int wps_build_cred(struct wps_data *wps, struct wpabuf *msg)
{
	struct wpabuf *cred;

	if (wps->wps->registrar->skip_cred_build)
		goto skip_cred_build;

	da16x_wps_prt("[%s] WPS:  * Credential\n", __func__);
	if (wps->use_cred) {
		os_memcpy(&wps->cred, wps->use_cred, sizeof(wps->cred));
		goto use_provided;
	}
	os_memset(&wps->cred, 0, sizeof(wps->cred));

	os_memcpy(wps->cred.ssid, wps->wps->ssid, wps->wps->ssid_len);
	wps->cred.ssid_len = wps->wps->ssid_len;

	/* Select the best authentication and encryption type */
	if (wps->auth_type & WPS_AUTH_WPA2PSK)
		wps->auth_type = WPS_AUTH_WPA2PSK;
	else if (wps->auth_type & WPS_AUTH_WPAPSK)
		wps->auth_type = WPS_AUTH_WPAPSK;
	else if (wps->auth_type & WPS_AUTH_OPEN)
		wps->auth_type = WPS_AUTH_OPEN;
	else {
		da16x_wps_prt("[%s] WPS: Unsupported auth_type 0x%x\n\n", __func__,
			   wps->auth_type);
		return -1;
	}
	wps->cred.auth_type = wps->auth_type;

	if (wps->auth_type == WPS_AUTH_WPA2PSK ||
	    wps->auth_type == WPS_AUTH_WPAPSK) {
		if (wps->encr_type & WPS_ENCR_AES)
			wps->encr_type = WPS_ENCR_AES;
		else if (wps->encr_type & WPS_ENCR_TKIP)
			wps->encr_type = WPS_ENCR_TKIP;
		else {
			da16x_wps_prt("[%s] WPS: No suitable encryption "
				   "type for WPA/WPA2\n", __func__);
			return -1;
		}
	} else {
		if (wps->encr_type & WPS_ENCR_NONE)
			wps->encr_type = WPS_ENCR_NONE;
#ifdef CONFIG_TESTING_OPTIONS
		else if (wps->encr_type & WPS_ENCR_WEP)
			wps->encr_type = WPS_ENCR_WEP;
#endif /* CONFIG_TESTING_OPTIONS */
		else {
			da16x_wps_prt("[%s] WPS: No suitable encryption "
				   "type for non-WPA/WPA2 mode\n", __func__);
			return -1;
		}
	}
	wps->cred.encr_type = wps->encr_type;
	/*
	 * Set MAC address in the Credential to be the Enrollee's MAC address
	 */
	os_memcpy(wps->cred.mac_addr, wps->mac_addr_e, ETH_ALEN);

	if (wps->wps->wps_state == WPS_STATE_NOT_CONFIGURED && wps->wps->ap &&
	    !wps->wps->registrar->disable_auto_conf) {
		u8 r[16];
		/* Generate a random passphrase */
		if (random_get_bytes(r, sizeof(r)) < 0)
			return -1;
		os_free(wps->new_psk);
		wps->new_psk = base64_encode(r, sizeof(r), &wps->new_psk_len);
		if (wps->new_psk == NULL)
			return -1;
		wps->new_psk_len--; /* remove newline */
		while (wps->new_psk_len &&
		       wps->new_psk[wps->new_psk_len - 1] == '=')
			wps->new_psk_len--;
		da16x_ascii_dump("WPS: Generated passphrase",
				      wps->new_psk, wps->new_psk_len);
		os_memcpy(wps->cred.key, wps->new_psk, wps->new_psk_len);
		wps->cred.key_len = wps->new_psk_len;
	} else if (!wps->wps->registrar->force_per_enrollee_psk &&
		   wps->use_psk_key && wps->wps->psk_set) {
		char hex[65];
		da16x_wps_prt("[%s] WPS: Use PSK format for Network Key\n", __func__);
		wpa_snprintf_hex(hex, sizeof(hex), wps->wps->psk, 32);
		os_memcpy(wps->cred.key, hex, 32 * 2);
		wps->cred.key_len = 32 * 2;
	} else if (!wps->wps->registrar->force_per_enrollee_psk &&
		   wps->wps->network_key) {
		os_memcpy(wps->cred.key, wps->wps->network_key,
			  wps->wps->network_key_len);
		wps->cred.key_len = wps->wps->network_key_len;
	} else if (wps->auth_type & (WPS_AUTH_WPAPSK | WPS_AUTH_WPA2PSK)) {
		char hex[65];
		/* Generate a random per-device PSK */
		os_free(wps->new_psk);
		wps->new_psk_len = 32;
		wps->new_psk = os_malloc(wps->new_psk_len);
		if (wps->new_psk == NULL)
			return -1;
		if (random_get_bytes(wps->new_psk, wps->new_psk_len) < 0) {
			os_free(wps->new_psk);
			wps->new_psk = NULL;
			return -1;
		}
		da16x_dump("WPS: Generated per-device PSK",
				wps->new_psk, wps->new_psk_len);
		wpa_snprintf_hex(hex, sizeof(hex), wps->new_psk,
				 wps->new_psk_len);
		os_memcpy(wps->cred.key, hex, wps->new_psk_len * 2);
		wps->cred.key_len = wps->new_psk_len * 2;
	}

use_provided:
#ifdef CONFIG_WPS_TESTING
	if (wps_testing_dummy_cred)
		cred = wpabuf_alloc(200);
	else
		cred = NULL;
	if (cred) {
		struct wps_credential dummy;
		da16x_wps_prt("[%s] WPS: Add dummy credential\n", __func__);
		os_memset(&dummy, 0, sizeof(dummy));
		os_memcpy(dummy.ssid, "dummy", 5);
		dummy.ssid_len = 5;
		dummy.auth_type = WPS_AUTH_WPA2PSK;
		dummy.encr_type = WPS_ENCR_AES;
		os_memcpy(dummy.key, "dummy psk", 9);
		dummy.key_len = 9;
		os_memcpy(dummy.mac_addr, wps->mac_addr_e, ETH_ALEN);
		wps_build_credential(cred, &dummy);
#ifdef ENABLE_WPS_DBG
		da16x_dump("WPS: Dummy Credential", cred);
#endif
		wpabuf_put_be16(msg, ATTR_CRED);
		wpabuf_put_be16(msg, wpabuf_len(cred));
		wpabuf_put_buf(msg, cred);

		wpabuf_free(cred);
	}
#endif /* CONFIG_WPS_TESTING */

	cred = wpabuf_alloc(200);
	if (cred == NULL)
		return -1;

	if (wps_build_credential(cred, &wps->cred)) {
		wpabuf_free(cred);
		return -1;
	}

	wpabuf_put_be16(msg, ATTR_CRED);
	wpabuf_put_be16(msg, wpabuf_len(cred));
	wpabuf_put_buf(msg, cred);
	wpabuf_free(cred);

skip_cred_build:
	if (wps->wps->registrar->extra_cred) {
		da16x_wps_prt("[%s] WPS:  * Credential (pre-configured)\n", __func__);
		wpabuf_put_buf(msg, wps->wps->registrar->extra_cred);
	}

	return 0;
}


static int wps_build_ap_settings(struct wps_data *wps, struct wpabuf *msg)
{
	da16x_wps_prt("[%s] WPS:  * AP Settings\n", __func__);

	if (wps_build_credential(msg, &wps->cred))
		return -1;

	return 0;
}


static struct wpabuf * wps_build_ap_cred(struct wps_data *wps)
{
	struct wpabuf *msg, *plain;

	msg = wpabuf_alloc(1000);
	if (msg == NULL)
		return NULL;

	plain = wpabuf_alloc(200);
	if (plain == NULL) {
		wpabuf_free(msg);
		return NULL;
	}

	if (wps_build_ap_settings(wps, plain)) {
		wpabuf_free(plain);
		wpabuf_free(msg);
		return NULL;
	}

	wpabuf_put_be16(msg, ATTR_CRED);
	wpabuf_put_be16(msg, wpabuf_len(plain));
	wpabuf_put_buf(msg, plain);
	wpabuf_free(plain);

	return msg;
}


static struct wpabuf * wps_build_m2(struct wps_data *wps)
{
	struct wpabuf *msg;
	int config_in_m2 = 0;

	if (random_get_bytes(wps->nonce_r, WPS_NONCE_LEN) < 0)
		return NULL;
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Registrar Nonce",
		    wps->nonce_r, WPS_NONCE_LEN);
	da16x_dump("WPS: UUID-R", wps->uuid_r, WPS_UUID_LEN);
#endif

	da16x_wps_prt("[%s] WPS: Building Message M2\n", __func__);
	msg = wpabuf_alloc(1000);
	if (msg == NULL)
		return NULL;

	if (wps_build_version(msg) ||
	    wps_build_msg_type(msg, WPS_M2) ||
	    wps_build_enrollee_nonce(wps, msg) ||
	    wps_build_registrar_nonce(wps, msg) ||
	    wps_build_uuid_r(wps, msg) ||
	    wps_build_public_key(wps, msg) ||
	    wps_derive_keys(wps) ||
	    wps_build_auth_type_flags(wps, msg) ||
	    wps_build_encr_type_flags(wps, msg) ||
	    wps_build_conn_type_flags(wps, msg) ||
	    wps_build_config_methods_r(wps->wps->registrar, msg) ||
	    wps_build_device_attrs(&wps->wps->dev, msg) ||
	    wps_build_rf_bands(&wps->wps->dev, msg,
			       wps->wps->rf_band_cb(wps->wps->cb_ctx)) ||
	    wps_build_assoc_state(wps, msg) ||
	    wps_build_config_error(msg, WPS_CFG_NO_ERROR) ||
	    wps_build_dev_password_id(msg, wps->dev_pw_id) ||
	    wps_build_os_version(&wps->wps->dev, msg) ||
	    wps_build_wfa_ext(msg, 0, NULL, 0)) {
		wpabuf_free(msg);
		return NULL;
	}

	if (wps_build_authenticator(wps, msg)) {
		wpabuf_free(msg);
		return NULL;
	}

	wps->int_reg = 1;
	wps->state = config_in_m2 ? RECV_DONE : RECV_M3;
	return msg;
}


static struct wpabuf * wps_build_m2d(struct wps_data *wps)
{
	struct wpabuf *msg;
	u16 err = wps->config_error;

	da16x_wps_prt("[%s] WPS: Building Message M2D\n", __func__);
	msg = wpabuf_alloc(1000);
	if (msg == NULL)
		return NULL;

#ifdef	CONFIG_WPS_AP
	if (wps->wps->ap && wps->wps->ap_setup_locked &&
	    err == WPS_CFG_NO_ERROR)
		err = WPS_CFG_SETUP_LOCKED;
#endif	/* CONFIG_WPS_AP */

	if (wps_build_version(msg) ||
	    wps_build_msg_type(msg, WPS_M2D) ||
	    wps_build_enrollee_nonce(wps, msg) ||
	    wps_build_registrar_nonce(wps, msg) ||
	    wps_build_uuid_r(wps, msg) ||
	    wps_build_auth_type_flags(wps, msg) ||
	    wps_build_encr_type_flags(wps, msg) ||
	    wps_build_conn_type_flags(wps, msg) ||
	    wps_build_config_methods_r(wps->wps->registrar, msg) ||
	    wps_build_device_attrs(&wps->wps->dev, msg) ||
	    wps_build_rf_bands(&wps->wps->dev, msg,
			       wps->wps->rf_band_cb(wps->wps->cb_ctx)) ||
	    wps_build_assoc_state(wps, msg) ||
	    wps_build_config_error(msg, err) ||
	    wps_build_os_version(&wps->wps->dev, msg) ||
	    wps_build_wfa_ext(msg, 0, NULL, 0)) {
		wpabuf_free(msg);
		return NULL;
	}

	wps->state = RECV_M2D_ACK;
	return msg;
}


static struct wpabuf * wps_build_m4(struct wps_data *wps)
{
	struct wpabuf *msg, *plain;

	da16x_wps_prt("[%s] WPS: Building Message M4\n", __func__);

	wps_derive_psk(wps, wps->dev_password, wps->dev_password_len);

	plain = wpabuf_alloc(200);
	if (plain == NULL)
		return NULL;

	msg = wpabuf_alloc(1000);
	if (msg == NULL) {
		wpabuf_free(plain);
		return NULL;
	}

	if (wps_build_version(msg) ||
	    wps_build_msg_type(msg, WPS_M4) ||
	    wps_build_enrollee_nonce(wps, msg) ||
	    wps_build_r_hash(wps, msg) ||
	    wps_build_r_snonce1(wps, plain) ||
	    wps_build_key_wrap_auth(wps, plain) ||
	    wps_build_encr_settings(wps, msg, plain) ||
	    wps_build_wfa_ext(msg, 0, NULL, 0) ||
	    wps_build_authenticator(wps, msg)) {
		wpabuf_free(plain);
		wpabuf_free(msg);
		return NULL;
	}
	wpabuf_free(plain);

	wps->state = RECV_M5;
	return msg;
}


static struct wpabuf * wps_build_m6(struct wps_data *wps)
{
	struct wpabuf *msg, *plain;

	da16x_wps_prt("[%s] WPS: Building Message M6\n", __func__);

	plain = wpabuf_alloc(200);
	if (plain == NULL)
		return NULL;

	msg = wpabuf_alloc(1000);
	if (msg == NULL) {
		wpabuf_free(plain);
		return NULL;
	}

	if (wps_build_version(msg) ||
	    wps_build_msg_type(msg, WPS_M6) ||
	    wps_build_enrollee_nonce(wps, msg) ||
	    wps_build_r_snonce2(wps, plain) ||
	    wps_build_key_wrap_auth(wps, plain) ||
	    wps_build_encr_settings(wps, msg, plain) ||
	    wps_build_wfa_ext(msg, 0, NULL, 0) ||
	    wps_build_authenticator(wps, msg)) {
		wpabuf_free(plain);
		wpabuf_free(msg);
		return NULL;
	}
	wpabuf_free(plain);

	wps->wps_pin_revealed = 1;
	wps->state = RECV_M7;
	return msg;
}


static struct wpabuf * wps_build_m8(struct wps_data *wps)
{
	struct wpabuf *msg, *plain;

	da16x_wps_prt("[%s] WPS: Building Message M8\n", __func__);

	plain = wpabuf_alloc(500);
	if (plain == NULL)
		return NULL;

	msg = wpabuf_alloc(1000);
	if (msg == NULL) {
		wpabuf_free(plain);
		return NULL;
	}

	if (wps_build_version(msg) ||
	    wps_build_msg_type(msg, WPS_M8) ||
	    wps_build_enrollee_nonce(wps, msg) ||
	    ((wps->wps->ap || wps->er) && wps_build_cred(wps, plain)) ||
	    (!wps->wps->ap && !wps->er && wps_build_ap_settings(wps, plain)) ||
	    wps_build_key_wrap_auth(wps, plain) ||
	    wps_build_encr_settings(wps, msg, plain) ||
	    wps_build_wfa_ext(msg, 0, NULL, 0) ||
	    wps_build_authenticator(wps, msg)) {
		wpabuf_free(plain);
		wpabuf_free(msg);
		return NULL;
	}
	wpabuf_free(plain);

	wps->state = RECV_DONE;
	return msg;
}


struct wpabuf * wps_registrar_get_msg(struct wps_data *wps,
				      enum wsc_op_code *op_code)
{
	struct wpabuf *msg;

#ifdef CONFIG_WPS_UPNP
	if (!wps->int_reg && wps->wps->wps_upnp) {
		struct upnp_pending_message *p, *prev = NULL;
		if (wps->ext_reg > 1)
			wps_registrar_free_pending_m2(wps->wps);
		p = wps->wps->upnp_msgs;
		/* TODO: check pending message MAC address */
		while (p && p->next) {
			prev = p;
			p = p->next;
		}
		if (p) {
			da16x_wps_prt("[%s] WPS: Use pending message from "
				   "UPnP\n", __func__);
			if (prev)
				prev->next = NULL;
			else
				wps->wps->upnp_msgs = NULL;
			msg = p->msg;
			switch (p->type) {
			case WPS_WSC_ACK:
				*op_code = WSC_ACK;
				break;
			case WPS_WSC_NACK:
				*op_code = WSC_NACK;
				break;
			default:
				*op_code = WSC_MSG;
				break;
			}
			os_free(p);
			if (wps->ext_reg == 0)
				wps->ext_reg = 1;
			return msg;
		}
	}
	if (wps->ext_reg) {
		da16x_wps_prt("[%s] WPS: Using external Registrar, but no "
			   "pending message available\n", __func__);
		return NULL;
	}
#endif /* CONFIG_WPS_UPNP */

	switch (wps->state) {
	case SEND_M2:
		if (wps_get_dev_password(wps) < 0)
			msg = wps_build_m2d(wps);
		else
			msg = wps_build_m2(wps);
		*op_code = WSC_MSG;
		break;
	case SEND_M2D:
		msg = wps_build_m2d(wps);
		*op_code = WSC_MSG;
		break;
	case SEND_M4:
		msg = wps_build_m4(wps);
		*op_code = WSC_MSG;
		break;
	case SEND_M6:
		msg = wps_build_m6(wps);
		*op_code = WSC_MSG;
		break;
	case SEND_M8:
		msg = wps_build_m8(wps);
		*op_code = WSC_MSG;
		break;
	case RECV_DONE:
		msg = wps_build_wsc_ack(wps);
		*op_code = WSC_ACK;
		break;
	case SEND_WSC_NACK:
		msg = wps_build_wsc_nack(wps);
		*op_code = WSC_NACK;
		break;
	default:
		da16x_wps_prt("[%s] WPS: Unsupported state %d for building "
			   "a message\n\n", __func__, wps->state);
		msg = NULL;
		break;
	}

	if (*op_code == WSC_MSG && msg) {
		/* Save a copy of the last message for Authenticator derivation
		 */
		wpabuf_free(wps->last_msg);
		wps->last_msg = wpabuf_dup(msg);
	}

	return msg;
}


static int wps_process_enrollee_nonce(struct wps_data *wps, const u8 *e_nonce)
{
	if (e_nonce == NULL) {
		da16x_wps_prt("[%s] WPS: No Enrollee Nonce received\n", __func__);
		return -1;
	}

	os_memcpy(wps->nonce_e, e_nonce, WPS_NONCE_LEN);
	
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Enrollee Nonce",
		    wps->nonce_e, WPS_NONCE_LEN);
#endif

	return 0;
}


static int wps_process_registrar_nonce(struct wps_data *wps, const u8 *r_nonce)
{
	if (r_nonce == NULL) {
		da16x_wps_prt("[%s] WPS: No Registrar Nonce received\n", __func__);
		return -1;
	}

	if (os_memcmp(wps->nonce_r, r_nonce, WPS_NONCE_LEN) != 0) {
		da16x_wps_prt("[%s] WPS: Invalid Registrar Nonce received\n", __func__);
		return -1;
	}

	return 0;
}


static int wps_process_uuid_e(struct wps_data *wps, const u8 *uuid_e)
{
	if (uuid_e == NULL) {
		da16x_wps_prt("[%s] WPS: No UUID-E received\n", __func__);
		return -1;
	}

	os_memcpy(wps->uuid_e, uuid_e, WPS_UUID_LEN);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: UUID-E", wps->uuid_e, WPS_UUID_LEN);
#endif

	return 0;
}


static int wps_process_dev_password_id(struct wps_data *wps, const u8 *pw_id)
{
	if (pw_id == NULL) {
		da16x_wps_prt("[%s] WPS: No Device Password ID received\n", __func__);
		return -1;
	}

	wps->dev_pw_id = WPA_GET_BE16(pw_id);
	da16x_wps_prt("[%s] WPS: Device Password ID %d\n\n", __func__, wps->dev_pw_id);

	return 0;
}


static int wps_process_e_hash1(struct wps_data *wps, const u8 *e_hash1)
{
	if (e_hash1 == NULL) {
		da16x_wps_prt("[%s] WPS: No E-Hash1 received\n", __func__);
		return -1;
	}

	os_memcpy(wps->peer_hash1, e_hash1, WPS_HASH_LEN);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: E-Hash1", wps->peer_hash1, WPS_HASH_LEN);
#endif

	return 0;
}


static int wps_process_e_hash2(struct wps_data *wps, const u8 *e_hash2)
{
	if (e_hash2 == NULL) {
		da16x_wps_prt("[%s] WPS: No E-Hash2 received\n", __func__);
		return -1;
	}

	os_memcpy(wps->peer_hash2, e_hash2, WPS_HASH_LEN);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: E-Hash2", wps->peer_hash2, WPS_HASH_LEN);
#endif

	return 0;
}


static int wps_process_e_snonce1(struct wps_data *wps, const u8 *e_snonce1)
{
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[4];
	size_t len[4];

	if (e_snonce1 == NULL) {
		da16x_wps_prt("[%s] WPS: No E-SNonce1 received\n", __func__);
		return -1;
	}

#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: E-SNonce1", e_snonce1,
			WPS_SECRET_NONCE_LEN);
#endif

	/* E-Hash1 = HMAC_AuthKey(E-S1 || PSK1 || PK_E || PK_R) */
	addr[0] = e_snonce1;
	len[0] = WPS_SECRET_NONCE_LEN;
	addr[1] = wps->psk1;
	len[1] = WPS_PSK_LEN;
	addr[2] = wpabuf_head(wps->dh_pubkey_e);
	len[2] = wpabuf_len(wps->dh_pubkey_e);
	addr[3] = wpabuf_head(wps->dh_pubkey_r);
	len[3] = wpabuf_len(wps->dh_pubkey_r);
	hmac_sha256_vector(wps->authkey, WPS_AUTHKEY_LEN, 4, addr, len, hash);

	if (os_memcmp(wps->peer_hash1, hash, WPS_HASH_LEN) != 0) {
		da16x_wps_prt("[%s] WPS: E-Hash1 derived from E-S1 does "
			   "not match with the pre-committed value\n", __func__);
		wps->config_error = WPS_CFG_DEV_PASSWORD_AUTH_FAILURE;
		wps_pwd_auth_fail_event(wps->wps, 0, 1, wps->mac_addr_e);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee proved knowledge of the first "
		   "half of the device password\n", __func__);

	return 0;
}


static int wps_process_e_snonce2(struct wps_data *wps, const u8 *e_snonce2)
{
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[4];
	size_t len[4];

	if (e_snonce2 == NULL) {
		da16x_wps_prt("[%s] WPS: No E-SNonce2 received\n", __func__);
		return -1;
	}

#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: E-SNonce2", e_snonce2,
			WPS_SECRET_NONCE_LEN);
#endif

	/* E-Hash2 = HMAC_AuthKey(E-S2 || PSK2 || PK_E || PK_R) */
	addr[0] = e_snonce2;
	len[0] = WPS_SECRET_NONCE_LEN;
	addr[1] = wps->psk2;
	len[1] = WPS_PSK_LEN;
	addr[2] = wpabuf_head(wps->dh_pubkey_e);
	len[2] = wpabuf_len(wps->dh_pubkey_e);
	addr[3] = wpabuf_head(wps->dh_pubkey_r);
	len[3] = wpabuf_len(wps->dh_pubkey_r);
	hmac_sha256_vector(wps->authkey, WPS_AUTHKEY_LEN, 4, addr, len, hash);

	if (os_memcmp(wps->peer_hash2, hash, WPS_HASH_LEN) != 0) {
		da16x_wps_prt("[%s] WPS: E-Hash2 derived from E-S2 does "
			   "not match with the pre-committed value\n", __func__);
		wps_registrar_invalidate_pin(wps->wps->registrar, wps->uuid_e);
		wps->config_error = WPS_CFG_DEV_PASSWORD_AUTH_FAILURE;
		wps_pwd_auth_fail_event(wps->wps, 0, 2, wps->mac_addr_e);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee proved knowledge of the second "
		   "half of the device password\n", __func__);
	wps->wps_pin_revealed = 0;
	wps_registrar_unlock_pin(wps->wps->registrar, wps->uuid_e);

	/*
	 * In case wildcard PIN is used and WPS handshake succeeds in the first
	 * attempt, wps_registrar_unlock_pin() would not free the PIN, so make
	 * sure the PIN gets invalidated here.
	 */
	wps_registrar_invalidate_pin(wps->wps->registrar, wps->uuid_e);

	return 0;
}


static int wps_process_mac_addr(struct wps_data *wps, const u8 *mac_addr)
{
	if (mac_addr == NULL) {
		da16x_wps_prt("[%s] WPS: No MAC Address received\n", __func__);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee MAC Address " MACSTR "\n", __func__,
		   MAC2STR(mac_addr));
	os_memcpy(wps->mac_addr_e, mac_addr, ETH_ALEN);
	os_memcpy(wps->peer_dev.mac_addr, mac_addr, ETH_ALEN);

	return 0;
}


static int wps_process_pubkey(struct wps_data *wps, const u8 *pk,
			      size_t pk_len)
{
	if (pk == NULL || pk_len == 0) {
		da16x_wps_prt("[%s] WPS: No Public Key received\n", __func__);
		return -1;
	}

	wpabuf_free(wps->dh_pubkey_e);
	wps->dh_pubkey_e = wpabuf_alloc_copy(pk, pk_len);
	if (wps->dh_pubkey_e == NULL)
		return -1;

	return 0;
}


static int wps_process_auth_type_flags(struct wps_data *wps, const u8 *auth)
{
	u16 auth_types;

	if (auth == NULL) {
		da16x_wps_prt("[%s] WPS: No Authentication Type flags "
			   "received\n", __func__);
		return -1;
	}

	auth_types = WPA_GET_BE16(auth);

	da16x_wps_prt("[%s] WPS: Enrollee Authentication Type flags 0x%x\n\n", __func__,
		   auth_types);
	wps->auth_type = wps->wps->auth_types & auth_types;
	if (wps->auth_type == 0) {
		da16x_wps_prt("[%s] WPS: No match in supported "
			   "authentication types (own 0x%x Enrollee 0x%x)\n\n", __func__,
			   wps->wps->auth_types, auth_types);
#ifdef WPS_WORKAROUNDS
		/*
		 * Some deployed implementations seem to advertise incorrect
		 * information in this attribute. For example, Linksys WRT350N
		 * seems to have a byteorder bug that breaks this negotiation.
		 * In order to interoperate with existing implementations,
		 * assume that the Enrollee supports everything we do.
		 */
		da16x_wps_prt("[%s] WPS: Workaround - assume Enrollee "
			   "does not advertise supported authentication types "
			   "correctly\n", __func__);
		wps->auth_type = wps->wps->auth_types;
#else /* WPS_WORKAROUNDS */
		return -1;
#endif /* WPS_WORKAROUNDS */
	}

	return 0;
}


static int wps_process_encr_type_flags(struct wps_data *wps, const u8 *encr)
{
	u16 encr_types;

	if (encr == NULL) {
		da16x_wps_prt("[%s] WPS: No Encryption Type flags "
			   "received\n", __func__);
		return -1;
	}

	encr_types = WPA_GET_BE16(encr);

	da16x_wps_prt("[%s] WPS: Enrollee Encryption Type flags 0x%x\n\n", __func__,
		   encr_types);
	wps->encr_type = wps->wps->encr_types & encr_types;
	if (wps->encr_type == 0) {
		da16x_wps_prt("[%s] WPS: No match in supported "
			   "encryption types (own 0x%x Enrollee 0x%x)\n\n", __func__,
			   wps->wps->encr_types, encr_types);
#ifdef WPS_WORKAROUNDS
		/*
		 * Some deployed implementations seem to advertise incorrect
		 * information in this attribute. For example, Linksys WRT350N
		 * seems to have a byteorder bug that breaks this negotiation.
		 * In order to interoperate with existing implementations,
		 * assume that the Enrollee supports everything we do.
		 */
		da16x_wps_prt("[%s] WPS: Workaround - assume Enrollee "
			   "does not advertise supported encryption types "
			   "correctly\n", __func__);
		wps->encr_type = wps->wps->encr_types;
#else /* WPS_WORKAROUNDS */
		return -1;
#endif /* WPS_WORKAROUNDS */
	}

	return 0;
}


static int wps_process_conn_type_flags(struct wps_data *wps, const u8 *conn)
{
	if (conn == NULL) {
		da16x_wps_prt("[%s] WPS: No Connection Type flags "
			   "received\n", __func__);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee Connection Type flags 0x%x\n\n", __func__,
		   *conn);

	return 0;
}


static int wps_process_config_methods(struct wps_data *wps, const u8 *methods)
{
	u16 m;

	if (methods == NULL) {
		da16x_wps_prt("[%s] WPS: No Config Methods received\n", __func__);
		return -1;
	}

	m = WPA_GET_BE16(methods);

	da16x_wps_prt("[%s] WPS: Enrollee Config Methods 0x%x"
#if 0
		   "%s%s%s%s%s%s%s%s%s\n\n", __func__, m,
#else
		   "%s%s%s%s%s%s\n\n", __func__, m,
#endif	/* 0 */
		   m & WPS_CONFIG_USBA ? " [USBA]" : "",
		   m & WPS_CONFIG_ETHERNET ? " [Ethernet]" : "",
		   m & WPS_CONFIG_LABEL ? " [Label]" : "",
		   m & WPS_CONFIG_DISPLAY ? " [Display]" : "",
#if 0
		   m & WPS_CONFIG_EXT_NFC_TOKEN ? " [Ext NFC Token]" : "",
		   m & WPS_CONFIG_INT_NFC_TOKEN ? " [Int NFC Token]" : "",
		   m & WPS_CONFIG_NFC_INTERFACE ? " [NFC]" : "",
#endif	/* 0 */
		   m & WPS_CONFIG_PUSHBUTTON ? " [PBC]" : "",
		   m & WPS_CONFIG_KEYPAD ? " [Keypad]" : "");

	if (!(m & WPS_CONFIG_DISPLAY) && !wps->use_psk_key) {
		/*
		 * The Enrollee does not have a display so it is unlikely to be
		 * able to show the passphrase to a user and as such, could
		 * benefit from receiving PSK to reduce key derivation time.
		 */
		da16x_wps_prt("[%s] WPS: Prefer PSK format key due to "
			   "Enrollee not supporting display\n", __func__);
		wps->use_psk_key = 1;
	}

	return 0;
}


static int wps_process_wps_state(struct wps_data *wps, const u8 *state)
{
	if (state == NULL) {
		da16x_wps_prt("[%s] WPS: No Wi-Fi Protected Setup State received\n", __func__);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee Wi-Fi Protected Setup State %d\n\n", __func__,
		   *state);

	return 0;
}


static int wps_process_assoc_state(struct wps_data *wps, const u8 *assoc)
{
	if (assoc == NULL) {
		da16x_wps_prt("[%s] WPS: No Association State received\n", __func__);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee Association State %d\n\n",
				__func__, WPA_GET_BE16(assoc));

	return 0;
}


static int wps_process_config_error(struct wps_data *wps, const u8 *err)
{
	if (err == NULL) {
		da16x_wps_prt("[%s] WPS: No Configuration Error received\n", __func__);
		return -1;
	}

	da16x_wps_prt("[%s] WPS: Enrollee Configuration Error %d\n\n",
				__func__, WPA_GET_BE16(err));

	return 0;
}


#ifdef CONFIG_P2P
static int wps_registrar_p2p_dev_addr_match(struct wps_data *wps)
{
	struct wps_registrar *reg = wps->wps->registrar;

	if (is_zero_ether_addr(reg->p2p_dev_addr))
		return 1; /* no filtering in use */

	if (os_memcmp(reg->p2p_dev_addr, wps->p2p_dev_addr, ETH_ALEN) != 0) {
		da16x_wps_prt("[%s] WPS: No match on P2P Device Address "
			   "filtering for PBC: expected " MACSTR " was "
			   MACSTR " - indicate PBC session overlap\n\n", __func__,
			   MAC2STR(reg->p2p_dev_addr),
			   MAC2STR(wps->p2p_dev_addr));
		return 0;
	}
	return 1;
}
#endif /* CONFIG_P2P */

static int wps_registrar_skip_overlap(struct wps_data *wps)
{
#ifdef CONFIG_P2P
	struct wps_registrar *reg = wps->wps->registrar;

	if (is_zero_ether_addr(reg->p2p_dev_addr))
		return 0; /* no specific Enrollee selected */

	if (os_memcmp(reg->p2p_dev_addr, wps->p2p_dev_addr, ETH_ALEN) == 0) {
		da16x_wps_prt("[%s] WPS: Skip PBC overlap due to selected "
			   "Enrollee match\n", __func__);
		return 1;
	}
#endif /* CONFIG_P2P */
	return 0;
}


static enum wps_process_res wps_process_m1(struct wps_data *wps,
					   struct wps_parse_attr *attr)
{
	da16x_wps_prt("[%s] WPS: Received M1\n", __func__);

	if (wps->state != RECV_M1) {
		da16x_wps_prt("[%s] WPS: Unexpected state (%d) for "
			   "receiving M1\n\n", __func__, wps->state);
		return WPS_FAILURE;
	}

	if (wps_process_uuid_e(wps, attr->uuid_e) ||
	    wps_process_mac_addr(wps, attr->mac_addr) ||
	    wps_process_enrollee_nonce(wps, attr->enrollee_nonce) ||
	    wps_process_pubkey(wps, attr->public_key, attr->public_key_len) ||
	    wps_process_auth_type_flags(wps, attr->auth_type_flags) ||
	    wps_process_encr_type_flags(wps, attr->encr_type_flags) ||
	    wps_process_conn_type_flags(wps, attr->conn_type_flags) ||
	    wps_process_config_methods(wps, attr->config_methods) ||
	    wps_process_wps_state(wps, attr->wps_state) ||
	    wps_process_device_attrs(&wps->peer_dev, attr) ||
	    wps_process_rf_bands(&wps->peer_dev, attr->rf_bands) ||
	    wps_process_assoc_state(wps, attr->assoc_state) ||
	    wps_process_dev_password_id(wps, attr->dev_password_id) ||
	    wps_process_config_error(wps, attr->config_error) ||
	    wps_process_os_version(&wps->peer_dev, attr->os_version))
		return WPS_FAILURE;

	if (wps->dev_pw_id < 0x10 &&
	    wps->dev_pw_id != DEV_PW_DEFAULT &&
	    wps->dev_pw_id != DEV_PW_USER_SPECIFIED &&
	    wps->dev_pw_id != DEV_PW_MACHINE_SPECIFIED &&
	    wps->dev_pw_id != DEV_PW_REGISTRAR_SPECIFIED &&
	    (wps->dev_pw_id != DEV_PW_PUSHBUTTON ||
	     !wps->wps->registrar->pbc)) {
		da16x_wps_prt("[%s] WPS: Unsupported Device Password ID %d\n\n", __func__,
			   wps->dev_pw_id);
		wps->state = SEND_M2D;
		return WPS_CONTINUE;
	}

	if (wps->dev_pw_id == DEV_PW_PUSHBUTTON) {
		if ((   wps->wps->registrar->force_pbc_overlap
			 || wps_registrar_pbc_overlap(wps->wps->registrar, wps->mac_addr_e, wps->uuid_e)
#ifdef CONFIG_P2P
			 || !wps_registrar_p2p_dev_addr_match(wps)
#endif /* CONFIG_P2P */
			)
			&& !wps_registrar_skip_overlap(wps))
		{
			da16x_wps_prt("[%s] WPS: PBC overlap - deny PBC "
				   "negotiation\n", __func__);
			wps->state = SEND_M2D;
			wps->config_error = WPS_CFG_MULTIPLE_PBC_DETECTED;
			wps_pbc_overlap_event(wps->wps);
			wps_fail_event(wps->wps, WPS_M1,
				       WPS_CFG_MULTIPLE_PBC_DETECTED,
				       WPS_EI_NO_ERROR, wps->mac_addr_e);
			wps->wps->registrar->force_pbc_overlap = 1;
			return WPS_CONTINUE;
		}
		wps_registrar_add_pbc_session(wps->wps->registrar,
					      wps->mac_addr_e, wps->uuid_e);
		wps->pbc = 1;
	}

#ifdef WPS_WORKAROUNDS
	/*
	 * It looks like Mac OS X 10.6.3 and 10.6.4 do not like Network Key in
	 * passphrase format. To avoid interop issues, force PSK format to be
	 * used.
	 */
	if (!wps->use_psk_key &&
	    wps->peer_dev.manufacturer &&
	    os_strncmp(wps->peer_dev.manufacturer, "Apple ", 6) == 0 &&
	    wps->peer_dev.model_name &&
	    os_strcmp(wps->peer_dev.model_name, "AirPort") == 0) {
		da16x_wps_prt("[%s] WPS: Workaround - Force Network Key in "
			   "PSK format\n", __func__);
		wps->use_psk_key = 1;
	}
#endif /* WPS_WORKAROUNDS */

	wps->state = SEND_M2;
	return WPS_CONTINUE;
}


static enum wps_process_res wps_process_m3(struct wps_data *wps,
					   const struct wpabuf *msg,
					   struct wps_parse_attr *attr)
{
	da16x_wps_prt("[%s] WPS: Received M3\n", __func__);

	if (wps->state != RECV_M3) {
		da16x_wps_prt("[%s] WPS: Unexpected state (%d) for "
			   "receiving M3\n\n", __func__, wps->state);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	if (wps->pbc && wps->wps->registrar->force_pbc_overlap &&
	    !wps_registrar_skip_overlap(wps)) {
		da16x_wps_prt("[%s] WPS: Reject negotiation due to PBC "
			   "session overlap\n", __func__);
		wps->state = SEND_WSC_NACK;
		wps->config_error = WPS_CFG_MULTIPLE_PBC_DETECTED;
		return WPS_CONTINUE;
	}

	if (wps_process_registrar_nonce(wps, attr->registrar_nonce) ||
	    wps_process_authenticator(wps, attr->authenticator, msg) ||
	    wps_process_e_hash1(wps, attr->e_hash1) ||
	    wps_process_e_hash2(wps, attr->e_hash2)) {
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	wps->state = SEND_M4;
	return WPS_CONTINUE;
}


static enum wps_process_res wps_process_m5(struct wps_data *wps,
					   const struct wpabuf *msg,
					   struct wps_parse_attr *attr)
{
	struct wpabuf *decrypted;
	struct wps_parse_attr eattr;

	da16x_wps_prt("[%s] WPS: Received M5\n", __func__);

	if (wps->state != RECV_M5) {
		da16x_wps_prt("[%s] WPS: Unexpected state (%d) for receiving M5\n\n", __func__, wps->state);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	if (wps->pbc && wps->wps->registrar->force_pbc_overlap &&
	    !wps_registrar_skip_overlap(wps)) {
		da16x_wps_prt("[%s] WPS: Reject negotiation due to PBC "
			   "session overlap\n", __func__);
		wps->state = SEND_WSC_NACK;
		wps->config_error = WPS_CFG_MULTIPLE_PBC_DETECTED;
		return WPS_CONTINUE;
	}

	if (wps_process_registrar_nonce(wps, attr->registrar_nonce) ||
	    wps_process_authenticator(wps, attr->authenticator, msg)) {
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	decrypted = wps_decrypt_encr_settings(wps, attr->encr_settings,
					      attr->encr_settings_len);
	if (decrypted == NULL) {
		da16x_wps_prt("[%s] WPS: Failed to decrypted Encrypted "
			   "Settings attribute\n", __func__);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	if (wps_validate_m5_encr(decrypted, attr->version2 != NULL) < 0) {
		wpabuf_free(decrypted);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	da16x_wps_prt("[%s] WPS: Processing decrypted Encrypted Settings "
		   "attribute\n", __func__);
	if (wps_parse_msg(decrypted, &eattr) < 0 ||
	    wps_process_key_wrap_auth(wps, decrypted, eattr.key_wrap_auth) ||
	    wps_process_e_snonce1(wps, eattr.e_snonce1)) {
		wpabuf_free(decrypted);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}
	wpabuf_free(decrypted);

	wps->state = SEND_M6;
	return WPS_CONTINUE;
}


static void wps_sta_cred_cb(struct wps_data *wps)
{
	/*
	 * Update credential to only include a single authentication and
	 * encryption type in case the AP configuration includes more than one
	 * option.
	 */
	if (wps->cred.auth_type & WPS_AUTH_WPA2PSK)
		wps->cred.auth_type = WPS_AUTH_WPA2PSK;
	else if (wps->cred.auth_type & WPS_AUTH_WPAPSK)
		wps->cred.auth_type = WPS_AUTH_WPAPSK;
	if (wps->cred.encr_type & WPS_ENCR_AES)
		wps->cred.encr_type = WPS_ENCR_AES;
	else if (wps->cred.encr_type & WPS_ENCR_TKIP)
		wps->cred.encr_type = WPS_ENCR_TKIP;
	da16x_wps_prt("[%s] WPS: Update local configuration based on the "
		   "AP configuration\n", __func__);
	if (wps->wps->cred_cb)
		wps->wps->cred_cb(wps->wps->cb_ctx, &wps->cred);
}


static void wps_cred_update(struct wps_credential *dst,
			    struct wps_credential *src)
{
	os_memcpy(dst->ssid, src->ssid, sizeof(dst->ssid));
	dst->ssid_len = src->ssid_len;
	dst->auth_type = src->auth_type;
	dst->encr_type = src->encr_type;
	dst->key_idx = src->key_idx;
	os_memcpy(dst->key, src->key, sizeof(dst->key));
	dst->key_len = src->key_len;
}


static int wps_process_ap_settings_r(struct wps_data *wps,
				     struct wps_parse_attr *attr)
{
	struct wpabuf *msg;

	if (wps->wps->ap || wps->er)
		return 0;

	/* AP Settings Attributes in M7 when Enrollee is an AP */
	if (wps_process_ap_settings(attr, &wps->cred) < 0)
		return -1;

	da16x_wps_prt("[%s] WPS: Received old AP configuration from AP\n", __func__);

	if (wps->new_ap_settings) {
		da16x_wps_prt("[%s] WPS: Update AP configuration based on "
			   "new settings\n", __func__);
		wps_cred_update(&wps->cred, wps->new_ap_settings);
		return 0;
	} else {
		/*
		 * Use the AP PIN only to receive the current AP settings, not
		 * to reconfigure the AP.
		 */

		/*
		 * Clear selected registrar here since we do not get to
		 * WSC_Done in this protocol run.
		 */
		wps_registrar_pin_completed(wps->wps->registrar);

		msg = wps_build_ap_cred(wps);
		if (msg == NULL)
			return -1;
		wps->cred.cred_attr = wpabuf_head(msg);
		wps->cred.cred_attr_len = wpabuf_len(msg);

		if (wps->ap_settings_cb) {
			wps->ap_settings_cb(wps->ap_settings_cb_ctx,
					    &wps->cred);
			wpabuf_free(msg);
			return 1;
		}
		wps_sta_cred_cb(wps);

		wps->cred.cred_attr = NULL;
		wps->cred.cred_attr_len = 0;
		wpabuf_free(msg);

		return 1;
	}
}


static enum wps_process_res wps_process_m7(struct wps_data *wps,
					   const struct wpabuf *msg,
					   struct wps_parse_attr *attr)
{
	struct wpabuf *decrypted;
	struct wps_parse_attr eattr;

	da16x_wps_prt("[%s] WPS: Received M7\n", __func__);

	if (wps->state != RECV_M7) {
		da16x_wps_prt("[%s] WPS: Unexpected state (%d) for "
			   "receiving M7\n\n", __func__, wps->state);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	if (wps->pbc && wps->wps->registrar->force_pbc_overlap &&
	    !wps_registrar_skip_overlap(wps)) {
		da16x_wps_prt("[%s] WPS: Reject negotiation due to PBC "
			   "session overlap\n", __func__);
		wps->state = SEND_WSC_NACK;
		wps->config_error = WPS_CFG_MULTIPLE_PBC_DETECTED;
		return WPS_CONTINUE;
	}

	if (wps_process_registrar_nonce(wps, attr->registrar_nonce) ||
	    wps_process_authenticator(wps, attr->authenticator, msg)) {
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	decrypted = wps_decrypt_encr_settings(wps, attr->encr_settings,
					      attr->encr_settings_len);
	if (decrypted == NULL) {
		da16x_wps_prt("[%s] WPS: Failed to decrypt Encrypted "
			   "Settings attribute\n", __func__);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	if (wps_validate_m7_encr(decrypted, wps->wps->ap || wps->er,
				 attr->version2 != NULL) < 0) {
		wpabuf_free(decrypted);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	da16x_wps_prt("[%s] WPS: Processing decrypted Encrypted Settings "
		   "attribute\n", __func__);
	if (wps_parse_msg(decrypted, &eattr) < 0 ||
	    wps_process_key_wrap_auth(wps, decrypted, eattr.key_wrap_auth) ||
	    wps_process_e_snonce2(wps, eattr.e_snonce2) ||
	    wps_process_ap_settings_r(wps, &eattr)) {
		wpabuf_free(decrypted);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	wpabuf_free(decrypted);

	wps->state = SEND_M8;
	return WPS_CONTINUE;
}


static enum wps_process_res wps_process_wsc_msg(struct wps_data *wps,
						const struct wpabuf *msg)
{
	struct wps_parse_attr attr;
	enum wps_process_res ret = WPS_CONTINUE;

	da16x_wps_prt("[%s] WPS: Received WSC_MSG\n", __func__);

	if (wps_parse_msg(msg, &attr) < 0)
		return WPS_FAILURE;

	if (attr.msg_type == NULL) {
		da16x_wps_prt("[%s] WPS: No Message Type attribute\n", __func__);
		wps->state = SEND_WSC_NACK;
		return WPS_CONTINUE;
	}

	if (*attr.msg_type != WPS_M1 &&
	    (attr.registrar_nonce == NULL ||
	     os_memcmp(wps->nonce_r, attr.registrar_nonce,
		       WPS_NONCE_LEN) != 0)) {
		da16x_wps_prt("[%s] WPS: Mismatch in registrar nonce\n", __func__);
		return WPS_FAILURE;
	}

	switch (*attr.msg_type) {
	case WPS_M1:
		if (wps_validate_m1(msg) < 0)
			return WPS_FAILURE;
#ifdef CONFIG_WPS_UPNP
		if (wps->wps->wps_upnp && attr.mac_addr) {
			/* Remove old pending messages when starting new run */
			wps_free_pending_msgs(wps->wps->upnp_msgs);
			wps->wps->upnp_msgs = NULL;

			upnp_wps_device_send_wlan_event(
				wps->wps->wps_upnp, attr.mac_addr,
				UPNP_WPS_WLANEVENT_TYPE_EAP, msg);
		}
#endif /* CONFIG_WPS_UPNP */
		ret = wps_process_m1(wps, &attr);
		break;
	case WPS_M3:
		if (wps_validate_m3(msg) < 0)
			return WPS_FAILURE;
		ret = wps_process_m3(wps, msg, &attr);
		if (ret == WPS_FAILURE || wps->state == SEND_WSC_NACK)
			wps_fail_event(wps->wps, WPS_M3, wps->config_error,
				       wps->error_indication, wps->mac_addr_e);
		break;
	case WPS_M5:
		if (wps_validate_m5(msg) < 0)
			return WPS_FAILURE;
		ret = wps_process_m5(wps, msg, &attr);
		if (ret == WPS_FAILURE || wps->state == SEND_WSC_NACK)
			wps_fail_event(wps->wps, WPS_M5, wps->config_error,
				       wps->error_indication, wps->mac_addr_e);
		break;
	case WPS_M7:
		if (wps_validate_m7(msg) < 0)
			return WPS_FAILURE;
		ret = wps_process_m7(wps, msg, &attr);
		if (ret == WPS_FAILURE || wps->state == SEND_WSC_NACK)
			wps_fail_event(wps->wps, WPS_M7, wps->config_error,
				       wps->error_indication, wps->mac_addr_e);
		break;
	default:
		da16x_wps_prt("[%s] WPS: Unsupported Message Type %d\n\n", __func__,
			   *attr.msg_type);
		return WPS_FAILURE;
	}

	if (ret == WPS_CONTINUE) {
		/* Save a copy of the last message for Authenticator derivation
		 */
		wpabuf_free(wps->last_msg);
		wps->last_msg = wpabuf_dup(msg);
	}

	return ret;
}


static enum wps_process_res wps_process_wsc_ack(struct wps_data *wps,
						const struct wpabuf *msg)
{
	struct wps_parse_attr attr;

	da16x_wps_prt("[%s] WPS: Received WSC_ACK\n", __func__);

	if (wps_parse_msg(msg, &attr) < 0)
		return WPS_FAILURE;

	if (attr.msg_type == NULL) {
		da16x_wps_prt("[%s] WPS: No Message Type attribute\n", __func__);
		return WPS_FAILURE;
	}

	if (*attr.msg_type != WPS_WSC_ACK) {
		da16x_wps_prt("[%s] WPS: Invalid Message Type %d\n", __func__,
			   *attr.msg_type);
		return WPS_FAILURE;
	}

#ifdef CONFIG_WPS_UPNP
	if (wps->wps->wps_upnp && wps->ext_reg && wps->state == RECV_M2D_ACK &&
	    upnp_wps_subscribers(wps->wps->wps_upnp)) {
		if (wps->wps->upnp_msgs)
			return WPS_CONTINUE;
		da16x_wps_prt("[%s] WPS: Wait for response from an "
			   "external Registrar\n", __func__);
		return WPS_PENDING;
	}
#endif /* CONFIG_WPS_UPNP */

	if (attr.registrar_nonce == NULL ||
	    os_memcmp(wps->nonce_r, attr.registrar_nonce, WPS_NONCE_LEN) != 0)
	{
		da16x_wps_prt("[%s] WPS: Mismatch in registrar nonce\n", __func__);
		return WPS_FAILURE;
	}

	if (attr.enrollee_nonce == NULL ||
	    os_memcmp(wps->nonce_e, attr.enrollee_nonce, WPS_NONCE_LEN) != 0) {
		da16x_wps_prt("[%s] WPS: Mismatch in enrollee nonce\n", __func__);
		return WPS_FAILURE;
	}

	if (wps->state == RECV_M2D_ACK) {
#ifdef CONFIG_WPS_UPNP
		if (wps->wps->wps_upnp &&
		    upnp_wps_subscribers(wps->wps->wps_upnp)) {
			if (wps->wps->upnp_msgs)
				return WPS_CONTINUE;
			if (wps->ext_reg == 0)
				wps->ext_reg = 1;
			da16x_wps_prt("[%s] WPS: Wait for response from an "
				   "external Registrar\n", __func__);
			return WPS_PENDING;
		}
#endif /* CONFIG_WPS_UPNP */

		da16x_wps_prt("[%s] WPS: No more registrars available - "
			   "terminate negotiation\n", __func__);
	}

	return WPS_FAILURE;
}


static enum wps_process_res wps_process_wsc_nack(struct wps_data *wps,
						 const struct wpabuf *msg)
{
	struct wps_parse_attr attr;
	int old_state;
	u16 config_error;

	da16x_wps_prt("[%s] WPS: Received WSC_NACK\n", __func__);

	old_state = wps->state;
	wps->state = SEND_WSC_NACK;

	if (wps_parse_msg(msg, &attr) < 0)
		return WPS_FAILURE;

	if (attr.msg_type == NULL) {
		da16x_wps_prt("[%s] WPS: No Message Type attribute\n", __func__);
		return WPS_FAILURE;
	}

	if (*attr.msg_type != WPS_WSC_NACK) {
		da16x_wps_prt("[%s] WPS: Invalid Message Type %d\n\n", __func__,
			   *attr.msg_type);
		return WPS_FAILURE;
	}

#ifdef CONFIG_WPS_UPNP
	if (wps->wps->wps_upnp && wps->ext_reg) {
		da16x_wps_prt("[%s] WPS: Negotiation using external "
			   "Registrar terminated by the Enrollee\n", __func__);
		return WPS_FAILURE;
	}
#endif /* CONFIG_WPS_UPNP */

	if (attr.registrar_nonce == NULL ||
	    os_memcmp(wps->nonce_r, attr.registrar_nonce, WPS_NONCE_LEN) != 0)
	{
		da16x_wps_prt("[%s] WPS: Mismatch in registrar nonce\n", __func__);
		return WPS_FAILURE;
	}

	if (attr.enrollee_nonce == NULL ||
	    os_memcmp(wps->nonce_e, attr.enrollee_nonce, WPS_NONCE_LEN) != 0) {
		da16x_wps_prt("[%s] WPS: Mismatch in enrollee nonce\n", __func__);
		return WPS_FAILURE;
	}

	if (attr.config_error == NULL) {
		da16x_wps_prt("[%s] WPS: No Configuration Error attribute "
			   "in WSC_NACK\n", __func__);
		return WPS_FAILURE;
	}

	config_error = WPA_GET_BE16(attr.config_error);
	da16x_wps_prt("[%s] WPS: Enrollee terminated negotiation with "
		   "Configuration Error %d\n\n", __func__, config_error);

	switch (old_state) {
	case RECV_M3:
		wps_fail_event(wps->wps, WPS_M2, config_error,
			       wps->error_indication, wps->mac_addr_e);
		break;
	case RECV_M5:
		wps_fail_event(wps->wps, WPS_M4, config_error,
			       wps->error_indication, wps->mac_addr_e);
		break;
	case RECV_M7:
		wps_fail_event(wps->wps, WPS_M6, config_error,
			       wps->error_indication, wps->mac_addr_e);
		break;
	case RECV_DONE:
		wps_fail_event(wps->wps, WPS_M8, config_error,
			       wps->error_indication, wps->mac_addr_e);
		break;
	default:
		break;
	}

	return WPS_FAILURE;
}


static enum wps_process_res wps_process_wsc_done(struct wps_data *wps,
						 const struct wpabuf *msg)
{
	struct wps_parse_attr attr;

	da16x_wps_prt("[%s] WPS: Received WSC_Done\n", __func__);

#ifdef	CONFIG_WPS_UPNP
	if (wps->state != RECV_DONE && (!wps->wps->wps_upnp || !wps->ext_reg)) {
#else
	if (wps->state != RECV_DONE && !wps->ext_reg) {
#endif	/* CONFIG_WPS_UPNP */
		da16x_wps_prt("[%s] WPS: Unexpected state (%d) for "
			   "receiving WSC_Done\n\n", __func__, wps->state);
		return WPS_FAILURE;
	}

	if (wps_parse_msg(msg, &attr) < 0)
		return WPS_FAILURE;

	if (attr.msg_type == NULL) {
		da16x_wps_prt("[%s] WPS: No Message Type attribute\n", __func__);
		return WPS_FAILURE;
	}

	if (*attr.msg_type != WPS_WSC_DONE) {
		da16x_wps_prt("[%s] WPS: Invalid Message Type %d\n\n", __func__,
			   *attr.msg_type);
		return WPS_FAILURE;
	}

#ifdef CONFIG_WPS_UPNP
	if (wps->wps->wps_upnp && wps->ext_reg) {
		da16x_wps_prt("[%s] WPS: Negotiation using external "
			   "Registrar completed successfully\n", __func__);
#if 0	/* by Shingu 20160630 (WPS Optimize) */
		wps_device_store(wps->wps->registrar, &wps->peer_dev,
				 wps->uuid_e);
#endif	/* 0 */
		return WPS_DONE;
	}
#endif /* CONFIG_WPS_UPNP */

	if (attr.registrar_nonce == NULL ||
	    os_memcmp(wps->nonce_r, attr.registrar_nonce, WPS_NONCE_LEN) != 0)
	{
		da16x_wps_prt("[%s] WPS: Mismatch in registrar nonce\n", __func__);
		return WPS_FAILURE;
	}

	if (attr.enrollee_nonce == NULL ||
	    os_memcmp(wps->nonce_e, attr.enrollee_nonce, WPS_NONCE_LEN) != 0) {
		da16x_wps_prt("[%s] WPS: Mismatch in enrollee nonce\n", __func__);
		return WPS_FAILURE;
	}

	da16x_wps_prt("[%s] WPS: Negotiation completed successfully\n", __func__);
#if 0	/* by Shingu 20160630 (WPS Optimize) */
	wps_device_store(wps->wps->registrar, &wps->peer_dev,
			 wps->uuid_e);
#endif	/* 0 */

	if (wps->wps->wps_state == WPS_STATE_NOT_CONFIGURED && wps->new_psk &&
	    wps->wps->ap && !wps->wps->registrar->disable_auto_conf) {
		struct wps_credential cred;

		da16x_wps_prt("[%s] WPS: Moving to Configured state based "
			   "on first Enrollee connection\n", __func__);

		os_memset(&cred, 0, sizeof(cred));
		os_memcpy(cred.ssid, wps->wps->ssid, wps->wps->ssid_len);
		cred.ssid_len = wps->wps->ssid_len;
		cred.auth_type = WPS_AUTH_WPAPSK | WPS_AUTH_WPA2PSK;
		cred.encr_type = WPS_ENCR_TKIP | WPS_ENCR_AES;
		os_memcpy(cred.key, wps->new_psk, wps->new_psk_len);
		cred.key_len = wps->new_psk_len;

		wps->wps->wps_state = WPS_STATE_CONFIGURED;
		da16x_ascii_dump("WPS: Generated random passphrase",
				      wps->new_psk, wps->new_psk_len);
		if (wps->wps->cred_cb)
			wps->wps->cred_cb(wps->wps->cb_ctx, &cred);

		os_free(wps->new_psk);
		wps->new_psk = NULL;
	}

	if (!wps->wps->ap && !wps->er)
		wps_sta_cred_cb(wps);

	if (wps->new_psk) {
		if (wps_cb_new_psk(wps->wps->registrar, wps->mac_addr_e,
#ifdef CONFIG_P2P
				   wps->p2p_dev_addr,
#else
					NULL,
#endif /* CONFIG_P2P */
					wps->new_psk,
				   wps->new_psk_len)) {
			da16x_wps_prt("[%s] WPS: Failed to configure the "
				   "new PSK\n", __func__);
		}
		os_free(wps->new_psk);
		wps->new_psk = NULL;
	}

	wps_cb_reg_success(wps->wps->registrar, wps->mac_addr_e, wps->uuid_e,
			   wps->dev_password, wps->dev_password_len);

	if (wps->pbc) {
		wps_registrar_remove_pbc_session(wps->wps->registrar,
						 wps->uuid_e,
#ifdef CONFIG_P2P
						 wps->p2p_dev_addr
#else
						NULL
#endif /* CONFIG_P2P */
						);
		wps_registrar_pbc_completed(wps->wps->registrar);
#ifdef WPS_WORKAROUNDS
		os_get_reltime(&wps->wps->registrar->pbc_ignore_start);
#endif /* WPS_WORKAROUNDS */
		os_memcpy(wps->wps->registrar->pbc_ignore_uuid, wps->uuid_e,
			  WPS_UUID_LEN);
	} else {
		wps_registrar_pin_completed(wps->wps->registrar);
	}
	/* TODO: maintain AuthorizedMACs somewhere separately for each ER and
	 * merge them into APs own list.. */

	wps_success_event(wps->wps, wps->mac_addr_e);

	return WPS_DONE;
}


enum wps_process_res wps_registrar_process_msg(struct wps_data *wps,
					       enum wsc_op_code op_code,
					       const struct wpabuf *msg)
{
	enum wps_process_res ret;

	da16x_wps_prt("\n[%s] WPS: Processing received message (len=%lu "
		   "op_code=%d)\n\n", __func__,
		   (unsigned long) wpabuf_len(msg), op_code);

#ifdef CONFIG_WPS_UPNP
	if (wps->wps->wps_upnp && op_code == WSC_MSG && wps->ext_reg == 1) {
		struct wps_parse_attr attr;
		if (wps_parse_msg(msg, &attr) == 0 && attr.msg_type &&
		    *attr.msg_type == WPS_M3)
			wps->ext_reg = 2; /* past M2/M2D phase */
	}
	if (wps->ext_reg > 1)
		wps_registrar_free_pending_m2(wps->wps);
	if (wps->wps->wps_upnp && wps->ext_reg &&
	    wps->wps->upnp_msgs == NULL &&
	    (op_code == WSC_MSG || op_code == WSC_Done || op_code == WSC_NACK))
	{
		struct wps_parse_attr attr;
		int type;
		if (wps_parse_msg(msg, &attr) < 0 || attr.msg_type == NULL)
			type = -1;
		else
			type = *attr.msg_type;
		da16x_wps_prt("[%s] WPS: Sending received message (type %d)"
			   " to external Registrar for processing\n\n", __func__, type);
		upnp_wps_device_send_wlan_event(wps->wps->wps_upnp,
						wps->mac_addr_e,
						UPNP_WPS_WLANEVENT_TYPE_EAP,
						msg);
		if (op_code == WSC_MSG)
			return WPS_PENDING;
	} else if (wps->wps->wps_upnp && wps->ext_reg && op_code == WSC_MSG) {
		da16x_wps_prt("[%s] WPS: Skip internal processing - using "
			   "external Registrar\n", __func__);
		return WPS_CONTINUE;
	}
#endif /* CONFIG_WPS_UPNP */

	switch (op_code) {
	case WSC_MSG:
		return wps_process_wsc_msg(wps, msg);
	case WSC_ACK:
		if (wps_validate_wsc_ack(msg) < 0)
			return WPS_FAILURE;
		return wps_process_wsc_ack(wps, msg);
	case WSC_NACK:
		if (wps_validate_wsc_nack(msg) < 0)
			return WPS_FAILURE;
		return wps_process_wsc_nack(wps, msg);
	case WSC_Done:
		if (wps_validate_wsc_done(msg) < 0)
			return WPS_FAILURE;
		ret = wps_process_wsc_done(wps, msg);
		if (ret == WPS_FAILURE) {
			wps->state = SEND_WSC_NACK;
			wps_fail_event(wps->wps, WPS_WSC_DONE,
				       wps->config_error,
				       wps->error_indication, wps->mac_addr_e);
		}
		return ret;
	default:
		da16x_wps_prt("[%s] WPS: Unsupported op_code %d\n\n", __func__, op_code);
		return WPS_FAILURE;
	}
}


int wps_registrar_update_ie(struct wps_registrar *reg)
{
	return wps_set_ie(reg);
}


static void wps_registrar_set_selected_timeout(void *eloop_ctx,
					       void *timeout_ctx)
{
	struct wps_registrar *reg = eloop_ctx;

	da16x_wps_prt("[%s] WPS: Selected Registrar timeout - "
		   "unselect internal Registrar\n", __func__);
	da16x_notice_prt("[%s] WPS-TIMEOUT\n", __func__);
	reg->selected_registrar = 0;
	reg->pbc = 0;
	wps_registrar_selected_registrar_changed(reg, 0);
}


#ifdef CONFIG_WPS_UPNP
static void wps_registrar_sel_reg_add(struct wps_registrar *reg,
				      struct subscription *s)
{
	int i, j;
	da16x_wps_prt("[%s] WPS: External Registrar selected (dev_pw_id=%d "
		   "config_methods=0x%x)\n\n", __func__,
		   s->dev_password_id, s->config_methods);
	reg->sel_reg_union = 1;
	if (reg->sel_reg_dev_password_id_override != DEV_PW_PUSHBUTTON)
		reg->sel_reg_dev_password_id_override = s->dev_password_id;
	if (reg->sel_reg_config_methods_override == -1)
		reg->sel_reg_config_methods_override = 0;
	reg->sel_reg_config_methods_override |= s->config_methods;
	for (i = 0; i < WPS_MAX_AUTHORIZED_MACS; i++)
		if (is_zero_ether_addr(reg->authorized_macs_union[i]))
			break;
	for (j = 0; i < WPS_MAX_AUTHORIZED_MACS && j < WPS_MAX_AUTHORIZED_MACS; j++) {
		if (is_zero_ether_addr(s->authorized_macs[j]))
			break;
		da16x_wps_prt("[%s] WPS: Add authorized MAC into union: "
			   MACSTR, __func__, MAC2STR(s->authorized_macs[j]));
		os_memcpy(reg->authorized_macs_union[i],
			  s->authorized_macs[j], ETH_ALEN);
		i++;
	}

#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Authorized MACs union",
		    (u8 *) reg->authorized_macs_union,
		    sizeof(reg->authorized_macs_union));
#endif
}
#endif /* CONFIG_WPS_UPNP */


static void wps_registrar_sel_reg_union(struct wps_registrar *reg)
{
#ifdef CONFIG_WPS_UPNP
	struct subscription *s;

	if (reg->wps->wps_upnp == NULL)
		return;

	dl_list_for_each(s, &reg->wps->wps_upnp->subscriptions,
			 struct subscription, list) {
		struct subscr_addr *sa;
		sa = dl_list_first(&s->addr_list, struct subscr_addr, list);
		if (sa) {
			da16x_wps_prt("[%s] WPS: External Registrar %s:%d\n\n", __func__,
				   da16x_inet_ntoa(sa->saddr.sin_addr),
				   ntohs(sa->saddr.sin_port));
		}
		if (s->selected_registrar)
			wps_registrar_sel_reg_add(reg, s);
		else
			da16x_wps_prt("[%s] WPS: External Registrar not "
				   "selected\n", __func__);
	}
#endif /* CONFIG_WPS_UPNP */
}


/**
 * wps_registrar_selected_registrar_changed - SetSelectedRegistrar change
 * @reg: Registrar data from wps_registrar_init()
 *
 * This function is called when selected registrar state changes, e.g., when an
 * AP receives a SetSelectedRegistrar UPnP message.
 */
void wps_registrar_selected_registrar_changed(struct wps_registrar *reg,
					      u16 dev_pw_id)
{
	da16x_wps_prt("[%s] WPS: Selected registrar information changed\n", __func__);

	reg->sel_reg_union = reg->selected_registrar;
	reg->sel_reg_dev_password_id_override = -1;
	reg->sel_reg_config_methods_override = -1;
	os_memcpy(reg->authorized_macs_union, reg->authorized_macs,
		  WPS_MAX_AUTHORIZED_MACS * ETH_ALEN);
#ifdef ENABLE_WPS_DBG
	da16x_dump("WPS: Authorized MACs union (start with own)",
		    (u8 *) reg->authorized_macs_union,
		    sizeof(reg->authorized_macs_union));
#endif
	if (reg->selected_registrar) {
		u16 methods;

		methods = reg->wps->config_methods & ~WPS_CONFIG_PUSHBUTTON;
		methods &= ~(WPS_CONFIG_VIRT_PUSHBUTTON |
			     WPS_CONFIG_PHY_PUSHBUTTON);
		if (reg->pbc) {
			reg->sel_reg_dev_password_id_override = DEV_PW_PUSHBUTTON;
			wps_set_pushbutton(&methods, reg->wps->config_methods);
		} else if (dev_pw_id)
			reg->sel_reg_dev_password_id_override = dev_pw_id;

		da16x_wps_prt("[%s] WPS: Internal Registrar selected "
			   "(pbc=%d)\n\n", __func__, reg->pbc);
		reg->sel_reg_config_methods_override = methods;
	} else
		da16x_wps_prt("[%s] WPS: Internal Registrar not selected\n", __func__);

	wps_registrar_sel_reg_union(reg);

	wps_set_ie(reg);
	wps_cb_set_sel_reg(reg);
}


int wps_registrar_get_info(struct wps_registrar *reg, const u8 *addr,
			   char *buf, size_t buflen)
{
	struct wps_registrar_device *d;
	int len = 0, ret;
	char uuid[40];
	char devtype[WPS_DEV_TYPE_BUFSIZE];

	d = wps_device_get(reg, addr);
	if (d == NULL)
		return 0;
	if (uuid_bin2str(d->uuid, uuid, sizeof(uuid)))
		return 0;

	ret = os_snprintf(buf + len, buflen - len,
			  "wpsUuid=%s\n"
			  "wpsPrimaryDeviceType=%s\n"
			  "wpsDeviceName=%s\n"
			  "wpsManufacturer=%s\n"
			  "wpsModelName=%s\n"
			  "wpsModelNumber=%s\n"
			  "wpsSerialNumber=%s\n",
			  uuid,
			  wps_dev_type_bin2str(d->dev.pri_dev_type, devtype,
					       sizeof(devtype)),
			  d->dev.device_name ? d->dev.device_name : "",
			  d->dev.manufacturer ? d->dev.manufacturer : "",
			  d->dev.model_name ? d->dev.model_name : "",
			  d->dev.model_number ? d->dev.model_number : "",
			  d->dev.serial_number ? d->dev.serial_number : "");
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	return len;
}


int wps_registrar_config_ap(struct wps_registrar *reg,
			    struct wps_credential *cred)
{
	da16x_wps_prt("[%s] WPS: encr_type=0x%x\n\n", __func__, cred->encr_type);
	if (!(cred->encr_type & (WPS_ENCR_NONE | WPS_ENCR_TKIP |
				 WPS_ENCR_AES))) {
		if (cred->encr_type & WPS_ENCR_WEP) {
			da16x_wps_prt("[%s] WPS: Reject new AP settings "
				   "due to WEP configuration\n", __func__);
			return -1;
		}

		da16x_wps_prt("[%s] WPS: Reject new AP settings due to "
			   "invalid encr_type 0x%x\n\n", __func__, cred->encr_type);
		return -1;
	}

	if ((cred->encr_type & (WPS_ENCR_TKIP | WPS_ENCR_AES)) ==
	    WPS_ENCR_TKIP) {
		da16x_wps_prt("[%s] WPS: Upgrade encr_type TKIP -> "
			   "TKIP+AES\n", __func__);
		cred->encr_type |= WPS_ENCR_AES;
	}

	if ((cred->auth_type & (WPS_AUTH_WPAPSK | WPS_AUTH_WPA2PSK)) ==
	    WPS_AUTH_WPAPSK) {
		da16x_wps_prt("[%s] WPS: Upgrade auth_type WPAPSK -> "
			   "WPAPSK+WPA2PSK\n", __func__);
		cred->auth_type |= WPS_AUTH_WPA2PSK;
	}

	if (reg->wps->cred_cb)
		return reg->wps->cred_cb(reg->wps->cb_ctx, cred);

	return -1;
}


#endif	/* CONFIG_WPS */

/* EOF */
