#ifndef PLATFORM_H
#define PLATFORM_H

#include "includes.h"
#include "supp_common.h"

#define le16_to_cpu		le_to_host16
#define le32_to_cpu		le_to_host32

#define get_unaligned(p)					\
({								\
	struct packed_dummy_struct {				\
		typeof(*(p)) __val;				\
	} __attribute__((packed)) *__ptr = (void *) (p);	\
								\
	__ptr->__val;						\
})
#define diw_get_unaligned_le16(p)	le16_to_cpu(get_unaligned((uint16_t *)(p)))
#define diw_get_unaligned_le32(p)	le32_to_cpu(get_unaligned((uint32_t *)(p)))

#endif /* PLATFORM_H */
