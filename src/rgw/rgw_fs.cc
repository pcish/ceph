#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/xattr.h>

#include "rgw_access.h"
#include "rgw_fs.h"

#include <string>
#include <iostream>
#include <vector>
#include <map>

using namespace std;

struct rgwfs_state {
  DIR *dir;
};

#define DIR_NAME "/tmp/radosgw"

int RGWFS::list_buckets_init(string& id, RGWAccessHandle *handle)
{
  DIR *dir = opendir(DIR_NAME);
  struct rgwfs_state *state;

  if (!dir)
    return -errno;

  state = (struct rgwfs_state *)malloc(sizeof(struct rgwfs_state));
  if (!state)
    return -ENOMEM;

  state->dir = dir;

  *handle = (RGWAccessHandle)state;

  return 0;
}

int RGWFS::list_buckets_next(string& id, RGWObjEnt& obj, RGWAccessHandle *handle)
{
  struct rgwfs_state *state;
  struct dirent *dirent;
#define BUF_SIZE 512

  if (!handle)
    return -EINVAL;
  state = *(struct rgwfs_state **)handle;

  if (!state)
    return -EINVAL;

  while (1) {
    dirent = readdir(state->dir);
    if (!dirent) {
      closedir(state->dir);
      *handle = NULL;
      return -ENOENT;
    }

    if (dirent->d_name[0] == '.')
      continue;

    obj.name = dirent->d_name;

    char buf[BUF_SIZE];
    struct stat statbuf;
    snprintf(buf, BUF_SIZE, "%s/%s", DIR_NAME, obj.name.c_str());
    if (stat(buf, &statbuf) < 0)
      continue;
    obj.mtime = statbuf.st_mtime;
    obj.size = statbuf.st_size;
    return 0;
  }
}

int RGWFS::list_objects(string& id, string& bucket, int max, string& prefix, string& delim,
                       string& marker, vector<RGWObjEnt>& result, map<string, bool>& common_prefixes)
{
  map<string, bool> dir_map;
  char path[BUF_SIZE];

  snprintf(path, BUF_SIZE, "%s/%s", DIR_NAME, bucket.c_str());

  DIR *dir = opendir(path);
  if (!dir)
    return -errno;

  while (1) {
    struct dirent *dirent;
    dirent = readdir(dir);
    if (!dirent)
      break;
    if (dirent->d_name[0] == '.')
      continue;

    if (prefix.empty() ||
        (prefix.compare(0, prefix.size(), prefix) == 0)) {
      dir_map[dirent->d_name] = true;
    }
  }

  closedir(dir);


  map<string, bool>::iterator iter;
  if (!marker.empty())
    iter = dir_map.lower_bound(marker);
  else
    iter = dir_map.begin();

  if (max < 0)
    max = INT_MAX;

  result.clear();
  int i;
  for (i=0; i<max && iter != dir_map.end(); i++, ++iter) {
    RGWObjEnt obj;
    char buf[BUF_SIZE];
    struct stat statbuf;
    obj.name = iter->first;
    snprintf(buf, BUF_SIZE, "%s/%s", path, obj.name.c_str());
    if (stat(buf, &statbuf) < 0)
      continue;
    obj.mtime = statbuf.st_mtime;
    obj.size = statbuf.st_size;
    char *etag;
    if (get_attr(RGW_ATTR_ETAG, buf, &etag) >= 0) {
      strncpy(obj.etag, etag, sizeof(obj.etag));
      obj.etag[sizeof(obj.etag)-1] = '\0';
      free(etag);
    }
    result.push_back(obj);
  }

  return i;
}


int RGWFS::create_bucket(std::string& id, std::string& bucket, map<std::string, bufferlist>& attrs, __u64 auid)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1;
  char buf[len];
  snprintf(buf, len, "%s/%s", DIR_NAME, bucket.c_str());

  if (mkdir(buf, 0755) < 0)
    return -errno;

  map<std::string, bufferlist>::iterator iter;
  for (iter = attrs.begin(); iter != attrs.end(); ++iter) {
    const string& name = iter->first;
    bufferlist& bl = iter->second;
    
    if (bl.length()) {
      int r = setxattr(buf, name.c_str(), bl.c_str(), bl.length(), 0);
      if (r < 0) {
        r = -errno;
        rmdir(buf);
        return r;
      }
    }
  }

  return 0;
}

int RGWFS::put_obj(std::string& id, std::string& bucket, std::string& obj, const char *data, size_t size,
                  time_t *mtime,
                  map<string, bufferlist>& attrs)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1 + obj.size() + 1;
  char buf[len];
  snprintf(buf, len, "%s/%s/%s", DIR_NAME, bucket.c_str(), obj.c_str());
  int fd;

  fd = open(buf, O_CREAT | O_WRONLY, 0755);
  if (fd < 0)
    return -errno;

  int r;
  map<string, bufferlist>::iterator iter;
  for (iter = attrs.begin(); iter != attrs.end(); ++iter) {
    const string& name = iter->first;
    bufferlist& bl = iter->second;
    
    if (bl.length()) {
      r = fsetxattr(fd, name.c_str(), bl.c_str(), bl.length(), 0);
      if (r < 0) {
        r = -errno;
        close(fd);
        return r;
      }
    }
  }

  r = write(fd, data, size);
  if (r < 0) {
    r = -errno;
    close(fd);
    unlink(buf);
    return r;
  }

  if (mtime) {
    struct stat st;
    r = fstat(fd, &st);
    if (r < 0) {
      r = -errno;
      close(fd);
      unlink(buf);
      return -errno;
    }
    *mtime = st.st_mtime;
  }

  r = close(fd);
  if (r < 0)
    return -errno;

  return 0;
}

int RGWFS::copy_obj(std::string& id, std::string& dest_bucket, std::string& dest_obj,
               std::string& src_bucket, std::string& src_obj,
               time_t *mtime,
               const time_t *mod_ptr,
               const time_t *unmod_ptr,
               const char *if_match,
               const char *if_nomatch,
               map<string, bufferlist>& attrs,
               struct rgw_err *err)
{
  int ret;
  char *data;

  map<string, bufferlist> attrset;
  ret = get_obj(src_bucket, src_obj, &data, 0, -1, &attrset,
                mod_ptr, unmod_ptr, if_match, if_nomatch, true, err);
  if (ret < 0)
    return ret;

  map<string, bufferlist>::iterator iter;
  for (iter = attrs.begin(); iter != attrs.end(); ++iter) {
    attrset[iter->first] = iter->second;
  }
  attrs = attrset;

  ret = put_obj(id, dest_bucket, dest_obj, data, ret, mtime, attrs);

  return ret;
}


int RGWFS::delete_bucket(std::string& id, std::string& bucket)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1;
  char buf[len];
  snprintf(buf, len, "%s/%s", DIR_NAME, bucket.c_str());

  if (rmdir(buf) < 0)
    return -errno;

  return 0;
}


int RGWFS::delete_obj(std::string& id, std::string& bucket, std::string& obj)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1 + obj.size() + 1;
  char buf[len];
  snprintf(buf, len, "%s/%s/%s", DIR_NAME, bucket.c_str(), obj.c_str());

  if (unlink(buf) < 0)
    return -errno;

  return 0;
}

int RGWFS::get_attr(const char *name, int fd, char **attr)
{
  char *attr_buf;
#define ETAG_LEN 32
  size_t len = ETAG_LEN;
  ssize_t attr_len;

  while (1) {  
    attr_buf = (char *)malloc(len);
    attr_len = fgetxattr(fd, name, attr_buf, len);
    if (attr_len  > 0)
      break;

    free(attr_buf);
    switch (errno) {
    case ERANGE:
      break;
    default:
      return -errno;
    }
    len *= 2;
  }
  *attr = attr_buf;

  return attr_len;
}

int RGWFS::get_attr(const char *name, const char *path, char **attr)
{
  char *attr_buf;
  size_t len = ETAG_LEN;
  ssize_t attr_len;

  while (1) {  
    attr_buf = (char *)malloc(len);
    attr_len = getxattr(path, name, attr_buf, len);
    if (attr_len  > 0)
      break;

    free(attr_buf);
    switch (errno) {
    case ERANGE:
      break;
    default:
      cerr << "getxattr on " << path << " returned" << -errno << std::endl;
      return -errno;
    }
    len *= 2;
  }
  *attr = attr_buf;

  return attr_len;
}

int RGWFS::get_attr(std::string& bucket, std::string& obj,
                       const char *name, bufferlist& dest)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1 + obj.size() + 1;
  char buf[len];
  int r = -EINVAL;
  char *data = NULL;

  snprintf(buf, len, "%s/%s/%s", DIR_NAME, bucket.c_str(), obj.c_str());

  r = get_attr(name, buf, &data);
  if (r < 0)
      goto done;
  dest.append(data, r);
done:
  free(data);

  return r;
}

int RGWFS::set_attr(std::string& bucket, std::string& obj,
                       const char *name, bufferlist& bl)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1 + obj.size() + 1;
  char buf[len];
  int r;

  snprintf(buf, len, "%s/%s/%s", DIR_NAME, bucket.c_str(), obj.c_str());

  r = setxattr(buf, name, bl.c_str(), bl.length(), 0);

  int ret = (r < 0 ? -errno : 0);
  cerr << "setxattr: path=" << buf << " ret=" << ret << std::endl;

  return ret;
}

int RGWFS::get_obj(std::string& bucket, std::string& obj, 
            char **data, off_t ofs, off_t end,
            map<string, bufferlist> *attrs,
            const time_t *mod_ptr,
            const time_t *unmod_ptr,
            const char *if_match,
            const char *if_nomatch,
            bool get_data,
            struct rgw_err *err)
{
  int len = strlen(DIR_NAME) + 1 + bucket.size() + 1 + obj.size() + 1;
  char buf[len];
  int fd;
  struct stat st;
  int r = -EINVAL;
  size_t max_len, pos;
  char *etag = NULL;

  snprintf(buf, len, "%s/%s/%s", DIR_NAME, bucket.c_str(), obj.c_str());

  fd = open(buf, O_RDONLY, 0755);

  if (fd < 0)
    return -errno;

  r = fstat(fd, &st);
  if (r < 0)
    return -errno;

  if (end < 0)
    end = st.st_size - 1;

  max_len = end - ofs + 1;

  r = -ECANCELED;
  if (mod_ptr) {
    if (st.st_mtime < *mod_ptr) {
      err->num = "304";
      err->code = "PreconditionFailed";
      goto done;
    }
  }

  if (unmod_ptr) {
    if (st.st_mtime >= *mod_ptr) {
      err->num = "412";
      err->code = "PreconditionFailed";
      goto done;
    }
  }
  if (if_match || if_nomatch) {
    r = get_attr(RGW_ATTR_ETAG, fd, &etag);
    if (r < 0)
      goto done;
 
    r = -ECANCELED;
    if (if_match) {
      cerr << "etag=" << etag << " " << " if_match=" << if_match << endl;
      if (strcmp(if_match, etag)) {
        err->num = "412";
        err->code = "PreconditionFailed";
        goto done;
      }
    }

    if (if_nomatch) {
      cerr << "etag=" << etag << " " << " if_nomatch=" << if_nomatch << endl;
      if (strcmp(if_nomatch, etag) == 0) {
        err->num = "412";
        err->code = "PreconditionFailed";
        goto done;
      }
    }
  }

  if (!get_data) {
    r = max_len;
    goto done;
  }
  *data = (char *)malloc(max_len);
  if (!*data) {
    r = -ENOMEM;
    goto done;
  }

  pos = 0;
  while (pos < max_len) {
    r = read(fd, (*data) + pos, max_len);
    if (r > 0) {
      pos += r;
    } else {
      if (!r) {
        cerr << "pos=" << pos << " r=" << r << " max_len=" << max_len << endl;
        r = -EIO; /* should not happen as we validated file size earlier */
        goto done;
      }
      switch (errno) {
      case EINTR:
        break;
      default:
        r = -errno;
        goto done;
      }
    }
  } 

  r = max_len;
done:
  free(etag);
  close(fd);  

  return r;
}


