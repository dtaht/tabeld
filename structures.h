#include "ttypes.h"

/* Various defines */

#define IS_V6 0
#define IS_V4 1
#define IS_SS 2
#define IS_MARTIAN 4 // link local and martian I think I can filter out on input always
#define IS_LL 8
#define IS_DEFAULT 16
#define IS_FILTERED 32
#define IS_ENABLED 64 // if it wasn't for having space, I'd not do this

#define IS_V4SS (IS_V4 | IS_SS)
#define RTE_TAG_SIZE 7

/* On the way out we have 4 memory pools that we hopefully never see, because they are abstracted out */

/* On the way in we have a C11 anonymous union */

typedef u128 addr6;
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
  addr4 dst;
  u8  dst_plen;
  nh  via;
  hash hh;
};

struct rte4s {
  addr4 src;
  addr4 dst;
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
  int metric;
};

struct metric {
  int base;
};

struct proto_route {
  rte_idx r;
  nh via;
};


