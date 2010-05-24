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


#include "MDSMonitor.h"
#include "Monitor.h"
#include "MonitorStore.h"
#include "OSDMonitor.h"

#include "messages/MMDSMap.h"
#include "messages/MMDSBeacon.h"
#include "messages/MMDSLoadTargets.h"
#include "messages/MMonCommand.h"

#include "messages/MGenericMessage.h"


#include "common/Timer.h"

#include <sstream>

#include "config.h"

#define DOUT_SUBSYS mon
#undef dout_prefix
#define dout_prefix _prefix(mon, mdsmap)
static ostream& _prefix(Monitor *mon, MDSMap& mdsmap) {
  return *_dout << dbeginl
		<< "mon" << mon->whoami 
		<< (mon->is_starting() ? (const char*)"(starting)":(mon->is_leader() ? (const char*)"(leader)":(mon->is_peon() ? (const char*)"(peon)":(const char*)"(?\?)")))
		<< ".mds e" << mdsmap.get_epoch() << " ";
}



// my methods

void MDSMonitor::print_map(MDSMap &m, int dbl)
{
  dout(7) << "print_map\n";
  m.print(*_dout);
  *_dout << dendl;
}



// service methods
void MDSMonitor::create_initial(bufferlist& bl)
{
  dout(10) << "create_initial" << dendl;
  pending_mdsmap.max_mds = 1;
  pending_mdsmap.created = g_clock.now();
  pending_mdsmap.data_pg_pools.push_back(CEPH_DATA_RULE);
  pending_mdsmap.metadata_pg_pool = CEPH_METADATA_RULE;
  pending_mdsmap.cas_pg_pool = CEPH_CASDATA_RULE;
  pending_mdsmap.compat = mdsmap_compat;
  print_map(pending_mdsmap);
}


bool MDSMonitor::update_from_paxos()
{
  version_t paxosv = paxos->get_version();
  if (paxosv == mdsmap.epoch) return true;
  assert(paxosv >= mdsmap.epoch);

  dout(10) << "update_from_paxos paxosv " << paxosv 
	   << ", my e " << mdsmap.epoch << dendl;

  // read and decode
  mdsmap_bl.clear();
  bool success = paxos->read(paxosv, mdsmap_bl);
  assert(success);
  dout(10) << "update_from_paxos  got " << paxosv << dendl;
  mdsmap.decode(mdsmap_bl);

  // save as 'latest', too.
  paxos->stash_latest(paxosv, mdsmap_bl);

  // new map
  dout(4) << "new map" << dendl;
  print_map(mdsmap, 0);

  check_subs();

  return true;
}

void MDSMonitor::create_pending()
{
  pending_mdsmap = mdsmap;
  pending_mdsmap.epoch++;
  dout(10) << "create_pending e" << pending_mdsmap.epoch << dendl;
}

void MDSMonitor::encode_pending(bufferlist &bl)
{
  dout(10) << "encode_pending e" << pending_mdsmap.epoch << dendl;

  pending_mdsmap.modified = g_clock.now();

  //print_map(pending_mdsmap);

  // apply to paxos
  assert(paxos->get_version() + 1 == pending_mdsmap.epoch);
  pending_mdsmap.encode(bl);
}


bool MDSMonitor::preprocess_query(PaxosServiceMessage *m)
{
  dout(10) << "preprocess_query " << *m << " from " << m->get_orig_source_inst() << dendl;

  switch (m->get_type()) {
    
  case MSG_MDS_BEACON:
    return preprocess_beacon((MMDSBeacon*)m);
    
  case MSG_MON_COMMAND:
    return preprocess_command((MMonCommand*)m);

  case MSG_MDS_OFFLOAD_TARGETS:
    return preprocess_offload_targets((MMDSLoadTargets*)m);

  default:
    assert(0);
    m->put();
    return true;
  }
}

void MDSMonitor::_note_beacon(MMDSBeacon *m)
{
  uint64_t gid = m->get_global_id();
  version_t seq = m->get_seq();

  dout(15) << "_note_beacon " << *m << " noting time" << dendl;
  last_beacon[gid].stamp = g_clock.now();  
  last_beacon[gid].seq = seq;
}

bool MDSMonitor::preprocess_beacon(MMDSBeacon *m)
{
  entity_addr_t addr = m->get_orig_source_inst().addr;
  int state = m->get_state();
  uint64_t gid = m->get_global_id();
  version_t seq = m->get_seq();
  MDSMap::mds_info_t info;

  // check privileges, ignore if fails
  MonSession *session = m->get_session();
  if (!session)
    goto out;
  if (!session->caps.check_privileges(PAXOS_MDSMAP, MON_CAP_X)) {
    dout(0) << "preprocess_beason got MMDSBeacon from entity with insufficient privileges "
	    << session->caps << dendl;
    goto out;
  }

  if (ceph_fsid_compare(&m->get_fsid(), &mon->monmap->fsid)) {
    dout(0) << "preprocess_beacon on fsid " << m->get_fsid() << " != " << mon->monmap->fsid << dendl;
    goto out;
  }

  dout(12) << "preprocess_beacon " << *m
	   << " from " << m->get_orig_source_inst()
	   << dendl;

  // fw to leader?
  if (!mon->is_leader())
    return false;

  // booted, but not in map?
  if (pending_mdsmap.is_dne_gid(gid)) {
    if (state != MDSMap::STATE_BOOT) {
      dout(7) << "mds_beacon " << *m << " is not in mdsmap" << dendl;
      mon->send_reply(m, new MMDSMap(mon->monmap->fsid, &mdsmap));
      goto out;
    } else {
      return false;  // not booted yet.
    }
  }
  info = pending_mdsmap.get_info_gid(gid);

  // old seq?
  if (info.state_seq > seq) {
    dout(7) << "mds_beacon " << *m << " has old seq, ignoring" << dendl;
    goto out;
  }

  if (mdsmap.get_epoch() != m->get_last_epoch_seen()) {
    dout(10) << "mds_beacon " << *m
	     << " ignoring requested state, because mds hasn't seen latest map" << dendl;
    goto ignore;
  }

  if (info.laggy()) {
    _note_beacon(m);
    return false;  // no longer laggy, need to update map.
  }
  if (state == MDSMap::STATE_BOOT) {
    // ignore, already booted.
    goto out;
  }
  // is there a state change here?
  if (info.state != state) {    
    // legal state change?
    if ((info.state == MDSMap::STATE_STANDBY ||
	 info.state == MDSMap::STATE_STANDBY_REPLAY) && state > 0) {
      dout(10) << "mds_beacon mds can't activate itself (" << ceph_mds_state_name(info.state)
	       << " -> " << ceph_mds_state_name(state) << ")" << dendl;
      goto ignore;
    }
    
    if (info.state == MDSMap::STATE_STANDBY &&
	state == MDSMap::STATE_STANDBY_REPLAY &&
	(pending_mdsmap.is_degraded() ||
	 pending_mdsmap.get_state(info.rank) < MDSMap::STATE_ACTIVE)) {
      dout(10) << "mds_beacon can't standby-replay mds" << info.rank << " at this time (cluster degraded, or mds not active)" << dendl;
      goto ignore;
    }
    _note_beacon(m);
    return false;  // need to update map
  }

 ignore:
  // note time and reply
  _note_beacon(m);
  mon->send_reply(m,
		  new MMDSBeacon(mon->monmap->fsid, m->get_global_id(), m->get_name(),
				 mdsmap.get_epoch(), state, seq));
  
  // done
 out:
  m->put();
  return true;
}

bool MDSMonitor::preprocess_offload_targets(MMDSLoadTargets* m)
{
  dout(10) << "preprocess_offload_targets " << *m << " from " << m->get_orig_source() << dendl;
  uint64_t gid;
  
  // check privileges, ignore message if fails
  MonSession *session = m->get_session();
  if (!session)
    goto done;
  if (!session->caps.check_privileges(PAXOS_MDSMAP, MON_CAP_X)) {
    dout(0) << "preprocess_offload_targets got MMDSLoadTargets from entity with insufficient caps "
	    << session->caps << dendl;
    goto done;
  }

  gid = m->global_id;
  if (mdsmap.mds_info.count(gid) &&
      m->targets == mdsmap.mds_info[gid].export_targets)
    goto done;

  return false;

 done:
  m->put();
  return true;
}


bool MDSMonitor::prepare_update(PaxosServiceMessage *m)
{
  dout(7) << "prepare_update " << *m << dendl;

  switch (m->get_type()) {
    
  case MSG_MDS_BEACON:
    return prepare_beacon((MMDSBeacon*)m);

  case MSG_MON_COMMAND:
    return prepare_command((MMonCommand*)m);

  case MSG_MDS_OFFLOAD_TARGETS:
    return prepare_offload_targets((MMDSLoadTargets*)m);
  
  default:
    assert(0);
    m->put();
  }

  return true;
}



bool MDSMonitor::prepare_beacon(MMDSBeacon *m)
{
  // -- this is an update --
  dout(12) << "prepare_beacon " << *m << " from " << m->get_orig_source_inst() << dendl;
  entity_addr_t addr = m->get_orig_source_inst().addr;
  uint64_t gid = m->get_global_id();
  int state = m->get_state();
  version_t seq = m->get_seq();

  // boot?
  if (state == MDSMap::STATE_BOOT) {
    // add
    MDSMap::mds_info_t& info = pending_mdsmap.mds_info[gid];
    info.global_id = gid;
    info.name = m->get_name();
    info.rank = -1;
    info.addr = addr;
    info.state = MDSMap::STATE_STANDBY;
    info.state_seq = seq;
    info.standby_for_rank = m->get_standby_for_rank();
    info.standby_for_name = m->get_standby_for_name();

    // initialize the beacon timer
    last_beacon[gid].stamp = g_clock.now();
    last_beacon[gid].seq = seq;

  } else {
    // state change
    MDSMap::mds_info_t& info = pending_mdsmap.get_info_gid(gid);

    if (info.laggy()) {
      dout(10) << "prepare_beacon clearing laggy flag on " << addr << dendl;
      info.clear_laggy();
    }
  
    dout(10) << "prepare_beacon mds" << info.rank
	     << " " << ceph_mds_state_name(info.state)
	     << " -> " << ceph_mds_state_name(state)
	     << dendl;
    if (state == MDSMap::STATE_STOPPED) {
      pending_mdsmap.up.erase(info.rank);
      pending_mdsmap.in.erase(info.rank);
      pending_mdsmap.stopped.insert(info.rank);
      pending_mdsmap.mds_info.erase(gid);  // last! info is a ref into this map
      last_beacon.erase(gid);
    } else {
      info.state = state;
      info.state_seq = seq;
    }
  }

  dout(7) << "prepare_beacon pending map now:" << dendl;
  print_map(pending_mdsmap);
  
  paxos->wait_for_commit(new C_Updated(this, m));

  return true;
}

bool MDSMonitor::prepare_offload_targets(MMDSLoadTargets *m)
{
  uint64_t gid = m->global_id;
  if (pending_mdsmap.mds_info.count(gid)) {
    dout(10) << "prepare_offload_targets " << gid << " " << m->targets << dendl;
    pending_mdsmap.mds_info[gid].export_targets = m->targets;
  } else {
    dout(10) << "prepare_offload_targets " << gid << " not in map" << dendl;
  }
  return true;
}

bool MDSMonitor::should_propose(double& delay)
{
  delay = 0.0;
  return true;
}

void MDSMonitor::_updated(MMDSBeacon *m)
{
  dout(10) << "_updated " << m->get_orig_source() << " " << *m << dendl;
  stringstream ss;
  ss << m->get_orig_source_inst() << " " << ceph_mds_state_name(m->get_state());
  mon->get_logclient()->log(LOG_INFO, ss);

  if (m->get_state() == MDSMap::STATE_STOPPED) {
    // send the map manually (they're out of the map, so they won't get it automatic)
    mon->send_reply(m, new MMDSMap(mon->monmap->fsid, &mdsmap));
  }

  m->put();
}


void MDSMonitor::committed()
{
  tick();
}


bool MDSMonitor::preprocess_command(MMonCommand *m)
{
  int r = -1;
  bufferlist rdata;
  stringstream ss;

  if (m->cmd.size() > 1) {
    if (m->cmd[1] == "stat") {
      ss << mdsmap;
      r = 0;
    } 
    else if (m->cmd[1] == "dump") {
      MDSMap *p = &mdsmap;
      if (m->cmd.size() > 2) {
	epoch_t e = atoi(m->cmd[2].c_str());
	bufferlist b;
	mon->store->get_bl_sn(b,"mdsmap",e);
	if (!b.length()) {
	  p = 0;
	  r = -ENOENT;
	} else {
	  p = new MDSMap;
	  p->decode(b);
	}
      }
      if (p) {
	stringstream ds;
	p->print(ds);
	rdata.append(ds);
	ss << "dumped mdsmap epoch " << p->get_epoch();
	if (p != &mdsmap)
	  delete p;
	r = 0;
      }
    }
    else if (m->cmd[1] == "getmap") {
      if (m->cmd.size() > 2) {
	epoch_t e = atoi(m->cmd[2].c_str());
	bufferlist b;
	mon->store->get_bl_sn(b,"mdsmap",e);
	if (!b.length()) {
	  r = -ENOENT;
	} else {
	  MDSMap m;
	  m.decode(b);
	  m.encode(rdata);
	  ss << "got mdsmap epoch " << m.get_epoch();
	}
      } else {
	mdsmap.encode(rdata);
	ss << "got mdsmap epoch " << mdsmap.get_epoch();
      }
      r = 0;
    }
    else if (m->cmd[1] == "injectargs" && m->cmd.size() == 4) {
      if (m->cmd[2] == "*") {
	for (unsigned i=0; i<mdsmap.get_max_mds(); i++)
	  if (mdsmap.is_active(i))
	    mon->inject_args(mdsmap.get_inst(i), m->cmd[3]);
	r = 0;
	ss << "ok bcast";
      } else {
	errno = 0;
	int who = strtol(m->cmd[2].c_str(), 0, 10);
	if (!errno && who >= 0) {
	  if (mdsmap.is_up(who)) {
	    mon->inject_args(mdsmap.get_inst(who), m->cmd[3]);
	    r = 0;
	    ss << "ok";
	  } else {
	    ss << "mds" << who << " not up";
	    r = -ENOENT;
	  }
	} else
	  ss << "specify mds number or *";
      }
    }
    else if (m->cmd[1] == "tell") {
      m->cmd.erase(m->cmd.begin()); //take out first two args; don't need them
      m->cmd.erase(m->cmd.begin());
      if (m->cmd[0] == "*") {
	m->cmd.erase(m->cmd.begin()); //and now we're done with the target num
	for (unsigned i = 0; i < mdsmap.get_max_mds(); ++i) {
	  if (mdsmap.is_active(i))
	    mon->send_command(mdsmap.get_inst(i), m->cmd, paxos->get_version());
	}
      } else {
	errno = 0;
	int who = strtol(m->cmd[0].c_str(), 0, 10);
	m->cmd.erase(m->cmd.begin()); //done with target num now
	if (!errno && who >= 0) {
	  if (mdsmap.is_up(who)) {
	    mon->send_command(mdsmap.get_inst(who), m->cmd, paxos->get_version());
	    r = 0;
	    ss << "ok";
	  } else {
	    ss << "mds" << who << " no up";
	    r = -ENOENT;
	  }
	} else ss << "specify mds number or *";
      }
    }
  }

  if (r != -1) {
    string rs;
    getline(ss, rs);
    mon->reply_command(m, r, rs, rdata, paxos->get_version());
    return true;
  } else
    return false;
}

bool MDSMonitor::prepare_command(MMonCommand *m)
{
  int r = -EINVAL;
  stringstream ss;
  bufferlist rdata;

  if (m->cmd.size() > 1) {
    if (m->cmd[1] == "stop" && m->cmd.size() > 2) {
      int who = atoi(m->cmd[2].c_str());
      if (mdsmap.is_active(who)) {
	r = 0;
	uint64_t gid = pending_mdsmap.up[who];
	ss << "telling mds" << who << " " << pending_mdsmap.mds_info[gid].addr << " to stop";
	pending_mdsmap.mds_info[gid].state = MDSMap::STATE_STOPPING;
      } else {
	r = -EEXIST;
	ss << "mds" << who << " not active (" 
	   << ceph_mds_state_name(mdsmap.get_state(who)) << ")";
      }
    }
    else if (m->cmd[1] == "set_max_mds" && m->cmd.size() > 2) {
      pending_mdsmap.max_mds = atoi(m->cmd[2].c_str());
      r = 0;
      ss << "max_mds = " << pending_mdsmap.max_mds;
    }
    else if (m->cmd[1] == "setmap" && m->cmd.size() == 3) {
      MDSMap map;
      map.decode(m->get_data());
      epoch_t e = atoi(m->cmd[2].c_str());
      //if (ceph_fsid_compare(&map.get_fsid(), &mon->monmap->fsid) == 0) {
      if (pending_mdsmap.epoch == e) {
	map.epoch = pending_mdsmap.epoch;  // make sure epoch is correct
	pending_mdsmap = map;
	string rs = "set mds map";
	paxos->wait_for_commit(new Monitor::C_Command(mon, m, 0, rs, paxos->get_version()));
	return true;
      } else
	ss << "next mdsmap epoch " << pending_mdsmap.epoch << " != " << e;
	//} else
	//ss << "mdsmap fsid " << map.fsid << " does not match monitor fsid " << mon->monmap->fsid;
    }
    else if (m->cmd[1] == "set_state" && m->cmd.size() == 4) {
      uint64_t gid = atoi(m->cmd[2].c_str());
      int state = atoi(m->cmd[3].c_str());
      if (!pending_mdsmap.is_dne_gid(gid)) {
	MDSMap::mds_info_t& info = pending_mdsmap.get_info_gid(gid);
	info.state = state;
	stringstream ss;
	ss << "set mds gid " << gid << " to state " << state << " " << ceph_mds_state_name(state);
	string rs;
	getline(ss, rs);
	paxos->wait_for_commit(new Monitor::C_Command(mon, m, 0, rs, paxos->get_version()));
	return true;
      }
    }
  }
  if (r == -EINVAL) 
    ss << "unrecognized command";
  string rs;
  getline(ss, rs);

  if (r >= 0) {
    // success.. delay reply
    paxos->wait_for_commit(new Monitor::C_Command(mon, m, r, rs, paxos->get_version()));
    return true;
  } else {
    // reply immediately
    mon->reply_command(m, r, rs, rdata, paxos->get_version());
    return false;
  }
}


void MDSMonitor::check_subs()
{
  string type = "mdsmap";
  xlist<Subscription*>::iterator p = mon->session_map.subs[type].begin();
  while (!p.end()) {
    Subscription *sub = *p;
    ++p;
    check_sub(sub);
  }
}

void MDSMonitor::check_sub(Subscription *sub)
{
  if (sub->last < mdsmap.get_epoch()) {
    mon->messenger->send_message(new MMDSMap(mon->monmap->fsid, &mdsmap),
				 sub->session->inst);
    if (sub->onetime)
      mon->session_map.remove_sub(sub);
    else
      sub->last = mdsmap.get_epoch();
  }
}

void MDSMonitor::tick()
{
  // make sure mds's are still alive
  // ...if i am an active leader
  if (!paxos->is_active()) return;

  update_from_paxos();
  dout(10) << mdsmap << dendl;
  
  bool do_propose = false;

  if (!mon->is_leader()) return;

  // expand mds cluster (add new nodes to @in)?
  while (pending_mdsmap.get_num_mds() < pending_mdsmap.get_max_mds() &&
	 !pending_mdsmap.is_degraded()) {
    int mds = 0;
    string name;
    while (pending_mdsmap.is_in(mds))
      mds++;
    uint64_t newgid = pending_mdsmap.find_standby_for(mds, name);
    if (!newgid)
      break;

    MDSMap::mds_info_t& info = pending_mdsmap.mds_info[newgid];
    dout(1) << "adding standby " << info.addr << " as mds" << mds << dendl;
    
    info.rank = mds;
    if (pending_mdsmap.stopped.count(mds)) {
      info.state = MDSMap::STATE_STARTING;
      pending_mdsmap.stopped.erase(mds);
    } else
      info.state = MDSMap::STATE_CREATING;
    info.inc = ++pending_mdsmap.inc[mds];
    pending_mdsmap.in.insert(mds);
    pending_mdsmap.up[mds] = newgid;
    do_propose = true;
  }

  // check beacon timestamps
  utime_t now = g_clock.now();
  utime_t cutoff = now;
  cutoff -= g_conf.mds_beacon_grace;

  // make sure last_beacon is fully populated
  for (map<uint64_t,MDSMap::mds_info_t>::iterator p = pending_mdsmap.mds_info.begin();
       p != pending_mdsmap.mds_info.end();
       ++p) {
    if (last_beacon.count(p->first) == 0) {
      const MDSMap::mds_info_t& info = p->second;
      dout(10) << " adding " << p->second.addr << " mds" << info.rank << "." << info.inc
	       << " " << ceph_mds_state_name(info.state)
	       << " to last_beacon" << dendl;
      last_beacon[p->first].stamp = g_clock.now();
      last_beacon[p->first].seq = 0;
    }
  }

  if (mon->osdmon()->paxos->is_writeable()) {

    bool propose_osdmap = false;

    map<uint64_t, beacon_info_t>::iterator p = last_beacon.begin();
    while (p != last_beacon.end()) {
      uint64_t gid = p->first;
      utime_t since = p->second.stamp;
      uint64_t seq = p->second.seq;
      p++;
      
      if (pending_mdsmap.mds_info.count(gid) == 0) {
	// clean it out
	last_beacon.erase(gid);
	continue;
      }

      if (since >= cutoff)
	continue;

      MDSMap::mds_info_t& info = pending_mdsmap.mds_info[gid];

      dout(10) << "no beacon from " << info.addr << " mds" << info.rank << "." << info.inc
	       << " " << ceph_mds_state_name(info.state)
	       << " since " << since << dendl;
      
      // are we in?
      // and is there a non-laggy standby that can take over for us?
      uint64_t sgid;
      if (info.rank >= 0 &&
	  info.state != CEPH_MDS_STATE_STANDBY &&
	  (sgid = pending_mdsmap.find_standby_for(info.rank, info.name)) != 0) {
	MDSMap::mds_info_t& si = pending_mdsmap.mds_info[sgid];
	dout(10) << " replacing " << info.addr << " mds" << info.rank << "." << info.inc
		 << " " << ceph_mds_state_name(info.state)
		 << " with " << sgid << "/" << si.name << " " << si.addr << dendl;
	switch (info.state) {
	case MDSMap::STATE_CREATING:
	case MDSMap::STATE_STARTING:
	case MDSMap::STATE_STANDBY_REPLAY:
	  si.state = info.state;
	  break;
	case MDSMap::STATE_REPLAY:
	case MDSMap::STATE_RESOLVE:
	case MDSMap::STATE_RECONNECT:
	case MDSMap::STATE_REJOIN:
	case MDSMap::STATE_CLIENTREPLAY:
	case MDSMap::STATE_ACTIVE:
	case MDSMap::STATE_STOPPING:
	  si.state = MDSMap::STATE_REPLAY;
	  break;
	default:
	  assert(0);
	}

	info.state_seq = seq;
	si.rank = info.rank;
	si.inc = ++pending_mdsmap.inc[info.rank];
	pending_mdsmap.up[info.rank] = sgid;
	if (si.state > 0)
	  pending_mdsmap.last_failure = pending_mdsmap.epoch;
	if (si.state > 0 ||
	    si.state == MDSMap::STATE_CREATING ||
	    si.state == MDSMap::STATE_STARTING) {
	  // blacklist laggy mds
	  utime_t until = now;
	  until += g_conf.mds_blacklist_interval;
	  mon->osdmon()->blacklist(info.addr, until);
	  propose_osdmap = true;
	}
	pending_mdsmap.mds_info.erase(gid);
	last_beacon.erase(gid);
	do_propose = true;
      } else if (info.state == MDSMap::STATE_STANDBY_REPLAY) {
	dout(10) << " failing " << info.addr << " mds" << info.rank << "." << info.inc
		 << " " << ceph_mds_state_name(info.state)
		 << dendl;
	pending_mdsmap.mds_info.erase(gid);
	last_beacon.erase(gid);
	do_propose = true;
      } else if (!info.laggy()) {
	// just mark laggy
	dout(10) << " marking " << info.addr << " mds" << info.rank << "." << info.inc
		 << " " << ceph_mds_state_name(info.state)
		 << " laggy" << dendl;
	info.laggy_since = now;
	do_propose = true;
      }
    }

    if (propose_osdmap)
      mon->osdmon()->propose_pending();

  }


  // have a standby take over?
  set<int> failed;
  pending_mdsmap.get_failed_mds_set(failed);
  if (!failed.empty()) {
    set<int>::iterator p = failed.begin();
    while (p != failed.end()) {
      int f = *p++;
      uint64_t sgid;
      string name;  // FIXME
      sgid = pending_mdsmap.find_standby_for(f, name);
      if (sgid) {
	MDSMap::mds_info_t& si = pending_mdsmap.mds_info[sgid];
	dout(0) << " taking over failed mds" << f << " with " << sgid << "/" << si.name << " " << si.addr << dendl;
	si.state = MDSMap::STATE_REPLAY;
	si.rank = f;
	si.inc = ++pending_mdsmap.inc[f];
	pending_mdsmap.in.insert(f);
	pending_mdsmap.up[f] = sgid;
	do_propose = true;
      }
    }
  }

  // have a standby replay/shadow an active mds?
  if (false &&
      !pending_mdsmap.is_degraded() &&
      pending_mdsmap.get_num_mds(MDSMap::STATE_STANDBY) >= pending_mdsmap.get_num_mds()) {
    // see which nodes are shadowed
    set<int> shadowed;
    map<int, set<uint64_t> > avail;
    for (map<uint64_t,MDSMap::mds_info_t>::iterator p = pending_mdsmap.mds_info.begin();
	 p != pending_mdsmap.mds_info.end();
	 p++) {
      if (p->second.state == MDSMap::STATE_STANDBY_REPLAY) 
	shadowed.insert(p->second.rank);
      if (p->second.state == MDSMap::STATE_STANDBY &&
	  !p->second.laggy())
	avail[p->second.rank].insert(p->first);
    }

    // find an mds that needs a standby
    set<int> in;
    pending_mdsmap.get_mds_set(in);
    for (set<int>::iterator p = in.begin(); p != in.end(); p++) {
      if (shadowed.count(*p))
	continue;  // already shadowed.
      if (pending_mdsmap.get_state(*p) < MDSMap::STATE_ACTIVE)
	continue;  // only shadow active mds
      uint64_t sgid;
      if (avail[*p].size()) {
	sgid = *avail[*p].begin();
	avail[*p].erase(avail[*p].begin());
      } else if (avail[-1].size()) {
	sgid = *avail[-1].begin();
	avail[-1].erase(avail[-1].begin());
      } else
	continue;
      dout(10) << "mds" << *p << " will be shadowed by " << sgid << dendl;

      MDSMap::mds_info_t& info = pending_mdsmap.mds_info[sgid];
      info.rank = *p;
      info.state = MDSMap::STATE_STANDBY_REPLAY;
      do_propose = true;
    }
  }

  if (do_propose)
    propose_pending();
}


void MDSMonitor::do_stop()
{
  // hrm...
  if (!mon->is_leader() ||
      !paxos->is_active()) {
    dout(-10) << "do_stop can't stop right now, mdsmap not writeable" << dendl;
    return;
  }

  dout(7) << "do_stop stopping active mds nodes" << dendl;
  print_map(mdsmap);

  map<uint64_t,MDSMap::mds_info_t>::iterator p = pending_mdsmap.mds_info.begin();
  while (p != pending_mdsmap.mds_info.end()) {
    uint64_t gid = p->first;
    MDSMap::mds_info_t& info = p->second;
    p++;
    switch (info.state) {
    case MDSMap::STATE_ACTIVE:
    case MDSMap::STATE_STOPPING:
      info.state = MDSMap::STATE_STOPPING;
      break;
    case MDSMap::STATE_STARTING:
      pending_mdsmap.stopped.insert(info.rank);
    case MDSMap::STATE_CREATING:
      pending_mdsmap.up.erase(info.rank);
      pending_mdsmap.mds_info.erase(gid);
      pending_mdsmap.in.erase(info.rank);
      break;
    case MDSMap::STATE_REPLAY:
    case MDSMap::STATE_RESOLVE:
    case MDSMap::STATE_RECONNECT:
    case MDSMap::STATE_REJOIN:
    case MDSMap::STATE_CLIENTREPLAY:
      // BUG: hrm, if this is the case, the STOPPING guys won't be able to stop, will they?
      pending_mdsmap.failed.insert(info.rank);
      pending_mdsmap.up.erase(info.rank);
      pending_mdsmap.mds_info.erase(gid);
      pending_mdsmap.in.erase(info.rank);
      break;
    }
  }

  propose_pending();
}
