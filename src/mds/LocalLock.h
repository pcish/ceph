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


#ifndef __LOCALLOCK_H
#define __LOCALLOCK_H

#include "SimpleLock.h"

class LocalLock : public SimpleLock {
public:
  client_t last_wrlock_client;
  
  LocalLock(MDSCacheObject *o, LockType *t) : 
    SimpleLock(o, t) {
    set_state(LOCK_LOCK); // always.
  }

  bool can_wrlock() {
    return !is_xlocked();
  }
  void get_wrlock(client_t client) {
    assert(can_wrlock());
    SimpleLock::get_wrlock();
    last_wrlock_client = client;
  }
  void put_wrlock() {
    SimpleLock::put_wrlock();
    if (get_num_wrlocks() == 0)
      last_wrlock_client = client_t();
  }
  client_t get_last_wrlock_client() { return last_wrlock_client; }
  
  virtual void print(ostream& out) {
    out << "(";
    _print(out);
    if (last_wrlock_client >= 0)
      out << " last_client=" << last_wrlock_client;
    out << ")";
  }
};


#endif
