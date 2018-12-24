#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <getopt.h>

#include "structures.h"

#define NSEC_PER_SEC (1000000000L)

#define QSTRING "Is:H:Khr:t:n:i:m:p:j:J:R:"

struct arg {
  int count;
  struct timespec interval;
  double finterval;
  char *interface;
  char *command;
  char buffer;
  int proto;
  int expires;
  int route_table;
  int routes;
  int metric;
  int random_metric;
  int seed;
  bool invalid;
  bool ipv4;
  bool killme;
  int jitter;
  char *host;  
};

typedef struct arg args;

static const struct option long_options[] = {
	{ "jitter", required_argument	, NULL , 'j' } ,
	{ "jitter-err", required_argument	, NULL , 'J' } ,
	{ "sadr", required_argument	, NULL , 's' } ,
	{ "invalid", required_argument	, NULL , 'I' } ,
	{ "host", required_argument	, NULL , 'H' } ,
	{ "killme", required_argument	, NULL , 'K' } ,
	{ "proto"    , required_argument	, NULL , 'p' } ,
	{ "expires"    , required_argument	, NULL , 't' } ,
	{ "random-metric"    , required_argument	, NULL , 'M' } ,
	{ "metric"    , required_argument	, NULL , 'm' } ,
	{ "routes"    , required_argument	, NULL , 'r' } ,
	{ "route-table"    , required_argument	, NULL , 'R' } ,
	{ "count"    , required_argument	, NULL , 'c' } ,
	{ "interval" , required_argument	, NULL , 'I' } ,
	{ "debug"     , required_argument	, NULL , 'd' } ,
	{ "help"     , no_argument		, NULL , 'h' } ,
	{ "buffer"   , no_argument		, NULL , 'b' } ,
	{ "command"   , required_argument		, NULL , 'C' } ,
};

void usage (char *err) {
	if(err) fprintf(stderr,"%s\n",err);
	printf("rtod [options]\n");
	printf(
	       "\t-H --host override the default host to generate ip addresses from\n"
	       "\t-p --proto [50*] Kernel protocol number, for inserting routes\n"
	       "\t-t --expires [300*] Duration to keep the routes around\n"
	       "\t-r --routes [1024*] Number of routes to insert\n"
	       "\t-4 --ipv4 [prefix: 10/8*, ] Use IPv4\n"
	       "\t-6 --ipv6 [prefix: fc::/8*] Use IPv6\n"
	       "\t-R --route-table [0] Route table to use\n"
	       "\t-K --killme flush all rtod related routes and interfaces\n"
	       "\t-m --metric [default] - default metric to use\n"
	       "\t-M --random-metric [range] use random metrics in the range of x-y\n"
	       "\t-I --invalid Attempt to insert well known invalid routes (127, 224, etc)\n"
	       "\t-j --jitter apply jitter\n"
	       "\t-J --jitterer jitter in this range\n"
	       "\t-S --seed Use a fixed random seed (for repeatable tests)\n"
	       "\t-s --sadr [prefix] use source specific routes\n"
	       "\t-h --help \n"
	       "\t-b --buffer \n"
	       "\t-i --interface [rtod0*,wlan0,etc]\n"
	       "\t-c --count     [number of iterations]\n"
	       "\t-I --interval  [fractional number of seconds]\n"
	       "\t-C --command   Periodically run this command. e.g., ip route show\n");
	exit(-1);
}

static void defaults(args *a) {
	a->interface = "rtod";
	a->command = "ip -6 route show";
	a->finterval=.2;
	a->count=10;
	a->interval.tv_nsec = 0;
	a->interval.tv_sec = 0;
	a->buffer = 0;
}

int process_options(int argc, char **argv, args *o)
{
	int          option_index = 0;
	int          opt = 0;
	optind       = 1;

	while(1)
	{
		opt = getopt_long(argc, argv,
				  QSTRING,
				  long_options, &option_index);
		if(opt == -1) break;

		switch (opt)
		{
		case 'c': o->count = strtoul(optarg,NULL,10);  break;
		case 'I': o->finterval = strtod(optarg,NULL); break;
		case 'C': o->command = optarg; break;
		case 'H': o->host = optarg; break;
		case 'i': o->interface = optarg; break;
		case 'b': o->buffer = true; break;
		case 'r': o->routes = strtoul(optarg,NULL,10);  break;
		case 'R': o->route_table = strtoul(optarg,NULL,10);  break;
		case 't': o->expires = strtoul(optarg,NULL,10);  break;
		case 'p': o->proto = strtoul(optarg,NULL,10);  break;
		case 'S': o->seed = strtoul(optarg,NULL,10);  break;
		case '?':
		case 'h': usage(NULL); break;
		default:  usage(NULL);
		}
	}
	o->interval.tv_sec = floor(o->finterval);
	o->interval.tv_nsec = (long long) ((o->finterval - o->interval.tv_sec) * NSEC_PER_SEC);
	return 0;
}

int main(int argc,char **argv) {
	args a;
	int status = 0;
	defaults(&a);
	process_options(argc, argv, &a);
	return status;
}
