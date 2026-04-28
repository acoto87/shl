# memzone_audit — per-zone audit logging for memzone

`memzone_audit.h` is a companion single-header to `memzone.h` that records
every allocator mutation to a structured text log file.  It is entirely
optional: `memzone.h` has zero knowledge of it, and including it does not
affect the size or layout of `memzone_t`.

## Motivation

When tracking down allocator misuse — double-frees, use-after-free, leaks at
shutdown, or unexpected realloc strategies — a chronological log of every
event with its call site is far more useful than a post-mortem heap dump.
`memzone_audit.h` provides exactly that with no changes to existing call sites.

## Usage

In **exactly one** translation unit define both implementation guards before
including the header:

```c
#define SHL_MZ_IMPLEMENTATION
#define SHL_MZ_AUDIT_IMPLEMENTATION
#include "memzone_audit.h"
```

In all other translation units include without the guards, or include
`memzone.h` directly if audit logging is not needed there:

```c
#include "memzone_audit.h"   /* gets declarations + macro redirections */
```

The header includes `memzone.h` internally, so a separate `#include
"memzone.h"` is not required.

`SHL_MZ_AUDIT_IMPLEMENTATION` must appear in the same translation unit as
`SHL_MZ_IMPLEMENTATION` because the realloc wrapper inspects private
allocator internals (`memblock_t`, `mz__findBlock`, etc.) that are only
visible there.

## Public API

Once the header is included, the following symbols become available in
addition to the full `memzone.h` API:

| Symbol | Description |
| --- | --- |
| `mz_audit_format_t` | Enum: `SHL_MZ_AUDIT_FORMAT_VERBOSE` or `SHL_MZ_AUDIT_FORMAT_COMPACT` |
| `mz_initAudit(size, fmt, path)` | Create a zone and begin logging immediately (INIT event recorded) |
| `mz_auditConfigure(zone, fmt, path)` | Attach or reconfigure audit on an existing zone; no INIT event logged |
| `mz_auditFlush(zone)` | Flush the log without closing it |
| `mz_auditClose(zone)` | Flush and close the log; safe on zones with no active log |

The standard mutating API — `mz_init`, `mz_destroy`, `mz_alloc`, `mz_free`,
`mz_realloc`, `mz_allocAligned`, `mz_reset` — is transparently redirected
through the audit layer via macros.  Every call site's `__FILE__` and
`__LINE__` are captured automatically without any source changes.

`mz_destroy` closes the log and writes a DESTROY event (including a live
allocation count) before freeing the zone memory.

## Compile-time knobs

| Define | Effect |
| --- | --- |
| `SHL_MZ_AUDIT_VERBOSE` | Emit a full block-list dump after each logged event |
| `SHL_MZ_AUDIT_APPEND` | Open log files in append mode (default: truncate on each run) |
| `SHL_MZ_AUDIT_FILE` | Default log path when none is provided (default: `"memzone_audit.log"`) |
| `SHL_MZ_AUDIT_MAX_ZONES` | Maximum number of simultaneously audited zones (default: `64`) |

## Log formats

### Verbose (default)

Each event is a bordered block with per-field labels:

```
================================================================================
EVENT #0001  INIT             2025-04-28 12:00:00.123  myfile.c:42
--------------------------------------------------------------------------------
  Zone       : 0x55f3c8d02a40
  Max size   : 4096 bytes
  Result     : [OK]
  Zone after : used=72, max=4096 (1.76% full), blocks=1, frag=0.00%, free=4024 bytes
================================================================================

================================================================================
EVENT #0002  ALLOC            2025-04-28 12:00:00.124  myfile.c:45
--------------------------------------------------------------------------------
  Zone       : 0x55f3c8d02a40
  Request    : size=64 bytes, alignment=8 bytes
  Result     : ptr=0x55f3c8d02a88 [OK]
  Zone after : used=160, max=4096 (3.91% full), blocks=2, frag=0.00%, free=3936 bytes
================================================================================
```

When `SHL_MZ_AUDIT_VERBOSE` is defined each event footer also includes a
full dump of the block list:

```
  Block list :
    [000] block=0x55f3c8d02a48  size=88       user=0x55f3c8d02a88  next=...  prev=...
    [001] block=0x55f3c8d02aa0  size=3984      FREE  next=...  prev=...
```

### Compact

Each event is a single line prefixed with `#NNNN`, suitable for `grep` and
machine processing:

```
# memzone audit log
# started  : 2025-04-28 12:00:00.123
# format   : compact
# mode     : truncate

#0001 INIT     ts=2025-04-28 12:00:00.123 site=myfile.c:42 zone=0x... maxSize=4096 result=OK used=72, ...
#0002 ALLOC    ts=2025-04-28 12:00:00.124 site=myfile.c:45 zone=0x... size=64 align=8 ptr=0x... result=OK ...
#0003 FREE     ts=2025-04-28 12:00:00.125 site=myfile.c:46 zone=0x... ptr=0x... alloc_size=88 result=OK ...
#0004 DESTROY  ts=2025-04-28 12:00:00.126 site=myfile.c:47 zone=0x... ... live_allocs=0
```

## Realloc strategy identification

The REALLOC event logs a human-readable `strategy` field that identifies
which internal path `mz_realloc` took:

| Strategy string | Meaning |
| --- | --- |
| `no-op (block already has sufficient capacity)` | Request fits in the existing block; pointer unchanged |
| `in-place expand (absorbed adjacent free block)` | Grew into the immediately following free block |
| `alloc + copy + free (no usable adjacent space)` | A new block was allocated, data was copied, old block freed |
| `equivalent to alloc (old ptr is NULL)` | `p == NULL`, behaves like `mz_alloc` |
| `equivalent to free (new size is 0)` | `size == 0`, behaves like `mz_free` |
| `invalid pointer` | `p` was not found in the zone |

## Design

Audit state is stored in a fixed-size external table (`mz__audit_table`)
rather than inside `memzone_t`, so:

- `memzone.h` is completely unmodified and audit-free.
- Any `memzone_t` can be retrospectively enrolled via `mz_auditConfigure`
  without being re-created.
- The table holds up to `SHL_MZ_AUDIT_MAX_ZONES` (default 64) entries;
  each slot records the zone pointer, open `FILE*`, sequence counter, format,
  and log path.  Lookup is O(n) — acceptable for a diagnostic facility.

## Multiple zones

Each zone has its own independent slot in the table and writes to its own
log file.  There is no cross-contamination between zones.

```c
memzone_t* za = mz_initAudit(4096, SHL_MZ_AUDIT_FORMAT_COMPACT, "zone_a.log");
memzone_t* zb = mz_initAudit(8192, SHL_MZ_AUDIT_FORMAT_VERBOSE, "zone_b.log");

mz_alloc(za, 32);  /* logged only in zone_a.log */
mz_alloc(zb, 64);  /* logged only in zone_b.log */

mz_destroy(za);
mz_destroy(zb);
```

## Example — basic usage

```c
#define SHL_MZ_IMPLEMENTATION
#define SHL_MZ_AUDIT_IMPLEMENTATION
#include "memzone_audit.h"

int main(void)
{
    /* Create a 1 MB zone and start logging in verbose format. */
    memzone_t* zone = mz_initAudit(1024 * 1024,
                                    SHL_MZ_AUDIT_FORMAT_VERBOSE,
                                    "myapp_audit.log");

    void* a = mz_alloc(zone, 256);
    void* b = mz_alloc(zone, 512);
    mz_free(zone, a);
    b = mz_realloc(zone, b, 1024);
    mz_reset(zone);

    /* mz_destroy writes the DESTROY event and closes the log. */
    mz_destroy(zone);
    return 0;
}
```

## Example — attach audit after creation

```c
memzone_t* zone = mz_init(65536);

/* Audit not active yet; switch it on before the interesting code path. */
mz_auditConfigure(zone, SHL_MZ_AUDIT_FORMAT_COMPACT, "late_audit.log");

/* All subsequent calls are now logged. */
void* p = mz_alloc(zone, 128);
mz_free(zone, p);

mz_destroy(zone);
```
