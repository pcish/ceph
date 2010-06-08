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

#ifndef __LOGCLIENT_H
#define __LOGCLIENT_H

#include "msg/Dispatcher.h"

#include "common/Mutex.h"
#include "include/LogEntry.h"

#include <sstream>

class Messenger;
class MLog;
class MLogAck;
class MonMap;


class LogClient : public Dispatcher {
  Messenger *messenger;
  MonMap *monmap;
  int mon;

  bool ms_dispatch(Message *m);
  bool is_synchronous;
  void _send_log();

  void ms_handle_connect(Connection *con);

  bool ms_handle_reset(Connection *con) { return false; }
  void ms_handle_remote_reset(Connection *con) {}


 public:

  // -- log --
  Mutex log_lock;
  deque<LogEntry> log_queue;
  version_t last_log;

  void log(log_type type, const char *s);
  void log(log_type type, string& s);
  void log(log_type type, stringstream& s);
  void send_log();
  void handle_log_ack(MLogAck *m);
  void set_synchronous(bool sync) { is_synchronous = sync; }
  void set_mon(int mon_id) { mon = mon_id; }

  LogClient(Messenger *m, MonMap *mm) : 
    messenger(m), monmap(mm), mon(-1), is_synchronous(false),
    log_lock("LogClient::log_lock"), last_log(0) { }
};

#endif
