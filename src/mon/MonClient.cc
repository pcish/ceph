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

#include "msg/SimpleMessenger.h"
#include "messages/MMonGetMap.h"
#include "messages/MMonMap.h"
#include "messages/MAuth.h"
#include "messages/MAuthReply.h"

#include "messages/MMonSubscribe.h"
#include "messages/MMonSubscribeAck.h"
#include "common/ConfUtils.h"

#include "MonClient.h"
#include "MonMap.h"

#include "auth/Auth.h"
#include "auth/KeyRing.h"

#include "include/str_list.h"

#include "config.h"


#define DOUT_SUBSYS monc
#undef dout_prefix
#define dout_prefix *_dout << dbeginl << "monclient" << (hunting ? "(hunting)":"") << ": "


/*
 * build an initial monmap with any known monitor
 * addresses.
 */
int MonClient::build_initial_monmap()
{
  dout(10) << "build_initial_monmap" << dendl;

  // file?
  if (g_conf.monmap) {
    const char *monmap_fn = g_conf.monmap;
    int r = monmap.read(monmap_fn);
    if (r >= 0)
      return 0;
    char buf[80];
    cerr << "unable to read monmap from " << monmap_fn << ": " << strerror_r(errno, buf, sizeof(buf)) << std::endl;
  }

  // -m foo?
  if (g_conf.mon_host) {
    vector<entity_addr_t> addrs;
    if (parse_ip_port_vec(g_conf.mon_host, addrs)) {
      for (unsigned i=0; i<addrs.size(); i++) {
	entity_inst_t inst;
	inst.name = entity_name_t::MON(i);
	inst.addr = addrs[i];
	monmap.add_mon(inst);
      }
      return 0;
    }
    cerr << "unable to parse addrs in '" << g_conf.mon_host << "'" << std::endl;
  }

  // config file?
  string conf = g_conf.conf;
  list<string> ls;
  get_str_list(conf, ls);
  if (!ls.empty()) {
    for (list<string>::iterator p = ls.begin(); p != ls.end(); p++) {
      ConfFile c(p->c_str());
      if (c.parse()) {
	static string monstr;
	for (int i=0; i<15; i++) {
	  char monname[20];
	  char *val = 0;
	  snprintf(monname, sizeof(monname), "mon%d", i);
	  c.read(monname, "mon addr", &val, 0);
	  if (!val || !val[0])
	    break;
	  
	  entity_inst_t inst;
	  if (!parse_ip_port(val, inst.addr)) {
	    cerr << "unable to parse conf's addr for " << monname << " (" << val << ")" << std::endl;
	    continue;
	  }
	  inst.name = entity_name_t::MON(monmap.mon_inst.size());
	  monmap.add_mon(inst);
	}
	break;
      }
    }
    if (monmap.size())
      return 0;
    cerr << "unable to find any monitors in conf" << std::endl;
    return -EINVAL;
  }

  cerr << "please specify monitors via -m monaddr or -c ceph.conf" << std::endl;
  return -ENOENT;
}


int MonClient::get_monmap()
{
  dout(10) << "get_monmap" << dendl;
  Mutex::Locker l(monc_lock);
  
  _sub_want("monmap", monmap.get_epoch());
  if (cur_mon < 0)
    _reopen_session();

  while (want_monmap)
    map_cond.Wait(monc_lock);

  dout(10) << "get_monmap done" << dendl;
  return 0;
}

int MonClient::get_monmap_privately()
{
  dout(10) << "get_monmap_privately" << dendl;
  Mutex::Locker l(monc_lock);
  
  bool temp_msgr = false;
  SimpleMessenger* smessenger = NULL;
  if (!messenger) {
    messenger = smessenger = new SimpleMessenger();
    smessenger->register_entity(entity_name_t::CLIENT(-1));
    messenger->add_dispatcher_head(this);
    smessenger->start(true);  // do not daemonize!
    temp_msgr = true; 
  }
  
  int attempt = 10;
  int i = 0;
  
  dout(10) << "have " << monmap.epoch << dendl;
  
  while (monmap.epoch == 0) {
    i = rand() % monmap.mon_inst.size();
    dout(10) << "querying " << monmap.mon_inst[i] << dendl;
    messenger->send_message(new MMonGetMap, monmap.mon_inst[i]);
    
    if (--attempt == 0)
      break;
    
    utime_t interval(1, 0);
    map_cond.WaitInterval(monc_lock, interval);
  }

  if (temp_msgr) {
    monc_lock.Unlock();
    messenger->shutdown();
    smessenger->wait();
    messenger->destroy();
    messenger = 0;
    monc_lock.Lock();
  }
 
  hunting = true;  // reset this to true!

  if (monmap.epoch)
    return 0;
  return -1;
}


bool MonClient::ms_dispatch(Message *m)
{
  if (my_addr == entity_addr_t())
    my_addr = messenger->get_myaddr();

  switch (m->get_type()) {
  case CEPH_MSG_MON_MAP:
    handle_monmap((MMonMap*)m);
    return true;

  case CEPH_MSG_AUTH_REPLY:
    handle_auth((MAuthReply*)m);
    return true;

  case CEPH_MSG_MON_SUBSCRIBE_ACK:
    handle_subscribe_ack((MMonSubscribeAck*)m);
    return true;
  }


  return false;
}

void MonClient::handle_monmap(MMonMap *m)
{
  dout(10) << "handle_monmap " << *m << dendl;
  monc_lock.Lock();
  
  bufferlist::iterator p = m->monmapbl.begin();
  ::decode(monmap, p);
  if (cur_mon >= monmap.size() || monmap.get_inst(cur_mon).addr != cur_mon_addr)
  {
    for (unsigned i=0; i<monmap.size(); i++)
    {
      if (cur_mon_addr == monmap.get_inst(i).addr)
      	_set_cur_mon(i);
    }
    if (i >= monmap.size()) // can't find the mon we were talking to
    {
      cur_mon = -1; // so _pick_new_mon won't mark some random monitor as down
      _pick_new_mon();
    }
  }
  dout(10) << " got monmap " << monmap.epoch << dendl;

  _sub_got("monmap", monmap.get_epoch());

  map_cond.Signal();
  want_monmap = false;

  if (cur_mon >= 0)
    _finish_hunting();

  monc_lock.Unlock();
  m->put();
}

// ----------------------

void MonClient::init()
{
  dout(10) << "init" << dendl;
  messenger->add_dispatcher_head(this);

  entity_name = *g_conf.entity_name;
  
  Mutex::Locker l(monc_lock);
  timer.add_event_after(10.0, new C_Tick(this));

  // seed rng so we choose a different monitor each time
  srand(getpid());

  auth_supported.clear();
  string str = g_conf.supported_auth;
  list<string> sup_list;
  get_str_list(str, sup_list);
  for (list<string>::iterator iter = sup_list.begin(); iter != sup_list.end(); ++iter) {
    if (iter->compare("cephx") == 0) {
      dout(10) << "supporting cephx auth protocol" << dendl;
      auth_supported.insert(CEPH_AUTH_CEPHX);
    } else if (iter->compare("none") == 0) {
      auth_supported.insert(CEPH_AUTH_NONE);
      dout(10) << "supporting *none* auth protocol" << dendl;
    } else {
      dout(0) << "WARNING: unknown auth protocol defined: " << *iter << dendl;
    }
  }
}

void MonClient::shutdown()
{
  timer.cancel_all_events();
}

int MonClient::authenticate(double timeout)
{
  Mutex::Locker lock(monc_lock);

  if (state == MC_STATE_HAVE_SESSION) {
    dout(5) << "already authenticated" << dendl;;
    return 0;
  }

  _sub_want("monmap", monmap.get_epoch());
  if (cur_mon < 0)
    _reopen_session();

  utime_t until = g_clock.now();
  until += timeout;
  if (timeout > 0.0)
    dout(10) << "authenticate will time out at " << until << dendl;
  while (state != MC_STATE_HAVE_SESSION && !authenticate_err) {
    if (timeout > 0.0) {
      int r = auth_cond.WaitUntil(monc_lock, until);
      if (r == ETIMEDOUT) {
	dout(0) << "authenticate timed out after " << timeout << dendl;
	authenticate_err = -r;
      }
    } else
      auth_cond.Wait(monc_lock);
  }

  if (state == MC_STATE_HAVE_SESSION) {
    dout(5) << "authenticate success, global_id " << global_id << dendl;
  }

  return authenticate_err;
}

void MonClient::handle_auth(MAuthReply *m)
{
  bufferlist::iterator p = m->result_bl.begin();
  if (state == MC_STATE_NEGOTIATING) {
    if (!auth || (int)m->protocol != auth->get_protocol()) {
      delete auth;
      auth = get_auth_client_handler(m->protocol, rotating_secrets);
      if (!auth) {
	m->put();
	return;
      }
      auth->set_want_keys(want_keys);
      auth->init(entity_name);
      auth->set_global_id(global_id);
    } else {
      auth->reset();
    }
    state = MC_STATE_AUTHENTICATING;
  }
  assert(auth);
  if (m->global_id && m->global_id != global_id) {
    global_id = m->global_id;
    auth->set_global_id(global_id);
    dout(10) << "my global_id is " << m->global_id << dendl;
  }

  int ret = auth->handle_response(m->result, p);
  m->put();

  if (ret == -EAGAIN) {
    MAuth *m = new MAuth;
    m->protocol = auth->get_protocol();
    ret = auth->build_request(m->auth_payload);
    _send_mon_message(m, true);
    return;
  }

  _finish_hunting();

  authenticate_err = ret;
  if (ret == 0) {
    if (state != MC_STATE_HAVE_SESSION) {
      state = MC_STATE_HAVE_SESSION;
      while (!waiting_for_session.empty()) {
	_send_mon_message(waiting_for_session.front());
	waiting_for_session.pop_front();
      }
    }
  
    _check_auth_tickets();
  }
  auth_cond.SignalAll();
}


// ---------

void MonClient::_send_mon_message(Message *m, bool force)
{
  assert(cur_mon >= 0);
  if (force || state == MC_STATE_HAVE_SESSION) {
    messenger->send_message(m, monmap.mon_inst[cur_mon]);
  } else {
    waiting_for_session.push_back(m);
  }
}

void MonClient::_pick_new_mon()
{
  if (cur_mon >= 0)
    messenger->mark_down(monmap.get_inst(cur_mon).addr);

  if (cur_mon >= 0 && monmap.size() > 1) {
    // pick a _different_ mon
    int n = rand() % (monmap.size() - 1);
    if (n >= cur_mon)
      n++;
    _set_cur_mon(n);
  } else {
    _set_cur_mon(rand() % monmap.size());
  }
  dout(10) << "_pick_new_mon picked mon" << cur_mon << dendl;
}

inline void MonClient::_set_cur_mon(int new_mon)
{
  cur_mon = new_mon;
  cur_mon_addr = monmap.get_inst(cur_mon).addr;
}

void MonClient::_reopen_session()
{
  dout(10) << "_reopen_session" << dendl;

  _pick_new_mon();

  // throw out old queued messages
  while (!waiting_for_session.empty()) {
    waiting_for_session.front()->put();
    waiting_for_session.pop_front();
  }

  // restart authentication handshake
  state = MC_STATE_NEGOTIATING;

  MAuth *m = new MAuth;
  m->protocol = 0;
  __u8 struct_v = 1;
  ::encode(struct_v, m->auth_payload);
  ::encode(auth_supported, m->auth_payload);
  ::encode(entity_name, m->auth_payload);
  ::encode(global_id, m->auth_payload);
  _send_mon_message(m, true);

  if (!sub_have.empty())
    _renew_subs();
}


bool MonClient::ms_handle_reset(Connection *con)
{
  Mutex::Locker lock(monc_lock);

  if (con->get_peer_type() == CEPH_ENTITY_TYPE_MON) {
    if (con->get_peer_addr() != monmap.get_inst(cur_mon).addr) {
      dout(10) << "ms_handle_reset stray mon " << con->get_peer_addr() << dendl;
      return true;
    } else {
      dout(10) << "ms_handle_reset current mon " << con->get_peer_addr() << dendl;
      if (hunting)
	return true;
      
      dout(0) << "hunting for new mon" << dendl;
      hunting = true;
      _reopen_session();
    }
  }
  return false;
}

void MonClient::_finish_hunting()
{
  if (hunting) {
    dout(0) << "found mon" << cur_mon << dendl; 
    hunting = false;
  }
}

void MonClient::tick()
{
  dout(10) << "tick" << dendl;

  _check_auth_tickets();
  
  if (hunting) {
    dout(1) << "continuing hunt" << dendl;
    _reopen_session();
  } else if (cur_mon >= 0) {
    // just renew as needed
    utime_t now = g_clock.now();
    if (now > sub_renew_after)
      _renew_subs();

    messenger->send_keepalive(monmap.mon_inst[cur_mon]);
  }

  if (auth)
    auth->tick();

  timer.add_event_after(10.0, new C_Tick(this));
}


// ---------

void MonClient::_renew_subs()
{
  if (sub_have.empty()) {
    dout(10) << "renew_subs - empty" << dendl;
    return;
  }

  dout(10) << "renew_subs" << dendl;
  if (cur_mon < 0)
    _reopen_session();
  else {
    if (sub_renew_sent == utime_t())
      sub_renew_sent = g_clock.now();

    MMonSubscribe *m = new MMonSubscribe;
    m->what = sub_have;
    _send_mon_message(m);
  }
}

void MonClient::handle_subscribe_ack(MMonSubscribeAck *m)
{
  _finish_hunting();

  if (sub_renew_sent != utime_t()) {
    sub_renew_after = sub_renew_sent;
    sub_renew_after += m->interval / 2.0;
    dout(10) << "handle_subscribe_ack sent " << sub_renew_sent << " renew after " << sub_renew_after << dendl;
    sub_renew_sent = utime_t();
  } else {
    dout(10) << "handle_subscribe_ack sent " << sub_renew_sent << ", ignoring" << dendl;
  }

  m->put();
}

int MonClient::_check_auth_tickets()
{
  if (state == MC_STATE_HAVE_SESSION && auth) {
    if (auth->need_tickets()) {
      dout(10) << "_check_auth_tickets getting new tickets!" << dendl;
      MAuth *m = new MAuth;
      m->protocol = auth->get_protocol();
      auth->build_request(m->auth_payload);
      _send_mon_message(m);
    }

    _check_auth_rotating();
  }
  return 0;
}

int MonClient::_check_auth_rotating()
{
  if (!rotating_secrets ||
      !auth_principal_needs_rotating_keys(entity_name)) {
    dout(20) << "_check_auth_rotating not needed by " << entity_name << dendl;
    return 0;
  }

  if (!auth || state != MC_STATE_HAVE_SESSION) {
    dout(10) << "_check_auth_rotating waiting for auth session" << dendl;
    return 0;
  }

  utime_t cutoff = g_clock.now();
  cutoff -= MIN(30.0, g_conf.auth_service_ticket_ttl / 4.0);
  if (!rotating_secrets->need_new_secrets(cutoff)) {
    dout(10) << "_check_auth_rotating have uptodate secrets (they expire after " << cutoff << ")" << dendl;
    rotating_secrets->dump_rotating();
    return 0;
  }

  dout(10) << "_check_auth_rotating renewing rotating keys (they expired before " << cutoff << ")" << dendl;
  MAuth *m = new MAuth;
  m->protocol = auth->get_protocol();
  if (auth->build_rotating_request(m->auth_payload)) {
    _send_mon_message(m);
  } else {
    m->put();
  }
  return 0;
}

int MonClient::wait_auth_rotating(double timeout)
{
  Mutex::Locker l(monc_lock);
  utime_t until = g_clock.now();
  until += timeout;

  if (auth->get_protocol() == CEPH_AUTH_NONE)
    return 0;
  
  if (!rotating_secrets)
    return 0;

  while (auth_principal_needs_rotating_keys(entity_name) &&
	 rotating_secrets->need_new_secrets()) {
    utime_t now = g_clock.now();
    if (now >= until) {
      dout(0) << "wait_auth_rotating timed out after " << timeout << dendl;
      return -ETIMEDOUT;
    }
    dout(10) << "wait_auth_rotating waiting (until " << until << ")" << dendl;
    auth_cond.WaitUntil(monc_lock, until);
  }
  dout(10) << "wait_auth_rotating done" << dendl;
  return 0;
}


