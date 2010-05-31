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

#include <sys/stat.h>
#include <iostream>
#include <string>
using namespace std;

#include "config.h"

#include "client/Client.h"
#include "client/fuse.h"
#include "client/fuse_ll.h"

#include "msg/SimpleMessenger.h"

#include "mon/MonClient.h"

#include "common/Timer.h"
#include "common/common_init.h"
       
#ifndef DARWIN
#include <envz.h>
#endif // DARWIN

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, const char **argv, const char *envp[]) {

  //cerr << "cfuse starting " << myrank << "/" << world << std::endl;
  vector<const char*> args;
  argv_to_vec(argc, argv, args);
  env_to_vec(args);
  common_init(args, "cfuse", false, true);

  // args for fuse
  vec_to_argv(args, argc, argv);

  // FUSE will chdir("/"); be ready.
  g_conf.chdir = strdup("/");

  if (g_conf.clock_tare) g_clock.tare();

  // check for 32-bit arch
  if (sizeof(long) == 4) {
    cerr << std::endl;
    cerr << "WARNING: Ceph inode numbers are 64 bits wide, and FUSE on 32-bit kernels does" << std::endl;
    cerr << "         not cope well with that situation.  Expect to crash shortly." << std::endl;
    cerr << std::endl;
  }

  // get monmap
  MonClient mc;
  if (mc.build_initial_monmap() < 0)
    return -1;

  // start up network
  SimpleMessenger *messenger = new SimpleMessenger();
  cout << "mounting ceph" << std::endl;
  messenger->register_entity(entity_name_t::CLIENT());
  Client *client = new Client(messenger, &mc);

  messenger->start();

  int r;

  // start client
  client->init();
    
  // start up fuse
  // use my argc, argv (make sure you pass a mount point!)
  r = client->mount();
  if (r < 0)
    goto out_shutdown;

  _dout_create_courtesy_output_symlink("client", client->get_nodeid().v);
  cout << "starting fuse" << std::endl;

  //cerr << "starting fuse on pid " << getpid() << std::endl;
  if (g_conf.fuse_ll)
    ceph_fuse_ll_main(client, argc, argv);
  else
    ceph_fuse_main(client, argc, argv);
  //cerr << "fuse finished on pid " << getpid() << std::endl;

  cout << "fuse finished, unmounting ceph" << std::endl;
  client->unmount();
  cout << "unmounted" << std::endl;

 out_shutdown:
  client->shutdown();
  delete client;
  
  // wait for messenger to finish
  messenger->wait();
  
  return r;
}

