#ifndef __CEPH_DECODE_H
#define __CEPH_DECODE_H

#include <asm/unaligned.h>

#include "types.h"

/*
 * in all cases,
 *   void **p     pointer to position pointer
 *   void *end    pointer to end of buffer (last byte + 1)
 */

static inline u64 ceph_decode_64(void **p)
{
	u64 v = get_unaligned_le64(*p);
	*p += sizeof(u64);
	return v;
}
static inline u32 ceph_decode_32(void **p)
{
	u32 v = get_unaligned_le32(*p);
	*p += sizeof(u32);
	return v;
}
static inline u16 ceph_decode_16(void **p)
{
	u16 v = get_unaligned_le16(*p);
	*p += sizeof(u16);
	return v;
}
static inline u8 ceph_decode_8(void **p)
{
	u8 v = *(u8 *)*p;
	(*p)++;
	return v;
}
static inline void ceph_decode_copy(void **p, void *pv, size_t n)
{
	memcpy(pv, *p, n);
	*p += n;
}

/*
 * bounds check input.
 */
#define ceph_decode_need(p, end, n, bad)		\
	do {						\
		if (unlikely(*(p) + (n) > (end))) 	\
			goto bad;			\
	} while (0)

#define ceph_decode_64_safe(p, end, v, bad)			\
	do {							\
		ceph_decode_need(p, end, sizeof(u64), bad);	\
		v = ceph_decode_64(p);				\
	} while (0)
#define ceph_decode_32_safe(p, end, v, bad)			\
	do {							\
		ceph_decode_need(p, end, sizeof(u32), bad);	\
		v = ceph_decode_32(p);				\
	} while (0)
#define ceph_decode_16_safe(p, end, v, bad)			\
	do {							\
		ceph_decode_need(p, end, sizeof(u16), bad);	\
		v = ceph_decode_16(p);				\
	} while (0)

#define ceph_decode_copy_safe(p, end, pv, n, bad)		\
	do {							\
		ceph_decode_need(p, end, n, bad);		\
		ceph_decode_copy(p, pv, n);			\
	} while (0)

/*
 * struct ceph_timespec <-> struct timespec
 */
static inline void ceph_decode_timespec(struct timespec *ts,
					struct ceph_timespec *tv)
{
	ts->tv_sec = le32_to_cpu(tv->tv_sec);
	ts->tv_nsec = le32_to_cpu(tv->tv_nsec);
}
static inline void ceph_encode_timespec(struct ceph_timespec *tv,
					struct timespec *ts)
{
	tv->tv_sec = cpu_to_le32(ts->tv_sec);
	tv->tv_nsec = cpu_to_le32(ts->tv_nsec);
}

/*
 * encoders
 */
static inline void ceph_encode_64(void **p, u64 v)
{
	put_unaligned_le64(v, (__le64 *)*p);
	*p += sizeof(u64);
}
static inline void ceph_encode_32(void **p, u32 v)
{
	put_unaligned_le32(v, (__le32 *)*p);
	*p += sizeof(u32);
}
static inline void ceph_encode_16(void **p, u16 v)
{
	put_unaligned_le16(v, (__le16 *)*p);
	*p += sizeof(u16);
}
static inline void ceph_encode_8(void **p, u8 v)
{
	*(u8 *)*p = v;
	(*p)++;
}

/*
 * filepath, string encoders
 */
static inline void ceph_encode_filepath(void **p, void *end,
					u64 ino, const char *path)
{
	u32 len = path ? strlen(path) : 0;
	BUG_ON(*p + sizeof(ino) + sizeof(len) + len > end);
	ceph_encode_64(p, ino);
	ceph_encode_32(p, len);
	if (len)
		memcpy(*p, path, len);
	*p += len;
}

static inline void ceph_encode_string(void **p, void *end,
				      const char *s, u32 len)
{
	BUG_ON(*p + sizeof(len) + len > end);
	ceph_encode_32(p, len);
	if (len)
		memcpy(*p, s, len);
	*p += len;
}


#endif
