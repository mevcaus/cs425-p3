#define _POSIX_C_SOURCE 200809L /* getline() */

#include "lab.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* ssize_t */

/* The forwarding table is a growable array of routes. */
struct fwd_table
{
  route_t *routes;
  size_t count;
  size_t capacity;
};

/* Prefixes over 32 are treated as 32 everywhere, the same way cidr_mask() does. */
static uint8_t clamp_prefix(uint8_t prefix)
{
  return prefix > 32 ? 32 : prefix;
}

/* ------------------------------------------------------------------------
 * Task 1 - Address parsing and formatting
 * --------------------------------------------------------------------- */

/*
 * Parse one decimal number from 0 to max with no sign and no leading zero.
 * On success *value is set and the pointer just past the last digit is
 * returned. On failure NULL is returned.
 */
static const char *parse_decimal(const char *s, uint32_t max, uint32_t *value)
{
  if (!isdigit((unsigned char)*s))
  {
    return NULL;
  }
  /* "0" is fine but "00" or "010" is not */
  if (s[0] == '0' && isdigit((unsigned char)s[1]))
  {
    return NULL;
  }

  uint32_t v = 0;
  while (isdigit((unsigned char)*s))
  {
    v = v * 10 + (uint32_t)(*s - '0');
    if (v > max)
    {
      return NULL; /* bail before it can overflow */
    }
    s++;
  }
  *value = v;
  return s;
}

/*
 * Parse the four octets of a dotted quad at the start of s. Returns the
 * pointer just past the last octet, or NULL if s does not start with one.
 * The caller decides what is allowed to follow.
 */
static const char *parse_quad(const char *s, uint32_t *addr)
{
  uint32_t result = 0;
  for (int i = 0; i < 4; i++)
  {
    if (i > 0)
    {
      if (*s != '.')
      {
        return NULL;
      }
      s++;
    }
    uint32_t octet;
    s = parse_decimal(s, 255, &octet);
    if (s == NULL)
    {
      return NULL;
    }
    result = (result << 8) | octet;
  }
  *addr = result;
  return s;
}

bool ip_parse(const char *text, uint32_t *addr)
{
  if (text == NULL || addr == NULL)
  {
    return false;
  }
  uint32_t value;
  const char *end = parse_quad(text, &value);
  if (end == NULL || *end != '\0')
  {
    return false;
  }
  *addr = value;
  return true;
}

void ip_format(uint32_t addr, char out[IP_STR_LEN])
{
  snprintf(out, IP_STR_LEN, "%u.%u.%u.%u",
           (unsigned)((addr >> 24) & 0xFFu),
           (unsigned)((addr >> 16) & 0xFFu),
           (unsigned)((addr >> 8) & 0xFFu),
           (unsigned)(addr & 0xFFu));
}

bool cidr_parse(const char *text, cidr_t *block)
{
  if (text == NULL || block == NULL)
  {
    return false;
  }
  uint32_t addr;
  const char *end = parse_quad(text, &addr);
  if (end == NULL || *end != '/')
  {
    return false;
  }
  uint32_t prefix;
  end = parse_decimal(end + 1, 32, &prefix);
  if (end == NULL || *end != '\0')
  {
    return false;
  }
  block->addr = addr;
  block->prefix = (uint8_t)prefix;
  return true;
}

void cidr_format(cidr_t block, char out[CIDR_STR_LEN])
{
  /*
   * The dotted quad takes at most 15 characters and "/32" at most 3 more, so
   * it always fits in CIDR_STR_LEN. The prefix is written by hand because
   * gcc can't tell at -O0 that it is at most two digits and warns that a
   * snprintf() of it might truncate.
   */
  ip_format(block.addr, out);
  size_t len = strlen(out);
  unsigned prefix = clamp_prefix(block.prefix);
  out[len++] = '/';
  if (prefix >= 10)
  {
    out[len++] = (char)('0' + prefix / 10);
  }
  out[len++] = (char)('0' + prefix % 10);
  out[len] = '\0';
}

/* ------------------------------------------------------------------------
 * Task 2 - Subnet arithmetic
 * --------------------------------------------------------------------- */

uint32_t cidr_mask(uint8_t prefix)
{
  /*
   * The usual 0xFFFFFFFF << (32 - prefix) shifts by 32 for a /0, which is
   * undefined behavior (x86 shifts by 0 and returns all ones). A /0 has no
   * network bits at all, so it gets its own case.
   */
  if (prefix == 0)
  {
    return 0;
  }
  prefix = clamp_prefix(prefix);
  return (uint32_t)(0xFFFFFFFFu << (32u - prefix));
}

uint32_t cidr_network(cidr_t block)
{
  return block.addr & cidr_mask(block.prefix);
}

uint32_t cidr_broadcast(cidr_t block)
{
  return block.addr | ~cidr_mask(block.prefix);
}

uint32_t cidr_first_host(cidr_t block)
{
  uint32_t network = cidr_network(block);
  /* /31 (RFC 3021 point to point) and /32 (one host) have no network address */
  if (clamp_prefix(block.prefix) >= 31)
  {
    return network;
  }
  return network + 1;
}

uint32_t cidr_last_host(cidr_t block)
{
  uint32_t broadcast = cidr_broadcast(block);
  /* /31 and /32 have no broadcast address either */
  if (clamp_prefix(block.prefix) >= 31)
  {
    return broadcast;
  }
  return broadcast - 1;
}

uint64_t cidr_host_count(cidr_t block)
{
  uint8_t prefix = clamp_prefix(block.prefix);
  if (prefix == 32)
  {
    return 1;
  }
  if (prefix == 31)
  {
    return 2;
  }
  /* 64 bits so that the 2^32 of a /0 does not overflow */
  return (UINT64_C(1) << (32u - prefix)) - 2;
}

bool cidr_contains(cidr_t block, uint32_t addr)
{
  uint32_t mask = cidr_mask(block.prefix);
  return (addr & mask) == (block.addr & mask);
}

bool cidr_is_network(cidr_t block)
{
  return (block.addr & ~cidr_mask(block.prefix)) == 0;
}

bool cidr_subnet(cidr_t block, uint32_t n, uint32_t i, cidr_t *out)
{
  if (out == NULL || !cidr_is_network(block))
  {
    return false;
  }
  /* a power of two has exactly one bit set */
  if (n == 0 || (n & (n - 1)) != 0)
  {
    return false;
  }

  /* splitting into n = 2^bits pieces borrows bits from the host part */
  unsigned bits = 0;
  while ((UINT32_C(1) << bits) != n)
  {
    bits++;
  }

  unsigned prefix = clamp_prefix(block.prefix);
  unsigned new_prefix = prefix + bits;
  if (new_prefix > 32 || i >= n)
  {
    return false;
  }

  /*
   * Each subnet holds 2^(32 - new_prefix) addresses, so subnet i starts i of
   * those past the network address. The shift is done in 64 bits because
   * splitting a /0 into 1 piece would shift by 32.
   */
  uint64_t offset = (uint64_t)i << (32u - new_prefix);
  out->addr = cidr_network(block) + (uint32_t)offset;
  out->prefix = (uint8_t)new_prefix;
  return true;
}

/* ------------------------------------------------------------------------
 * Task 3 - Longest prefix match
 * --------------------------------------------------------------------- */

/* Fields in a route line are separated by spaces or tabs. */
static bool is_separator(char c)
{
  return c == ' ' || c == '\t';
}

/*
 * Copy the field [start, end) into buf as a string. Returns false if it does
 * not fit, which also means it is too long to be valid.
 */
static bool copy_field(const char *start, const char *end, char *buf, size_t size)
{
  size_t len = (size_t)(end - start);
  if (len >= size)
  {
    return false;
  }
  memcpy(buf, start, len);
  buf[len] = '\0';
  return true;
}

bool route_parse(const char *line, route_t *route)
{
  if (line == NULL || route == NULL)
  {
    return false;
  }

  /* trim leading and trailing whitespace, including the newline */
  const char *start = line;
  while (isspace((unsigned char)*start))
  {
    start++;
  }
  const char *end = start + strlen(start);
  while (end > start && isspace((unsigned char)end[-1]))
  {
    end--;
  }

  /* split what is left into exactly three fields */
  const char *field_start[3];
  const char *field_end[3];
  int fields = 0;
  const char *p = start;
  while (p < end)
  {
    if (fields == 3)
    {
      return false; /* a fourth field */
    }
    field_start[fields] = p;
    while (p < end && !is_separator(*p))
    {
      p++;
    }
    field_end[fields] = p;
    fields++;
    while (p < end && is_separator(*p))
    {
      p++;
    }
  }
  if (fields != 3)
  {
    return false;
  }

  route_t result;
  memset(&result, 0, sizeof(result));

  /* destination: "default" or a network in CIDR notation */
  char buf[CIDR_STR_LEN];
  if (!copy_field(field_start[0], field_end[0], buf, sizeof(buf)))
  {
    return false;
  }
  if (strcmp(buf, "default") == 0)
  {
    result.dest.addr = 0;
    result.dest.prefix = 0;
  }
  else if (!cidr_parse(buf, &result.dest) || !cidr_is_network(result.dest))
  {
    return false;
  }

  /* gateway: "direct" or a dotted quad */
  if (!copy_field(field_start[1], field_end[1], buf, sizeof(buf)))
  {
    return false;
  }
  if (strcmp(buf, "direct") == 0)
  {
    result.gateway = 0;
  }
  else if (!ip_parse(buf, &result.gateway))
  {
    return false;
  }

  /* device: 1 to DEV_NAME_LEN - 1 characters with no whitespace in them */
  if (!copy_field(field_start[2], field_end[2], result.dev, sizeof(result.dev)))
  {
    return false;
  }
  for (const char *c = result.dev; *c != '\0'; c++)
  {
    if (isspace((unsigned char)*c))
    {
      return false;
    }
  }

  *route = result;
  return true;
}

fwd_table_t *table_create(void)
{
  /* calloc so routes starts as NULL and count and capacity start at 0 */
  return calloc(1, sizeof(fwd_table_t));
}

void table_destroy(fwd_table_t *table)
{
  if (table == NULL)
  {
    return;
  }
  free(table->routes);
  free(table);
}

size_t table_count(const fwd_table_t *table)
{
  return table == NULL ? 0 : table->count;
}

bool table_add(fwd_table_t *table, const route_t *route)
{
  if (table == NULL || route == NULL)
  {
    return false;
  }
  if (route->dest.prefix > 32 || !cidr_is_network(route->dest))
  {
    return false;
  }
  for (size_t i = 0; i < table->count; i++)
  {
    const cidr_t *have = &table->routes[i].dest;
    if (have->addr == route->dest.addr && have->prefix == route->dest.prefix)
    {
      return false; /* same block twice, like ip route add */
    }
  }

  if (table->count == table->capacity)
  {
    size_t capacity = table->capacity == 0 ? 8 : table->capacity * 2;
    route_t *grown = realloc(table->routes, capacity * sizeof(route_t));
    if (grown == NULL) // GCOVR_EXCL_START
    {
      return false; /* the old array is still valid, so the table is unchanged */
    } // GCOVR_EXCL_STOP
    table->routes = grown;
    table->capacity = capacity;
  }

  route_t *slot = &table->routes[table->count];
  *slot = *route;
  slot->dev[DEV_NAME_LEN - 1] = '\0'; /* never trust the caller's terminator */
  table->count++;
  return true;
}

/* true if the line holds nothing but whitespace */
static bool is_blank(const char *line)
{
  while (*line != '\0')
  {
    if (!isspace((unsigned char)*line))
    {
      return false;
    }
    line++;
  }
  return true;
}

bool table_load(fwd_table_t *table, FILE *in, size_t *bad_line)
{
  if (bad_line != NULL)
  {
    *bad_line = 0;
  }
  if (table == NULL || in == NULL)
  {
    return false;
  }

  char *line = NULL;
  size_t size = 0;
  size_t number = 0;
  bool ok = true;

  ssize_t length;
  while ((length = getline(&line, &size, in)) != -1)
  {
    number++;

    /* a '\0' inside the line would hide whatever follows it from the parser */
    bool whole = strlen(line) == (size_t)length;

    /* everything from a '#' on is a comment */
    char *comment = strchr(line, '#');
    if (comment != NULL)
    {
      *comment = '\0';
    }
    if (whole && is_blank(line))
    {
      continue;
    }

    route_t route;
    if (!whole || !route_parse(line, &route) || !table_add(table, &route))
    {
      if (bad_line != NULL)
      {
        *bad_line = number;
      }
      ok = false;
      break;
    }
  }

  /* getline() returns -1 for a read error as well as for end of file */
  if (ok && ferror(in))
  {
    ok = false;
  }

  free(line);
  return ok;
}

const route_t *table_lookup(const fwd_table_t *table, uint32_t dest)
{
  if (table == NULL)
  {
    return NULL;
  }

  /*
   * Check every route and keep the longest prefix that contains dest, so the
   * order the routes were added in does not matter. Two different matching
   * routes can never tie, because table_add() refuses a second route to the
   * same block and two blocks with the same prefix that both contain dest are
   * the same block. The default route is just a /0 that always matches and
   * always loses to anything longer.
   */
  const route_t *best = NULL;
  for (size_t i = 0; i < table->count; i++)
  {
    const route_t *r = &table->routes[i];
    if (cidr_contains(r->dest, dest) && (best == NULL || r->dest.prefix > best->dest.prefix))
    {
      best = r;
    }
  }
  return best;
}
