# Project 3

- Name: Mev Causevic
- Email: mevludincausevic@u.boisestate.edu
- Class: CS425-001

## Known Bugs or Issues

None that I know of. Everything in `src/lab.h` is implemented, all 65 unit tests pass,
coverage of `src/lab.c` is 100% (lines and branches), and `make leak` and `make leak-test` are clean.

## Experience

This class is AI forward, so I built this one with Claude. A few things that came up
along the way:

- gcc warned that `snprintf()` in `cidr_format()` might truncate, but only in the `-O0`
  builds. At `-O0` it can't see that the prefix is at most 32, so it assumes three digits
  and 15 + 1 + 3 doesn't fit in 19 bytes with the `'\0'`. I write the prefix digits by
  hand now and every build is warning free.
- Testing the "file could not be read" path in `table_load()` was tricky because
  `tmpfile()` is always readable. Opening `/dev/null` for writing only and handing that to
  `table_load()` makes `getline()` fail with a real read error.
- The order independence test caught nothing in my code, but it's the test that would
  catch a lookup that returns the first match instead of the longest one, so it stays.

## Design

### Parsing

`ip_parse()` and `cidr_parse()` share two small helpers. `parse_decimal()` reads one
number with a max (255 for an octet, 32 for a prefix), rejects a leading zero, and stops
as soon as the value passes the max so it can't overflow. `parse_quad()` reads exactly
four octets with a single period between them and returns where it stopped. Then
`ip_parse()` requires the string to end right there, and `cidr_parse()` requires a `/`
and a prefix and then the end. That is what rejects `1.2.3.4x`, `1.2.3.4/024` and the rest
of the list from the assignment without `inet_aton()` or `sscanf()`.

### /0, /31 and /32

Everything is built on `cidr_mask()`, so the edge cases mostly live in one place.

- **/0.** `0xFFFFFFFF << (32 - prefix)` shifts by 32 for a /0, which is undefined
  behavior. On x86 it shifts by 0 and returns all ones, which would make the default route
  match only 0.0.0.0. So `cidr_mask(0)` returns 0 before it ever gets to the shift. The
  host count of a /0 is 2^32 - 2, and the 2^32 doesn't fit in 32 bits, so
  `cidr_host_count()` does that math with `UINT64_C(1) << (32 - prefix)`. `cidr_subnet()`
  has the same problem when you split a /0 into 1 piece (the offset shift is by 32), so it
  does its shift in 64 bits too.
- **/31.** A point to point link (RFC 3021) has no network or broadcast address, so both
  addresses are hosts. `cidr_first_host()` returns the network address and
  `cidr_last_host()` returns the broadcast address instead of adding or subtracting one,
  and the host count is 2.
- **/32.** One host. Network, broadcast, first and last host are all the address itself,
  and the host count is 1.
- **Over 32.** `cidr_mask()` treats anything over 32 as 32, so a small `clamp_prefix()`
  helper does the same everywhere else (formatting, host range, host count, subnetting)
  to keep it consistent. `table_add()` refuses a prefix over 32 since it isn't a real block.

### Forwarding table

`fwd_table_t` is a growable array of `route_t` with a count and a capacity. It starts
empty, `table_add()` doubles the capacity with `realloc()` when it fills up (starting at
8), and `table_destroy()` frees the array and the struct. If `realloc()` fails the old
array is still valid, so the table is unchanged like the spec says. `table_add()` also
refuses a route whose block has host bits set, and a second route to the same block (same
network and same prefix), which is what `ip route add` does.

`table_load()` reads with `getline()`, cuts each line off at the first `#`, skips lines
that are blank after that, and hands the rest to `route_parse()` and `table_add()`. On
the first bad line it records the 1-based line number and stops. A line with a `'\0'` in
it counts as bad too, since the parser would only see the part before it. The routes before it stay
in the table, and the line buffer is freed on every path. `getline()` returns -1 for a read
error as well as end of file, so it checks `ferror()` after the loop and reports line 0 for
a read error.

### Longest prefix match

`table_lookup()` is a linear scan. It looks at every route, and if the route's block
contains the address and its prefix is longer than the best match so far, that route
becomes the best match. Since it always checks every route, the order they were added in
doesn't matter. There are no ties to break: two different blocks with the same prefix
can't both contain the same address, and `table_add()` won't hold the same block twice.

The default route isn't a special case. It's a /0, so it contains every address and loses
to any match with a longer prefix, which makes it the route of last resort. A trie would
make each lookup at most 32 steps no matter how big the table gets, but a scan is plenty
for tables this size and much easier to get to 100% coverage.

### Command line

`src/main.c` has one function per command. `info` and `split` print exactly the format
from the assignment. `lookup` loads the table first (exit 2 if it can't be opened or has a
bad line, with the file and line number in the message), then handles every address even
after a bad one. If the table fails and an address is also malformed, it still reports the
address and exits 1, since the spec says a malformed address wins when a run hits more than
one error. A malformed address prints an error to stderr, an address with no route
prints `<address> unreachable`, and the exit code is 1 if any address was malformed,
otherwise 3 if any was unreachable, otherwise 0. Running it with no arguments prints the
usage to stdout and exits 0, and any other bad command line prints the usage to stderr and
exits 1.
