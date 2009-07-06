#ifndef __S3FS_H
#define __S3FS_H

#include "s3access.h"


class S3FS  : public S3Access
{
public:
  int list_buckets_init(std::string& id, S3AccessHandle *handle);
  int list_buckets_next(std::string& id, S3ObjEnt& obj, S3AccessHandle *handle);

  int list_objects(std::string& id, std::string& bucket, int max, std::string& prefix, std::string& marker, std::vector<S3ObjEnt>& result);

  int create_bucket(std::string& id, std::string& bucket, std::vector<std::pair<std::string, bufferlist> >& attrs);
  int put_obj(std::string& id, std::string& bucket, std::string& obj, const char *data, size_t size,
              time_t *mtime,
              std::vector<std::pair<std::string, bufferlist> >& attrs);
  int copy_obj(std::string& id, std::string& dest_bucket, std::string& dest_obj,
               std::string& src_bucket, std::string& src_obj,
               char **petag,
               time_t *mtime,
               const time_t *mod_ptr,
               const time_t *unmod_ptr,
               const char *if_match,
               const char *if_nomatch,
               std::vector<std::pair<std::string, bufferlist> >& attrs,
               struct s3_err *err);
  int delete_bucket(std::string& id, std::string& bucket);
  int delete_obj(std::string& id, std::string& bucket, std::string& obj);

  int get_attr(const char *name, int fd, char **attr);
  int get_attr(const char *name, const char *path, char **attr);
  int get_attr(std::string& bucket, std::string& obj,
               const char *name, bufferlist& dest);
  int set_attr(std::string& bucket, std::string& obj,
                       const char *name, bufferlist& bl);

 int get_obj(std::string& bucket, std::string& obj, 
            char **data, off_t ofs, off_t end, char **petag,
            const time_t *mod_ptr,
            const time_t *unmod_ptr,
            const char *if_match,
            const char *if_nomatch,
            bool get_data,
            struct s3_err *err);
};

#endif