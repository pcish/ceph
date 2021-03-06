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

#ifndef __MMONCOMMANDACK_H
#define __MMONCOMMANDACK_H

#include "messages/PaxosServiceMessage.h"

class MMonCommandAck : public PaxosServiceMessage {
 public:
  vector<string> cmd;
  __s32 r;
  string rs;
  
  MMonCommandAck() : PaxosServiceMessage(MSG_MON_COMMAND_ACK, 0) {}
  MMonCommandAck(vector<string>& c, int _r, string s, version_t v) : 
    PaxosServiceMessage(MSG_MON_COMMAND_ACK, v),
    cmd(c), r(_r), rs(s) { }
  
  const char *get_type_name() { return "mon_command"; }
  void print(ostream& o) {
    o << "mon_command_ack(" << cmd << "=" << r << " " << rs << " v" << version << ")";
  }
  
  void encode_payload() {
    paxos_encode();
    ::encode(r, payload);
    ::encode(rs, payload);
    ::encode(cmd, payload);
  }
  void decode_payload() {
    bufferlist::iterator p = payload.begin();
    paxos_decode(p);
    ::decode(r, p);
    ::decode(rs, p);
    ::decode(cmd, p);
  }
};

#endif
