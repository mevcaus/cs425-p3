#include "lab.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TEST
#define main main_exclude
#endif

/* Exit codes required by the assignment */
#define EXIT_OK 0
#define EXIT_USAGE 1       /* bad command line or malformed address */
#define EXIT_TABLE 2       /* table file can not be opened or has a bad line */
#define EXIT_UNREACHABLE 3 /* an address matched no route */

static void usage(FILE *out)
{
  fprintf(out, "Usage: myapp info <address>/<prefix>\n"
               "       myapp split <network>/<prefix> <count>\n"
               "       myapp lookup <table-file> <address>...\n");
}

/* myapp info <address>/<prefix> */
static int cmd_info(const char *arg)
{
  cidr_t block;
  if (!cidr_parse(arg, &block))
  {
    fprintf(stderr, "myapp: '%s' is not a valid <address>/<prefix>\n", arg);
    return EXIT_USAGE;
  }

  char addr[IP_STR_LEN], mask[IP_STR_LEN], network[IP_STR_LEN], broadcast[IP_STR_LEN];
  char first[IP_STR_LEN], last[IP_STR_LEN];
  ip_format(block.addr, addr);
  ip_format(cidr_mask(block.prefix), mask);
  ip_format(cidr_network(block), network);
  ip_format(cidr_broadcast(block), broadcast);
  ip_format(cidr_first_host(block), first);
  ip_format(cidr_last_host(block), last);

  printf("address    %s\n", addr);
  printf("prefix     /%u\n", (unsigned)block.prefix);
  printf("netmask    %s\n", mask);
  printf("network    %s\n", network);
  printf("broadcast  %s\n", broadcast);
  printf("first host %s\n", first);
  printf("last host  %s\n", last);
  printf("hosts      %" PRIu64 "\n", cidr_host_count(block));
  return EXIT_OK;
}

/*
 * Parse a subnet count: decimal digits only, no leading zero (same rule as a
 * prefix), and it has to fit in 32 bits.
 */
static int parse_count(const char *text, uint32_t *count)
{
  if (*text == '\0' || (text[0] == '0' && text[1] != '\0'))
  {
    return 0;
  }
  uint64_t value = 0;
  for (const char *c = text; *c != '\0'; c++)
  {
    if (*c < '0' || *c > '9')
    {
      return 0;
    }
    value = value * 10 + (uint64_t)(*c - '0');
    if (value > UINT32_MAX)
    {
      return 0;
    }
  }
  *count = (uint32_t)value;
  return 1;
}

/* myapp split <network>/<prefix> <count> */
static int cmd_split(const char *block_arg, const char *count_arg)
{
  cidr_t block;
  if (!cidr_parse(block_arg, &block))
  {
    fprintf(stderr, "myapp: '%s' is not a valid <network>/<prefix>\n", block_arg);
    return EXIT_USAGE;
  }
  if (!cidr_is_network(block))
  {
    char network[CIDR_STR_LEN];
    cidr_t suggestion = {cidr_network(block), block.prefix};
    cidr_format(suggestion, network);
    fprintf(stderr, "myapp: %s has host bits set, so it is not a network (did you mean %s?)\n",
            block_arg, network);
    return EXIT_USAGE;
  }

  uint32_t count;
  if (!parse_count(count_arg, &count))
  {
    fprintf(stderr, "myapp: '%s' is not a valid count\n", count_arg);
    return EXIT_USAGE;
  }
  if (count == 0 || (count & (count - 1)) != 0)
  {
    fprintf(stderr, "myapp: %s is not a power of two, so a block can not be split into that many equal subnets\n",
            count_arg);
    return EXIT_USAGE;
  }

  cidr_t subnet;
  if (!cidr_subnet(block, count, 0, &subnet))
  {
    fprintf(stderr, "myapp: a /%u can not be split into %s subnets, the prefix would pass /32\n",
            (unsigned)block.prefix, count_arg);
    return EXIT_USAGE;
  }

  char text[CIDR_STR_LEN];
  for (uint32_t i = 0; i < count; i++)
  {
    cidr_subnet(block, count, i, &subnet);
    cidr_format(subnet, text);
    printf("%s\n", text);
  }
  return EXIT_OK;
}

/*
 * Report every malformed address on stderr. Used when the table failed to
 * load, since a malformed address still decides the exit code then.
 */
static bool report_malformed(int count, char **addrs)
{
  bool malformed = false;
  for (int i = 0; i < count; i++)
  {
    uint32_t dest;
    if (!ip_parse(addrs[i], &dest))
    {
      fprintf(stderr, "myapp: '%s' is not a valid address\n", addrs[i]);
      malformed = true;
    }
  }
  return malformed;
}

/* myapp lookup <table-file> <address>... */
static int cmd_lookup(const char *path, int count, char **addrs)
{
  FILE *in = fopen(path, "r");
  if (in == NULL)
  {
    fprintf(stderr, "myapp: %s: %s\n", path, strerror(errno));
    return report_malformed(count, addrs) ? EXIT_USAGE : EXIT_TABLE;
  }

  fwd_table_t *table = table_create();
  if (table == NULL)
  {
    fprintf(stderr, "myapp: out of memory\n");
    fclose(in);
    return EXIT_TABLE;
  }

  size_t bad_line = 0;
  bool loaded = table_load(table, in, &bad_line);
  fclose(in);
  if (!loaded)
  {
    if (bad_line > 0)
    {
      fprintf(stderr, "myapp: %s:%zu: not a valid route\n", path, bad_line);
    }
    else
    {
      fprintf(stderr, "myapp: %s: could not read the file\n", path);
    }
    table_destroy(table);
    return report_malformed(count, addrs) ? EXIT_USAGE : EXIT_TABLE;
  }

  bool malformed = false;
  bool unreachable = false;
  for (int i = 0; i < count; i++)
  {
    uint32_t dest;
    if (!ip_parse(addrs[i], &dest))
    {
      /* flush first so the error lands in order when stdout and stderr are merged */
      fflush(stdout);
      fprintf(stderr, "myapp: '%s' is not a valid address\n", addrs[i]);
      malformed = true;
      continue;
    }

    char addr[IP_STR_LEN];
    ip_format(dest, addr);
    const route_t *route = table_lookup(table, dest);
    if (route == NULL)
    {
      printf("%s unreachable\n", addr);
      unreachable = true;
    }
    else if (route->gateway != 0)
    {
      char gateway[IP_STR_LEN];
      ip_format(route->gateway, gateway);
      printf("%s via %s dev %s\n", addr, gateway, route->dev);
    }
    else
    {
      printf("%s dev %s\n", addr, route->dev);
    }
  }

  table_destroy(table);
  if (malformed)
  {
    return EXIT_USAGE;
  }
  return unreachable ? EXIT_UNREACHABLE : EXIT_OK;
}

int main(int argc, char **argv)
{
  if (argc == 1)
  {
    usage(stdout);
    return EXIT_OK;
  }

  const char *cmd = argv[1];
  if (strcmp(cmd, "info") == 0 && argc == 3)
  {
    return cmd_info(argv[2]);
  }
  if (strcmp(cmd, "split") == 0 && argc == 4)
  {
    return cmd_split(argv[2], argv[3]);
  }
  if (strcmp(cmd, "lookup") == 0 && argc >= 4)
  {
    return cmd_lookup(argv[2], argc - 3, argv + 3);
  }

  usage(stderr);
  return EXIT_USAGE;
}
