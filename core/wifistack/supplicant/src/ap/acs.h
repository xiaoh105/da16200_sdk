/*
 * ACS - Automatic Channel Selection module
 * Copyright (c) 2011, Atheros Communications
 * Copyright (c) 2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef ACS_H
#define ACS_H

#ifdef CONFIG_ACS

enum hostapd_chan_status acs_init(struct hostapd_iface *iface);
void acs_cleanup(struct hostapd_iface *iface);

#else /* CONFIG_ACS */

static inline enum hostapd_chan_status acs_init(struct hostapd_iface *iface)
{
	da16x_warn_prt("     [%s] ACS was disabled on your build, "
		"rebuild hostapd with CONFIG_ACS=y or set channel\n", __func__);
	return HOSTAPD_CHAN_INVALID;
}

#endif /* CONFIG_ACS */

#endif /* ACS_H */
