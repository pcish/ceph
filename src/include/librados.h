#ifndef __LIBRADOS_H
#define __LIBRADOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>
#include <linux/types.h>
#include <string.h>

#ifndef CEPH_OSD_TMAP_SET
#define CEPH_OSD_TMAP_HDR 'h'
#define CEPH_OSD_TMAP_SET 's'
#define CEPH_OSD_TMAP_RM  'r'
#endif

/* initialization */
int rados_initialize(int argc, const char **argv); /* arguments are optional */
void rados_deinitialize();

/* pools */
typedef void *rados_pool_t;
typedef void *rados_list_ctx_t;
typedef uint64_t rados_snap_t;

struct rados_pool_stat_t {
  uint64_t num_bytes;    // in bytes
  uint64_t num_kb;       // in KB
  uint64_t num_objects;
  uint64_t num_object_clones;
  uint64_t num_object_copies;  // num_objects * num_replicas
  uint64_t num_objects_missing_on_primary;
  uint64_t num_objects_degraded;
  uint64_t num_rd, num_rd_kb,num_wr, num_wr_kb;
};

struct rados_statfs_t {
  uint64_t kb, kb_used, kb_avail;
  uint64_t num_objects;
};

int rados_open_pool(const char *name, rados_pool_t *pool);
int rados_close_pool(rados_pool_t pool);
int rados_lookup_pool(const char *name);

int rados_stat_pool(rados_pool_t pool, struct rados_pool_stat_t *stats);

void rados_set_snap(rados_pool_t pool, rados_snap_t snap);

int rados_create_pool(const char *name);
int rados_create_pool_with_auid(const char *name, uint64_t auid);
int rados_delete_pool(const rados_pool_t pool);
int rados_change_pool_auid(const rados_pool_t pool, uint64_t auid);

/* objects */
int rados_list_objects_open(rados_pool_t pool, rados_list_ctx_t *ctx);
int rados_list_objects_next(rados_list_ctx_t ctx, const char **entry);
void rados_list_objects_close(rados_list_ctx_t ctx);


/* snapshots */
int rados_snap_create(const rados_pool_t pool, const char *snapname);
int rados_snap_remove(const rados_pool_t pool, const char *snapname);
int rados_selfmanaged_snap_create(const rados_pool_t pool, uint64_t *snapid);
int rados_selfmanaged_snap_remove(const rados_pool_t pool, uint64_t snapid);
int rados_snap_list(rados_pool_t pool, rados_snap_t *snaps, int maxlen);
int rados_snap_lookup(rados_pool_t pool, const char *name, rados_snap_t *id);
int rados_snap_get_name(rados_pool_t pool, rados_snap_t id, char *name, int maxlen);

/* sync io */
int rados_write(rados_pool_t pool, const char *oid, off_t off, const char *buf, size_t len);
int rados_read(rados_pool_t pool, const char *oid, off_t off, char *buf, size_t len);
int rados_remove(rados_pool_t pool, const char *oid);

/* attrs */
int rados_getxattr(rados_pool_t pool, const char *o, const char *name, char *buf, size_t len);
int rados_setxattr(rados_pool_t pool, const char *o, const char *name, const char *buf, size_t len);

/* misc */
int rados_stat(rados_pool_t pool, const char *o, uint64_t *psize, time_t *pmtime);
int rados_tmap_update(rados_pool_t pool, const char *o, const char *cmdbuf, size_t cmdbuflen);
int rados_exec(rados_pool_t pool, const char *oid, const char *cls, const char *method,
	       const char *in_buf, size_t in_len, char *buf, size_t out_len);

/* async io */
typedef void *rados_completion_t;
typedef void (*rados_callback_t)(rados_completion_t cb, void *arg);

int rados_aio_create_completion(void *cb_arg, rados_callback_t cb_complete, rados_callback_t cb_safe,
				rados_completion_t *pc);
int rados_aio_wait_for_complete(rados_completion_t c);
int rados_aio_wait_for_safe(rados_completion_t c);
int rados_aio_is_complete(rados_completion_t c);
int rados_aio_is_safe(rados_completion_t c);
int rados_aio_get_return_value(rados_completion_t c);
void rados_aio_release(rados_completion_t c);
int rados_aio_write(rados_pool_t pool, const char *oid,
		    off_t off, const char *buf, size_t len,
		    rados_completion_t completion);
int rados_aio_read(rados_pool_t pool, const char *oid,
		   off_t off, char *buf, size_t len,
		   rados_completion_t completion);

#ifdef __cplusplus
}
#endif

#endif
