/*
 * wpa_supplicant - WPA2/RSN PMKSA cache functions
 * Copyright (c) 2003-2009, 2011-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PMKSA_CACHE_H
#define PMKSA_CACHE_H

/**
 * struct rsn_pmksa_cache_entry - PMKSA cache entry
 */
struct rsn_pmksa_cache_entry {
	struct rsn_pmksa_cache_entry *next;
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	os_time_t expiration;
	int akmp; /* WPA_KEY_MGMT_* */
	u8 aa[ETH_ALEN];

	/*
	 * If FILS Cache Identifier is included (fils_cache_id_set), this PMKSA
	 * cache entry is applicable to all BSSs (any BSSID/aa[]) that
	 * advertise the same FILS Cache Identifier within the same ESS.
	 */
	u8 fils_cache_id[2];
	unsigned int fils_cache_id_set:1;

	os_time_t reauth_time;

	/**
	 * network_ctx - Network configuration context
	 *
	 * This field is only used to match PMKSA cache entries to a specific
	 * network configuration (e.g., a specific SSID and security policy).
	 * This can be a pointer to the configuration entry, but PMKSA caching
	 * code does not dereference the value and this could be any kind of
	 * identifier.
	 */
	void *network_ctx;
	int opportunistic;
};

struct rsn_pmksa_cache;

enum pmksa_free_reason {
	PMKSA_FREE,
	PMKSA_REPLACE,
	PMKSA_EXPIRE,
};

#ifdef IEEE8021X_EAPOL

struct rsn_pmksa_cache *
pmksa_cache_init(void (*free_cb)(struct rsn_pmksa_cache_entry *entry,
				 void *ctx, enum pmksa_free_reason reason),
		 void *ctx, struct wpa_sm *sm);
void pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa);
#if 1
struct rsn_pmksa_cache_entry * pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
					       const u8 *aa, const u8 *pmkid,
					       const void *network_ctx,
					       int akmp);
#else
struct rsn_pmksa_cache_entry * pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
					       const u8 *aa, const u8 *pmkid,
					       const void *network_ctx);
#endif /* 1 */
#if 0	/* by Shingu 20160929 (WPA_CLI Optimize) */
int pmksa_cache_list(struct rsn_pmksa_cache *pmksa, char *buf, size_t len);
#endif	/* 0 */

#if 1
struct rsn_pmksa_cache_entry *
pmksa_cache_add(struct rsn_pmksa_cache *pmksa, const u8 *pmk, size_t pmk_len,
		const u8 *pmkid, const u8 *kck, size_t kck_len,
		const u8 *aa, const u8 *spa, void *network_ctx, int akmp,
		const u8 *cache_id);
#else
struct rsn_pmksa_cache_entry *
pmksa_cache_add(struct rsn_pmksa_cache *pmksa, const u8 *pmk, size_t pmk_len,
		const u8 *aa, const u8 *spa, void *network_ctx, int akmp);
#endif /* 1 */

struct rsn_pmksa_cache_entry *
pmksa_cache_add_entry(struct rsn_pmksa_cache *pmksa,
		      struct rsn_pmksa_cache_entry *entry);

struct rsn_pmksa_cache_entry * pmksa_cache_get_current(struct wpa_sm *sm);
void pmksa_cache_clear_current(struct wpa_sm *sm);
#if 1
int pmksa_cache_set_current(struct wpa_sm *sm, const u8 *pmkid,
			    const u8 *bssid, void *network_ctx,
			    int try_opportunistic, const u8 *fils_cache_id,
			    int akmp);
#else
int pmksa_cache_set_current(struct wpa_sm *sm, const u8 *pmkid,
			    const u8 *bssid, void *network_ctx,
			    int try_opportunistic);
#endif /* 1 */
struct rsn_pmksa_cache_entry *
pmksa_cache_get_opportunistic(struct rsn_pmksa_cache *pmksa,
			      void *network_ctx, const u8 *aa, int akmp);
void pmksa_cache_flush(struct rsn_pmksa_cache *pmksa, void *network_ctx,
		       const u8 *pmk, size_t pmk_len);

#else /* IEEE8021X_EAPOL */

static inline struct rsn_pmksa_cache *
pmksa_cache_init(void (*free_cb)(struct rsn_pmksa_cache_entry *entry,
				 void *ctx, enum pmksa_free_reason reason),
		 void *ctx, struct wpa_sm *sm)
{
	return (void *) -1;
}

static inline void pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa)
{
}

#if 1
static inline struct rsn_pmksa_cache_entry *
pmksa_cache_get(struct rsn_pmksa_cache *pmksa, const u8 *aa, const u8 *pmkid,
		const void *network_ctx, int akmp)
{
	return NULL;
}
#else
static inline struct rsn_pmksa_cache_entry *
pmksa_cache_get(struct rsn_pmksa_cache *pmksa, const u8 *aa, const u8 *pmkid,
		const void *network_ctx)
{
	return NULL;
}
#endif /* 1 */

static inline struct rsn_pmksa_cache_entry *
pmksa_cache_get_current(struct wpa_sm *sm)
{
	return NULL;
}

#if 0	/* by Shingu 20160929 (WPA_CLI Optimize) */
static inline int pmksa_cache_list(struct rsn_pmksa_cache *pmksa, char *buf,
				   size_t len)
{
	return -1;
}
#endif	/* 0 */

static inline struct rsn_pmksa_cache_entry *
pmksa_cache_add_entry(struct rsn_pmksa_cache *pmksa,
		      struct rsn_pmksa_cache_entry *entry)
{
	return NULL;
}

static inline struct rsn_pmksa_cache_entry *
pmksa_cache_add(struct rsn_pmksa_cache *pmksa, const u8 *pmk, size_t pmk_len,
		const u8 *pmkid, const u8 *kck, size_t kck_len,
		const u8 *aa, const u8 *spa, void *network_ctx, int akmp,
		const u8 *cache_id)
{
	return NULL;
}

static inline void pmksa_cache_clear_current(struct wpa_sm *sm)
{
}

static inline int pmksa_cache_set_current(struct wpa_sm *sm, const u8 *pmkid,
					  const u8 *bssid,
					  void *network_ctx,
					  int try_opportunistic)
{
	return -1;
}

static inline void pmksa_cache_flush(struct rsn_pmksa_cache *pmksa,
				     void *network_ctx,
				     const u8 *pmk, size_t pmk_len)
{
}

#endif /* IEEE8021X_EAPOL */

#endif /* PMKSA_CACHE_H */
