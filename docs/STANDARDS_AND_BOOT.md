# Boot tracking, peer reporting, and the standards this stack actually meets

Two questions answered in one place: what Bootvine's boot-console model means for
WuBuOS's EDR boot tracking, and which published specifications each leg of the
distributed-model stack conforms to.

## 1. Bootvine's model versus WuBuOS boot attestation

Bootvine replaces a scrolling boot log with a **live tree of systemd jobs**: the
units still pending stay on screen, and the room the tree leaves holds a timeline
of the slowest units that ran. It also follows shutdowns, answers passphrase
prompts in place, and shows why a unit failed from its last journal lines.

Its real insight is not the drawing. It is that a boot console should be a
**state machine view**, not a stream: *what is running, what is stuck, what took
long*. A wall of text answers none of those until you scroll back.

WuBuOS does not use systemd as PID 1, so there are no unit states to tree. What it
does have is a stage list, and each stage has a start time and a duration. That is
the same information:

| Bootvine | WuBuOS |
|---|---|
| units still pending | stages not yet reached |
| timeline of the slowest units | per-stage duration records |
| why a unit failed | the stage record plus its error code |
| follows shutdown | shutdown is a final record, not a blank screen |

The boot attestation spell (`src/spells/networking/aggregation/bootattest/`)
encodes exactly that: an 84-byte header (magic, version, flags, stage count, boot
counter, seal time, PCR digest, kernel digest), one 24-byte record per stage
(name, start, duration), and a 32-byte signature over the whole report.

### "Insecure by design, always reported to everyone peer wise"

That sentence is a requirement, not a caveat, and the implementation matches it:

- **Measurement-bound, not authentication.** The hash chain binds each stage to
  the previous one. The tree has no signing primitive -- firmware RSA is
  verify-only and the private key never leaves the TPM -- so the header says out
  loud that this is a measurement, not a signature. Claiming otherwise would be
  the only real security bug here.
- **Every report is offered to every route.** `wubu_boot_attest_relay` takes a
  sealed report and hands it to the SDR relay, which offers it to all peers. The
  sender cannot choose who sees it, because the report is built before any
  routing decision is made.
- **It rides the same ring as everything else.** `EDR_EV_BOOT_ATTEST = 27`
  appends after `AGENT_ACTION` (26) so no existing event value shifts. One
  disclosure surface, one viewer, one retention policy.
- **Exempt from the GDPR age gate deliberately.** Boot telemetry is OS/kernel
  measurement, not an agent action; gating it behind an age check would make the
  boot record depend on a person's birthday, which is absurd and dangerous.

What a peer can conclude from a report: *this machine booted these stages in this
order with these durations, and nothing between them was changed without changing
a hash*. What they cannot conclude: that the machine is trustworthy. The header
says so. Transparency about the limit is the point.

## 2. Standards conformance

Checked against the published specifications rather than memory. Two real bugs
came out of it, both fixed:

### yamux frame types were off by one

The libp2p spec numbers them `0x0 Data`, `0x1 Window Update`, `0x2 Ping`,
`0x3 Go Away`. The enum started at 1, so every DATA frame went out claiming to be
a WINDOW_UPDATE -- a peer would adjust its flow-control window by the payload size
and never deliver the bytes. `make test_yamux` now asserts the spec's own numbers
for all four types and all four flag bits.

### Multiformats varints allowed non-minimal encodings and ten bytes

The spec is explicit: implementations MUST restrict a varint to a maximum of
**nine bytes (63 bits)**, and leading zeros must be rejected when decoding
because one number must have exactly one spelling. `{0x81, 0x00}` decodes to 1
but wastes a byte; it was accepted. `UINT64_MAX` grew to ten bytes; it was
accepted.

protobuf differs in exactly one way that matters: a negative int32 is
sign-extended to a 64-bit value, so `-1` is **ten** bytes (`ff` x9, `01`). That
is why `wubu_pb_varint` exists separately from `wubu_varint_encode`; refusing it
would emit protobuf no peer can read.

### Leg-by-leg status

| Leg | Specification | State |
|---|---|---|
| magnet link | BEP 9 | **built** -- `wubu_magnet.c`, 32 checks |
| SHA-1 | FIPS 180-4 | **built** -- `wubu_sha1.c`, 12 checks |
| `.torrent` info span | BEP 3 (info dict) | **built** -- locates `info`, SHA-1s it |
| DHT KRPC | BEP 5 | **built** -- `wubu_dht.c`, 48 checks |
| DHT routing table | BEP 5 | **built** -- buckets, closest, reap, 41 checks |
| multiaddr | libp2p multiaddr | **built** -- 80 checks, IPv6 vectors verified against Python's `ipaddress` |
| CID | CIDv0/v1 + unsigned-varint | **built** -- 92 checks |
| bitswap messages | go-bitswap protobuf schema | **built** -- Wantlist / Block / BlockPresence, 45 checks |
| multistream-select | `/multistream/1.0.0` | **built** -- framing, proposals, `na`, 48 checks |
| yamux framing | libp2p yamux | **built** -- 53 checks |
| noise handshake | noise protocol framework | **not built** -- nothing is encrypted |
| yamux stream state | libp2p yamux | framing only; no window accounting or reassembly |
| OpenAI-compatible API | provider-specific | **not built** |
| DePIN compute (Akash, io.net, Gensyn, Bittensor) | provider-specific | **not built** |

### How WuBuOS uses this

The AGI does not need a new wire format per task. Each leg is fail-closed and
self-describing:

- **Address a peer** with a multiaddr (`/ip4/…/tcp/…`).
- **Agree on a protocol** with multistream-select (`/ipfs/bitswap/1.2.0`).
- **Name content** with a CID.
- **Ask for blocks** with a bitswap wantlist, carrying those CIDs.
- **Verify** each block against the CID it asked for, using SHA-1/SHA-256.
- **Find more peers** through the DHT.
- **Find models** by magnet, which resolves to the same info-span SHA-1.

Nothing here needs the OS to know what a "model" is. The AGI layer decides that;
these legs decide only how to move bytes it can already name and check.

## 3. GUI standards

Separate concern, same principle: one owner per number. See
`src/gui/wubu_tokens.h`, which now carries the spacing grid, radii, borders and
control metrics. Every surface reads it; nothing may hardcode geometry. The gap
audit (`docs/GAP_AUDIT_WIN11_MINT.md`) tracks *which* surfaces exist, not their
spacing.
