/* Threaded route dumping/monitoring tool.

   What I wanted to do was utilize recvmmsg, netlink, and threads, in C, to rapidly encode
   routes into 4 differently sized tables */

/* return (a < b) ? -1 : (a > b); fast version */

/* "Every piece of knowledge must have a single, unambiguous, authoritative representation within a system" */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>
#include <endian.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "structures.h"

// Cheap and dirty memory-pool-ish implementation

#define TEST_ROUTES 128

rte   poolv6raw[TEST_ROUTES];
rte4s poolv4s[TEST_ROUTES];
rte4  poolv4[TEST_ROUTES];
rte6s poolv6s[TEST_ROUTES];
rte6  poolv6[TEST_ROUTES];

nexthop6 poolnh6[TEST_ROUTES];
nexthop poolnh[TEST_ROUTES];

const addr6 v4prefix_mask = { .b = 
			 {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 } };

const addr6 v4local = { .b = 
			 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 127, 0, 0, 1 } };

const addr6 v4prefix = { .b = 
			 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 } };

const addr6 llprefix = { .b = {0xFE, 0x80} };

int
linklocal(const addr6 address)
{
  return address.d[0] == llprefix.d[0];
}

/*
I wrote this once correctly elsewhere, fixme
int
v4mapped_sse(const addr6 address)
{ addr6 m = address & v4_prefix_mask;
  return is_not_zero(xor(m,v4prefix));
}
*/
  
int
v4mapped(const addr6 address)
{
  return address.d[0] == 0 && address.z == v4prefix.z; // doing it this way to make sure I got endianess right
}


static inline rte from_rte6s(rte6s r)
{
  rte route = { .src = r.src, .dst = r.dst, .src_plen = r.src_plen, .dst_plen = r.dst_plen };
  return route;
}

static inline rte from_rte6(rte6 r)
{
  rte route = { .dst = r.dst, .dst_plen = r.dst_plen };
  return route;
}

static inline rte from_rte4s(rte4s r)
{
  rte route = { .src.w = r.src.v, .dst.w = r.dst.v, .dst.z = htobe32(0xFFFF), .src.z = htobe32(0xFFFF),
		.src_plen = r.src_plen + 96, .dst_plen = r.dst_plen + 96 };
  return route;
}

static inline rte from_rte4(rte4 r)
{
  rte route = { .dst.w = r.dst.v, .dst.z = htobe32(0xFFFF), .dst_plen = r.dst_plen + 96 };
  return route;
}

static rte getroute(rte_idx r)
{
  u32 idx = r >> RTE_TAG_SIZE;
  rte route;
  switch(r & 3) {
  case IS_V6: route = from_rte6(poolv6[idx]); break;
  case IS_V4: route = from_rte4(poolv4[idx]); break;
  case IS_SS: route = from_rte6s(poolv6s[idx]); break;
  case IS_V4SS: route = from_rte4s(poolv4s[idx]); break;
  }
  return route;
}

static inline rte6s rte2_rte6s(rte r)
{
  rte6s route = { .src = r.src, .dst = r.dst, .src_plen = r.src_plen, .dst_plen = r.dst_plen };
  route.hh = 0;
  return route;
}

static inline rte6 rte2_rte6(rte r)
{
  rte6 route = { .dst = r.dst, .dst_plen = r.dst_plen };
  route.hh = 0;
  return route;
}

static inline rte4s rte2_rte4s(rte r)
{
  rte4s route = { .src.v = r.src.w, .dst.v = r.dst.w, .src_plen = r.src_plen - 96, .dst_plen = r.dst_plen - 96 };
  route.hh = 0;
  return route;
}

static inline rte4 rte2_rte4(rte r)
{
  rte4 route = { .dst.v = r.dst.w, .dst_plen = r.dst_plen - 96 };
  route.hh = 0;
  return route;
}

static inline bool check_rte_ss(rte r)
{
  if ((r.src.d[0] | r.src.d[1]) != 0) return true;
  return false;
}

static inline bool check_rte_v6(rte r)
{
  //  return !v4mapped(r.dst);
     
  if(r.dst.z == htobe32(0xFFFF) && r.dst.d[0] == 0L) return false;
  return true;
}

const rte4 martians4[] = {
  { .dst.b = { 127, 0, 0, 0 }, .dst_plen = 8 },
  { .dst.b = { 255, 255, 255, 255 }, .dst_plen = 0 },
  { .dst.b = { 224, 0, 0, 0 }, .dst_plen = 4 },
  //  { .dst.b = { 240, 0, 0, 0 }, .dst_plen = 4 },
  { .dst.b = { 0, 0, 0, 0 }, .dst_plen = 8 }
};

// FIXME, what if we get 225.0.0.0/3 ? or the 0.0.0.0/0 default?
  
bool martian_check4(addr4 a, u8 plen)
{
  for(int i = 0; i < 5; i++) {
    if ( martians4[i].dst.v == a.v && plen >= martians4[i].dst_plen) return true;
  }
  return false;
}

bool martian_check6(addr6 a, u8 plen)
{
  return false;
}

// FIXME -1 should be a martian
// -2

static int rte_classify(rte route)
{
  // return 1;
  //   return check_rte_v6(route) | ((check_rte_ss(route)) << 1);

  if(check_rte_v6(route)) {
    if(check_rte_ss(route)) return IS_SS;
    else return 0;
  } else {
    if(check_rte_ss(route)) return IS_V4SS;
    else return IS_V4;
  }
}

#define insaddr(X) _Generic((X),		\
			    rte4: insaddr4,	\
			    default: insaddr_generic,  \
			    rte6: insaddr6,	       \
			    rte4s: insaddr4s,	       \
			    rte6s: insaddr6s	       \
)(X)


rte_idx insaddr4(rte4 route) {
  static int ctr = 0;
  poolv4[ctr] = route;
  return ctr++ << RTE_TAG_SIZE;
}

rte_idx insaddr4s(rte4s route) {
  static int ctr = 0;
  poolv4s[ctr] = route;
  return ctr++ << RTE_TAG_SIZE;
}

rte_idx insaddr6(rte6 route) {
  static int ctr = 0;
  poolv6[ctr] = route;
  return ctr++ << RTE_TAG_SIZE;
}

rte_idx insaddr6s(rte6s route) {
  static int ctr = 0;
  poolv6s[ctr] = route;
  return ctr++ << RTE_TAG_SIZE;
}

/* This was a misguided attempt at trying generics

rte_idx insaddr_generic(rte route)
{
  rte_idx r = rte_classify(route);
  switch(r & 2) {
  case 0: r |= insaddr(rte2_rte6(route)); break;
  case 1: r |= insaddr(rte2_rte6s(route)); break;
  case 2: r |= insaddr(rte2_rte4(route)); break;
  case 3: r |= insaddr(rte2_rte4s(route)); break;
  }
  return r;
}
*/

rte_idx insaddr_generic(rte route)
{
  rte_idx r = rte_classify(route);
  switch(r & 3) {
  case IS_V6: r = r | insaddr6(rte2_rte6(route)); break;
  case IS_V4: r = r | insaddr4(rte2_rte4(route)); break;
  case IS_SS: r = r | insaddr6s(rte2_rte6s(route)); break;
  case IS_V4SS: r = r | insaddr4s(rte2_rte4s(route)); break;
  }
  return r;
}

// Switch to epollish events here

/* 
int ksockets[] = { RTNLGRP_IPV6_ROUTE, RTNLGRP_IPV4_ROUTE, RTNLGRP_LINK,
		   RTNLGRP_IPV4_IFADDR, RTNLGRP_IPV6_IFADDR,
		   RTNLGRP_IPV4_RULE, RTNLGRP_IPV6_IFADDR };

int ksocket[8];

int
kernel_setup_sockets(int setup)
{
    int rc;

    if(setup) {
      for(int i = 0; i < 7; i++) {
      rc = netlink_socket(&ksocket[i], rtnlgrp_to_mask(ksockets[i]));
      if(rc < 0) {
            perror("netlink_socket(_ROUTE | _LINK | _IFADDR | _RULE)");
            kernel_socket = -1;
            return -1;
        }

        kernel_socket = nl_listen.sock;

        return 1;

    } else {

        close(nl_listen.sock);
        nl_listen.sock = -1;
        kernel_socket = -1;

        return 1;

    }
}

*/
    
// extern print_test(void *);

/* epoll */

/*

Note: the logger thread *only* will be fiddling with output. 

format_route();
format_addr()
format_prefix();

kernel_dump_v4();
kernel_dump_v6();

// EBPF
// log is a thread
// we cannot block

mon_route_v4();
mon_route_v6();
mon_iface();
mon_table();

*/


// callbacks();

/*
int
martian_prefix(const unsigned char *prefix, int plen)
{
    return
        (plen >= 8 && prefix[0] == 0xFF) ||
        (plen >= 10 && prefix[0] == 0xFE && (prefix[1] & 0xC0) == 0x80) ||
        (plen >= 128 && memcmp(prefix, zeroes, 15) == 0 &&
         (prefix[15] == 0 || prefix[15] == 1)) ||
        (plen >= 96 && v4mapped(prefix) &&
         ((plen >= 104 && (prefix[12] == 127 || prefix[12] == 0)) ||
          (plen >= 100 &&
	    ((prefix[12] & 0xF0) == 0xF0 && 
	     (prefix[12] == 0xFF && prefix[13] == 0xFF && prefix[14] == 0xFF &&
		 prefix[15] == 0xFF)
		 && ((prefix[12] & 0xE0) == 0xE0)))));
}

*/

const char *
format_address6(addr6 address)
{
    static char buf[4][INET6_ADDRSTRLEN];
    static int i = 0;
    i = (i + 1) % 4;
    inet_ntop(AF_INET6, &address.b, buf[i], INET6_ADDRSTRLEN);
    return buf[i];
}

const char *
format_address4(addr4 address)
{
    static char buf[4][INET6_ADDRSTRLEN];
    static int i = 0;
    i = (i + 1) % 4;
    inet_ntop(AF_INET, &address, buf[i], INET6_ADDRSTRLEN);
    return buf[i];
}

const char *
format_address(addr6 address)
{
    static char buf[4][INET6_ADDRSTRLEN];
    static int i = 0;
    i = (i + 1) % 4;
    if(v4mapped(address))
        inet_ntop(AF_INET, &address.z, buf[i], INET6_ADDRSTRLEN);
    else
        inet_ntop(AF_INET6, &address.b, buf[i], INET6_ADDRSTRLEN);
    return buf[i];
}

const char *
format_prefix4(addr4 address, u8 plen)
{
    static char buf[4][INET6_ADDRSTRLEN + 4];
    static int i = 0;
    int n;
    i = (i + 1) % 4;
    inet_ntop(AF_INET, &address, buf[i], INET6_ADDRSTRLEN);
    n = strlen(buf[i]);
    snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
    return buf[i];
}

const char *
format_prefix6(addr6 address, u8 plen)
{
    static char buf[4][INET6_ADDRSTRLEN + 4];
    static int i = 0;
    int n;
    i = (i + 1) % 4;
    inet_ntop(AF_INET6, &address.b, buf[i], INET6_ADDRSTRLEN);
    n = strlen(buf[i]);
    snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
    return buf[i];
}

/* FIXME at some point a test of xmm-ness

const char *
format_prefixes6(addr6 address, u8 plen, addr6 address2, u8 plen2)
{
  static char buf[4][2*(INET6_ADDRSTRLEN + 4)];
  static int i = 0;
  int n;
  i = (i + 1) % 4;
  inet_ntop(AF_INET6, &address.b, buf[i], INET6_ADDRSTRLEN);
  n = strlen(buf[i]);
  snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
  inet_ntop(AF_INET6, &address2.b, buf[i] + n + 4, INET6_ADDRSTRLEN);
  n = strlen(buf[i]); // inefficient fixme
  snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
  return buf[i];
}

*/

const char *
format_prefix(addr6 address, u8 plen)
{
    static char buf[4][INET6_ADDRSTRLEN + 4];
    static int i = 0;
    int n;
    i = (i + 1) % 4;
    if(plen >= 96 && v4mapped(address)) {
        inet_ntop(AF_INET, &address.z, buf[i], INET6_ADDRSTRLEN);
        n = strlen(buf[i]);
        snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen - 96);
    } else {
        inet_ntop(AF_INET6, &address.b, buf[i], INET6_ADDRSTRLEN);
        n = strlen(buf[i]);
        snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
    }
    return buf[i];
}

// Backward compatability stuff

int
v4mapped_str(const unsigned char *address)
{
    return memcmp(address, &v4prefix.b[0], 12) == 0;
}

const char *
format_prefix_str(const unsigned char *prefix, unsigned char plen)
{
    static char buf[4][INET6_ADDRSTRLEN + 4];
    static int i = 0;
    int n;
    i = (i + 1) % 4;
    if(plen >= 96 && v4mapped_str(prefix)) {
        inet_ntop(AF_INET, prefix + 12, buf[i], INET6_ADDRSTRLEN);
        n = strlen(buf[i]);
        snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen - 96);
    } else {
        inet_ntop(AF_INET6, prefix, buf[i], INET6_ADDRSTRLEN);
        n = strlen(buf[i]);
        snprintf(buf[i] + n, INET6_ADDRSTRLEN + 4 - n, "/%d", plen);
    }
    return buf[i];
}

/* Struct return ABI is different */

#include <stdio.h>

// route_format(rte_idx r)

void route_type (rte_idx r)
{
  int i = r >> RTE_TAG_SIZE;
  switch(r & 3) {
  case IS_V6: {
    rte6 r1 = poolv6[i];
    printf("Route %d %s is an IPv6 route\n", i,
	   format_prefix6(r1.dst, r1.dst_plen)); }
    break;
  case IS_SS: {
    rte6s r1 = poolv6s[i];
    printf("Route %d from %s %s is an IPv6 SADR route\n", i,
	   format_prefix6(r1.src, r1.src_plen),
	   format_prefix6(r1.dst, r1.dst_plen)); }
    break;
  case IS_V4: {
    rte4 r1 = poolv4[i];
    printf("Route %d %s is an IPv4 route\n", i,
	   format_prefix4(r1.dst, r1.dst_plen)); } break;
  case IS_V4SS: {
    rte4s r1 = poolv4s[i];
    printf("Route %d from %s %s is an IPv4 SADR route\n", i,
	   format_prefix4(r1.src, r1.src_plen),
	   format_prefix4(r1.dst, r1.dst_plen)); }
    break;
  }
		   
}

void route_typeless (rte_idx r)
{
  rte r1 = getroute(r);
  printf("Route %d %s to %s is a %s %s route\n", r >> RTE_TAG_SIZE,
	 format_prefix(r1.src, r1.src_plen), format_prefix(r1.dst, r1.dst_plen),
	 ((r & IS_V4) != 0) ? "v4" : "v6", ((r & IS_SS) != 0) ? "SADR" : "not SADR");	
}


int main(int argc, char **argv) {
  /* Test vectors for ipv6 checking */
  rte_idx r[8];

  rte v6 = { .dst.b[0] = 0xfc, .dst.b[15] = 1, .dst_plen = 7 };
  rte v6s = { .src.b[0] = 0xfd,
	      .dst.f[0] = htobe32(0xfd999999),
	      .dst_plen = 64, .src_plen = 64 };

  const rte v4 = {
    .dst_plen = 128,
    .dst.b = 
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 255, 255,255, 254 } };

  const rte v4s = {
    .src_plen = 96 + 4, .dst_plen = 96 + 32,
    .src.b = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 240, 0, 0, 0 },
    .dst.b = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 255, 255, 255, 254 }
  };


  printf("V4local looks like: %s\n", format_prefix_str(&v4local.b[0], 96 ));
  printf("V4mapped looks like: %s\n", format_prefix_str(&v4prefix.b[0], 96 ));
  printf("LL looks like: %s\n", format_prefix_str(&llprefix.b[0], 64));

  printf("Vs mine\n");
  printf("V4 looks like: %s\n", format_prefix_str(&v4.dst.b[0], v4.dst_plen));
  printf("V6 looks like: %s\n", format_prefix_str(&v6.dst.b[0], v6.dst_plen));
  printf("V4SS looks like: from %s %s\n", format_prefix_str(&v4s.src.b[0], v4s.src_plen), format_prefix_str(&v4s.dst.b[0], v4s.dst_plen));
  printf("V6SS looks like: from %s %s\n", format_prefix_str(&v6s.src.b[0], v6s.src_plen), format_prefix_str(&v6s.dst.b[0], v6s.dst_plen));

/*  r[6] = insaddr(v4prefix);
  r[5] = insaddr(llprefix.src);
  r[4] = insaddr(v4local.src);
*/
  printf("pool %d\n", r[3] = insaddr(v4));
  printf("pool %d\n", r[2] = insaddr(v4s));
  printf("pool %d\n", r[1] = insaddr(v6));
  printf("pool %d\n", r[0] = insaddr(v6s));

  for(int i = 0; i < 4; i++) {
    route_type(r[i]);
  }
}


// kill_the_martians
