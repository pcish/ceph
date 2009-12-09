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



#ifndef __CDIR_H
#define __CDIR_H

#include "include/types.h"
#include "include/buffer.h"
#include "mdstypes.h"
#include "config.h"
#include "common/DecayCounter.h"

#include <iostream>

#include <list>
#include <set>
#include <map>
#include <string>
using namespace std;


#include "CInode.h"

class CDentry;
class MDCache;
class MDCluster;
class Context;

class ObjectOperation;

ostream& operator<<(ostream& out, class CDir& dir);


class CDir : public MDSCacheObject {
private:
  static boost::pool<> pool;
public:
  static void *operator new(size_t num_bytes) { 
    return pool.malloc();
  }
  void operator delete(void *p) {
    pool.free(p);
  }

public:
  // -- pins --
  static const int PIN_DNWAITER =     1;
  static const int PIN_INOWAITER =    2;
  static const int PIN_CHILD =        3;
  static const int PIN_FROZEN =       4;
  static const int PIN_SUBTREE =      5;
  static const int PIN_IMPORTING =    7;
  static const int PIN_IMPORTBOUND =  9;
  static const int PIN_EXPORTBOUND = 10;
  static const int PIN_STICKY =      11;
  static const int PIN_SUBTREETEMP = 12;  // used by MDCache::trim_non_auth()
  const char *pin_name(int p) {
    switch (p) {
    case PIN_DNWAITER: return "dnwaiter";
    case PIN_INOWAITER: return "inowaiter";
    case PIN_CHILD: return "child";
    case PIN_FROZEN: return "frozen";
    case PIN_SUBTREE: return "subtree";
    case PIN_IMPORTING: return "importing";
    case PIN_IMPORTBOUND: return "importbound";
    case PIN_EXPORTBOUND: return "exportbound";
    case PIN_STICKY: return "sticky";
    case PIN_SUBTREETEMP: return "subtreetemp";
    default: return generic_pin_name(p);
    }
  }

  // -- state --
  static const unsigned STATE_COMPLETE =      (1<< 1);   // the complete contents are in cache
  static const unsigned STATE_FROZENTREE =    (1<< 2);   // root of tree (bounded by exports)
  static const unsigned STATE_FREEZINGTREE =  (1<< 3);   // in process of freezing 
  static const unsigned STATE_FROZENDIR =     (1<< 4);
  static const unsigned STATE_FREEZINGDIR =   (1<< 5);
  static const unsigned STATE_COMMITTING =    (1<< 6);   // mid-commit
  static const unsigned STATE_FETCHING =      (1<< 7);   // currenting fetching
  static const unsigned STATE_IMPORTBOUND =   (1<<10);
  static const unsigned STATE_EXPORTBOUND =   (1<<11);
  static const unsigned STATE_EXPORTING =     (1<<12);
  static const unsigned STATE_IMPORTING =     (1<<13);
  static const unsigned STATE_FRAGMENTING =   (1<<14);
  static const unsigned STATE_STICKY =        (1<<15);  // sticky pin due to inode stickydirs
  static const unsigned STATE_DNPINNEDFRAG =  (1<<16);  // dir is refragmenting

  // common states
  static const unsigned STATE_CLEAN =  0;
  static const unsigned STATE_INITIAL = 0;

  // these state bits are preserved by an import/export
  // ...except if the directory is hashed, in which case none of them are!
  static const unsigned MASK_STATE_EXPORTED = 
  (STATE_COMPLETE|STATE_DIRTY);
  static const unsigned MASK_STATE_IMPORT_KEPT = 
  (						  
   STATE_IMPORTING
   |STATE_IMPORTBOUND|STATE_EXPORTBOUND
   |STATE_FROZENTREE
   |STATE_STICKY);
  static const unsigned MASK_STATE_EXPORT_KEPT = 
  (STATE_EXPORTING
   |STATE_IMPORTBOUND|STATE_EXPORTBOUND
   |STATE_FROZENTREE
   |STATE_FROZENDIR
   |STATE_STICKY);
  static const unsigned MASK_STATE_FRAGMENT_KEPT = 
  (STATE_DIRTY |
   STATE_COMPLETE |
   STATE_EXPORTBOUND |
   STATE_IMPORTBOUND);

  // -- rep spec --
  static const int REP_NONE =     0;
  static const int REP_ALL =      1;
  static const int REP_LIST =     2;


  static const int NONCE_EXPORT  = 1;


  // -- wait masks --
  static const __u64 WAIT_DENTRY       = (1<<0);  // wait for item to be in cache
  static const __u64 WAIT_COMPLETE     = (1<<1);  // wait for complete dir contents
  static const __u64 WAIT_FROZEN       = (1<<2);  // auth pins removed

  static const int WAIT_DNLOCK_OFFSET = 4;

  static const __u64 WAIT_ANY_MASK  = (0xffffffff);
  static const __u64 WAIT_ATFREEZEROOT = (WAIT_UNFREEZE);
  static const __u64 WAIT_ATSUBTREEROOT = (WAIT_SINGLEAUTH);




 public:
  // context
  MDCache  *cache;

  CInode          *inode;  // my inode
  frag_t           frag;   // my frag

  bool is_lt(const MDSCacheObject *r) const {
    return dirfrag() < ((const CDir*)r)->dirfrag();
  }

  fnode_t fnode;
  snapid_t first;
  map<snapid_t,old_rstat_t> dirty_old_rstat;  // [value.first,key]

protected:
  version_t projected_version;
  list<fnode_t*> projected_fnode;

  xlist<CDir*>::item xlist_dirty, xlist_new;


public:
  version_t get_version() { return fnode.version; }
  void set_version(version_t v) { 
    assert(projected_fnode.empty());
    projected_version = fnode.version = v; 
  }
  version_t get_projected_version() { return projected_version; }

  fnode_t *get_projected_fnode() {
    if (projected_fnode.empty())
      return &fnode;
    else
      return projected_fnode.back();
  }
  fnode_t *project_fnode();

  void pop_and_dirty_projected_fnode(LogSegment *ls);
  bool is_projected() { return get_projected_version() > get_version(); }
  version_t pre_dirty(version_t min=0);
  void _mark_dirty(LogSegment *ls);
  void _set_dirty_flag() {
    if (!state_test(STATE_DIRTY)) {
      state_set(STATE_DIRTY);
      get(PIN_DIRTY);
    }
  }
  void mark_dirty(version_t pv, LogSegment *ls);
  void log_mark_dirty();
  void mark_clean();

  void mark_new(LogSegment *ls);

public:
  typedef map<dentry_key_t, CDentry*> map_t;
protected:

  // contents
  map_t items;       // non-null AND null
  unsigned num_head_items;
  unsigned num_head_null;
  unsigned num_snap_items;
  unsigned num_snap_null;

  int num_dirty;

  // state
  version_t committing_version;
  version_t committed_version;


  // lock nesting, freeze
  int auth_pins;
#ifdef MDS_AUTHPIN_SET
  multiset<void*> auth_pin_set;
#endif
  int nested_auth_pins, dir_auth_pins;
  int request_pins;

  int nested_anchors;

  // cache control  (defined for authority; hints for replicas)
  __s32      dir_rep;
  set<__s32> dir_rep_by;      // if dir_rep == REP_LIST

  // popularity
  dirfrag_load_vec_t pop_me;
  dirfrag_load_vec_t pop_nested;
  dirfrag_load_vec_t pop_auth_subtree;
  dirfrag_load_vec_t pop_auth_subtree_nested;
 
  utime_t last_popularity_sample;

  load_spread_t pop_spread;

  // and to provide density
  int num_dentries_nested;
  int num_dentries_auth_subtree;
  int num_dentries_auth_subtree_nested;


  // friends
  friend class Migrator;
  friend class CInode;
  friend class MDCache;
  friend class MDiscover;
  friend class MDBalancer;

  friend class CDirDiscover;
  friend class CDirExport;

 public:
  CDir(CInode *in, frag_t fg, MDCache *mdcache, bool auth);
  ~CDir() {
    g_num_dir--;
    g_num_dirs++;
  }



  // -- accessors --
  inodeno_t ino()     const { return inode->ino(); }          // deprecate me?
  frag_t    get_frag()    const { return frag; }
  dirfrag_t dirfrag() const { return dirfrag_t(inode->ino(), frag); }

  CInode *get_inode()    { return inode; }
  CDir *get_parent_dir() { return inode->get_parent_dir(); }

  map_t::iterator begin() { return items.begin(); }
  map_t::iterator end() { return items.end(); }

  unsigned get_num_head_items() { return num_head_items; }
  unsigned get_num_head_null() { return num_head_null; }
  unsigned get_num_snap_items() { return num_snap_items; }
  unsigned get_num_snap_null() { return num_snap_null; }
  unsigned get_num_any() { return num_head_items + num_head_null + num_snap_items + num_snap_null; }
  
  void inc_num_dirty() { num_dirty++; }
  void dec_num_dirty() { 
    assert(num_dirty > 0);
    num_dirty--; 
  }
  int get_num_dirty() {
    return num_dirty;
  }


  // -- dentries and inodes --
 public:
  CDentry* lookup_exact_snap(const nstring& dname, snapid_t last) {
    map_t::iterator p = items.find(dentry_key_t(last, dname.c_str()));
    if (p == items.end())
      return NULL;
    return p->second;
  }
  CDentry* lookup(const string& n, snapid_t snap=CEPH_NOSNAP) {
    return lookup(n.c_str(), snap);
  }
  CDentry* lookup(const nstring& ns, snapid_t snap=CEPH_NOSNAP) {
    return lookup(ns.c_str(), snap);
  }
  CDentry* lookup(const char *n, snapid_t snap=CEPH_NOSNAP);

  CDentry* add_null_dentry(const nstring& dname, 
			   snapid_t first=2, snapid_t last=CEPH_NOSNAP);
  CDentry* add_primary_dentry(const nstring& dname, CInode *in, 
			      snapid_t first=2, snapid_t last=CEPH_NOSNAP);
  CDentry* add_remote_dentry(const nstring& dname, inodeno_t ino, unsigned char d_type, 
			     snapid_t first=2, snapid_t last=CEPH_NOSNAP);
  void remove_dentry( CDentry *dn );         // delete dentry
  void link_remote_inode( CDentry *dn, inodeno_t ino, unsigned char d_type);
  void link_remote_inode( CDentry *dn, CInode *in );
  void link_primary_inode( CDentry *dn, CInode *in );
  void unlink_inode( CDentry *dn );
  void try_remove_unlinked_dn(CDentry *dn);
private:
  void link_inode_work( CDentry *dn, CInode *in );
  void unlink_inode_work( CDentry *dn );
  void remove_null_dentries();
  void purge_stale_snap_data(const set<snapid_t>& snaps);

public:
  void split(int bits, list<CDir*>& subs, list<Context*>& waiters, bool replay);
  void merge(int bits, list<Context*>& waiters, bool replay);
private:
  void steal_dentry(CDentry *dn);  // from another dir.  used by merge/split.
  void purge_stolen(list<Context*>& waiters, bool replay);
  void init_fragment_pins();


  // -- authority --
  /*
   *     normal: <parent,unknown>   !subtree_root
   * delegation: <mds,unknown>       subtree_root
   *  ambiguous: <mds1,mds2>         subtree_root
   *             <parent,mds2>       subtree_root     
   */
  pair<int,int> dir_auth;

 public:
  pair<int,int> authority();
  pair<int,int> get_dir_auth() { return dir_auth; }
  void set_dir_auth(pair<int,int> a);
  void set_dir_auth(int a) { set_dir_auth(pair<int,int>(a, CDIR_AUTH_UNKNOWN)); }
  bool is_ambiguous_dir_auth() {
    return dir_auth.second != CDIR_AUTH_UNKNOWN;
  }
  bool is_full_dir_auth() {
    return is_auth() && !is_ambiguous_dir_auth();
  }
  bool is_full_dir_nonauth() {
    return !is_auth() && !is_ambiguous_dir_auth();
  }
  
  bool is_subtree_root() {
    return dir_auth != CDIR_AUTH_DEFAULT;
  }

  bool contains(CDir *x);  // true if we are x or an ancestor of x 


  // for giving to clients
  void get_dist_spec(set<int>& ls, int auth) {
    if (is_rep()) {
      for (map<int,int>::iterator p = replicas_begin();
	   p != replicas_end(); 
	   ++p)
	ls.insert(p->first);
      if (!ls.empty()) 
	ls.insert(auth);
    }
  }
  void encode_dirstat(bufferlist& bl, int whoami) {
    /*
     * note: encoding matches struct ceph_client_reply_dirfrag
     */
    frag_t frag = get_frag();
    __s32 auth;
    set<__s32> dist;
    
    auth = dir_auth.first;
    if (is_auth()) 
      get_dist_spec(dist, whoami);

    ::encode(frag, bl);
    ::encode(auth, bl);
    ::encode(dist, bl);
  }

  void encode_replica(int who, bufferlist& bl) {
    __u32 nonce = add_replica(who);
    ::encode(nonce, bl);
    ::encode(first, bl);
    ::encode(dir_rep, bl);
    ::encode(dir_rep_by, bl);
  }
  void decode_replica(bufferlist::iterator& p) {
    __u32 nonce;
    ::decode(nonce, p);
    replica_nonce = nonce;
    ::decode(first, p);
    ::decode(dir_rep, p);
    ::decode(dir_rep_by, p);
  }



  // -- state --
  bool is_complete() { return state & STATE_COMPLETE; }
  bool is_exporting() { return state & STATE_EXPORTING; }
  bool is_importing() { return state & STATE_IMPORTING; }

  int get_dir_rep() { return dir_rep; }
  bool is_rep() { 
    if (dir_rep == REP_NONE) return false;
    return true;
  }
 
  // -- fetch --
  object_t get_ondisk_object() { 
    return file_object_t(ino(), frag);
  }
  void fetch(Context *c, bool ignore_authpinnability=false);
  void _fetched(bufferlist &bl);

  // -- commit --
  map<version_t, list<Context*> > waiting_for_commit;

  void commit_to(version_t want);
  void commit(version_t want, Context *c);
  void _commit(version_t want);
  void _commit_full(ObjectOperation& m, const set<snapid_t> *snaps);
  void _commit_partial(ObjectOperation& m, const set<snapid_t> *snaps);
  void _encode_dentry(CDentry *dn, bufferlist& bl, const set<snapid_t> *snaps);
  void _committed(version_t v, version_t last_renamed_version);
  void wait_for_commit(Context *c, version_t v=0);

  // -- dirtyness --
  version_t get_committing_version() { return committing_version; }
  version_t get_committed_version() { return committed_version; }
  void set_committed_version(version_t v) { committed_version = v; }

  void mark_complete() { state_set(STATE_COMPLETE); }


  // -- reference counting --
  void first_get();
  void last_put();

  void request_pin_get() {
    if (request_pins == 0) get(PIN_REQUEST);
    request_pins++;
  }
  void request_pin_put() {
    request_pins--;
    if (request_pins == 0) put(PIN_REQUEST);
  }

    
  // -- waiters --
protected:
  map< string_snap_t, list<Context*> > waiting_on_dentry;
  map< inodeno_t, list<Context*> > waiting_on_ino;

public:
  bool is_waiting_for_dentry(const char *dname, snapid_t snap) {
    return waiting_on_dentry.count(string_snap_t(dname, snap));
  }
  void add_dentry_waiter(const nstring& dentry, snapid_t snap, Context *c);
  void take_dentry_waiting(const nstring& dentry, snapid_t first, snapid_t last, list<Context*>& ls);

  bool is_waiting_for_ino(inodeno_t ino) {
    return waiting_on_ino.count(ino);
  }
  void add_ino_waiter(inodeno_t ino, Context *c);
  void take_ino_waiting(inodeno_t ino, list<Context*>& ls);

  void take_sub_waiting(list<Context*>& ls);  // dentry or ino

  void add_waiter(__u64 mask, Context *c);
  void take_waiting(__u64 mask, list<Context*>& ls);  // may include dentry waiters
  void finish_waiting(__u64 mask, int result = 0);    // ditto
  

  // -- import/export --
  void encode_export(bufferlist& bl);
  void finish_export(utime_t now);
  void abort_export() { 
    put(PIN_TEMPEXPORTING);
  }
  void decode_import(bufferlist::iterator& blp);


  // -- auth pins --
  bool can_auth_pin() { return is_auth() && !(is_frozen() || is_freezing()); }
  int is_auth_pinned() { return auth_pins; }
  int get_cum_auth_pins() { return auth_pins + nested_auth_pins; }
  int get_auth_pins() { return auth_pins; }
  int get_nested_auth_pins() { return nested_auth_pins; }
  int get_dir_auth_pins() { return dir_auth_pins; }
  void auth_pin(void *who);
  void auth_unpin(void *who);

  void adjust_nested_auth_pins(int inc, int dirinc);
  void verify_fragstat();

  int get_nested_anchors() { return nested_anchors; }
  void adjust_nested_anchors(int by);

  // -- freezing --
  bool freeze_tree();
  void _freeze_tree();
  void unfreeze_tree();

  bool freeze_dir();
  void _freeze_dir();
  void unfreeze_dir();

  void maybe_finish_freeze() {
    if (auth_pins != 1 ||
	dir_auth_pins != 0)
      return;
    // we can freeze the _dir_ even with nested pins...
    if (state_test(STATE_FREEZINGDIR)) {
      _freeze_dir();
      auth_unpin(this);
      finish_waiting(WAIT_FROZEN);
    }
    if (nested_auth_pins != 0) 
      return;
    if (state_test(STATE_FREEZINGTREE)) {
      _freeze_tree();
      auth_unpin(this);
      finish_waiting(WAIT_FROZEN);
    }
  }

  bool is_freezing() { return is_freezing_tree() || is_freezing_dir(); }
  bool is_freezing_tree();
  bool is_freezing_tree_root() { return state & STATE_FREEZINGTREE; }
  bool is_freezing_dir() { return state & STATE_FREEZINGDIR; }

  bool is_frozen() { return is_frozen_dir() || is_frozen_tree(); }
  bool is_frozen_tree();
  bool is_frozen_tree_root() { return state & STATE_FROZENTREE; }
  bool is_frozen_dir() { return state & STATE_FROZENDIR; }
  
  bool is_freezeable(bool freezing=false) {
    // no nested auth pins.
    if ((auth_pins-freezing) > 0 || nested_auth_pins > 0) 
      return false;

    // inode must not be frozen.
    if (!is_subtree_root() && inode->is_frozen())
      return false;

    return true;
  }
  bool is_freezeable_dir(bool freezing=false) {
    if ((auth_pins-freezing) > 0 || dir_auth_pins > 0) 
      return false;

    // if not subtree root, inode must not be frozen (tree--frozen_dir is okay).
    if (!is_subtree_root() && inode->is_frozen() && !inode->is_frozen_dir())
      return false;

    return true;
  }

  CDir *get_frozen_tree_root();


  ostream& print_db_line_prefix(ostream& out);
  void print(ostream& out);
};

#endif