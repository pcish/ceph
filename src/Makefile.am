#
# note: the old Makefile is at Makefile.old
#

AUTOMAKE_OPTIONS = gnu

bin_PROGRAMS =
sbin_PROGRAMS =
bin_SCRIPTS = crun

# monitor
cmon_SOURCES = cmon.cc msg/SimpleMessenger.cc
cmon_LDADD = libmon.a libcrush.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += cmon

# osd
cosd_SOURCES = cosd.cc msg/SimpleMessenger.cc objclass/class_debug.cc \
	       objclass/class_api.cc
cosd_LDADD = libosd.a libos.a libcrush.a libcommon.a -ldl -lpthread -lm -lcrypto
bin_PROGRAMS += cosd

# mds
cmds_SOURCES = cmds.cc msg/SimpleMessenger.cc
cmds_LDADD = libmds.a libosdc.a libcrush.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += cmds

# admin tools
ceph_SOURCES = ceph.cc msg/SimpleMessenger.cc
ceph_LDADD = libcrush.a libcommon.a -ledit -lpthread -lm -lcrypto
mkmonfs_SOURCES = mkmonfs.cc
mkmonfs_LDADD = libmon.a libcrush.a libcommon.a -lpthread -lm -lcrypto
cconf_SOURCES = cconf.cc
cconf_LDADD = libcommon.a -lpthread -lm -lcrypto
cauthtool_SOURCES = cauthtool.cc
cauthtool_LDADD = libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += ceph mkmonfs cconf cauthtool

monmaptool_SOURCES = monmaptool.cc
monmaptool_LDADD = libcommon.a -lpthread -lm -lcrypto
crushtool_SOURCES = crushtool.cc
crushtool_LDADD = libcrush.a libcommon.a -lpthread -lm -lcrypto
osdmaptool_SOURCES = osdmaptool.cc
osdmaptool_LDADD = libcrush.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += monmaptool crushtool osdmaptool

mount_ceph_SOURCES = mount/mount.ceph.c
noinst_PROGRAMS = mount.ceph


core: cmon cosd cmds mkmonfs ceph cconf monmaptool osdmaptool crushtool


dumpjournal_SOURCES = dumpjournal.cc msg/SimpleMessenger.cc
dumpjournal_LDADD = libosdc.a libcrush.a libcommon.a -lpthread -lm -lcrypto
dupstore_SOURCES = dupstore.cc
dupstore_LDADD = libos.a libcommon.a -lpthread -lm -lcrypto
streamtest_SOURCES = streamtest.cc
streamtest_LDADD = libos.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += dumpjournal dupstore streamtest

test_trans_SOURCES = test_trans.cc
test_trans_LDADD = libos.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += test_trans

# synthetic client
csyn_SOURCES = csyn.cc msg/SimpleMessenger.cc
csyn_LDADD = libclient.a libosdc.a libcrush.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += csyn

testmsgr_SOURCES = testmsgr.cc msg/SimpleMessenger.cc
testmsgr_LDADD = libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += testmsgr

test_ioctls_SOURCES = client/test_ioctls.c
bin_PROGRAMS += test_ioctls

# fuse targets?
if WITH_FUSE
cfuse_SOURCES = cfuse.cc msg/SimpleMessenger.cc client/fuse.cc client/fuse_ll.cc
cfuse_LDADD = -lfuse libclient.a libosdc.a libcrush.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += cfuse

#if WITH_DEBUG
#fakefuse_SOURCES = fakefuse.cc msg/FakeMessenger.cc client/fuse.cc client/fuse_ll.cc
#fakefuse_LDADD = -lfuse libmon.a libmds.a libosd.a libos.a \
#	libclient.a libosdc.a libcrush.a 
#bin_PROGRAMS += fakefuse
#endif

endif

# debug targets?
if WITH_DEBUG
psim_SOURCES = psim.cc
psim_LDADD = libcrush.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += psim

#fakesyn_SOURCES = fakesyn.cc msg/FakeMessenger.cc
#fakesyn_LDADD = libmon.a libmds.a libosd.a libos.a \
#	libclient.a libosdc.a libcrush.a libcommon.a
#bin_PROGRAMS += fakesyn

endif


##########
BUILT_SOURCES =
lib_LTLIBRARIES = 

# libcrush.so
libcrush_la_SOURCES = \
	crush/builder.c \
	crush/mapper.c \
	crush/crush.c \
	crush/hash.c
libcrush_la_CFLAGS = ${AM_CFLAGS}
libcrush_la_CXXFLAGS = ${AM_CXXFLAGS}
libcrush_la_LDFLAGS = -version-info 1:0:0 -export-symbols-regex 'crush_.*' -lm
lib_LTLIBRARIES += libcrush.la

# libceph
libceph_la_SOURCES = \
	libceph.cc \
	client/Client.cc \
	msg/SimpleMessenger.cc \
	${libcommon_a_SOURCES} \
	${libosdc_a_SOURCES}
libceph_la_CFLAGS = ${AM_CFLAGS}
libceph_la_CXXFLAGS= ${AM_CXXFLAGS}
libceph_la_LIBADD = libcrush.la
libceph_la_LDFLAGS = -version-info 1:0:0 -export-symbols-regex '^ceph_.*' -lpthread
lib_LTLIBRARIES += libceph.la

testceph_SOURCES = client/testceph.cc
testceph_LDADD = libceph.la libcrush.la -lpthread -lm -lcrypto
bin_PROGRAMS += testceph

# librados
librados_SOURCES = \
	librados.cc \
	msg/SimpleMessenger.cc \
	osdc/Objecter.cc \
	${libcommon_a_SOURCES}
librados_la_SOURCES = ${librados_SOURCES}
librados_la_CFLAGS = ${AM_CFLAGS}
librados_la_CXXFLAGS = ${AM_CXXFLAGS}
librados_la_LIBADD = libcrush.la
librados_la_LDFLAGS = -version-info 1:0:0 -export-symbols-regex '^rados_.*' -lpthread
lib_LTLIBRARIES += librados.la

librados_a_SOURCES = ${librados_SOURCES}
librados_a_CFLAGS = ${AM_CFLAGS}
librados_a_CXXFLAGS = ${AM_CXXFLAGS}

rados_SOURCES = rados.cc
rados_LDADD = librados.la -lpthread -lm -lcrypto
bin_PROGRAMS += rados

testrados_SOURCES = testrados.c
testrados_LDADD = librados.la -lpthread -lm -lcrypto
testradospp_SOURCES = testradospp.cc
testradospp_LDADD = librados.la -lpthread -lm -lcrypto
radosacl_SOURCES = radosacl.cc
radosacl_LDADD = librados.la -lpthread -lm -lcrypto
rbdtool_SOURCES = rbdtool.cc
rbdtool_LDADD = librados.la -lpthread -lm -lcrypto
bin_PROGRAMS += testrados testradospp radosacl rbdtool

if WITH_RADOSGW
libradosgw_a_SOURCES = \
	rgw/rgw_fs.cc \
	rgw/rgw_rados.cc \
	rgw/rgw_acl.cc \
	rgw/rgw_user.cc \
	rgw/rgw_access.cc \
	rgw/rgw_op.cc \
	rgw/rgw_rest.cc \
	rgw/rgw_common.cc
libradosgw_a_CFLAGS = ${AM_CFLAGS}
# lib_LTLIBRARIES += libradosgw.a

radosgw_SOURCES = rgw/rgw_main.cc
radosgw_LDADD = libradosgw.a librados.a libcrush.a -lfcgi -lcrypto -lexpat -lpthread -lm
radosgw_admin_SOURCES = rgw/rgw_admin.cc
radosgw_admin_LDADD = libradosgw.a librados.a libcrush.a -lfcgi -lcrypto -lexpat -lpthread -lm
bin_PROGRAMS += radosgw radosgw_admin
endif

testcrypto_SOURCES = testcrypto.cc
testcrypto_LDADD = libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += testcrypto

testkeys_SOURCES = testkeys.cc
testkeys_LDADD = libmon.a libcommon.a -lpthread -lm -lcrypto
bin_PROGRAMS += testkeys
## object classes

# libtestclass.so: testclass.cc
# 	${CXX} -fPIC -shared -g -o libtestclass.so testclass.cc
# libbarclass.so: barclass.cc
# 	${CXX} -fPIC -shared -g -o libbarclass.so barclass.cc
# libfooclass.so: fooclass.cc
# 	${CXX} -fPIC -shared -g -o libfooclass.so fooclass.cc
# 
#BUILT_SOURCES += libtestclass.so libfooclass.so libbarclass.so

libcls_crypto.so: cls_crypto.cc
	${CXX} -I. -fPIC -shared -g -o libcls_crypto.so -lcrypto cls_crypto.cc
BUILT_SOURCES += libcls_crypto.so

libcls_acl.so: cls_acl.cc
	${CXX} -I. -fPIC -shared -g -o libcls_acl.so cls_acl.cc
BUILT_SOURCES += libcls_acl.so


## hadoop client
if WITH_HADOOPCLIENT
JAVA_BASE = /usr/lib/jvm/java-6-sun
libhadoopcephfs_la_SOURCES = client/hadoop/CephFSInterface.cc
libhadoopcephfs_la_LIBADD = libceph.la
libhadoopcephfs_la_CFLAGS = ${AM_CFLAGS}
libhadoopcephfs_la_CXXFLAGS = ${AM_CXXFLAGS}
libhadoopcephfs_la_LDFLAGS = -version-info 1:0:0 -export-symbols-regex 'hadoopcephfs_.*'
lib_LTLIBRARIES += libhadoopcephfs.la
endif


# shell scripts
editpaths = sed \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@libdir[@]|$(libdir)|g' \
	-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
	-e 's|@datadir[@]|$(pkgdatadir)|g' \
	-e 's|@prefix[@]|$(prefix)|g'

init-ceph mkcephfs: init-ceph.in mkcephfs.in Makefile
	rm -f $@ $@.tmp
	$(editpaths) '$(srcdir)/$@.in' >$@.tmp
	chmod +x $@.tmp
	chmod a-w $@.tmp
	mv $@.tmp $@

BUILT_SOURCES += init-ceph mkcephfs

##
INCLUDES = 
LDADD =

AM_CXXFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_REENTRANT -D_THREAD_SAFE -rdynamic
AM_CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_REENTRANT -D_THREAD_SAFE -rdynamic
AM_LDFLAGS =

noinst_LIBRARIES = \
	libcommon.a libcrush.a \
	libmon.a libmds.a libosdc.a libosd.a libclient.a \
	libos.a librados.a libradosgw.a

noinst_LIBRARIES +=  #libcephclient_so.a

# extra bits
EXTRA_DIST = verify-mds-journal.sh vstart.sh stop.sh \
	     crun ceph_common.sh init-ceph.in mkcephfs.in make_version

install-data-local:
	sh -c "if [ \"$(sbindir)\" = \"/usr/sbin\" ]; then mkdir -p $(DESTDIR)/sbin ; $(install_sh_SCRIPT) -m 0755 mount.ceph $(DESTDIR)/sbin/mount.ceph ; else mkdir -p $(DESTDIR)$(sbindir) ; $(install_sh_SCRIPT) -m 0755 mount.ceph $(DESTDIR)$(sbindir)/mount.ceph ; fi"
	mkdir -p $(DESTDIR)$(sbindir)/ceph
	$(install_sh_SCRIPT) -m 0755 mkcephfs $(DESTDIR)$(sbindir)/mkcephfs
	mkdir -p $(DESTDIR)$(libdir)/ceph
	$(install_sh_SCRIPT) -m 0755 ceph_common.sh $(DESTDIR)$(libdir)/ceph/ceph_common.sh
	mkdir -p $(DESTDIR)$(sysconfdir)/ceph
	$(install_sh_SCRIPT) -m 0600 sample.ceph.conf $(DESTDIR)$(sysconfdir)/ceph/sample.ceph.conf
	$(install_sh_SCRIPT) -m 0755 fetch_config $(DESTDIR)$(sysconfdir)/ceph/sample.fetch_config
	mkdir -p $(DESTDIR)$(includedir)/ceph
	$(install_sh_SCRIPT) -m 0644 client/libceph.h $(DESTDIR)$(includedir)/ceph/libceph.h
	mkdir -p $(DESTDIR)$(includedir)/rados
	$(install_sh_SCRIPT) -m 0644 include/librados.h $(DESTDIR)$(includedir)/rados/librados.h
	$(install_sh_SCRIPT) -m 0644 include/librados.hpp $(DESTDIR)$(includedir)/rados/librados.hpp
	$(install_sh_SCRIPT) -m 0644 include/buffer.h $(DESTDIR)$(includedir)/rados/buffer.h
	$(install_sh_SCRIPT) -m 0644 include/atomic.h $(DESTDIR)$(includedir)/rados/atomic.h
	$(install_sh_SCRIPT) -m 0644 include/page.h $(DESTDIR)$(includedir)/rados/page.h
	$(install_sh_SCRIPT) -m 0644 include/crc32c.h $(DESTDIR)$(includedir)/rados/crc32c.h
	$(install_sh_SCRIPT) -m 0644 include/Spinlock.h $(DESTDIR)$(includedir)/rados/Spinlock.h
	$(install_sh_SCRIPT) -m 0644 include/assert.h $(DESTDIR)$(includedir)/rados/assert.h
	mkdir -p $(DESTDIR)$(includedir)/crush
	$(install_sh_SCRIPT) -m 0644 crush/hash.h $(DESTDIR)$(includedir)/crush/hash.h
	$(install_sh_SCRIPT) -m 0644 crush/crush.h $(DESTDIR)$(includedir)/crush/crush.h
	$(install_sh_SCRIPT) -m 0644 crush/types.h $(DESTDIR)$(includedir)/crush/types.h
	$(install_sh_SCRIPT) -m 0644 crush/mapper.h $(DESTDIR)$(includedir)/crush/mapper.h

.make_last_ver:
	./make_version --check

ceph_ver.h : $(all_sources) .make_last_ver
	./make_version

ceph_ver.c : ceph_ver.h
debug.cc : ceph_ver.h



# cleaning
clean-local:
	-rm *.so
#	-rm crush/*.cxx
#	-rm CrushWrapper.pm

# libs
libcommon_a_SOURCES = \
	ceph_ver.c \
	$(libcommon_files)

# this list ommits the ceph_ver.c file
libcommon_files = \
	auth/AuthAuthorizeHandler.cc \
	auth/AuthClientHandler.cc \
	auth/AuthSupported.cc \
	auth/cephx/CephxAuthorizeHandler.cc \
	auth/cephx/CephxClientHandler.cc \
	auth/cephx/CephxProtocol.cc \
	auth/none/AuthNoneAuthorizeHandler.cc \
	auth/Crypto.cc \
	auth/ExportControl.cc \
	auth/KeyRing.cc \
	auth/RotatingKeyRing.cc \
	common/LogClient.cc \
	msg/Message.cc \
	common/BackTrace.cc \
	common/Logger.cc \
	common/ClassLibrary.cc \
	common/Clock.cc \
	common/Timer.cc \
	common/Finisher.cc \
	common/sctp_crc32.c\
	common/assert.cc \
	common/dyn_snprintf.c \
	common/WorkQueue.cc \
	common/ConfUtils.cc \
	common/MemoryModel.cc \
	common/armor.c \
	common/base64.c \
	common/str_list.cc \
	mon/MonMap.cc \
	mon/MonClient.cc \
	osd/OSDMap.cc \
	mds/MDSMap.cc \
	common/tls.cc \
	common/common_init.cc \
	common/buffer.cc \
	common/debug.cc \
	include/ceph_fs.cc \
	include/ceph_hash.cc \
	include/ceph_strings.cc \
	include/ceph_frag.cc \
	config.cc \
	common/page.cc \
	common/lockdep.cc

libcrush_a_SOURCES = \
	crush/builder.c \
	crush/mapper.c \
	crush/crush.c \
	crush/hash.c

libmon_a_SOURCES = \
	auth/cephx/CephxKeyServer.cc \
	auth/cephx/CephxServiceHandler.cc \
	auth/AuthServiceHandler.cc \
	mon/Monitor.cc \
	mon/Paxos.cc \
	mon/PaxosService.cc \
	mon/OSDMonitor.cc \
	mon/MDSMonitor.cc \
	mon/MonmapMonitor.cc \
	mon/PGMonitor.cc \
	mon/LogMonitor.cc \
	mon/ClassMonitor.cc \
	mon/AuthMonitor.cc \
	mon/Elector.cc \
	mon/MonitorStore.cc \
	mon/MonCaps.cc

libmds_a_SOURCES = \
	mds/MDS.cc \
	mds/locks.c \
	mds/journal.cc \
	mds/Server.cc \
	mds/MDCache.cc \
	mds/Locker.cc \
	mds/Migrator.cc \
	mds/MDBalancer.cc \
	mds/CDentry.cc \
	mds/CDir.cc \
	mds/CInode.cc \
	mds/LogEvent.cc \
	mds/MDSTable.cc \
	mds/InoTable.cc \
	mds/MDSTableClient.cc \
	mds/MDSTableServer.cc \
	mds/AnchorServer.cc \
	mds/AnchorClient.cc \
	mds/SnapServer.cc \
	mds/snap.cc \
	mds/SessionMap.cc \
	mds/MDLog.cc

libos_a_SOURCES = \
	os/FileJournal.cc \
	os/FileStore.cc \
	os/JournalingObjectStore.cc

libosd_a_SOURCES = \
	osd/PG.cc \
	osd/ReplicatedPG.cc \
	osd/Ager.cc \
	osd/OSD.cc \
	osd/OSDCaps.cc \
        common/ClassHandler.cc
#	osd/RAID4PG.cc

libosdc_a_SOURCES = \
	osdc/Objecter.cc \
	osdc/ObjectCacher.cc \
	osdc/Filer.cc \
	osdc/Journaler.cc

libclient_a_SOURCES = \
	client/Client.cc \
	client/SyntheticClient.cc \
	client/Trace.cc

# headers... and everything else we want to include in a 'make dist' 
# that autotools doesn't magically identify.
noinst_HEADERS = \
	auth/cephx/CephxAuthorizeHandler.h\
	auth/cephx/CephxKeyServer.h\
	auth/cephx/CephxProtocol.h\
	auth/cephx/CephxClientHandler.h\
	auth/cephx/CephxServiceHandler.h\
	auth/none/AuthNoneAuthorizeHandler.h\
	auth/none/AuthNoneClientHandler.h\
	auth/none/AuthNoneServiceHandler.h\
	auth/none/AuthNoneProtocol.h\
	auth/Auth.h\
	auth/AuthSupported.h\
	auth/AuthClientHandler.h\
	auth/AuthServiceHandler.h\
	auth/AuthAuthorizeHandler.h\
	auth/KeyRing.h\
	auth/RotatingKeyRing.h\
	auth/Crypto.h\
	auth/ExportControl.h\
	ceph_ver.h \
        client/Client.h\
        client/SyntheticClient.h\
        client/Trace.h\
        client/fuse.h\
        client/fuse_ll.h\
	client/ioctl.h\
	client/libceph.h\
        client/hadoop/CephFSInterface.h\
	cls_acl.cc\
	cls_crypto.cc\
	cm.txt\
        common/arch.h\
        common/armor.h\
	common/base64.h\
	common/debug.h\
	common/lockdep.h\
	common/BackTrace.h\
        common/ClassVersion.h\
        common/Clock.h\
        common/common_init.h\
        common/Cond.h\
        common/dyn_snprintf.h\
        common/ClassHandler.h\
        common/ConfUtils.h\
        common/DecayCounter.h\
        common/Finisher.h\
        common/LogType.h\
        common/Logger.h\
        common/MemoryModel.h\
        common/Mutex.h\
        common/RWLock.h\
        common/Semaphore.h\
        common/Thread.h\
        common/Throttle.h\
        common/Timer.h\
        common/tls.h\
	common/WorkQueue.h\
	common/LogClient.h\
        config.h\
        crush/CrushWrapper.h\
        crush/CrushWrapper.i\
        crush/builder.h\
        crush/crush.h\
        crush/grammar.h\
        crush/hash.h\
        crush/mapper.h\
        crush/sample.txt\
        crush/types.h\
	fetch_config\
	include/ClassLibrary.h\
        include/Context.h\
	include/CompatSet.h\
        include/Distribution.h\
	include/LogEntry.h\
	include/Spinlock.h\
	include/assert.h\
        include/atomic.h\
        include/bitmapper.h\
        include/blobhash.h\
        include/buffer.h\
        include/byteorder.h\
	include/ceph_frag.h\
        include/ceph_fs.h\
        include/ceph_hash.h\
	include/color.h\
	include/crc32c.h\
        include/cstring.h\
        include/encoding.h\
        include/err.h\
        include/error.h\
        include/filepath.h\
        include/frag.h\
        include/hash.h\
        include/intarith.h\
        include/interval_set.h\
        include/inttypes.h\
	include/librados.h\
	include/librados.hpp\
        include/lru.h\
	include/msgr.h\
        include/nstring.h\
        include/object.h\
        include/page.h\
        include/rangeset.h\
	include/rados.h\
	include/rbd_types.h\
        include/statlite.h\
	include/str_list.h\
        include/triple.h\
        include/tstring.h\
        include/types.h\
        include/uofs.h\
        include/utime.h\
        include/dlist.h\
        include/elist.h\
        include/xlist.h\
	mds/locks.c\
	mds/locks.h\
        mds/Anchor.h\
        mds/AnchorClient.h\
        mds/AnchorServer.h\
        mds/CDentry.h\
        mds/CDir.h\
        mds/CInode.h\
        mds/Capability.h\
        mds/InoTable.h\
        mds/LocalLock.h\
        mds/Locker.h\
        mds/LogEvent.h\
        mds/LogSegment.h\
        mds/MDBalancer.h\
        mds/MDCache.h\
        mds/MDLog.h\
        mds/MDS.h\
        mds/MDSMap.h\
	mds/MDSTable.h\
	mds/MDSTableServer.h\
	mds/MDSTableClient.h\
        mds/Migrator.h\
        mds/ScatterLock.h\
        mds/Server.h\
        mds/SessionMap.h\
        mds/SimpleLock.h\
        mds/SnapClient.h\
        mds/SnapServer.h\
        mds/events/ECommitted.h\
        mds/events/EExport.h\
        mds/events/EFragment.h\
        mds/events/EImportFinish.h\
        mds/events/EImportStart.h\
        mds/events/EMetaBlob.h\
        mds/events/EOpen.h\
        mds/events/ESession.h\
        mds/events/ESessions.h\
        mds/events/ESlaveUpdate.h\
        mds/events/EString.h\
        mds/events/ESubtreeMap.h\
	mds/events/ETableClient.h\
	mds/events/ETableServer.h\
        mds/events/EUpdate.h\
        mds/mds_table_types.h\
        mds/mdstypes.h\
        mds/snap.h\
        messages/MAuth.h\
        messages/MAuthReply.h\
	messages/MCacheExpire.h\
	messages/MClass.h\
	messages/MClassAck.h\
        messages/MClientCaps.h\
        messages/MClientCapRelease.h\
        messages/MClientLease.h\
        messages/MClientReconnect.h\
        messages/MClientReply.h\
        messages/MClientRequest.h\
        messages/MClientRequestForward.h\
        messages/MClientSession.h\
        messages/MClientSnap.h\
        messages/MDentryLink.h\
        messages/MDentryUnlink.h\
        messages/MDirUpdate.h\
        messages/MDiscover.h\
        messages/MDiscoverReply.h\
        messages/MExportCaps.h\
        messages/MExportCapsAck.h\
        messages/MExportDir.h\
        messages/MExportDirAck.h\
        messages/MExportDirCancel.h\
        messages/MExportDirDiscover.h\
        messages/MExportDirDiscoverAck.h\
        messages/MExportDirFinish.h\
        messages/MExportDirNotify.h\
        messages/MExportDirNotifyAck.h\
        messages/MExportDirPrep.h\
        messages/MExportDirPrepAck.h\
        messages/MExportDirWarning.h\
        messages/MExportDirWarningAck.h\
        messages/MGenericMessage.h\
        messages/MGetPoolStats.h\
        messages/MGetPoolStatsReply.h\
        messages/MHeartbeat.h\
        messages/MInodeFileCaps.h\
        messages/MLock.h\
	messages/MLog.h\
	messages/MLogAck.h\
        messages/MMDSBeacon.h\
        messages/MMDSBoot.h\
        messages/MMDSCacheRejoin.h\
	messages/MMDSLoadTargets.h\
        messages/MMDSFragmentNotify.h\
        messages/MMDSMap.h\
        messages/MMDSResolve.h\
        messages/MMDSResolveAck.h\
        messages/MMDSSlaveRequest.h\
        messages/MMDSTableRequest.h\
        messages/MMonCommand.h\
        messages/MMonCommandAck.h\
        messages/MMonElection.h\
        messages/MMonGetMap.h\
	messages/MMonGlobalID.h\
        messages/MMonMap.h\
        messages/MMonObserve.h\
        messages/MMonObserveNotify.h\
        messages/MMonPaxos.h\
        messages/MMonSubscribe.h\
        messages/MMonSubscribeAck.h\
        messages/MOSDAlive.h\
        messages/MOSDBoot.h\
        messages/MOSDFailure.h\
        messages/MOSDMap.h\
        messages/MOSDOp.h\
        messages/MOSDOpReply.h\
        messages/MOSDPGCreate.h\
        messages/MOSDPGInfo.h\
        messages/MOSDPGLog.h\
        messages/MOSDPGNotify.h\
        messages/MOSDPGQuery.h\
        messages/MOSDPGRemove.h\
        messages/MOSDPGTemp.h\
        messages/MOSDPGTrim.h\
        messages/MOSDPing.h\
	messages/MOSDScrub.h\
        messages/MOSDSubOp.h\
        messages/MOSDSubOpReply.h\
        messages/MPGStats.h\
        messages/MPGStatsAck.h\
        messages/MPing.h\
	messages/MPoolOp.h\
	messages/MPoolOpReply.h\
        messages/MRemoveSnaps.h\
	messages/MRoute.h\
	messages/MForward.h\
        messages/MStatfs.h\
        messages/MStatfsReply.h\
	messages/PaxosServiceMessage.h\
	mon/AuthMonitor.h\
        mon/ClassMonitor.h\
        mon/Elector.h\
	mon/LogMonitor.h\
        mon/MDSMonitor.h\
	mon/MonmapMonitor.h\
        mon/MonCaps.h\
        mon/MonClient.h\
        mon/MonMap.h\
        mon/Monitor.h\
        mon/MonitorStore.h\
        mon/OSDMonitor.h\
        mon/PGMap.h\
        mon/PGMonitor.h\
        mon/Paxos.h\
        mon/PaxosService.h\
        mon/Session.h\
        mon/mon_types.h\
	mount/canonicalize.c\
	mount/mtab.c\
        msg/Dispatcher.h\
        msg/FakeMessenger.h\
        msg/Message.h\
        msg/Messenger.h\
        msg/SimpleMessenger.h\
        msg/msg_types.h\
        msg/tcp.cc\
        msg/tcp.h\
	objclass/objclass.h\
	os/btrfs_ioctl.h\
        os/BDBMap.h\
        os/Fake.h\
        os/FakeStoreBDBCollections.h\
        os/FileJournal.h\
        os/FileStore.h\
        os/Journal.h\
        os/JournalingObjectStore.h\
        os/ObjectStore.h\
        osbdb/OSBDB.h\
        osd/Ager.h\
        osd/OSD.h\
        osd/OSDCaps.h\
        osd/OSDMap.h\
        osd/ObjectVersioner.h\
        osd/PG.h\
        osd/PGLS.h\
        osd/RAID4PG.h\
        osd/ReplicatedPG.h\
        osd/osd_types.h\
	osdc/rados_bencher.h\
        osdc/Blinker.h\
        osdc/Filer.h\
        osdc/Journaler.h\
        osdc/ObjectCacher.h\
        osdc/Objecter.h\
	rgw/rgw_access.h\
	rgw/rgw_acl.h\
	rgw/rgw_fs.h\
	rgw/rgw_rados.h\
	rgw/rgw_op.h\
	rgw/rgw_rest.h\
	rgw/rgw_common.h\
	rgw/rgw_user.h\
	sample.ceph.conf

all_sources = $(cmon_SOURCES) $(ceph_SOURCES) $(mkmonfs_SOURCES) $(cauthtool_SOURCES) $(monmaptool_SOURCES) \
	$(crushtool_SOURCES) $(osdmaptool_SOURCES) $(cconf_SOURCES) $(mount_ceph_SOURCES) $(cmds_SOURCES) \
	$(dumpjournal_SOURCES) $(cosd_SOURCES) $(dupstore_SOURCES) $(streamtest_SOURCES) $(csyn_SOURCES)  \
	$(testmsgr_SOURCES) $(cfuse_SOURCES) $(fakefuse_SOURCES) $(psim_SOURCES) \
	$(libcrush_so_a_SOURCES) $(libcommon_files) $(libcrush_a_SOURCES)  \
	$(libmon_a_SOURCES) $(libmds_a_SOURCES) $(libos_a_SOURCES) $(libosd_a_SOURCES) \
	$(libosdc_a_SOURCES) $(libclient_a_SOURCES)
