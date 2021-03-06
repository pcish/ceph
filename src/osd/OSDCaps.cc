
#include "OSDCaps.h"

#include "config.h"


bool OSDCaps::get_next_token(string s, size_t& pos, string& token)
{
  int start = s.find_first_not_of(" \t", pos);
  int end;

  if (s[start] == '=' || s[start] == ',' || s[start] == ';') {
    end = start + 1;
  } else {
    end = s.find_first_of(";,= \t", start+1);
  }

  if (start < 0) {
    return false; 
  }

  if (end < 0) {
    end=s.size();
  }

  token = s.substr(start, end - start);

  pos = end;

  return true;
}

bool OSDCaps::is_rwx(string& token, rwx_t& cap_val)
{
  const char *t = token.c_str();
  int val = 0;

  while (*t) {
    switch (*t) {
    case 'r':
      val |= OSD_POOL_CAP_R;
      break;
    case 'w':
      val |= OSD_POOL_CAP_W;
      break;
    case 'x':
      val |= OSD_POOL_CAP_X;
      break;
    default:
      return false;
    }
    t++;
  }

  cap_val = val;
  return true;
}

bool OSDCaps::parse(bufferlist::iterator& iter)
{
  string s;

  try {
    ::decode(s, iter);

    generic_dout(0) << "decoded caps: " << s << dendl;

    size_t pos = 0;
    string token;
    bool init = true;

    bool op_allow = false;
    bool op_deny = false;
    bool cmd_pool = false;
    bool cmd_uid = false;
    bool any_cmd = false;
    bool got_eq = false;
    list<int> num_list;
    bool last_is_comma = false;
    rwx_t cap_val = 0;

    while (pos < s.size()) {
      if (init) {
        op_allow = false;
        op_deny = false;
        cmd_pool = false;
	cmd_uid = false;
        any_cmd = false;
        got_eq = false;
        last_is_comma = false;
        cap_val = 0;
        init = false;
        num_list.clear();
      }

#define ASSERT_STATE(x) \
do { \
  if (!(x)) { \
       *_dout << "error parsing caps at pos=" << pos << " (" #x ")" << std::endl; \
  } \
} while (0)

      if (get_next_token(s, pos, token)) {
	if (token.compare("*") == 0) { //allow all operations 
	  ASSERT_STATE(op_allow);
	  allow_all = true;
	} else if (token.compare("=") == 0) {
          ASSERT_STATE(any_cmd);
          got_eq = true;
        } else if (token.compare("allow") == 0) {
          ASSERT_STATE((!op_allow) && (!op_deny));
          op_allow = true;
        } else if (token.compare("deny") == 0) {
          ASSERT_STATE((!op_allow) && (!op_deny));
          op_deny = true;
        } else if ((token.compare("pools") == 0) ||
                   (token.compare("pool") == 0)) {
          ASSERT_STATE(!cmd_uid && (op_allow || op_deny));
          cmd_pool = true;
          any_cmd = true;
	} else if (token.compare("uid") == 0) {
	  ASSERT_STATE(!cmd_pool && (op_allow || op_deny));
	  any_cmd = true;
	  cmd_uid = true;
        } else if (is_rwx(token, cap_val)) {
          ASSERT_STATE(op_allow || op_deny);
        } else if (token.compare(";") != 0) {
	  ASSERT_STATE(got_eq);
          if (token.compare(",") == 0) {
            ASSERT_STATE(!last_is_comma);
	    last_is_comma = true;
          } else {
            last_is_comma = false;
            int num = strtol(token.c_str(), NULL, 10);
            num_list.push_back(num);
          }
        }

        if (token.compare(";") == 0 || pos >= s.size()) {
          if (got_eq) {
            ASSERT_STATE(num_list.size() > 0);
            list<int>::iterator iter;
	    map<int, OSDCap> *working_map = &pools_map;
	    if (cmd_uid) working_map = &auid_map;
            for (iter = num_list.begin(); iter != num_list.end(); ++iter) {
              OSDCap& cap = (*working_map)[*iter];
              if (op_allow) {
                cap.allow |= cap_val;
              } else {
                cap.deny |= cap_val;
              }
            }
          } else {
            if (op_allow) {
              default_action |= cap_val;
            } else {
              default_action &= ~cap_val;
            }
          }
          init = true;
        }
        
      }
    }
  } catch (buffer::error *err) {
    return false;
  }

  generic_dout(0) << "default=" << (int)default_action << dendl;
  map<int, OSDCap>::iterator it;
  for (it = pools_map.begin(); it != pools_map.end(); ++it) {
    generic_dout(0) << it->first << " -> (" << (int)it->second.allow << "." << (int)it->second.deny << ")" << dendl;
  }

  return true;
}

/**
 * Get the caps given this OSDCaps object for a given pool id
 * and uid (the pool's owner).
 *
 * Basic strategy: You can never have more caps than the OSD
 * allows you. So, calculate the capabilities the pool owner
 * grants you, and then logical-AND these with your own caps.
 *
 * To check the granted caps, first see if you have caps on
 * the auid and apply, then apply any caps granted on the pool
 * (this lets users give out caps to their auid but keep one pool
 * private, for instance).
 * If these two steps haven't given you explicit caps
 * on the pool, check if you're the pool owner and grant full.
 * Otherwise, you get nothing.
 */
int OSDCaps::get_pool_cap(int pool_id, __u64 uid)
{
  if (allow_all)
    return OSD_POOL_CAP_ALL;

  int explicit_cap = 0; //explicitly granted caps
  
  map<int, OSDCap>::iterator iter;
  //if the owner is granted permissions on the pool owner's auid, grant them
  if ((iter = auid_map.find(uid)) != pools_map.end()) {
    OSDCap& auid_cap = iter->second;
    explicit_cap |= auid_cap.allow;
    explicit_cap &= ~auid_cap.deny;
  }
  //check for explicitly granted caps and apply if needed
  if ((iter = pools_map.find(pool_id)) != pools_map.end()) {
    OSDCap& c = iter->second;
    explicit_cap |= c.allow;
    explicit_cap &= ~c.deny;
  }
  
  //owner gets full perms by default:
  if (uid != CEPH_AUTH_UID_DEFAULT
      && uid == auid
      && explicit_cap == 0) {
    explicit_cap = OSD_POOL_CAP_ALL;
  }

  //so now we've checked for explicitly granted perms, and we combine
  //these with the perms the OSD allows this auid to determine what
  //actions the auid can take on this pool.
  int cap = explicit_cap & default_action;
  
  return cap;
}

