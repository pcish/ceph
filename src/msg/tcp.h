// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
#ifndef __TCP_H
#define __TCP_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

using std::ostream;

inline ostream& operator<<(ostream& out, const sockaddr_storage &ss)
{
  char buf[NI_MAXHOST] = { 0 };
  char serv[20] = { 0 };
  getnameinfo((struct sockaddr *)&ss, sizeof(ss), buf, sizeof(buf),
	      serv, sizeof(serv),
	      NI_NUMERICHOST | NI_NUMERICSERV);
  return out //<< ss.ss_family << ":"
	     << buf << ':' << serv;
}

inline ostream& operator<<(ostream& out, const sockaddr_in &ss)
{
  char buf[NI_MAXHOST] = { 0 };
  char serv[20] = { 0 };
  getnameinfo((struct sockaddr *)&ss, sizeof(ss), buf, sizeof(buf),
	      serv, sizeof(serv),
	      NI_NUMERICHOST | NI_NUMERICSERV);
  return out //<< ss.sin_family << ":"
	     << buf << ':' << serv;
}


extern int tcp_read(int sd, char *buf, int len);
extern int tcp_write(int sd, const char *buf, int len);


extern int tcp_hostlookup(char *str, sockaddr_in& ta);

inline bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
  return strncmp((const char*)&a, (const char*)&b, sizeof(a)) == 0;
}
inline bool operator!=(const sockaddr_in& a, const sockaddr_in& b) {
  return strncmp((const char*)&a, (const char*)&b, sizeof(a)) != 0;
}


#endif
