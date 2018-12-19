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

#include "ttypes.h"

/* On the way in we have a C11 anonymous union */

typedef u128 addr6;
typedef u32 addr4;
typedef u32 hash;

typedef struct nexthop nexthop;
typedef struct nexthop6 nexthop6;
typedef struct rte rte;

struct nexthop {
  int ifaddr;
  addr4 dst;
};

struct nexthop6 {
  int ifaddr;
  addr6 dst;
};

/* Default babeld uses 34 bytes for all routes.
   We use 12, 16, 20, 36 presently, and that's
   assuming I keep the hash and nh */

struct rte {
  addr6 src;
  addr6 dst;
  byte src_plen;
  byte dst_plen;
};

typedef u32 rte_idx; /* route index table */
typedef u16 nh; /* nexthop index */

/* So, great, now we need a hash */

typedef struct rte4  rte4;
typedef struct rte4s rte4s;
typedef struct rte6  rte6;
typedef struct rte6s rte6s;
typedef struct kernel_route kernel_route;

struct rte4 {
  u32 dst;
  u8  dst_plen;
  nh  via;
  hash hh;
};

struct rte4s {
  u32 src;
  u32 dst;
  u8 src_plen;
  u8 dst_plen;
  nh via;
  hash hh;
};

struct rte6 {
  addr6 dst;
  u8 dst_plen;
  nh via;
  hash hh;
};

struct rte6s {
  addr6 src;
  addr6 dst;
  u8 src_plen;
  u8 dst_plen;
  nh via;
  hash hh;
};

struct kernel_route {
  rte_idx r;
  nh via;
  u16 proto; // 8?
  u32 table;
  u32 expires;
};

struct proto_route {
  rte_idx r;
  nh via;
};

struct metric {
  int base;
};

/* On the way out we have 4 memory pools that we hopefully never see, because they are abstracted out */

#define IS_V4 1
#define IS_SS 2
#define IS_MARTIAN 4 // link local and martian I think I can filter out on input always
#define IS_LL 8
#define IS_DEFAULT 16
#define IS_FILTERED 32
#define IS_ENABLED 64 // if it wasn't for having space, I'd not do this

#define RTE_TAG_SIZE 7

// Cheap and dirty memory-pool-ish implementation

#define TEST_ROUTES 128

rte   poolv6raw[TEST_ROUTES];
rte4s poolv4s[TEST_ROUTES];
rte4  poolv4[TEST_ROUTES];
rte6s poolv6s[TEST_ROUTES];
rte6  poolv6[TEST_ROUTES];

nexthop6 poolnh6[TEST_ROUTES];
nexthop poolnh[TEST_ROUTES];
			
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
  rte route = { .src.w = r.src, .dst.w = r.dst, .dst.z = htobe32(0xFFFF), .src.z = htobe32(0xFFFF),
		.src_plen = r.src_plen, .dst_plen = r.dst_plen };
  return route;
}

static inline rte from_rte4(rte4 r)
{
  rte route = { .dst.w = r.dst, .dst.z = htobe32(0xFFFF), .dst_plen = r.dst_plen };
  return route;
}

static rte getroute(rte_idx r)
{
  u32 idx = r >> RTE_TAG_SIZE;
  rte route;
  switch(r & 2) {
  case 0: route = from_rte6(poolv6[idx]); break;
  case 1: route = from_rte6s(poolv6s[idx]); break;
  case 2: route = from_rte4(poolv4[idx]); break;
  case 3: route = from_rte4s(poolv4s[idx]); break;
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
  rte4s route = { .src = r.src.w, .dst = r.dst.w, .src_plen = r.src_plen, .dst_plen = r.dst_plen };
  route.hh = 0;
  return route;
}

static inline rte4 rte2_rte4(rte r)
{
  rte4 route = { .dst = r.dst.w, .dst_plen = r.dst_plen };
  route.hh = 0;
  return route;
}

inline bool check_rte_ss(rte r)
{
  if (r.src.d[0] | r.src.d[1] != 0) return 0; // actually I don't know how to encode this yet
  return 1;
}

inline int check_rte_v4(rte r)
{
  if(r.dst.z == htobe32(0xFFFF) && r.dst.d == 0L) return 1;
  return 0;
}
  
bool martian_check4(addr4 a, u8 plen)
{
}

bool martian_check6(addr6 a, u8 plen)
{
}

static int rte_classify(rte route)
{
  return (check_rte_v4(route) << 1) | !check_rte_ss(route);
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

const addr6 v4prefix = { .b = 
			 {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0 } };

const addr6 llprefix = { .b = {0xFE, 0x80} };

int
linklocal(const addr6 address)
{
  return address.d[0] == llprefix.d[0];
}

int
v4mapped(const addr6 address)
{
  return address.d[0] == v4prefix.d[0] && address.z == v4prefix.z;
}

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

/* Struct return ABI is different */

#include <stdio.h>

// route_format(rte_idx r)

void route_type (rte_idx r)
{
  rte r1 = getroute(r);
  printf("Route %d %s to %s is a %s %s route\n", r >> RTE_TAG_SIZE,
	 format_prefix(r1.src, r1.src_plen), format_prefix(r1.dst, r1.dst_plen),
	 r & IS_V4 ? "v4" : "v6", r & IS_SS ? "SADR" : "not SADR");	
}

int main(char * argv, int argc) {
  /* Test vectors for ipv6 checking */
  rte_idx r[4];
  rte v4 = { .dst.z = htobe32(0xFFFF), .dst.b[12] = 127, .dst.b[15] = 1} ;
  rte v4s = { .src.b[13] = 127, .src.z = htobe32(0xffff), .dst.z = htobe32(0xFFFF), .dst.b[12] = 127, .dst.b[15] = 1 };
  rte v6 = { .dst.b[1] = 0xfe, .dst.b[13] = 127 };
  rte v6s = { .src.b[13] = 127, .src.x = 0x0fafa, .dst.b[12] = 127 };

  printf("pool %d\n", r[3] = insaddr(v4));
  printf("pool %d\n", r[2] = insaddr(v4s));
  printf("pool %d\n", r[1] = insaddr(v6));
  printf("pool %d\n", r[0] = insaddr(v6s));

  for(int i = 0; i < 4; i++) {
    route_type(r[i]);
  }
}


// kill_the_martians
