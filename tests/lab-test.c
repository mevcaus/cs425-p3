#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "harness/unity.h"
#include "../src/lab.h"

/* A value no test expects, used to check outputs are untouched on failure. */
#define SENTINEL 0xDEADBEEFu

void setUp(void) {}

void tearDown(void) {}

/* ------------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

/* Build a cidr_t from text the test knows is valid. */
static cidr_t block_of(const char *text) {
  cidr_t block = {0, 0};
  TEST_ASSERT_TRUE_MESSAGE(cidr_parse(text, &block), text);
  return block;
}

/* Build an address from text the test knows is valid. */
static uint32_t addr_of(const char *text) {
  uint32_t addr = 0;
  TEST_ASSERT_TRUE_MESSAGE(ip_parse(text, &addr), text);
  return addr;
}

/* Build a route from a line the test knows is valid. */
static route_t route_of(const char *line) {
  route_t route;
  memset(&route, 0, sizeof(route));
  TEST_ASSERT_TRUE_MESSAGE(route_parse(line, &route), line);
  return route;
}

/* A FILE * holding text, rewound and ready to read. */
static FILE *file_of(const char *text) {
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  fputs(text, f);
  rewind(f);
  return f;
}

/* Load text as a table file into a new table. */
static fwd_table_t *table_of(const char *text) {
  fwd_table_t *table = table_create();
  TEST_ASSERT_NOT_NULL(table);
  FILE *f = file_of(text);
  size_t bad = 99;
  TEST_ASSERT_TRUE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(0, bad);
  fclose(f);
  return table;
}

/* The block a lookup picked, formatted, or "none" for no match. */
static const char *lookup_block(const fwd_table_t *table, const char *addr) {
  static char out[CIDR_STR_LEN];
  const route_t *route = table_lookup(table, addr_of(addr));
  if (route == NULL) {
    return "none";
  }
  cidr_format(route->dest, out);
  return out;
}

/* The forwarding table from the assignment, used by the grading. */
static const char *EXAMPLE_TABLE =
    "# destination      gateway          device\n"
    "default            132.178.227.1    eno1\n"
    "132.178.227.0/25   direct           eno1\n"
    "10.0.0.0/8         10.1.0.1         eth1\n"
    "10.1.0.0/16        direct           eth1\n"
    "10.1.2.0/24        direct           eth2\n"
    "10.1.2.128/25      10.1.2.1         eth2   # a router behind eth2\n"
    "192.168.0.0/16     10.1.0.1         eth1\n";

/* The same routes, sorted shortest prefix first, so the longest is found last. */
static const char *EXAMPLE_REVERSED =
    "10.1.2.128/25      10.1.2.1         eth2\n"
    "10.1.2.0/24        direct           eth2\n"
    "10.1.0.0/16        direct           eth1\n"
    "192.168.0.0/16     10.1.0.1         eth1\n"
    "10.0.0.0/8         10.1.0.1         eth1\n"
    "132.178.227.0/25   direct           eno1\n"
    "default            132.178.227.1    eno1\n";

/* The same routes again in a scrambled order. */
static const char *EXAMPLE_SHUFFLED =
    "10.1.0.0/16        direct           eth1\n"
    "default            132.178.227.1    eno1\n"
    "10.1.2.128/25      10.1.2.1         eth2\n"
    "192.168.0.0/16     10.1.0.1         eth1\n"
    "10.0.0.0/8         10.1.0.1         eth1\n"
    "10.1.2.0/24        direct           eth2\n"
    "132.178.227.0/25   direct           eno1\n";

/* ------------------------------------------------------------------------
 * Task 1 - ip_parse
 * --------------------------------------------------------------------- */

void test_ip_parse_valid(void) {
  uint32_t addr = 0;
  TEST_ASSERT_TRUE(ip_parse("192.168.1.10", &addr));
  TEST_ASSERT_EQUAL_HEX32(0xC0A8010A, addr);
  TEST_ASSERT_TRUE(ip_parse("132.178.227.11", &addr));
  TEST_ASSERT_EQUAL_HEX32(0x84B2E30B, addr);
  TEST_ASSERT_TRUE(ip_parse("10.0.0.1", &addr));
  TEST_ASSERT_EQUAL_HEX32(0x0A000001, addr);
  TEST_ASSERT_TRUE(ip_parse("1.2.3.4", &addr));
  TEST_ASSERT_EQUAL_HEX32(0x01020304, addr);
}

void test_ip_parse_boundaries(void) {
  uint32_t addr = SENTINEL;
  TEST_ASSERT_TRUE(ip_parse("0.0.0.0", &addr));
  TEST_ASSERT_EQUAL_HEX32(0x00000000, addr);
  TEST_ASSERT_TRUE(ip_parse("255.255.255.255", &addr));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, addr);
  TEST_ASSERT_TRUE(ip_parse("255.0.0.0", &addr));
  TEST_ASSERT_EQUAL_HEX32(0xFF000000, addr);
  TEST_ASSERT_TRUE(ip_parse("0.0.0.255", &addr));
  TEST_ASSERT_EQUAL_HEX32(0x000000FF, addr);
  TEST_ASSERT_TRUE(ip_parse("100.200.0.9", &addr));
  TEST_ASSERT_EQUAL_HEX32(0x64C80009, addr);
}

void test_ip_parse_rejects_malformed(void) {
  const char *bad[] = {
      /* the list from the assignment */
      "1.2.3", "1.2.3.4.5", "1..2.3", "256.1.1.1", "010.0.0.1", "1.2.3.4x",
      "1.2.3.4/33", "1.2.3.4/024", "1.2.3.4/", "1.2.3/24",
      /* wrong number of octets or periods */
      "", "1", "1.2", "1.2.3.", ".1.2.3", ".1.2.3.4", "1.2.3.4.", "1...4",
      "....",
      /* out of range */
      "1.2.3.256", "1.2.256.4", "1.256.3.4", "999.1.1.1", "1000.1.1.1",
      "99999999999.1.1.1", "1.2.3.4294967297",
      /* leading zeros, which inet_aton() would read as octal */
      "00.0.0.0", "1.02.3.4", "1.2.03.4", "1.2.3.04", "1.2.3.00", "0255.1.1.1",
      /* anything before, between or after */
      " 1.2.3.4", "1.2.3.4 ", "1.2.3.4\n", "+1.2.3.4", "-1.2.3.4",
      "1.+2.3.4", "1. 2.3.4", "1.2.3.4junk", "0x1.2.3.4", "a.b.c.d",
      "1,2,3,4", "1.2.3.-4", "1.2.3.4.", "1.2.3.4/24",
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    uint32_t addr = SENTINEL;
    TEST_ASSERT_FALSE_MESSAGE(ip_parse(bad[i], &addr), bad[i]);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(SENTINEL, addr, bad[i]);
  }
}

void test_ip_parse_null(void) {
  uint32_t addr = SENTINEL;
  TEST_ASSERT_FALSE(ip_parse(NULL, &addr));
  TEST_ASSERT_EQUAL_HEX32(SENTINEL, addr);
  TEST_ASSERT_FALSE(ip_parse("1.2.3.4", NULL));
}

/* ------------------------------------------------------------------------
 * Task 1 - ip_format
 * --------------------------------------------------------------------- */

void test_ip_format(void) {
  char out[IP_STR_LEN];
  ip_format(0xC0A8010A, out);
  TEST_ASSERT_EQUAL_STRING("192.168.1.10", out);
  ip_format(0x84B2E30B, out);
  TEST_ASSERT_EQUAL_STRING("132.178.227.11", out);
  ip_format(0x01020304, out);
  TEST_ASSERT_EQUAL_STRING("1.2.3.4", out);
}

void test_ip_format_boundaries(void) {
  char out[IP_STR_LEN];
  ip_format(0x00000000, out);
  TEST_ASSERT_EQUAL_STRING("0.0.0.0", out);
  /* the longest dotted quad has to fit in IP_STR_LEN with its '\0' */
  ip_format(0xFFFFFFFF, out);
  TEST_ASSERT_EQUAL_STRING("255.255.255.255", out);
  TEST_ASSERT_EQUAL_size_t(IP_STR_LEN - 1, strlen(out));
}

void test_ip_round_trip(void) {
  const char *texts[] = {"0.0.0.0", "8.8.8.8", "10.1.2.200", "172.16.254.1", "255.255.255.255"};
  for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
    char out[IP_STR_LEN];
    ip_format(addr_of(texts[i]), out);
    TEST_ASSERT_EQUAL_STRING(texts[i], out);
  }
}

/* ------------------------------------------------------------------------
 * Task 1 - cidr_parse
 * --------------------------------------------------------------------- */

void test_cidr_parse_valid(void) {
  cidr_t block = {0, 0};
  TEST_ASSERT_TRUE(cidr_parse("192.168.1.0/24", &block));
  TEST_ASSERT_EQUAL_HEX32(0xC0A80100, block.addr);
  TEST_ASSERT_EQUAL_UINT8(24, block.prefix);

  /* host bits are allowed, this is Onyx's interface address */
  TEST_ASSERT_TRUE(cidr_parse("132.178.227.11/25", &block));
  TEST_ASSERT_EQUAL_HEX32(0x84B2E30B, block.addr);
  TEST_ASSERT_EQUAL_UINT8(25, block.prefix);

  TEST_ASSERT_TRUE(cidr_parse("10.0.0.0/8", &block));
  TEST_ASSERT_EQUAL_HEX32(0x0A000000, block.addr);
  TEST_ASSERT_EQUAL_UINT8(8, block.prefix);
}

void test_cidr_parse_boundaries(void) {
  cidr_t block = {SENTINEL, 99};
  TEST_ASSERT_TRUE(cidr_parse("0.0.0.0/0", &block));
  TEST_ASSERT_EQUAL_HEX32(0, block.addr);
  TEST_ASSERT_EQUAL_UINT8(0, block.prefix);

  TEST_ASSERT_TRUE(cidr_parse("255.255.255.255/32", &block));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, block.addr);
  TEST_ASSERT_EQUAL_UINT8(32, block.prefix);

  TEST_ASSERT_TRUE(cidr_parse("10.0.0.0/31", &block));
  TEST_ASSERT_EQUAL_UINT8(31, block.prefix);

  TEST_ASSERT_TRUE(cidr_parse("1.2.3.4/0", &block));
  TEST_ASSERT_EQUAL_HEX32(0x01020304, block.addr);
  TEST_ASSERT_EQUAL_UINT8(0, block.prefix);

  TEST_ASSERT_TRUE(cidr_parse("1.2.3.4/10", &block));
  TEST_ASSERT_EQUAL_UINT8(10, block.prefix);
  TEST_ASSERT_TRUE(cidr_parse("1.2.3.4/30", &block));
  TEST_ASSERT_EQUAL_UINT8(30, block.prefix);
}

void test_cidr_parse_rejects_malformed(void) {
  const char *bad[] = {
      /* the list from the assignment */
      "1.2.3", "1.2.3.4.5", "1..2.3", "256.1.1.1", "010.0.0.1", "1.2.3.4x",
      "1.2.3.4/33", "1.2.3.4/024", "1.2.3.4/", "1.2.3/24",
      /* the slash and prefix are required */
      "1.2.3.4", "", "/24", "/",
      /* bad prefixes */
      "1.2.3.4/00", "1.2.3.4/01", "1.2.3.4/032", "1.2.3.4/40", "1.2.3.4/100",
      "1.2.3.4/4294967296", "1.2.3.4/-1", "1.2.3.4/+8", "1.2.3.4/8x",
      "1.2.3.4/8/8", "1.2.3.4/8.0", "1.2.3.4/ 8", "1.2.3.4/8 ", "1.2.3.4/8\n",
      "1.2.3.4/x", "1.2.3.4//8",
      /* bad addresses with a good prefix */
      "1.2.3.4 /8", " 1.2.3.4/8", "256.0.0.0/8", "1.2.3.04/8", "1.2.3.4.5/8",
      "1.2.3/8", "a.b.c.d/8",
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    cidr_t block = {SENTINEL, 99};
    TEST_ASSERT_FALSE_MESSAGE(cidr_parse(bad[i], &block), bad[i]);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(SENTINEL, block.addr, bad[i]);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(99, block.prefix, bad[i]);
  }
}

void test_cidr_parse_null(void) {
  cidr_t block = {SENTINEL, 99};
  TEST_ASSERT_FALSE(cidr_parse(NULL, &block));
  TEST_ASSERT_EQUAL_HEX32(SENTINEL, block.addr);
  TEST_ASSERT_FALSE(cidr_parse("1.2.3.0/24", NULL));
}

/* ------------------------------------------------------------------------
 * Task 1 - cidr_format
 * --------------------------------------------------------------------- */

void test_cidr_format(void) {
  char out[CIDR_STR_LEN];
  cidr_format((cidr_t){0xC0A80100, 24}, out);
  TEST_ASSERT_EQUAL_STRING("192.168.1.0/24", out);
  /* the address is written exactly as stored, host bits and all */
  cidr_format((cidr_t){0x84B2E30B, 25}, out);
  TEST_ASSERT_EQUAL_STRING("132.178.227.11/25", out);
}

void test_cidr_format_boundaries(void) {
  char out[CIDR_STR_LEN];
  cidr_format((cidr_t){0, 0}, out);
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", out);
  /* the longest CIDR string has to fit in CIDR_STR_LEN with its '\0' */
  cidr_format((cidr_t){0xFFFFFFFF, 32}, out);
  TEST_ASSERT_EQUAL_STRING("255.255.255.255/32", out);
  TEST_ASSERT_EQUAL_size_t(CIDR_STR_LEN - 1, strlen(out));
  cidr_format((cidr_t){0x0A000000, 31}, out);
  TEST_ASSERT_EQUAL_STRING("10.0.0.0/31", out);
  /* over 32 is written as 32 */
  cidr_format((cidr_t){0xFFFFFFFF, 33}, out);
  TEST_ASSERT_EQUAL_STRING("255.255.255.255/32", out);
  cidr_format((cidr_t){0x01020304, 255}, out);
  TEST_ASSERT_EQUAL_STRING("1.2.3.4/32", out);
}

void test_cidr_round_trip(void) {
  const char *texts[] = {"0.0.0.0/0", "10.0.0.0/8", "132.178.227.11/25", "10.1.2.128/25",
                         "192.168.1.0/31", "255.255.255.255/32"};
  for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
    char out[CIDR_STR_LEN];
    cidr_format(block_of(texts[i]), out);
    TEST_ASSERT_EQUAL_STRING(texts[i], out);
  }
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_mask
 * --------------------------------------------------------------------- */

void test_cidr_mask(void) {
  TEST_ASSERT_EQUAL_HEX32(0x80000000, cidr_mask(1));
  TEST_ASSERT_EQUAL_HEX32(0xFF000000, cidr_mask(8));
  TEST_ASSERT_EQUAL_HEX32(0xFFF00000, cidr_mask(12));
  TEST_ASSERT_EQUAL_HEX32(0xFFFF0000, cidr_mask(16));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFF00, cidr_mask(24));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFF80, cidr_mask(25));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFC, cidr_mask(30));
}

void test_cidr_mask_boundaries(void) {
  /* /0 must be 0, not the 0xFFFFFFFF a shift by 32 gives on x86 */
  TEST_ASSERT_EQUAL_HEX32(0x00000000, cidr_mask(0));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFE, cidr_mask(31));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_mask(32));
  /* anything larger is treated as 32 */
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_mask(33));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_mask(64));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_mask(255));
}

void test_cidr_mask_every_prefix(void) {
  /* a mask is always prefix ones followed by zeros */
  for (uint8_t prefix = 0; prefix <= 32; prefix++) {
    uint32_t mask = cidr_mask(prefix);
    uint32_t expected = 0;
    for (uint8_t bit = 0; bit < prefix; bit++) {
      expected |= UINT32_C(1) << (31 - bit);
    }
    TEST_ASSERT_EQUAL_HEX32(expected, mask);
  }
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_network and cidr_broadcast
 * --------------------------------------------------------------------- */

void test_cidr_network_and_broadcast_onyx(void) {
  cidr_t onyx = block_of("132.178.227.11/25");
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.0"), cidr_network(onyx));
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.127"), cidr_broadcast(onyx));
}

void test_cidr_network_and_broadcast(void) {
  cidr_t block = block_of("192.168.1.77/24");
  TEST_ASSERT_EQUAL_HEX32(addr_of("192.168.1.0"), cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("192.168.1.255"), cidr_broadcast(block));

  block = block_of("10.1.2.200/25");
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.2.128"), cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.2.255"), cidr_broadcast(block));

  block = block_of("172.16.5.4/12");
  TEST_ASSERT_EQUAL_HEX32(addr_of("172.16.0.0"), cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("172.31.255.255"), cidr_broadcast(block));
}

void test_cidr_network_and_broadcast_boundaries(void) {
  /* /0 covers everything */
  cidr_t block = block_of("8.8.8.8/0");
  TEST_ASSERT_EQUAL_HEX32(0x00000000, cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_broadcast(block));

  /* /31 is a pair */
  block = block_of("10.0.0.1/31");
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.0"), cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.1"), cidr_broadcast(block));

  /* /32 is just the address */
  block = block_of("10.0.0.7/32");
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.7"), cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.7"), cidr_broadcast(block));

  /* the edges of the address space */
  block = block_of("0.0.0.0/32");
  TEST_ASSERT_EQUAL_HEX32(0, cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(0, cidr_broadcast(block));
  block = block_of("255.255.255.255/24");
  TEST_ASSERT_EQUAL_HEX32(addr_of("255.255.255.0"), cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_broadcast(block));
  block = block_of("255.255.255.255/32");
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_network(block));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_broadcast(block));
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_first_host and cidr_last_host
 * --------------------------------------------------------------------- */

void test_cidr_host_range_onyx(void) {
  cidr_t onyx = block_of("132.178.227.11/25");
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.1"), cidr_first_host(onyx));
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.126"), cidr_last_host(onyx));
}

void test_cidr_host_range(void) {
  cidr_t block = block_of("192.168.1.0/24");
  TEST_ASSERT_EQUAL_HEX32(addr_of("192.168.1.1"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("192.168.1.254"), cidr_last_host(block));

  /* /30 is the smallest block that still has a network and broadcast */
  block = block_of("192.168.1.6/30");
  TEST_ASSERT_EQUAL_HEX32(addr_of("192.168.1.5"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("192.168.1.6"), cidr_last_host(block));
}

void test_cidr_host_range_boundaries(void) {
  cidr_t block = block_of("0.0.0.0/0");
  TEST_ASSERT_EQUAL_HEX32(addr_of("0.0.0.1"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("255.255.255.254"), cidr_last_host(block));

  /* /31: both addresses are hosts (RFC 3021) */
  block = block_of("10.0.0.1/31");
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.0"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.1"), cidr_last_host(block));
  block = block_of("255.255.255.254/31");
  TEST_ASSERT_EQUAL_HEX32(addr_of("255.255.255.254"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("255.255.255.255"), cidr_last_host(block));

  /* /32: the one host */
  block = block_of("10.0.0.7/32");
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.7"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.7"), cidr_last_host(block));
  block = block_of("0.0.0.0/32");
  TEST_ASSERT_EQUAL_HEX32(0, cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(0, cidr_last_host(block));
  block = block_of("255.255.255.255/32");
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, cidr_last_host(block));

  /* over 32 behaves like 32 */
  block = (cidr_t){addr_of("10.0.0.7"), 40};
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.7"), cidr_first_host(block));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.7"), cidr_last_host(block));
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_host_count
 * --------------------------------------------------------------------- */

void test_cidr_host_count(void) {
  TEST_ASSERT_EQUAL_UINT64(126, cidr_host_count(block_of("132.178.227.11/25")));
  TEST_ASSERT_EQUAL_UINT64(254, cidr_host_count(block_of("192.168.1.0/24")));
  TEST_ASSERT_EQUAL_UINT64(65534, cidr_host_count(block_of("10.1.0.0/16")));
  TEST_ASSERT_EQUAL_UINT64(16777214, cidr_host_count(block_of("10.0.0.0/8")));
  TEST_ASSERT_EQUAL_UINT64(2, cidr_host_count(block_of("192.168.1.4/30")));
}

void test_cidr_host_count_boundaries(void) {
  /* 2^32 - 2, which needs 64 bit arithmetic on the way */
  TEST_ASSERT_EQUAL_UINT64(UINT64_C(4294967294), cidr_host_count(block_of("0.0.0.0/0")));
  TEST_ASSERT_EQUAL_UINT64(UINT64_C(2147483646), cidr_host_count(block_of("0.0.0.0/1")));
  TEST_ASSERT_EQUAL_UINT64(2, cidr_host_count(block_of("10.0.0.0/31")));
  TEST_ASSERT_EQUAL_UINT64(1, cidr_host_count(block_of("10.0.0.7/32")));
  TEST_ASSERT_EQUAL_UINT64(1, cidr_host_count((cidr_t){0, 33}));
}

void test_cidr_host_count_matches_range(void) {
  /* for every prefix the count matches the first to last host range */
  for (uint8_t prefix = 0; prefix <= 32; prefix++) {
    cidr_t block = {addr_of("172.20.33.44"), prefix};
    uint64_t span = (uint64_t)cidr_last_host(block) - cidr_first_host(block) + 1;
    TEST_ASSERT_EQUAL_UINT64(span, cidr_host_count(block));
  }
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_contains
 * --------------------------------------------------------------------- */

void test_cidr_contains_onyx(void) {
  cidr_t onyx = block_of("132.178.227.11/25");
  TEST_ASSERT_TRUE(cidr_contains(onyx, addr_of("132.178.227.50")));
  TEST_ASSERT_FALSE(cidr_contains(onyx, addr_of("132.178.227.200")));
  /* the edges of the block are inside it, one past each edge is not */
  TEST_ASSERT_TRUE(cidr_contains(onyx, addr_of("132.178.227.0")));
  TEST_ASSERT_TRUE(cidr_contains(onyx, addr_of("132.178.227.127")));
  TEST_ASSERT_FALSE(cidr_contains(onyx, addr_of("132.178.227.128")));
  TEST_ASSERT_FALSE(cidr_contains(onyx, addr_of("132.178.226.255")));
}

void test_cidr_contains_boundaries(void) {
  /* /0 contains every address */
  cidr_t all = block_of("0.0.0.0/0");
  TEST_ASSERT_TRUE(cidr_contains(all, 0x00000000));
  TEST_ASSERT_TRUE(cidr_contains(all, 0xFFFFFFFF));
  TEST_ASSERT_TRUE(cidr_contains(all, addr_of("8.8.8.8")));

  /* /31 contains both of its addresses */
  cidr_t pair = block_of("10.0.0.0/31");
  TEST_ASSERT_TRUE(cidr_contains(pair, addr_of("10.0.0.0")));
  TEST_ASSERT_TRUE(cidr_contains(pair, addr_of("10.0.0.1")));
  TEST_ASSERT_FALSE(cidr_contains(pair, addr_of("10.0.0.2")));

  /* /32 contains only itself */
  cidr_t host = block_of("255.255.255.255/32");
  TEST_ASSERT_TRUE(cidr_contains(host, 0xFFFFFFFF));
  TEST_ASSERT_FALSE(cidr_contains(host, 0xFFFFFFFE));
  host = block_of("0.0.0.0/32");
  TEST_ASSERT_TRUE(cidr_contains(host, 0));
  TEST_ASSERT_FALSE(cidr_contains(host, 1));
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_is_network
 * --------------------------------------------------------------------- */

void test_cidr_is_network(void) {
  TEST_ASSERT_TRUE(cidr_is_network(block_of("192.168.1.0/24")));
  TEST_ASSERT_FALSE(cidr_is_network(block_of("192.168.1.5/24")));
  TEST_ASSERT_TRUE(cidr_is_network(block_of("132.178.227.0/25")));
  TEST_ASSERT_FALSE(cidr_is_network(block_of("132.178.227.11/25")));
  TEST_ASSERT_TRUE(cidr_is_network(block_of("10.1.2.128/25")));
  TEST_ASSERT_FALSE(cidr_is_network(block_of("10.1.2.128/24")));
}

void test_cidr_is_network_boundaries(void) {
  TEST_ASSERT_TRUE(cidr_is_network(block_of("0.0.0.0/0")));
  TEST_ASSERT_FALSE(cidr_is_network(block_of("1.2.3.4/0")));
  TEST_ASSERT_FALSE(cidr_is_network(block_of("255.255.255.255/0")));
  TEST_ASSERT_TRUE(cidr_is_network(block_of("10.0.0.0/31")));
  TEST_ASSERT_FALSE(cidr_is_network(block_of("10.0.0.1/31")));
  /* every /32 is a network of one */
  TEST_ASSERT_TRUE(cidr_is_network(block_of("10.0.0.7/32")));
  TEST_ASSERT_TRUE(cidr_is_network(block_of("255.255.255.255/32")));
  TEST_ASSERT_TRUE(cidr_is_network(block_of("0.0.0.0/32")));
}

/* ------------------------------------------------------------------------
 * Task 2 - cidr_subnet
 * --------------------------------------------------------------------- */

/* Check subnet i of the split of block into n is expected. */
static void assert_subnet(const char *block, uint32_t n, uint32_t i, const char *expected) {
  cidr_t out = {SENTINEL, 99};
  char text[CIDR_STR_LEN];
  TEST_ASSERT_TRUE_MESSAGE(cidr_subnet(block_of(block), n, i, &out), expected);
  cidr_format(out, text);
  TEST_ASSERT_EQUAL_STRING(expected, text);
}

/* Check the split of block into n is refused and out is untouched. */
static void assert_no_subnet(cidr_t block, uint32_t n, uint32_t i) {
  cidr_t out = {SENTINEL, 99};
  TEST_ASSERT_FALSE(cidr_subnet(block, n, i, &out));
  TEST_ASSERT_EQUAL_HEX32(SENTINEL, out.addr);
  TEST_ASSERT_EQUAL_UINT8(99, out.prefix);
}

void test_cidr_subnet(void) {
  /* the example from the assignment */
  assert_subnet("192.168.1.0/24", 4, 0, "192.168.1.0/26");
  assert_subnet("192.168.1.0/24", 4, 1, "192.168.1.64/26");
  assert_subnet("192.168.1.0/24", 4, 2, "192.168.1.128/26");
  assert_subnet("192.168.1.0/24", 4, 3, "192.168.1.192/26");

  assert_subnet("10.0.0.0/8", 2, 1, "10.128.0.0/9");
  assert_subnet("10.0.0.0/8", 256, 255, "10.255.0.0/16");
  assert_subnet("132.178.227.0/25", 2, 1, "132.178.227.64/26");
}

void test_cidr_subnet_one_piece(void) {
  /* splitting into 1 hands back the block itself */
  assert_subnet("192.168.1.0/24", 1, 0, "192.168.1.0/24");
  /* for a /0 that is a shift by 32 if done in 32 bits */
  assert_subnet("0.0.0.0/0", 1, 0, "0.0.0.0/0");
  assert_subnet("10.0.0.7/32", 1, 0, "10.0.0.7/32");
}

void test_cidr_subnet_boundaries(void) {
  /* /0 split in half */
  assert_subnet("0.0.0.0/0", 2, 0, "0.0.0.0/1");
  assert_subnet("0.0.0.0/0", 2, 1, "128.0.0.0/1");
  /* the largest legal split: a /0 into 2^31 /31 blocks */
  assert_subnet("0.0.0.0/0", UINT32_C(1) << 31, 0, "0.0.0.0/31");
  assert_subnet("0.0.0.0/0", UINT32_C(1) << 31, (UINT32_C(1) << 31) - 1, "255.255.255.254/31");
  /* down to /32 */
  assert_subnet("10.0.0.0/30", 4, 0, "10.0.0.0/32");
  assert_subnet("10.0.0.0/30", 4, 3, "10.0.0.3/32");
  assert_subnet("255.255.255.254/31", 2, 1, "255.255.255.255/32");
  /* a prefix over 32 is treated as 32 */
  cidr_t out;
  TEST_ASSERT_TRUE(cidr_subnet((cidr_t){0x0A000007, 40}, 1, 0, &out));
  TEST_ASSERT_EQUAL_HEX32(0x0A000007, out.addr);
  TEST_ASSERT_EQUAL_UINT8(32, out.prefix);
}

void test_cidr_subnet_rejects(void) {
  cidr_t block = block_of("192.168.1.0/24");
  /* NULL out */
  TEST_ASSERT_FALSE(cidr_subnet(block, 4, 0, NULL));
  /* not a network */
  assert_no_subnet(block_of("192.168.1.5/24"), 4, 0);
  assert_no_subnet(block_of("1.2.3.4/0"), 1, 0);
  /* not a power of two */
  assert_no_subnet(block, 0, 0);
  assert_no_subnet(block, 3, 0);
  assert_no_subnet(block, 6, 0);
  assert_no_subnet(block, 255, 0);
  assert_no_subnet(block, 0xFFFFFFFF, 0);
  /* the prefix would pass 32 */
  assert_no_subnet(block, 512, 0);
  assert_no_subnet(block_of("10.0.0.0/30"), 8, 0);
  assert_no_subnet(block_of("10.0.0.7/32"), 2, 0);
  assert_no_subnet(block_of("10.0.0.0/8"), UINT32_C(1) << 31, 0);
  /* i out of range */
  assert_no_subnet(block, 4, 4);
  assert_no_subnet(block, 1, 1);
  assert_no_subnet(block, 4, 0xFFFFFFFF);
}

void test_cidr_subnet_covers_block(void) {
  /* the pieces are contiguous, in order, and exactly cover the block */
  cidr_t block = block_of("172.16.0.0/20");
  uint32_t next = cidr_network(block);
  for (uint32_t i = 0; i < 16; i++) {
    cidr_t out;
    TEST_ASSERT_TRUE(cidr_subnet(block, 16, i, &out));
    TEST_ASSERT_EQUAL_UINT8(24, out.prefix);
    TEST_ASSERT_TRUE(cidr_is_network(out));
    TEST_ASSERT_EQUAL_HEX32(next, cidr_network(out));
    next = cidr_broadcast(out) + 1;
  }
  TEST_ASSERT_EQUAL_HEX32(cidr_broadcast(block) + 1, next);
}

/* ------------------------------------------------------------------------
 * Task 3 - route_parse
 * --------------------------------------------------------------------- */

void test_route_parse_valid(void) {
  route_t route;
  TEST_ASSERT_TRUE(route_parse("10.1.2.128/25 10.1.2.1 eth2", &route));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.2.128"), route.dest.addr);
  TEST_ASSERT_EQUAL_UINT8(25, route.dest.prefix);
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.2.1"), route.gateway);
  TEST_ASSERT_EQUAL_STRING("eth2", route.dev);

  /* direct is stored as gateway 0 */
  TEST_ASSERT_TRUE(route_parse("132.178.227.0/25   direct           eno1", &route));
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.0"), route.dest.addr);
  TEST_ASSERT_EQUAL_UINT8(25, route.dest.prefix);
  TEST_ASSERT_EQUAL_HEX32(0, route.gateway);
  TEST_ASSERT_EQUAL_STRING("eno1", route.dev);
}

void test_route_parse_default(void) {
  route_t route = {{SENTINEL, 99}, SENTINEL, "junk"};
  TEST_ASSERT_TRUE(route_parse("default 132.178.227.1 eno1", &route));
  TEST_ASSERT_EQUAL_HEX32(0, route.dest.addr);
  TEST_ASSERT_EQUAL_UINT8(0, route.dest.prefix);
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.1"), route.gateway);
  TEST_ASSERT_EQUAL_STRING("eno1", route.dev);

  /* 0.0.0.0/0 is the same route spelled out */
  TEST_ASSERT_TRUE(route_parse("0.0.0.0/0 direct lo", &route));
  TEST_ASSERT_EQUAL_HEX32(0, route.dest.addr);
  TEST_ASSERT_EQUAL_UINT8(0, route.dest.prefix);
  TEST_ASSERT_EQUAL_HEX32(0, route.gateway);
}

void test_route_parse_whitespace(void) {
  route_t route;
  /* tabs, runs of separators, and whitespace around the line */
  TEST_ASSERT_TRUE(route_parse("10.0.0.0/8\t10.1.0.1\teth1", &route));
  TEST_ASSERT_EQUAL_STRING("eth1", route.dev);
  TEST_ASSERT_TRUE(route_parse("  \t 10.0.0.0/8 \t  10.1.0.1  \t eth1 \t \n", &route));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.0.0.0"), route.dest.addr);
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.0.1"), route.gateway);
  TEST_ASSERT_EQUAL_STRING("eth1", route.dev);
  /* a Windows line ending */
  TEST_ASSERT_TRUE(route_parse("10.0.0.0/8 direct eth1\r\n", &route));
  TEST_ASSERT_EQUAL_STRING("eth1", route.dev);
}

void test_route_parse_boundaries(void) {
  route_t route;
  /* the longest device name that fits, and the shortest */
  TEST_ASSERT_TRUE(route_parse("10.0.0.0/8 direct abcdefghijklmno", &route));
  TEST_ASSERT_EQUAL_STRING("abcdefghijklmno", route.dev);
  TEST_ASSERT_TRUE(route_parse("10.0.0.0/8 direct x", &route));
  TEST_ASSERT_EQUAL_STRING("x", route.dev);
  /* the longest fields and the edges of the address space */
  TEST_ASSERT_TRUE(route_parse("255.255.255.255/32 255.255.255.255 eth0", &route));
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, route.dest.addr);
  TEST_ASSERT_EQUAL_UINT8(32, route.dest.prefix);
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, route.gateway);
  TEST_ASSERT_TRUE(route_parse("10.0.0.0/31 0.0.0.0 p2p", &route));
  TEST_ASSERT_EQUAL_UINT8(31, route.dest.prefix);
  TEST_ASSERT_EQUAL_HEX32(0, route.gateway);
}

void test_route_parse_rejects(void) {
  const char *bad[] = {
      "", "   ", "\n", " \t \n",
      /* wrong number of fields */
      "10.0.0.0/8", "10.0.0.0/8 direct", "10.0.0.0/8 direct eth0 extra",
      "default 132.178.227.1 eno1 metric 100",
      /* comments are handled by table_load(), not here */
      "10.0.0.0/8 direct eth0 # comment", "# just a comment",
      /* bad destinations */
      "10.1.2.5/24 direct eth0",   /* host bits set */
      "132.178.227.11/25 direct eno1",
      "10.0.0.0 direct eth0",      /* no prefix */
      "10.0.0.0/33 direct eth0", "010.0.0.0/8 direct eth0", "garbage direct eth0",
      "Default 132.178.227.1 eno1", "DEFAULT 132.178.227.1 eno1", "defaults 1.2.3.4 eth0",
      "1234567890123456789/8 direct eth0", /* too long to be a block */
      /* bad gateways */
      "10.0.0.0/8 10.0.0 eth0", "10.0.0.0/8 256.0.0.1 eth0", "10.0.0.0/8 gw eth0",
      "10.0.0.0/8 Direct eth0", "10.0.0.0/8 10.0.0.1/8 eth0",
      "10.0.0.0/8 1234567890123456789 eth0", /* too long to be an address */
      /* bad devices */
      "10.0.0.0/8 direct abcdefghijklmnop", /* DEV_NAME_LEN characters, one too many */
      "10.0.0.0/8 direct et\nh0",           /* whitespace that is not a separator */
      "10.0.0.0/8 direct et\vh0",
      /* separators other than spaces and tabs */
      "10.0.0.0/8,direct,eth0", "10.0.0.0/8\ndirect\neth0",
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    route_t route = {{SENTINEL, 99}, SENTINEL, "untouched"};
    TEST_ASSERT_FALSE_MESSAGE(route_parse(bad[i], &route), bad[i]);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(SENTINEL, route.dest.addr, bad[i]);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(SENTINEL, route.gateway, bad[i]);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("untouched", route.dev, bad[i]);
  }
}

void test_route_parse_null(void) {
  route_t route = {{SENTINEL, 99}, SENTINEL, "untouched"};
  TEST_ASSERT_FALSE(route_parse(NULL, &route));
  TEST_ASSERT_EQUAL_HEX32(SENTINEL, route.gateway);
  TEST_ASSERT_FALSE(route_parse("10.0.0.0/8 direct eth0", NULL));
}

/* ------------------------------------------------------------------------
 * Task 3 - table_create, table_destroy, table_count
 * --------------------------------------------------------------------- */

void test_table_create_and_destroy(void) {
  fwd_table_t *table = table_create();
  TEST_ASSERT_NOT_NULL(table);
  TEST_ASSERT_EQUAL_size_t(0, table_count(table));
  TEST_ASSERT_NULL(table_lookup(table, addr_of("8.8.8.8")));
  table_destroy(table);
  /* NULL is allowed */
  table_destroy(NULL);
  TEST_ASSERT_EQUAL_size_t(0, table_count(NULL));
}

/* ------------------------------------------------------------------------
 * Task 3 - table_add
 * --------------------------------------------------------------------- */

void test_table_add(void) {
  fwd_table_t *table = table_create();
  route_t route = route_of("10.0.0.0/8 10.1.0.1 eth1");
  TEST_ASSERT_TRUE(table_add(table, &route));
  TEST_ASSERT_EQUAL_size_t(1, table_count(table));

  /* the table keeps its own copy */
  route.gateway = 0;
  strcpy(route.dev, "changed");
  const route_t *found = table_lookup(table, addr_of("10.9.9.9"));
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.0.1"), found->gateway);
  TEST_ASSERT_EQUAL_STRING("eth1", found->dev);
  table_destroy(table);
}

void test_table_add_rejects(void) {
  fwd_table_t *table = table_create();
  route_t route = route_of("10.0.0.0/8 10.1.0.1 eth1");
  TEST_ASSERT_TRUE(table_add(table, &route));

  /* NULL arguments */
  TEST_ASSERT_FALSE(table_add(NULL, &route));
  TEST_ASSERT_FALSE(table_add(table, NULL));

  /* a second route to the same block, even with a different gateway */
  route_t same = route_of("10.0.0.0/8 direct eth9");
  TEST_ASSERT_FALSE(table_add(table, &same));

  /* host bits set */
  route_t host_bits = route;
  host_bits.dest = block_of("10.1.2.5/24");
  TEST_ASSERT_FALSE(table_add(table, &host_bits));

  /* not a real prefix */
  route_t too_long = route;
  too_long.dest = (cidr_t){addr_of("10.0.0.0"), 33};
  TEST_ASSERT_FALSE(table_add(table, &too_long));

  /* the table is unchanged by all of that */
  TEST_ASSERT_EQUAL_size_t(1, table_count(table));
  TEST_ASSERT_EQUAL_STRING("eth1", table_lookup(table, addr_of("10.0.0.1"))->dev);

  /* the same address with another prefix is a different block */
  route_t other = route_of("10.0.0.0/16 direct eth2");
  TEST_ASSERT_TRUE(table_add(table, &other));
  TEST_ASSERT_EQUAL_size_t(2, table_count(table));
  table_destroy(table);
}

void test_table_add_grows(void) {
  /* enough routes to grow the array several times */
  fwd_table_t *table = table_create();
  for (uint32_t i = 0; i < 300; i++) {
    route_t route;
    memset(&route, 0, sizeof(route));
    route.dest.addr = (UINT32_C(10) << 24) | (i << 8);
    route.dest.prefix = 24;
    route.gateway = i;
    snprintf(route.dev, sizeof(route.dev), "eth%u", (unsigned)i);
    TEST_ASSERT_TRUE(table_add(table, &route));
    TEST_ASSERT_EQUAL_size_t(i + 1, table_count(table));
  }
  /* every route survived the reallocs */
  for (uint32_t i = 0; i < 300; i++) {
    char dev[DEV_NAME_LEN];
    snprintf(dev, sizeof(dev), "eth%u", (unsigned)i);
    const route_t *found = table_lookup(table, (UINT32_C(10) << 24) | (i << 8) | 7);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_HEX32(i, found->gateway);
    TEST_ASSERT_EQUAL_STRING(dev, found->dev);
  }
  table_destroy(table);
}

void test_table_add_terminates_device(void) {
  /* a device name with no '\0' in it is cut short rather than overrun */
  fwd_table_t *table = table_create();
  route_t route = route_of("10.0.0.0/8 direct eth0");
  memset(route.dev, 'x', sizeof(route.dev));
  TEST_ASSERT_TRUE(table_add(table, &route));
  const route_t *found = table_lookup(table, addr_of("10.0.0.1"));
  TEST_ASSERT_EQUAL_size_t(DEV_NAME_LEN - 1, strlen(found->dev));
  table_destroy(table);
}

/* ------------------------------------------------------------------------
 * Task 3 - table_load
 * --------------------------------------------------------------------- */

void test_table_load_example(void) {
  fwd_table_t *table = table_of(EXAMPLE_TABLE);
  TEST_ASSERT_EQUAL_size_t(7, table_count(table));
  table_destroy(table);
}

void test_table_load_comments_and_blank_lines(void) {
  fwd_table_t *table = table_of(
      "\n"
      "# a whole line comment\n"
      "   # an indented comment\n"
      "   \t  \n"
      "10.0.0.0/8 10.1.0.1 eth1# a comment right after\n"
      "\n"
      "#10.1.0.0/16 direct eth1\n"
      "192.168.0.0/16\t10.1.0.1\teth1   \r\n"
      "default 132.178.227.1 eno1"); /* no newline at the end */
  TEST_ASSERT_EQUAL_size_t(3, table_count(table));
  TEST_ASSERT_EQUAL_STRING("10.0.0.0/8", lookup_block(table, "10.1.2.3"));
  TEST_ASSERT_EQUAL_STRING("192.168.0.0/16", lookup_block(table, "192.168.1.1"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "8.8.8.8"));
  table_destroy(table);
}

void test_table_load_empty(void) {
  fwd_table_t *table = table_of("");
  TEST_ASSERT_EQUAL_size_t(0, table_count(table));
  table_destroy(table);
  table = table_of("# nothing but comments\n\n   \n");
  TEST_ASSERT_EQUAL_size_t(0, table_count(table));
  table_destroy(table);
}

void test_table_load_bad_line_in_middle(void) {
  fwd_table_t *table = table_create();
  FILE *f = file_of(
      "# routes\n"
      "default 132.178.227.1 eno1\n"
      "10.0.0.0/8 10.1.0.1 eth1\n"
      "this is not a route\n"
      "10.1.0.0/16 direct eth1\n");
  size_t bad = 99;
  TEST_ASSERT_FALSE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(4, bad);
  /* the routes before the bad line stay, the ones after never load */
  TEST_ASSERT_EQUAL_size_t(2, table_count(table));
  TEST_ASSERT_EQUAL_STRING("10.0.0.0/8", lookup_block(table, "10.1.2.3"));
  fclose(f);
  table_destroy(table);
}

void test_table_load_bad_line_after_three_routes(void) {
  /* the case from Task 6: three routes and then a bad line 4 */
  fwd_table_t *table = table_create();
  FILE *f = file_of(
      "default 132.178.227.1 eno1\n"
      "10.0.0.0/8 10.1.0.1 eth1\n"
      "10.1.0.0/16 direct eth1\n"
      "10.1.2.5/24 direct eth2\n");
  size_t bad = 0;
  TEST_ASSERT_FALSE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(4, bad);
  TEST_ASSERT_EQUAL_size_t(3, table_count(table));
  fclose(f);
  table_destroy(table);
}

void test_table_load_duplicate_line(void) {
  /* a line that parses but that table_add() refuses also fails */
  fwd_table_t *table = table_create();
  FILE *f = file_of(
      "10.0.0.0/8 10.1.0.1 eth1\n"
      "\n"
      "10.0.0.0/8 direct eth2\n");
  size_t bad = 0;
  TEST_ASSERT_FALSE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(3, bad);
  TEST_ASSERT_EQUAL_size_t(1, table_count(table));
  fclose(f);
  table_destroy(table);
}

void test_table_load_bad_first_line(void) {
  fwd_table_t *table = table_create();
  FILE *f = file_of("10.0.0.0/8 direct eth0 extra\n10.1.0.0/16 direct eth1\n");
  size_t bad = 0;
  TEST_ASSERT_FALSE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(1, bad);
  TEST_ASSERT_EQUAL_size_t(0, table_count(table));
  fclose(f);
  table_destroy(table);
}

/* Load len bytes (which may hold a '\0') and check it fails on the expected line. */
static void assert_load_fails(const char *bytes, size_t len, size_t expected_line, size_t expected_count) {
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_size_t(len, fwrite(bytes, 1, len, f));
  rewind(f);
  fwd_table_t *table = table_create();
  size_t bad = 99;
  TEST_ASSERT_FALSE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(expected_line, bad);
  TEST_ASSERT_EQUAL_size_t(expected_count, table_count(table));
  table_destroy(table);
  fclose(f);
}

void test_table_load_embedded_nul(void) {
  /* a '\0' would hide the junk after it, so the whole line is bad */
  const char route_line[] = "10.0.0.0/8 direct eth1\n10.1.0.0/16 direct e\0junk\n";
  assert_load_fails(route_line, sizeof(route_line) - 1, 2, 1);
  /* even when the part before the '\0' looks blank */
  const char blank_line[] = "10.0.0.0/8 direct eth1\n   \0 garbage\n";
  assert_load_fails(blank_line, sizeof(blank_line) - 1, 2, 1);
}

void test_table_load_null_bad_line(void) {
  /* bad_line is optional on success and on failure */
  fwd_table_t *table = table_create();
  FILE *f = file_of("10.0.0.0/8 direct eth0\n");
  TEST_ASSERT_TRUE(table_load(table, f, NULL));
  fclose(f);
  f = file_of("nope\n");
  TEST_ASSERT_FALSE(table_load(table, f, NULL));
  fclose(f);
  TEST_ASSERT_EQUAL_size_t(1, table_count(table));
  table_destroy(table);
}

void test_table_load_null_arguments(void) {
  fwd_table_t *table = table_create();
  FILE *f = file_of("10.0.0.0/8 direct eth0\n");
  size_t bad = 99;
  TEST_ASSERT_FALSE(table_load(NULL, f, &bad));
  TEST_ASSERT_EQUAL_size_t(0, bad);
  bad = 99;
  TEST_ASSERT_FALSE(table_load(table, NULL, &bad));
  TEST_ASSERT_EQUAL_size_t(0, bad);
  TEST_ASSERT_FALSE(table_load(NULL, NULL, NULL));
  TEST_ASSERT_EQUAL_size_t(0, table_count(table));
  fclose(f);
  table_destroy(table);
}

void test_table_load_read_error(void) {
  /* reading a stream opened only for writing fails, which is a read error */
  FILE *f = fopen("/dev/null", "w");
  TEST_ASSERT_NOT_NULL(f);
  fwd_table_t *table = table_create();
  size_t bad = 99;
  TEST_ASSERT_FALSE(table_load(table, f, &bad));
  TEST_ASSERT_EQUAL_size_t(0, bad);
  TEST_ASSERT_EQUAL_size_t(0, table_count(table));
  fclose(f);
  table_destroy(table);
}

void test_table_load_appends(void) {
  /* loading adds to what is already in the table */
  fwd_table_t *table = table_create();
  route_t route = route_of("172.16.0.0/12 direct eth3");
  TEST_ASSERT_TRUE(table_add(table, &route));
  FILE *f = file_of(EXAMPLE_TABLE);
  TEST_ASSERT_TRUE(table_load(table, f, NULL));
  TEST_ASSERT_EQUAL_size_t(8, table_count(table));
  TEST_ASSERT_EQUAL_STRING("172.16.0.0/12", lookup_block(table, "172.20.1.1"));
  fclose(f);
  table_destroy(table);
}

/* ------------------------------------------------------------------------
 * Task 3 - table_lookup
 * --------------------------------------------------------------------- */

/* Every lookup the example table should get right. */
static void assert_example_lookups(const fwd_table_t *table) {
  /* 10.1.2.200 is inside four blocks and the /25 wins */
  TEST_ASSERT_EQUAL_STRING("10.1.2.128/25", lookup_block(table, "10.1.2.200"));
  TEST_ASSERT_EQUAL_STRING("10.1.2.128/25", lookup_block(table, "10.1.2.128"));
  TEST_ASSERT_EQUAL_STRING("10.1.2.128/25", lookup_block(table, "10.1.2.255"));
  /* the /24 for the lower half */
  TEST_ASSERT_EQUAL_STRING("10.1.2.0/24", lookup_block(table, "10.1.2.5"));
  TEST_ASSERT_EQUAL_STRING("10.1.2.0/24", lookup_block(table, "10.1.2.127"));
  /* the /16 and /8 for the rest of 10/8 */
  TEST_ASSERT_EQUAL_STRING("10.1.0.0/16", lookup_block(table, "10.1.3.1"));
  TEST_ASSERT_EQUAL_STRING("10.0.0.0/8", lookup_block(table, "10.2.0.1"));
  TEST_ASSERT_EQUAL_STRING("10.0.0.0/8", lookup_block(table, "10.255.255.255"));
  TEST_ASSERT_EQUAL_STRING("192.168.0.0/16", lookup_block(table, "192.168.5.5"));
  /* Onyx's subnet, and the default route for everything else */
  TEST_ASSERT_EQUAL_STRING("132.178.227.0/25", lookup_block(table, "132.178.227.50"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "132.178.227.200"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "8.8.8.8"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "11.0.0.0"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "0.0.0.0"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "255.255.255.255"));
}

void test_table_lookup_example(void) {
  fwd_table_t *table = table_of(EXAMPLE_TABLE);
  assert_example_lookups(table);

  /* the gateway and device of the routes from the assignment's output */
  const route_t *r = table_lookup(table, addr_of("132.178.227.50"));
  TEST_ASSERT_EQUAL_HEX32(0, r->gateway);
  TEST_ASSERT_EQUAL_STRING("eno1", r->dev);
  r = table_lookup(table, addr_of("8.8.8.8"));
  TEST_ASSERT_EQUAL_HEX32(addr_of("132.178.227.1"), r->gateway);
  TEST_ASSERT_EQUAL_STRING("eno1", r->dev);
  r = table_lookup(table, addr_of("10.1.2.5"));
  TEST_ASSERT_EQUAL_HEX32(0, r->gateway);
  TEST_ASSERT_EQUAL_STRING("eth2", r->dev);
  r = table_lookup(table, addr_of("10.1.2.200"));
  TEST_ASSERT_EQUAL_HEX32(addr_of("10.1.2.1"), r->gateway);
  TEST_ASSERT_EQUAL_STRING("eth2", r->dev);
  table_destroy(table);
}

void test_table_lookup_order_does_not_matter(void) {
  /* the same table added in other orders gives the same answers */
  fwd_table_t *reversed = table_of(EXAMPLE_REVERSED);
  assert_example_lookups(reversed);
  table_destroy(reversed);

  fwd_table_t *shuffled = table_of(EXAMPLE_SHUFFLED);
  assert_example_lookups(shuffled);
  table_destroy(shuffled);
}

void test_table_lookup_longest_wins(void) {
  /* several nested routes added shortest first, so first match is wrong */
  fwd_table_t *table = table_of(
      "10.0.0.0/8 direct a\n"
      "10.1.0.0/16 direct b\n"
      "10.1.2.0/24 direct c\n"
      "10.1.2.128/25 direct d\n"
      "10.1.2.200/32 direct e\n");
  TEST_ASSERT_EQUAL_STRING("e", table_lookup(table, addr_of("10.1.2.200"))->dev);
  TEST_ASSERT_EQUAL_STRING("d", table_lookup(table, addr_of("10.1.2.201"))->dev);
  TEST_ASSERT_EQUAL_STRING("c", table_lookup(table, addr_of("10.1.2.1"))->dev);
  TEST_ASSERT_EQUAL_STRING("b", table_lookup(table, addr_of("10.1.200.1"))->dev);
  TEST_ASSERT_EQUAL_STRING("a", table_lookup(table, addr_of("10.200.0.1"))->dev);
  TEST_ASSERT_NULL(table_lookup(table, addr_of("11.0.0.1")));
  table_destroy(table);
}

void test_table_lookup_no_default(void) {
  /* without a default route an address outside every block matches nothing */
  fwd_table_t *table = table_of(
      "132.178.227.0/25 direct eno1\n"
      "10.0.0.0/8 10.1.0.1 eth1\n");
  TEST_ASSERT_EQUAL_STRING("none", lookup_block(table, "8.8.8.8"));
  TEST_ASSERT_EQUAL_STRING("none", lookup_block(table, "132.178.227.200"));
  TEST_ASSERT_EQUAL_STRING("none", lookup_block(table, "0.0.0.0"));
  TEST_ASSERT_EQUAL_STRING("none", lookup_block(table, "255.255.255.255"));
  TEST_ASSERT_EQUAL_STRING("132.178.227.0/25", lookup_block(table, "132.178.227.50"));
  TEST_ASSERT_EQUAL_STRING("10.0.0.0/8", lookup_block(table, "10.9.8.7"));
  table_destroy(table);
}

void test_table_lookup_default_only(void) {
  /* a /0 must match every address, including both ends of the space */
  fwd_table_t *table = table_of("default 192.0.2.1 eth0\n");
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "0.0.0.0"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "8.8.8.8"));
  TEST_ASSERT_EQUAL_STRING("0.0.0.0/0", lookup_block(table, "255.255.255.255"));
  table_destroy(table);
}

void test_table_lookup_boundaries(void) {
  /* /31 and /32 routes, and routes at both ends of the address space */
  fwd_table_t *table = table_of(
      "0.0.0.0/32 direct zero\n"
      "255.255.255.255/32 direct ones\n"
      "10.0.0.0/31 direct p2p\n"
      "10.0.0.0/30 direct quad\n"
      "default 10.0.0.1 eth0\n");
  TEST_ASSERT_EQUAL_STRING("zero", table_lookup(table, 0)->dev);
  TEST_ASSERT_EQUAL_STRING("eth0", table_lookup(table, 1)->dev);
  TEST_ASSERT_EQUAL_STRING("ones", table_lookup(table, 0xFFFFFFFF)->dev);
  TEST_ASSERT_EQUAL_STRING("eth0", table_lookup(table, 0xFFFFFFFE)->dev);
  TEST_ASSERT_EQUAL_STRING("p2p", table_lookup(table, addr_of("10.0.0.0"))->dev);
  TEST_ASSERT_EQUAL_STRING("p2p", table_lookup(table, addr_of("10.0.0.1"))->dev);
  TEST_ASSERT_EQUAL_STRING("quad", table_lookup(table, addr_of("10.0.0.2"))->dev);
  TEST_ASSERT_EQUAL_STRING("quad", table_lookup(table, addr_of("10.0.0.3"))->dev);
  TEST_ASSERT_EQUAL_STRING("eth0", table_lookup(table, addr_of("10.0.0.4"))->dev);
  table_destroy(table);
}

void test_table_lookup_null(void) {
  TEST_ASSERT_NULL(table_lookup(NULL, addr_of("8.8.8.8")));
}

int main(void) {
  UNITY_BEGIN();

  /* Task 1 */
  RUN_TEST(test_ip_parse_valid);
  RUN_TEST(test_ip_parse_boundaries);
  RUN_TEST(test_ip_parse_rejects_malformed);
  RUN_TEST(test_ip_parse_null);
  RUN_TEST(test_ip_format);
  RUN_TEST(test_ip_format_boundaries);
  RUN_TEST(test_ip_round_trip);
  RUN_TEST(test_cidr_parse_valid);
  RUN_TEST(test_cidr_parse_boundaries);
  RUN_TEST(test_cidr_parse_rejects_malformed);
  RUN_TEST(test_cidr_parse_null);
  RUN_TEST(test_cidr_format);
  RUN_TEST(test_cidr_format_boundaries);
  RUN_TEST(test_cidr_round_trip);

  /* Task 2 */
  RUN_TEST(test_cidr_mask);
  RUN_TEST(test_cidr_mask_boundaries);
  RUN_TEST(test_cidr_mask_every_prefix);
  RUN_TEST(test_cidr_network_and_broadcast_onyx);
  RUN_TEST(test_cidr_network_and_broadcast);
  RUN_TEST(test_cidr_network_and_broadcast_boundaries);
  RUN_TEST(test_cidr_host_range_onyx);
  RUN_TEST(test_cidr_host_range);
  RUN_TEST(test_cidr_host_range_boundaries);
  RUN_TEST(test_cidr_host_count);
  RUN_TEST(test_cidr_host_count_boundaries);
  RUN_TEST(test_cidr_host_count_matches_range);
  RUN_TEST(test_cidr_contains_onyx);
  RUN_TEST(test_cidr_contains_boundaries);
  RUN_TEST(test_cidr_is_network);
  RUN_TEST(test_cidr_is_network_boundaries);
  RUN_TEST(test_cidr_subnet);
  RUN_TEST(test_cidr_subnet_one_piece);
  RUN_TEST(test_cidr_subnet_boundaries);
  RUN_TEST(test_cidr_subnet_rejects);
  RUN_TEST(test_cidr_subnet_covers_block);

  /* Task 3 */
  RUN_TEST(test_route_parse_valid);
  RUN_TEST(test_route_parse_default);
  RUN_TEST(test_route_parse_whitespace);
  RUN_TEST(test_route_parse_boundaries);
  RUN_TEST(test_route_parse_rejects);
  RUN_TEST(test_route_parse_null);
  RUN_TEST(test_table_create_and_destroy);
  RUN_TEST(test_table_add);
  RUN_TEST(test_table_add_rejects);
  RUN_TEST(test_table_add_grows);
  RUN_TEST(test_table_add_terminates_device);
  RUN_TEST(test_table_load_example);
  RUN_TEST(test_table_load_comments_and_blank_lines);
  RUN_TEST(test_table_load_empty);
  RUN_TEST(test_table_load_bad_line_in_middle);
  RUN_TEST(test_table_load_bad_line_after_three_routes);
  RUN_TEST(test_table_load_duplicate_line);
  RUN_TEST(test_table_load_bad_first_line);
  RUN_TEST(test_table_load_embedded_nul);
  RUN_TEST(test_table_load_null_bad_line);
  RUN_TEST(test_table_load_null_arguments);
  RUN_TEST(test_table_load_read_error);
  RUN_TEST(test_table_load_appends);
  RUN_TEST(test_table_lookup_example);
  RUN_TEST(test_table_lookup_order_does_not_matter);
  RUN_TEST(test_table_lookup_longest_wins);
  RUN_TEST(test_table_lookup_no_default);
  RUN_TEST(test_table_lookup_default_only);
  RUN_TEST(test_table_lookup_boundaries);
  RUN_TEST(test_table_lookup_null);

  return UNITY_END();
}
