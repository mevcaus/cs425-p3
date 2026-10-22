# Submission Report

- Submission generated at 10/22/2026 at 17:53:05

- Machine info: Linux runnervm8k3b6 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

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
usage to stdout and exits 0. An unknown command or the wrong number of arguments prints the
usage to stderr and exits 1, and a malformed argument (like `info 1.2.3.4`) prints an error
saying what was wrong and exits 1.

---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs425-p3/cs425-p3'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/lab.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p3/cs425-p3'
make[1]: Entering directory '/home/runner/work/cs425-p3/cs425-p3'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/lab.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs425-p3/cs425-p3'
make[1]: Entering directory '/home/runner/work/cs425-p3/cs425-p3'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/lab.c.o build/tests/main.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs425-p3/cs425-p3'
make[1]: Entering directory '/home/runner/work/cs425-p3/cs425-p3'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/lab.c.o build/debug-test/main.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p3/cs425-p3'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
tests/lab-test.c:1156:test_ip_parse_valid:PASS
tests/lab-test.c:1157:test_ip_parse_boundaries:PASS
tests/lab-test.c:1158:test_ip_parse_rejects_malformed:PASS
tests/lab-test.c:1159:test_ip_parse_null:PASS
tests/lab-test.c:1160:test_ip_format:PASS
tests/lab-test.c:1161:test_ip_format_boundaries:PASS
tests/lab-test.c:1162:test_ip_round_trip:PASS
tests/lab-test.c:1163:test_cidr_parse_valid:PASS
tests/lab-test.c:1164:test_cidr_parse_boundaries:PASS
tests/lab-test.c:1165:test_cidr_parse_rejects_malformed:PASS
tests/lab-test.c:1166:test_cidr_parse_null:PASS
tests/lab-test.c:1167:test_cidr_format:PASS
tests/lab-test.c:1168:test_cidr_format_boundaries:PASS
tests/lab-test.c:1169:test_cidr_round_trip:PASS
tests/lab-test.c:1172:test_cidr_mask:PASS
tests/lab-test.c:1173:test_cidr_mask_boundaries:PASS
tests/lab-test.c:1174:test_cidr_mask_every_prefix:PASS
tests/lab-test.c:1175:test_cidr_network_and_broadcast_onyx:PASS
tests/lab-test.c:1176:test_cidr_network_and_broadcast:PASS
tests/lab-test.c:1177:test_cidr_network_and_broadcast_boundaries:PASS
tests/lab-test.c:1178:test_cidr_host_range_onyx:PASS
tests/lab-test.c:1179:test_cidr_host_range:PASS
tests/lab-test.c:1180:test_cidr_host_range_boundaries:PASS
tests/lab-test.c:1181:test_cidr_host_count:PASS
tests/lab-test.c:1182:test_cidr_host_count_boundaries:PASS
tests/lab-test.c:1183:test_cidr_host_count_matches_range:PASS
tests/lab-test.c:1184:test_cidr_contains_onyx:PASS
tests/lab-test.c:1185:test_cidr_contains_boundaries:PASS
tests/lab-test.c:1186:test_cidr_is_network:PASS
tests/lab-test.c:1187:test_cidr_is_network_boundaries:PASS
tests/lab-test.c:1188:test_cidr_subnet:PASS
tests/lab-test.c:1189:test_cidr_subnet_one_piece:PASS
tests/lab-test.c:1190:test_cidr_subnet_boundaries:PASS
tests/lab-test.c:1191:test_cidr_subnet_rejects:PASS
tests/lab-test.c:1192:test_cidr_subnet_covers_block:PASS
tests/lab-test.c:1195:test_route_parse_valid:PASS
tests/lab-test.c:1196:test_route_parse_default:PASS
tests/lab-test.c:1197:test_route_parse_whitespace:PASS
tests/lab-test.c:1198:test_route_parse_boundaries:PASS
tests/lab-test.c:1199:test_route_parse_rejects:PASS
tests/lab-test.c:1200:test_route_parse_null:PASS
tests/lab-test.c:1201:test_table_create_and_destroy:PASS
tests/lab-test.c:1202:test_table_add:PASS
tests/lab-test.c:1203:test_table_add_rejects:PASS
tests/lab-test.c:1204:test_table_add_grows:PASS
tests/lab-test.c:1205:test_table_add_terminates_device:PASS
tests/lab-test.c:1206:test_table_load_example:PASS
tests/lab-test.c:1207:test_table_load_comments_and_blank_lines:PASS
tests/lab-test.c:1208:test_table_load_empty:PASS
tests/lab-test.c:1209:test_table_load_bad_line_in_middle:PASS
tests/lab-test.c:1210:test_table_load_bad_line_after_three_routes:PASS
tests/lab-test.c:1211:test_table_load_duplicate_line:PASS
tests/lab-test.c:1212:test_table_load_bad_first_line:PASS
tests/lab-test.c:1213:test_table_load_embedded_nul:PASS
tests/lab-test.c:1214:test_table_load_null_bad_line:PASS
tests/lab-test.c:1215:test_table_load_null_arguments:PASS
tests/lab-test.c:1216:test_table_load_read_error:PASS
tests/lab-test.c:1217:test_table_load_appends:PASS
tests/lab-test.c:1218:test_table_lookup_example:PASS
tests/lab-test.c:1219:test_table_lookup_order_does_not_matter:PASS
tests/lab-test.c:1220:test_table_lookup_longest_wins:PASS
tests/lab-test.c:1221:test_table_lookup_no_default:PASS
tests/lab-test.c:1222:test_table_lookup_default_only:PASS
tests/lab-test.c:1223:test_table_lookup_boundaries:PASS
tests/lab-test.c:1224:test_table_lookup_null:PASS

-----------------------
65 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
tests/lab-test.c:1156:test_ip_parse_valid:PASS
tests/lab-test.c:1157:test_ip_parse_boundaries:PASS
tests/lab-test.c:1158:test_ip_parse_rejects_malformed:PASS
tests/lab-test.c:1159:test_ip_parse_null:PASS
tests/lab-test.c:1160:test_ip_format:PASS
tests/lab-test.c:1161:test_ip_format_boundaries:PASS
tests/lab-test.c:1162:test_ip_round_trip:PASS
tests/lab-test.c:1163:test_cidr_parse_valid:PASS
tests/lab-test.c:1164:test_cidr_parse_boundaries:PASS
tests/lab-test.c:1165:test_cidr_parse_rejects_malformed:PASS
tests/lab-test.c:1166:test_cidr_parse_null:PASS
tests/lab-test.c:1167:test_cidr_format:PASS
tests/lab-test.c:1168:test_cidr_format_boundaries:PASS
tests/lab-test.c:1169:test_cidr_round_trip:PASS
tests/lab-test.c:1172:test_cidr_mask:PASS
tests/lab-test.c:1173:test_cidr_mask_boundaries:PASS
tests/lab-test.c:1174:test_cidr_mask_every_prefix:PASS
tests/lab-test.c:1175:test_cidr_network_and_broadcast_onyx:PASS
tests/lab-test.c:1176:test_cidr_network_and_broadcast:PASS
tests/lab-test.c:1177:test_cidr_network_and_broadcast_boundaries:PASS
tests/lab-test.c:1178:test_cidr_host_range_onyx:PASS
tests/lab-test.c:1179:test_cidr_host_range:PASS
tests/lab-test.c:1180:test_cidr_host_range_boundaries:PASS
tests/lab-test.c:1181:test_cidr_host_count:PASS
tests/lab-test.c:1182:test_cidr_host_count_boundaries:PASS
tests/lab-test.c:1183:test_cidr_host_count_matches_range:PASS
tests/lab-test.c:1184:test_cidr_contains_onyx:PASS
tests/lab-test.c:1185:test_cidr_contains_boundaries:PASS
tests/lab-test.c:1186:test_cidr_is_network:PASS
tests/lab-test.c:1187:test_cidr_is_network_boundaries:PASS
tests/lab-test.c:1188:test_cidr_subnet:PASS
tests/lab-test.c:1189:test_cidr_subnet_one_piece:PASS
tests/lab-test.c:1190:test_cidr_subnet_boundaries:PASS
tests/lab-test.c:1191:test_cidr_subnet_rejects:PASS
tests/lab-test.c:1192:test_cidr_subnet_covers_block:PASS
tests/lab-test.c:1195:test_route_parse_valid:PASS
tests/lab-test.c:1196:test_route_parse_default:PASS
tests/lab-test.c:1197:test_route_parse_whitespace:PASS
tests/lab-test.c:1198:test_route_parse_boundaries:PASS
tests/lab-test.c:1199:test_route_parse_rejects:PASS
tests/lab-test.c:1200:test_route_parse_null:PASS
tests/lab-test.c:1201:test_table_create_and_destroy:PASS
tests/lab-test.c:1202:test_table_add:PASS
tests/lab-test.c:1203:test_table_add_rejects:PASS
tests/lab-test.c:1204:test_table_add_grows:PASS
tests/lab-test.c:1205:test_table_add_terminates_device:PASS
tests/lab-test.c:1206:test_table_load_example:PASS
tests/lab-test.c:1207:test_table_load_comments_and_blank_lines:PASS
tests/lab-test.c:1208:test_table_load_empty:PASS
tests/lab-test.c:1209:test_table_load_bad_line_in_middle:PASS
tests/lab-test.c:1210:test_table_load_bad_line_after_three_routes:PASS
tests/lab-test.c:1211:test_table_load_duplicate_line:PASS
tests/lab-test.c:1212:test_table_load_bad_first_line:PASS
tests/lab-test.c:1213:test_table_load_embedded_nul:PASS
tests/lab-test.c:1214:test_table_load_null_bad_line:PASS
tests/lab-test.c:1215:test_table_load_null_arguments:PASS
tests/lab-test.c:1216:test_table_load_read_error:PASS
tests/lab-test.c:1217:test_table_load_appends:PASS
tests/lab-test.c:1218:test_table_lookup_example:PASS
tests/lab-test.c:1219:test_table_lookup_order_does_not_matter:PASS
tests/lab-test.c:1220:test_table_lookup_longest_wins:PASS
tests/lab-test.c:1221:test_table_lookup_no_default:PASS
tests/lab-test.c:1222:test_table_lookup_default_only:PASS
tests/lab-test.c:1223:test_table_lookup_boundaries:PASS
tests/lab-test.c:1224:test_table_lookup_null:PASS

-----------------------
65 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                         Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                      233      233   100%
------------------------------------------------------------------------------
TOTAL                                          233      233   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
tests/lab-test.c:1156:test_ip_parse_valid:PASS
tests/lab-test.c:1157:test_ip_parse_boundaries:PASS
tests/lab-test.c:1158:test_ip_parse_rejects_malformed:PASS
tests/lab-test.c:1159:test_ip_parse_null:PASS
tests/lab-test.c:1160:test_ip_format:PASS
tests/lab-test.c:1161:test_ip_format_boundaries:PASS
tests/lab-test.c:1162:test_ip_round_trip:PASS
tests/lab-test.c:1163:test_cidr_parse_valid:PASS
tests/lab-test.c:1164:test_cidr_parse_boundaries:PASS
tests/lab-test.c:1165:test_cidr_parse_rejects_malformed:PASS
tests/lab-test.c:1166:test_cidr_parse_null:PASS
tests/lab-test.c:1167:test_cidr_format:PASS
tests/lab-test.c:1168:test_cidr_format_boundaries:PASS
tests/lab-test.c:1169:test_cidr_round_trip:PASS
tests/lab-test.c:1172:test_cidr_mask:PASS
tests/lab-test.c:1173:test_cidr_mask_boundaries:PASS
tests/lab-test.c:1174:test_cidr_mask_every_prefix:PASS
tests/lab-test.c:1175:test_cidr_network_and_broadcast_onyx:PASS
tests/lab-test.c:1176:test_cidr_network_and_broadcast:PASS
tests/lab-test.c:1177:test_cidr_network_and_broadcast_boundaries:PASS
tests/lab-test.c:1178:test_cidr_host_range_onyx:PASS
tests/lab-test.c:1179:test_cidr_host_range:PASS
tests/lab-test.c:1180:test_cidr_host_range_boundaries:PASS
tests/lab-test.c:1181:test_cidr_host_count:PASS
tests/lab-test.c:1182:test_cidr_host_count_boundaries:PASS
tests/lab-test.c:1183:test_cidr_host_count_matches_range:PASS
tests/lab-test.c:1184:test_cidr_contains_onyx:PASS
tests/lab-test.c:1185:test_cidr_contains_boundaries:PASS
tests/lab-test.c:1186:test_cidr_is_network:PASS
tests/lab-test.c:1187:test_cidr_is_network_boundaries:PASS
tests/lab-test.c:1188:test_cidr_subnet:PASS
tests/lab-test.c:1189:test_cidr_subnet_one_piece:PASS
tests/lab-test.c:1190:test_cidr_subnet_boundaries:PASS
tests/lab-test.c:1191:test_cidr_subnet_rejects:PASS
tests/lab-test.c:1192:test_cidr_subnet_covers_block:PASS
tests/lab-test.c:1195:test_route_parse_valid:PASS
tests/lab-test.c:1196:test_route_parse_default:PASS
tests/lab-test.c:1197:test_route_parse_whitespace:PASS
tests/lab-test.c:1198:test_route_parse_boundaries:PASS
tests/lab-test.c:1199:test_route_parse_rejects:PASS
tests/lab-test.c:1200:test_route_parse_null:PASS
tests/lab-test.c:1201:test_table_create_and_destroy:PASS
tests/lab-test.c:1202:test_table_add:PASS
tests/lab-test.c:1203:test_table_add_rejects:PASS
tests/lab-test.c:1204:test_table_add_grows:PASS
tests/lab-test.c:1205:test_table_add_terminates_device:PASS
tests/lab-test.c:1206:test_table_load_example:PASS
tests/lab-test.c:1207:test_table_load_comments_and_blank_lines:PASS
tests/lab-test.c:1208:test_table_load_empty:PASS
tests/lab-test.c:1209:test_table_load_bad_line_in_middle:PASS
tests/lab-test.c:1210:test_table_load_bad_line_after_three_routes:PASS
tests/lab-test.c:1211:test_table_load_duplicate_line:PASS
tests/lab-test.c:1212:test_table_load_bad_first_line:PASS
tests/lab-test.c:1213:test_table_load_embedded_nul:PASS
tests/lab-test.c:1214:test_table_load_null_bad_line:PASS
tests/lab-test.c:1215:test_table_load_null_arguments:PASS
tests/lab-test.c:1216:test_table_load_read_error:PASS
tests/lab-test.c:1217:test_table_load_appends:PASS
tests/lab-test.c:1218:test_table_lookup_example:PASS
tests/lab-test.c:1219:test_table_lookup_order_does_not_matter:PASS
tests/lab-test.c:1220:test_table_lookup_longest_wins:PASS
tests/lab-test.c:1221:test_table_lookup_no_default:PASS
tests/lab-test.c:1222:test_table_lookup_default_only:PASS
tests/lab-test.c:1223:test_table_lookup_boundaries:PASS
tests/lab-test.c:1224:test_table_lookup_null:PASS

-----------------------
65 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

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

```

### lab.h

```c

#ifndef LAB_H
#define LAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Every address in this file is a uint32_t in host byte order, so the first
 * octet of 192.168.1.10 is the most significant byte: 0xC0A8010A.
 */

/** Room for the longest dotted quad, "255.255.255.255", and its '\0'. */
#define IP_STR_LEN 16

/** Room for the longest CIDR string, "255.255.255.255/32", and its '\0'. */
#define CIDR_STR_LEN 19

/** Room for an interface name; IFNAMSIZ on Linux, including the '\0'. */
#define DEV_NAME_LEN 16

/** An address block in CIDR notation: an address and a prefix length. */
typedef struct {
  uint32_t addr;  /**< any address in the block, host byte order */
  uint8_t prefix; /**< 0 to 32 */
} cidr_t;

/** One entry in a forwarding table. */
typedef struct {
  cidr_t dest;            /**< destination block; host bits must be zero */
  uint32_t gateway;       /**< next hop, or 0 when the block is directly connected */
  char dev[DEV_NAME_LEN]; /**< outgoing interface, for example "eth0" */
} route_t;

/** A forwarding table. Its layout is private to lab.c. */
typedef struct fwd_table fwd_table_t;

/* ------------------------------------------------------------------------
 * Task 1 - Address parsing and formatting
 * --------------------------------------------------------------------- */

/**
 * @brief Parse a dotted quad such as "192.168.1.10".
 *
 * Exactly four decimal octets from 0 to 255 separated by single periods, with
 * nothing before, between or after them. An octet with a leading zero ("010")
 * is rejected, because inet_aton() reads it as octal and that is never what a
 * person typing it meant.
 *
 * @param text The string to parse.
 * @param addr Where the address is stored on success; untouched on failure.
 * @return true if text is a valid dotted quad, false otherwise (including NULL).
 */
bool ip_parse(const char *text, uint32_t *addr);

/**
 * @brief Format an address as a dotted quad.
 * @param addr The address, host byte order.
 * @param out A buffer of at least IP_STR_LEN bytes.
 */
void ip_format(uint32_t addr, char out[IP_STR_LEN]);

/**
 * @brief Parse a block in CIDR notation such as "192.168.1.0/24".
 *
 * The address follows the rules of ip_parse() and the prefix is a decimal
 * number from 0 to 32 with no leading zero. The slash and the prefix are
 * required. Host bits may be set ("132.178.227.11/25" is an interface
 * address), so use cidr_is_network() when they must not be.
 *
 * @param text The string to parse.
 * @param block Where the block is stored on success; untouched on failure.
 * @return true if text is valid CIDR notation, false otherwise (including NULL).
 */
bool cidr_parse(const char *text, cidr_t *block);

/**
 * @brief Format a block as "a.b.c.d/p", using the address exactly as stored.
 *
 * A prefix over 32 is written as 32, the way cidr_mask() treats it.
 *
 * @param block The block.
 * @param out A buffer of at least CIDR_STR_LEN bytes.
 */
void cidr_format(cidr_t block, char out[CIDR_STR_LEN]);

/* ------------------------------------------------------------------------
 * Task 2 - Subnet arithmetic
 * --------------------------------------------------------------------- */

/**
 * @brief The subnet mask for a prefix length: /24 is 255.255.255.0.
 *
 * Careful with /0. Shifting a 32 bit value by 32 is undefined behavior in C.
 *
 * @param prefix 0 to 32; anything larger is treated as 32.
 * @return The mask, host byte order.
 */
uint32_t cidr_mask(uint8_t prefix);

/** @brief The lowest address in the block: the address with the host bits cleared. */
uint32_t cidr_network(cidr_t block);

/** @brief The highest address in the block: the address with the host bits set. */
uint32_t cidr_broadcast(cidr_t block);

/**
 * @brief The first address that can be given to a host.
 *
 * For /0 to /30 that is the network address plus one. A /31 is a point to
 * point link (RFC 3021) where both addresses are usable, and a /32 is a single
 * host, so for those two the first host is the network address.
 */
uint32_t cidr_first_host(cidr_t block);

/**
 * @brief The last address that can be given to a host.
 *
 * The broadcast address minus one for /0 to /30, and the broadcast address
 * itself for /31 and /32 (see cidr_first_host()).
 */
uint32_t cidr_last_host(cidr_t block);

/**
 * @brief How many addresses in the block can be given to hosts.
 *
 * 2^(32 - prefix) - 2 for /0 to /30, 2 for /31, and 1 for /32. For a /0 the
 * 2^32 in that formula needs 33 bits, more than a uint32_t holds, which is why
 * this returns a uint64_t.
 */
uint64_t cidr_host_count(cidr_t block);

/** @brief true if addr falls inside the block. */
bool cidr_contains(cidr_t block, uint32_t addr);

/** @brief true if the block's host bits are all zero, so it names a network. */
bool cidr_is_network(cidr_t block);

/**
 * @brief The i-th of n equally sized subnets of a block.
 *
 * Splitting a /24 into 4 gives four /26 blocks, and subnet 0 is the lowest.
 * Equal sizes are only possible when n is a power of two, and the new prefix
 * can not pass 32.
 *
 * @param block The block to split; it must be a network (cidr_is_network()).
 * @param n How many subnets, a power of two from 1 up.
 * @param i Which subnet, 0 to n - 1.
 * @param out Where the subnet is stored on success; untouched on failure.
 * @return false if out is NULL, the block is not a network, n is not a power
 *         of two, the split would need a prefix longer than 32, or i is out of
 *         range.
 */
bool cidr_subnet(cidr_t block, uint32_t n, uint32_t i, cidr_t *out);

/* ------------------------------------------------------------------------
 * Task 3 - Longest prefix match
 * --------------------------------------------------------------------- */

/**
 * @brief Parse one line of a forwarding table file.
 *
 * A line is three fields separated by spaces or tabs:
 *
 *     <destination> <gateway> <device>
 *
 * The destination is a block in CIDR notation with no host bits set, or the
 * word "default" for 0.0.0.0/0. The gateway is a dotted quad, or the word
 * "direct" for a directly connected block (stored as 0). The device is 1 to
 * DEV_NAME_LEN - 1 characters. Leading and trailing whitespace, including a
 * trailing newline, is ignored. Comments are not handled here.
 *
 * @param line The line.
 * @param route Where the route is stored on success; untouched on failure.
 * @return true if the line is a valid route, false otherwise (including NULL).
 */
bool route_parse(const char *line, route_t *route);

/**
 * @brief Create an empty forwarding table.
 * @return The table, which the caller frees with table_destroy(), or NULL if
 *         memory could not be allocated.
 */
fwd_table_t *table_create(void);

/** @brief Free a table and everything in it. NULL is allowed and does nothing. */
void table_destroy(fwd_table_t *table);

/** @brief The number of routes in the table; 0 for NULL. */
size_t table_count(const fwd_table_t *table);

/**
 * @brief Add a copy of a route to the table, growing it as needed.
 * @return false if either argument is NULL, the destination has host bits
 *         set, the table already holds a route to the same block, or memory
 *         could not be allocated. The table is unchanged on failure.
 */
bool table_add(fwd_table_t *table, const route_t *route);

/**
 * @brief Read a forwarding table file and add every route in it.
 *
 * Everything from a '#' to the end of a line is a comment, and a line that is
 * blank once the comment is removed is skipped. Every other line must be a
 * valid route (route_parse()) that table_add() accepts.
 *
 * @param table The table to add to.
 * @param in The open file to read.
 * @param bad_line If not NULL, set to the 1-based number of the first line
 *        that failed, or 0 if the arguments were NULL or the file could not
 *        be read.
 * @return true if every line loaded. On false, routes from the lines before
 *         the bad one stay in the table.
 */
bool table_load(fwd_table_t *table, FILE *in, size_t *bad_line);

/**
 * @brief Longest prefix match: the route whose block contains dest and has the
 *        longest prefix.
 *
 * A default route (0.0.0.0/0) matches everything, so it is chosen only when
 * nothing more specific does. The order the routes were added in does not
 * matter.
 *
 * @return The matching route, which stays owned by the table, or NULL if no
 *         route matches (or table is NULL).
 */
const route_t *table_lookup(const fwd_table_t *table, uint32_t dest);

#endif // LAB_H

```

### main.c

```c

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

```

## Tests Files
### lab-test.c

```c

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

```

## Scripts Files
Report generated on 10/22/2026 at 17:53:07


---

## End of Report

SHA-256 Hash of the report: 0dda604e25e6a23039fe2091a6c39748ff055154bbe6a085556be6a67a909145

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: mevcaus/cs425-p3
- The repository visibility is public.
- The workflow was triggered by mevcaus
