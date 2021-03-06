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



#ifndef __MDS_H
#define __MDS_H

#include "mdstypes.h"

#include "msg/Dispatcher.h"
#include "include/CompatSet.h"
#include "include/types.h"
#include "include/Context.h"
#include "common/DecayCounter.h"
#include "common/Logger.h"
#include "common/Mutex.h"
#include "common/Cond.h"
#include "common/Timer.h"
#include "common/LogClient.h"

#include "MDSMap.h"

#include "SessionMap.h"


enum {
  l_mds_first = 2000,
  l_mds_req,
  l_mds_reply,
  l_mds_replyl,
  l_mds_fw,
  l_mds_dir_f,
  l_mds_dir_c,
  l_mds_dir_sp,
  l_mds_dir_ffc,
  l_mds_imax,
  l_mds_i,
  l_mds_itop,
  l_mds_ibot,
  l_mds_iptail,
  l_mds_ipin,
  l_mds_iex,
  l_mds_icap,
  l_mds_cap,
  l_mds_dis,
  l_mds_t,
  l_mds_thit,
  l_mds_tfw,
  l_mds_tdis,
  l_mds_tdirf,
  l_mds_trino,
  l_mds_tlock,
  l_mds_l,
  l_mds_q,
  l_mds_popanyd,
  l_mds_popnest,
  l_mds_sm,
  l_mds_ex,
  l_mds_iexp,
  l_mds_im,
  l_mds_iim,
  l_mds_last,
};

enum {
  l_mdc_first = 3000,
  l_mdc_last,
};

enum {
  l_mdm_first = 2500,
  l_mdm_ino,
  l_mdm_inoa,
  l_mdm_inos,
  l_mdm_dir,
  l_mdm_dira,
  l_mdm_dirs,
  l_mdm_dn,
  l_mdm_dna,
  l_mdm_dns,
  l_mdm_cap,
  l_mdm_capa,
  l_mdm_caps,
  l_mdm_rss,
  l_mdm_heap,
  l_mdm_malloc,
  l_mdm_buf,
  l_mdm_last,
};



class filepath;

class MonClient;

class OSDMap;
class Objecter;
class Filer;

class Server;
class Locker;
class MDCache;
class MDLog;
class MDBalancer;

class CInode;
class CDir;
class CDentry;

class Messenger;
class Message;

class MClientRequest;
class MClientReply;

class MMDSBeacon;

class InoTable;
class SnapServer;
class SnapClient;
class AnchorServer;
class AnchorClient;

class MDSTableServer;
class MDSTableClient;

class MDS : public Dispatcher {
 public:
  Mutex        mds_lock;
  SafeTimer    timer;

  string name;
  int whoami;
  int incarnation;

  CompatSet mds_features;
  
  int standby_for_rank;
  string standby_for_name;
  int standby_replay_for;

  Messenger    *messenger;
  MonClient    *monc;
  MDSMap       *mdsmap;
  OSDMap       *osdmap;
  Objecter     *objecter;
  Filer        *filer;       // for reading/writing to/from osds
  LogClient    logclient;

  // sub systems
  Server       *server;
  MDCache      *mdcache;
  Locker       *locker;
  MDLog        *mdlog;
  MDBalancer   *balancer;

  InoTable     *inotable;

  AnchorServer *anchorserver;
  AnchorClient *anchorclient;

  SnapServer   *snapserver;
  SnapClient   *snapclient;

  MDSTableClient *get_table_client(int t);
  MDSTableServer *get_table_server(int t);

  Logger       *logger, *mlogger;


 protected:
  // -- MDS state --
  int last_state;
  int state;         // my confirmed state
  int want_state;    // the state i want

  list<Context*> waiting_for_active, waiting_for_replay, waiting_for_reconnect;
  list<Context*> replay_queue;
  map<int, list<Context*> > waiting_for_active_peer;
  list<Context*> waiting_for_nolaggy;

  map<int,version_t> peer_mdsmap_epoch;

  tid_t last_tid;    // for mds-initiated requests (e.g. stray rename)

 public:
  void wait_for_active(Context *c) { 
    waiting_for_active.push_back(c); 
  }
  void wait_for_active_peer(int who, Context *c) { 
    waiting_for_active_peer[who].push_back(c);
  }
  void wait_for_replay(Context *c) { 
    waiting_for_replay.push_back(c); 
  }
  void wait_for_reconnect(Context *c) {
    waiting_for_reconnect.push_back(c);
  }
  void enqueue_replay(Context *c) {
    replay_queue.push_back(c);
  }

  int get_state() { return state; } 
  int get_want_state() { return want_state; } 
  bool is_creating() { return state == MDSMap::STATE_CREATING; }
  bool is_starting() { return state == MDSMap::STATE_STARTING; }
  bool is_standby()  { return state == MDSMap::STATE_STANDBY; }
  bool is_replay()   { return state == MDSMap::STATE_REPLAY; }
  bool is_resolve()  { return state == MDSMap::STATE_RESOLVE; }
  bool is_reconnect() { return state == MDSMap::STATE_RECONNECT; }
  bool is_rejoin()   { return state == MDSMap::STATE_REJOIN; }
  bool is_clientreplay()   { return state == MDSMap::STATE_CLIENTREPLAY; }
  bool is_active()   { return state == MDSMap::STATE_ACTIVE; }
  bool is_stopping() { return state == MDSMap::STATE_STOPPING; }

  bool is_standby_replay()   { return state == MDSMap::STATE_STANDBY_REPLAY; }

  bool is_stopped()  { return mdsmap->is_stopped(whoami); }

  void request_state(int s);

  tid_t issue_tid() { return ++last_tid; }
    

  // -- waiters --
  list<Context*> finished_queue;

  void queue_waiter(Context *c) {
    finished_queue.push_back(c);
  }
  void queue_waiters(list<Context*>& ls) {
    finished_queue.splice( finished_queue.end(), ls );
  }
  bool queue_one_replay() {
    if (replay_queue.empty())
      return false;
    queue_waiter(replay_queue.front());
    replay_queue.pop_front();
    return true;
  }
  
  // -- keepalive beacon --
  version_t               beacon_last_seq;          // last seq sent to monitor
  map<version_t,utime_t>  beacon_seq_stamp;         // seq # -> time sent
  utime_t                 beacon_last_acked_stamp;  // last time we sent a beacon that got acked
  bool laggy;
  utime_t laggy_until;
  utime_t last_tick;

  bool is_laggy() { return laggy; }
  utime_t get_laggy_until() { return laggy_until; }

  class C_MDS_BeaconSender : public Context {
    MDS *mds;
  public:
    C_MDS_BeaconSender(MDS *m) : mds(m) {}
    void finish(int r) {
      mds->beacon_sender = 0;
      mds->beacon_send();
    }
  } *beacon_sender;
  class C_MDS_BeaconKiller : public Context {
    MDS *mds;
    utime_t lab;
  public:
    C_MDS_BeaconKiller(MDS *m, utime_t l) : mds(m), lab(l) {}
    void finish(int r) {
      if (mds->beacon_killer) {
	mds->beacon_killer = 0;
	mds->beacon_kill(lab);
      } 
      // else mds is pbly already shutting down
    }
  } *beacon_killer;

  // tick and other timer fun
  class C_MDS_Tick : public Context {
    MDS *mds;
  public:
    C_MDS_Tick(MDS *m) : mds(m) {}
    void finish(int r) {
      mds->tick_event = 0;
      mds->tick();
    }
  } *tick_event;
  void     reset_tick();

  // -- client map --
  SessionMap   sessionmap;
  epoch_t      last_client_mdsmap_bcast;
  //void log_clientmap(Context *c);


  // shutdown crap
  int req_rate;

  // ino's and fh's
 public:

  int get_req_rate() { return req_rate; }

 private:
  bool ms_dispatch(Message *m);
  bool ms_get_authorizer(int dest_type, AuthAuthorizer **authorizer, bool force_new);
  bool ms_verify_authorizer(Connection *con, int peer_type,
			       int protocol, bufferlist& authorizer_data, bufferlist& authorizer_reply,
			       bool& isvalid);


 public:
  MDS(const char *n, Messenger *m, MonClient *mc);
  ~MDS();

  // who am i etc
  int get_nodeid() { return whoami; }
  MDSMap *get_mds_map() { return mdsmap; }
  OSDMap *get_osd_map() { return osdmap; }

  void send_message_mds(Message *m, int mds);
  void forward_message_mds(Message *req, int mds);

  void send_message_client(Message *m, client_t client);
  void send_message_client(Message *m, entity_inst_t clientinst);


  // start up, shutdown
  int init();
  void reopen_logger(utime_t start);

  void bcast_mds_map();  // to mounted clients

  void boot_create();             // i am new mds.
  void boot_start(int step=0, int r=0);    // starting|replay

  void replay_start();
  void creating_done();
  void starting_done();
  void replay_done();

  void resolve_start();
  void resolve_done();
  void reconnect_start();
  void reconnect_done();
  void rejoin_joint_start();
  void rejoin_done();
  void recovery_done();
  void handle_mds_recovery(int who);
  void clientreplay_start();
  void clientreplay_done();
  void active_start();
  void stopping_start();
  void stopping_done();
  void suicide();

  void tick();
  
  void beacon_start();
  void beacon_send();
  void beacon_kill(utime_t lab);
  void handle_mds_beacon(MMDSBeacon *m);
  void reset_beacon_killer();

  // messages
  bool _dispatch(Message *m);
  
  void ms_handle_connect(Connection *con);
  bool ms_handle_reset(Connection *con);
  void ms_handle_remote_reset(Connection *con);

  // special message types
  void handle_command(class MMonCommand *m);
  void handle_mds_map(class MMDSMap *m);
};



class C_MDS_RetryMessage : public Context {
  Message *m;
  MDS *mds;
public:
  C_MDS_RetryMessage(MDS *mds, Message *m) {
    assert(m);
    this->m = m;
    this->mds = mds;
  }
  virtual void finish(int r) {
    mds->_dispatch(m);
  }
};

#endif
