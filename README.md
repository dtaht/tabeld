# tabeld project

An exploration of C11, structure assignment, sse, a better netlink
library, and hashed lookups.

## kdump

A highly threaded route monitoring utility

## rtod

A high speed route injection utility

## tabled

One day a drop in replacement for the babeld daemon

## An exploration of "pointerless programming"

the C idiom slings pointers around. The GO and rust idioms don't.
C++ doesn't, much either.

What if we tried to write in pure C something as lacking in pointers
as possible?

My first attempt here, has no pointers in it. My hope was, also, that
I'd be able to sling around various values in xmm registers. I'm using
a lot of them, but not effectively.  Perhaps exploding certain calls
from the structure into that will be a win.

## Limits

2m routes
64k next-hops
64k router_ids

## Vectorizer

## C11 issues

# nobody implements c11threads dang it

# going to use libnl

# Might leverage libnn

# Hashes?

# Memory pools

# More vectorization

## Threaded design

Let's go overboard on threads, like we would with go channels.

1 thread per interface
1 thread per router_id?
1 thread for multicast
1 or more threads for monitoring
2 thread for ipv4 kernel routes (r/w)
2 threads for ipv6 kernel routes (r/w)
