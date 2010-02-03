#ifndef __RGW_ACCESS_H
#define __RGW_ACCESS_H

#include <time.h>
#include <string>
#include <vector>
#include <include/types.h>

#include <openssl/md5.h>

#include "rgw_common.h"


class RGWAccess {
public:
  virtual int initialize(int argc, char *argv[]) { return 0; }
  virtual int list_buckets_init(std::string& id, RGWAccessHandle *handle) = 0;
  virtual int list_buckets_next(std::string& id, RGWObjEnt& obj, RGWAccessHandle *handle) = 0;

  virtual int list_objects(std::string& id, std::string& bucket, int max, std::string& prefix, std::string& delim,
                           std::string& marker, std::vector<RGWObjEnt>& result, map<string, bool>& common_prefixes) = 0;

  virtual int create_bucket(std::string& id, std::string& bucket, map<nstring, bufferlist>& attrs) = 0;
  virtual int put_obj(std::string& id, std::string& bucket, std::string& obj, const char *data, size_t size,
                      time_t *mtime,
                      map<nstring, bufferlist>& attrs) = 0;

  virtual int copy_obj(std::string& id, std::string& dest_bucket, std::string& dest_obj,
                      std::string& src_bucket, std::string& src_obj,
                      time_t *mtime,
                      const time_t *mod_ptr,
                      const time_t *unmod_ptr,
                      const char *if_match,
                      const char *if_nomatch,
                      map<nstring, bufferlist>& attrs,
                      struct rgw_err *err) = 0;
  virtual int delete_bucket(std::string& id, std::string& bucket) = 0;
  virtual int delete_obj(std::string& id, std::string& bucket, std::string& obj) = 0;

  virtual int get_obj(std::string& bucket, std::string& obj, 
            char **data, off_t ofs, off_t end,
            map<nstring, bufferlist> *attrs,
            const time_t *mod_ptr,
            const time_t *unmod_ptr,
            const char *if_match,
            const char *if_nomatch,
            bool get_data,
            struct rgw_err *err) = 0;

  virtual int get_attr(std::string& bucket, std::string& obj,
                       const char *name, bufferlist& dest) = 0;
  virtual int set_attr(std::string& bucket, std::string& obj,
                       const char *name, bufferlist& bl) = 0;

  static RGWAccess *init_storage_provider(const char *type, int argc, char *argv[]);
  static RGWAccess *store;
};

#define rgwstore RGWAccess::store



#endif