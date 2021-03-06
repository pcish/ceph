// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */



#include "CDentry.h"
#include "CInode.h"
#include "CDir.h"
#include "Anchor.h"

#include "MDS.h"
#include "MDCache.h"
#include "Locker.h"
#include "LogSegment.h"

#include "messages/MLock.h"

#define DOUT_SUBSYS mds
#undef dout_prefix
#define dout_prefix *_dout << dbeginl << "mds" << dir->cache->mds->get_nodeid() << ".cache.den(" << dir->dirfrag() << " " << name << ") "


ostream& CDentry::print_db_line_prefix(ostream& out) 
{
  return out << g_clock.now() << " mds" << dir->cache->mds->get_nodeid() << ".cache.den(" << dir->ino() << " " << name << ") ";
}

boost::pool<> CDentry::pool(sizeof(CDentry));

LockType CDentry::lock_type(CEPH_LOCK_DN);


// CDentry

ostream& operator<<(ostream& out, CDentry& dn)
{
  filepath path;
  dn.make_path(path);
  
  out << "[dentry " << path;
  
  if (true || dn.first != 0 || dn.last != CEPH_NOSNAP) {
    out << " [" << dn.first << ",";
    if (dn.last == CEPH_NOSNAP) 
      out << "head";
    else
      out << dn.last;
    out << ']';
  }

  if (dn.is_auth()) {
    out << " auth";
    if (dn.is_replicated()) 
      out << dn.get_replicas();
  } else {
    out << " rep@" << dn.authority();
    out << "." << dn.get_replica_nonce();
  }

  if (dn.get_linkage()->is_null()) out << " NULL";
  if (dn.get_linkage()->is_remote()) {
    out << " REMOTE(";
    switch (dn.get_linkage()->get_remote_d_type() << 12) {
    case S_IFSOCK: out << "sock"; break;
    case S_IFLNK: out << "lnk"; break;
    case S_IFREG: out << "reg"; break;
    case S_IFBLK: out << "blk"; break;
    case S_IFDIR: out << "dir"; break;
    case S_IFCHR: out << "chr"; break;
    case S_IFIFO: out << "fifo"; break;
    default: assert(0);
    }
    out << ")";
  }

  out << " " << dn.lock;

  if (dn.get_projected_version() != dn.get_version())
    out << " pv=" << dn.get_projected_version();
  out << " v=" << dn.get_version();

  out << " inode=" << dn.get_linkage()->get_inode();

  if (dn.is_new()) out << " state=new";

  if (dn.get_num_ref()) {
    out << " |";
    dn.print_pin_set(out);
  }

  out << " " << &dn;
  out << "]";
  return out;
}


bool operator<(const CDentry& l, const CDentry& r)
{
  if ((l.get_dir()->ino() < r.get_dir()->ino()) ||
      (l.get_dir()->ino() == r.get_dir()->ino() &&
       (l.get_name() < r.get_name() ||
	(l.get_name() == r.get_name() && l.last < r.last))))
    return true;
  return false;
}


void CDentry::print(ostream& out)
{
  out << *this;
}


/*
inodeno_t CDentry::get_ino()
{
  if (get_inode()) 
    return get_inode()->ino();
  return inodeno_t();
}
*/

pair<int,int> CDentry::authority()
{
  return dir->authority();
}


void CDentry::add_waiter(__u64 tag, Context *c)
{
  // wait on the directory?
  if (tag & (WAIT_UNFREEZE|WAIT_SINGLEAUTH)) {
    dir->add_waiter(tag, c);
    return;
  }
  MDSCacheObject::add_waiter(tag, c);
}


version_t CDentry::pre_dirty(version_t min)
{
  projected_version = dir->pre_dirty(min);
  dout(10) << " pre_dirty " << *this << dendl;
  return projected_version;
}


void CDentry::_mark_dirty(LogSegment *ls)
{
  // state+pin
  if (!state_test(STATE_DIRTY)) {
    state_set(STATE_DIRTY);
    dir->inc_num_dirty();
    get(PIN_DIRTY);
    assert(ls);
  }
  if (ls) 
    ls->dirty_dentries.push_back(&item_dirty);
}

void CDentry::mark_dirty(version_t pv, LogSegment *ls) 
{
  dout(10) << " mark_dirty " << *this << dendl;

  // i now live in this new dir version
  assert(pv <= projected_version);
  version = pv;
  _mark_dirty(ls);

  // mark dir too
  dir->mark_dirty(pv, ls);
}


void CDentry::mark_clean() 
{
  dout(10) << " mark_clean " << *this << dendl;
  assert(is_dirty());
  assert(dir->get_version() == 0 || version <= dir->get_version());  // hmm?

  // state+pin
  state_clear(STATE_DIRTY);
  dir->dec_num_dirty();
  put(PIN_DIRTY);
  
  item_dirty.remove_myself();

  clear_new();
}    

void CDentry::mark_new() 
{
  dout(10) << " mark_new " << *this << dendl;
  state_set(STATE_NEW);
}

void CDentry::make_path_string(string& s)
{
  if (dir) {
    dir->inode->make_path_string(s);
  } else {
    s = "???";
  }
  s += "/";
  s.append(name.data(), name.length());
}

void CDentry::make_path(filepath& fp)
{
  assert(dir);
  if (dir->inode->is_base())
    fp = filepath(dir->inode->ino());               // base case
  else if (dir->inode->get_parent_dn())
    dir->inode->get_parent_dn()->make_path(fp);  // recurse
  else
    fp = filepath(dir->inode->ino());               // relative but not base?  hrm!
  fp.push_dentry(name);
}

/*
void CDentry::make_path(string& s, inodeno_t tobase)
{
  assert(dir);
  
  if (dir->inode->is_root()) {
    s += "/";  // make it an absolute path (no matter what) if we hit the root.
  } 
  else if (dir->inode->get_parent_dn() &&
	   dir->inode->ino() != tobase) {
    dir->inode->get_parent_dn()->make_path(s, tobase);
    s += "/";
  }
  s += name;
}
*/

/** make_anchor_trace
 * construct an anchor trace for this dentry, as if it were linked to *in.
 */
void CDentry::make_anchor_trace(vector<Anchor>& trace, CInode *in)
{
  // start with parent dir inode
  if (dir)
    dir->inode->make_anchor_trace(trace);

  // add this inode (in my dirfrag) to the end
  trace.push_back(Anchor(in->ino(), dir->ino(), name, 0, 0));
  dout(10) << "make_anchor_trace added " << trace.back() << dendl;
}


/*
 * we only add ourselves to remote_parents when the linkage is
 * active (no longer projected).  if the passed dnl is projected,
 * don't link in, and do that work later in pop_projected_linkage().
 */
void CDentry::link_remote(CDentry::linkage_t *dnl, CInode *in)
{
  assert(dnl->is_remote());
  assert(in->ino() == dnl->get_remote_ino());
  dnl->inode = in;

  if (dnl == &linkage)
    in->add_remote_parent(this);
}

void CDentry::unlink_remote(CDentry::linkage_t *dnl)
{
  assert(dnl->is_remote());
  assert(dnl->inode);
  
  if (dnl == &linkage)
    dnl->inode->remove_remote_parent(this);

  dnl->inode = 0;
}

void CDentry::push_projected_linkage(CInode *inode)
{
  _project_linkage()->inode = inode;
  inode->push_projected_parent(this);
}

CDentry::linkage_t *CDentry::pop_projected_linkage()
{
  assert(projected.size());
  
  linkage_t& n = projected.front();

  /*
   * the idea here is that the link_remote_inode(), link_primary_inode(), 
   * etc. calls should make linkage identical to &n (and we assert as
   * much).
   */

  if (n.remote_ino) {
    dir->link_remote_inode(this, n.remote_ino, n.remote_d_type);
    if (n.inode) {
      linkage.inode = n.inode;
      linkage.inode->add_remote_parent(this);
    }
  } else if (n.inode) {
    dir->link_primary_inode(this, n.inode);
    n.inode->pop_projected_parent();
  }

  assert(n.inode == linkage.inode);
  assert(n.remote_ino == linkage.remote_ino);
  assert(n.remote_d_type == linkage.remote_d_type);

  projected.pop_front();

  return &linkage;
}



// ----------------------------
// auth pins

bool CDentry::can_auth_pin()
{
  assert(dir);
  return dir->can_auth_pin();
}

void CDentry::auth_pin(void *by)
{
  if (auth_pins == 0)
    get(PIN_AUTHPIN);
  auth_pins++;

#ifdef MDS_AUTHPIN_SET
  auth_pin_set.insert(by);
#endif

  dout(10) << "auth_pin by " << by << " on " << *this 
	   << " now " << auth_pins << "+" << nested_auth_pins
	   << dendl;

  dir->adjust_nested_auth_pins(1, 1);
}

void CDentry::auth_unpin(void *by)
{
  auth_pins--;

#ifdef MDS_AUTHPIN_SET
  assert(auth_pin_set.count(by));
  auth_pin_set.erase(auth_pin_set.find(by));
#endif

  if (auth_pins == 0)
    put(PIN_AUTHPIN);

  dout(10) << "auth_unpin by " << by << " on " << *this
	   << " now " << auth_pins << "+" << nested_auth_pins
	   << dendl;
  assert(auth_pins >= 0);

  dir->adjust_nested_auth_pins(-1, -1);
}

void CDentry::adjust_nested_auth_pins(int by, int dirby)
{
  nested_auth_pins += by;

  dout(35) << "adjust_nested_auth_pins by " << by 
	   << " now " << auth_pins << "+" << nested_auth_pins
	   << dendl;
  assert(nested_auth_pins >= 0);

  dir->adjust_nested_auth_pins(by, dirby);
}

bool CDentry::is_frozen()
{
  return dir->is_frozen();
}


void CDentry::adjust_nested_anchors(int by)
{
  nested_anchors += by;
  dout(20) << "adjust_nested_anchors by " << by << " -> " << nested_anchors << dendl;
  assert(nested_anchors >= 0);
  dir->adjust_nested_anchors(by);
}


void CDentry::decode_replica(bufferlist::iterator& p, bool is_new)
{
  __u32 nonce;
  ::decode(nonce, p);
  replica_nonce = nonce;
  
  ::decode(first, p);

  inodeno_t rino;
  unsigned char rdtype;
  ::decode(rino, p);
  ::decode(rdtype, p);
  if (rino) {
    if (linkage.is_null())
      dir->link_remote_inode(this, rino, rdtype);
    else
      assert(linkage.is_remote() && linkage.remote_ino == rino);
  }
  
  __s32 ls;
  ::decode(ls, p);
  if (is_new)
    lock.set_state(ls);
}



// ----------------------------
// locking

void CDentry::set_object_info(MDSCacheObjectInfo &info) 
{
  info.dirfrag = dir->dirfrag();
  info.dname = name;
  info.snapid = last;
}

void CDentry::encode_lock_state(int type, bufferlist& bl)
{
  ::encode(first, bl);

  // null, ino, or remote_ino?
  char c;
  if (linkage.is_primary()) {
    c = 1;
    ::encode(c, bl);
    ::encode(linkage.get_inode()->inode.ino, bl);
  }
  else if (linkage.is_remote()) {
    c = 2;
    ::encode(c, bl);
    ::encode(linkage.get_remote_ino(), bl);
  }
  else if (linkage.is_null()) {
    // encode nothing.
  }
  else assert(0);  
}

void CDentry::decode_lock_state(int type, bufferlist& bl)
{  
  bufferlist::iterator p = bl.begin();

  snapid_t newfirst;
  ::decode(newfirst, p);

  if (!is_auth() && newfirst != first) {
    dout(10) << "decode_lock_state first " << first << " -> " << newfirst << dendl;
    assert(newfirst > first);
    first = newfirst;
  }

  if (p.end()) {
    // null
    assert(linkage.is_null());
    return;
  }

  char c;
  inodeno_t ino;
  ::decode(c, p);

  switch (c) {
  case 1:
  case 2:
    ::decode(ino, p);
    // newly linked?
    if (linkage.is_null() && !is_auth()) {
      // force trim from cache!
      dout(10) << "decode_lock_state replica dentry null -> non-null, must trim" << dendl;
      //assert(get_num_ref() == 0);
    } else {
      // verify?
      
    }
    break;
  default: 
    assert(0);
  }
}


ClientLease *CDentry::add_client_lease(client_t c, int mask) 
{
  ClientLease *l;
  if (client_lease_map.count(c))
    l = client_lease_map[c];
  else {
    if (client_lease_map.empty())
      get(PIN_CLIENTLEASE);
    l = client_lease_map[c] = new ClientLease(c, this);
  }
  
  int adding = ~l->mask & mask;
  dout(20) << " had " << l->mask << " adding " << mask 
	   << " -> " << adding
	   << " ... now " << (l->mask | mask)
	   << dendl;
  int b = 0;
  while (adding) {
    if (adding & 1) {
      SimpleLock *lock = get_lock(1 << b);
      if (lock) {
	lock->get_client_lease();
	dout(20) << "get_client_lease on " << (1 << b) << " " << *lock << dendl;
      }
    }
    b++;
    adding = adding >> 1;
  }
  l->mask |= mask;
  
  return l;
}

int CDentry::remove_client_lease(ClientLease *l, int mask, Locker *locker) 
{
  assert(l->parent == this);

  list<SimpleLock*> to_gather;

  int removing = l->mask & mask;
  dout(20) << "had " << l->mask << " removing " << mask << " -> " << removing
	   << " ... now " << (l->mask & ~mask) << dendl;
  int b = 0;
  while (removing) {
    if (removing & 1) {
      SimpleLock *lock = get_lock(1 << b);
      if (lock) {
	lock->put_client_lease();
	dout(20) << "put_client_lease on " << (1 << b) << " " << *lock << dendl;
	if (lock->get_num_client_lease() == 0 && !lock->is_stable())
	  to_gather.push_back(lock);
      }
    }
    b++;
    removing = removing >> 1;
  }

  l->mask &= ~mask;
  int rc = l->mask;

  if (rc == 0) {
    dout(20) << "removing lease for client" << l->client << dendl;
    client_lease_map.erase(l->client);
    l->item_lease.remove_myself();
    l->item_session_lease.remove_myself();
    delete l;
    if (client_lease_map.empty())
      put(PIN_CLIENTLEASE);
  }

  // do pending gathers.
  while (!to_gather.empty()) {
    locker->eval_gather(to_gather.front());
    to_gather.pop_front();
  }
   
  return rc;
}


