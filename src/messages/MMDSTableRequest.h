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


#ifndef __MMDSTABLEREQUEST_H
#define __MMDSTABLEREQUEST_H

#include "msg/Message.h"
#include "mds/mds_table_types.h"

class MMDSTableRequest : public Message {
 public:
  __u16 table;
  __s16 op;
  __u64 reqid;
  version_t tid;
  bufferlist bl;

  MMDSTableRequest() {}
  MMDSTableRequest(int tab, int o, __u64 r, version_t v=0) : 
    Message(MSG_MDS_TABLE_REQUEST),
    table(tab), op(o), reqid(r), tid(v) { }
  
  virtual const char *get_type_name() { return "mds_table_request"; }
  void print(ostream& o) {
    o << "mds_table_request(" << get_mdstable_name(table)
      << " " << get_mdstableserver_opname(op);
    if (reqid) o << " " << reqid;
    if (tid) o << " tid " << tid;
    if (bl.length()) o << " " << bl.length() << " bytes";
    o << ")";
  }

  virtual void decode_payload() {
    bufferlist::iterator p = payload.begin();
    ::decode(table, p);
    ::decode(op, p);
    ::decode(reqid, p);
    ::decode(tid, p);
    ::decode(bl, p);
  }

  virtual void encode_payload() {
    ::encode(table, payload);
    ::encode(op, payload);
    ::encode(reqid, payload);
    ::encode(tid, payload);
    ::encode(bl, payload);
  }
};

#endif