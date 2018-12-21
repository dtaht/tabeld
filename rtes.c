// SPDX-WHATEVER
#ifndef RTES_C
#define RTES_C

#include <stdio.h>
#include "structures.h"

/* This is an attempt at a more compressed data structure for route lookups.
   Based on the snark, I don't think I ever need to store more than 3 routes
   per destination. Maybe it's four. Maybe it's a couple per interface.
   Don't know. It certainly is more
   complicated to do it this way, but appears cache-friendly. We'll see. */

#define unlikely(a) (a)

typedef struct {
	union {
	  u32 v;
	  struct {
	    u16 rid;
	    u16 nh;
	  };
	};
} rte_tbls;

// flags?

typedef struct {
  union {
    u64       v;
    struct {
      u16  add_metric;
      u16  cost;
      u16  ref_metric;
      u16  metric;
    };
  };
} metrics;

// Route lookup query

#define MAX_RTES 3

typedef struct {
	rte_idx r;
	rte_tbls rid[MAX_RTES];
	metrics  metric[MAX_RTES];
} rtes;

typedef struct {
	rte_idx r;
	rte_tbls rid;
	metrics  metric;
} rtq;

extern rtes *rlookup(rte_idx q);
extern rtes *rinsert(rtq q);

void tell() {
  printf("Something changed you should have a hook for\n");
}

#define CMPM(a,b) ( a.v > b.v ? 1 : a.v < b.v ? -1 : 0)

bool update_rtes(rtq q)
{
  int i;
  int rc;
  bool changed = false;
  rtes *r = rlookup(q.r);
  
  if(unlikely(!r)) {
    rinsert(q);
    tell();
    return true;
  }

  // I prefer the switch based idiom 
  for(i = 0; i < MAX_RTES && changed == false ; i++) {
    if (r->rid[i].v == q.rid.v) {
      rc = CMPM(r->metric[i], q.metric);
      if(rc == 1) {
	/* if it got better compare with prev */
	if(i == 0) {
	  r->rid[i] = q.rid;
	  r->metric[i] = q.metric;
	  tell();
	  changed = true;
	} else {
	    for (int j = 0; j < i; j++) {
	      // FIXME;
	      // We don't need to tell anyone else if we didn't change
	    }
	  }
	break; 
	/* if it got worse compare with next */
      } else {
	if(rc == -1) {
	  for (int j = i; j < MAX_RTES; j++) {
	    // FIXME
	    // We don't need to tell anyone else if we didn't change the main rte
	  }
	}
      }
      break;
    } else {
      // if it points nowhere, add
      if (r->rid[i].v == 0) {
	r->rid[i] = q.rid;
	r->metric[i] = q.metric;
	if(i == 0)  {
	  r->r = q.r;
	  tell();
	}
	changed = true;
      }
    }
  }
  return changed;
}

int main(int argc, char **argv)
{
  metrics e = { .add_metric = 96, .metric = 32, .cost = 0, .ref_metric = 0 };
  metrics w = { .add_metric = 128, .metric = 128, .cost = 0, .ref_metric = 0 };
  
}

#endif
