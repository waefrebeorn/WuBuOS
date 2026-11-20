# Signed cross-device revocation-feed sync

Revocation state lives in the pkgmgr SQLite DB and is reloaded on every
`wubu_pkgmgr_init()`.  Sync is the bounded, fail-closed way to bring that state
in from another device without weakening the single-admin (or M-of-N quorum)
authentication policy.

## Trust model (config key)

The config key is `trusted_sync_url`.  Put one per line in the per-DB config
file `<dbdir>/sync.conf`:

```
trusted_sync_url=https://keys.example.org/revoke/feed
```

The full URL text must match exactly; there is no prefix, wildcard, or
redirect-following.  Three additional sources feed the same bounded set:
the `WUBU_PKGMGR_SYNC_URLS` environment variable (comma-separated), the CLI
`--trusted` flag, and the GUI settings store key `trusted_sync_url`.
The settings key is persisted by `wubu_settings_save()` and read through
`wubu_settings_trusted_sync_url()` inside `wubu_pkg_sync_config_load()`, so a
URL configured in GUI settings works on a fresh process with no config file,
no environment variable and no `--trusted` flag.  An URL outside the set is
refused before any network or database activity.

## Wire format

* `<url>` — the exact revocation-feed bytes documented in
  `src/gui/wubu_pkgmgr_revoke_feed.c`.  A signed feed carries an OPTIONAL
  first line `{"seq":N}` (positive decimal, `N <= INT64_MAX`); because the
  signature covers the exact bytes, the generation cannot be edited.
* `<url>.sig` — detached signature: 128 lowercase hex chars, an Ed25519
  signature over the domain-separated `WUBU-PKG-REVOCATION-v1/feed` hash of the
  exact feed bytes (including the `seq` line when present).

The importer enforces the generation atomically: after `BEGIN IMMEDIATE` it
reads `pkg_feed_gen.generation`, refuses (`ROLLBACK`, zero DB writes) when
`N <= generation`, and otherwise writes the entries **and** advances the
generation to `N` in the same commit.  Replaying an older or already-applied
signed feed is therefore refused with no database change; a higher signed
generation applies normally.  Seq-less legacy feeds (generation 0) keep the
old idempotent behavior and cannot be forged by stripping `seq`, since that
changes the signed bytes.

`wubu_pkgmgr_key_feed_sync()` fetches both with the existing OCI HTTP client
(so `https://` requires TLS chain + hostname verification), then hands the
exact bytes and signature to the existing importer.  That importer verifies the
signature **before** beginning its single all-or-nothing transaction, so a
refusal never touches the DB.

## CLI

```
wubu_pkg_key_ctl --db DIR sync <url> [--trusted url[,url...]]
```

prints `OK sync <url> applied N` or `REFUSED sync <url>`.

## Fail-closed

Refused with no DB change: untrusted URL, non-http(s) scheme, userinfo/query/
fragment/dot-dot in the path, TLS verify failure (untrusted CA, wrong host),
HTTP redirect or error, feed or sidecar over the byte cap, missing/malformed
signature, unreachable endpoint, bad signature, tampered feed, and a stale or
replayed signed generation.  HTTPS succeeds only when the server certificate
chains to the CA in `WUBU_CA_BUNDLE` and its DNS name matches the URL host.

## Control Panel widget and scheduled loop

The **Revocation Sync** applet in the Control Panel is the GUI face of this
leaf (`src/gui/wubu_pkgmgr_sync_panel.c`, registered through
`controlpanel.c`). It shows the committed `trusted_sync_url`, lets the user
type a new URL and press Enter (or click **Save**), persists it through
`wubu_settings_mut()` + `wubu_settings_save()`, and runs one bounded signed
sync on **Sync Now**. Every action goes through the importer above; the widget
contains no crypto or DB code of its own.

The same applet carries a **bounded scheduled loop**: **Auto** off by default
(no daemon), an interval in seconds configured with the `+`/`-` buttons, and a
tick from `desktop_tick()` that performs at most one fetch per interval. Both
flags persist in the settings store as `sync_auto_enabled` and
`sync_interval_secs`. The loop re-reads trust from settings on every attempt,
so a rotated feed URL is picked up on the next tick; refusals leave the DB
untouched (fail-closed). The clock is injectable for deterministic gates.

### Bounded retry backoff

Consecutive scheduled refusals switch the loop from its normal interval to an
**exponential backoff with a cap**, so a failing feed cannot be hammered:

```
delay(n) = min(sync_backoff_initial_secs * 2^(n-1), sync_backoff_max_secs)
```

Both values are settings keys (persisted through the widget's Save action):
`sync_backoff_initial_secs` (default 2) and `sync_backoff_max_secs` (default
300, bounded to 86400). The first refusal waits `initial`; each further
refusal doubles it until the cap. A successful sync resets `fail_count` and
the delay back to `initial`. Only scheduled attempts move this state; a manual
**Sync Now** click does not perturb it. Reloading settings also clears it.
`cp_pkg_sync_backoff_delay()` in `src/gui/wubu_pkgmgr_sync_panel.c` is the
single implementation used by both the tick and the gate.

## HTTPS in the hosted build: verified negative + config gate

The **default** hosted build deliberately does **not** link mbedTLS:
`src/runtime/oci/oci_http_client.c` returns -1 for `use_tls` without
`-DWUBU_HAVE_MBEDTLS`. Exact build evidence is a recipe with no
`-DWUBU_HAVE_MBEDTLS` and no `-lmbedtls` on the link line (captured by
`tools/hosted_tls_gate.sh`). Making a network-fetched dev root a hard dependency
of every hosted build would break bare runners, so instead TLS is a
**configuration gate**:

```bash
make hosted HOSTED_TLS=1 WUBU_MBEDTLS_ROOT=<extracted mbedTLS dev root>
```

That compiles the mbedTLS-backed HTTPS path into the hosted product and links
`-lmbedtls -lmbedx509 -lmbedcrypto`. The widget additionally refuses an
`https://` URL up front when built without TLS (no socket is opened) and
reports the `HOSTED_TLS=1` requirement. `make test_hosted_tls` proves both
states with build evidence plus a loopback TLS fixture (untrusted CA refused,
trusted CA accepted) and is part of `test_optin_crypto`.

## Gate

`make test_pkg_revoke_sync` runs `tools/pkg_revoke_sync_gate.sh`: ephemeral
keys and an ephemeral local CA, a loopback HTTP + HTTPS fixture, a valid signed
generation that persists across fresh processes, replay of N-1/N refused,
N+1 applied, HTTPS trusted-CA success with untrusted-CA and wrong-host
refusals, GUI settings save/load read-path survival, tampered/wrong-key/
unsigned/oversize/redirect/untrusted/TLS/unreachable refusals, and a
pre-change negative control.  It is also part of `make test_optin_crypto`.

`make test_sync_panel` runs `tools/sync_panel_gate.sh`: a hosted GUI test
(`src/gui/test_sync_panel.c`) drives the real applet key/mouse callbacks to
save a loopback trusted URL, trigger Sync Now (keys revoked and persisted
across a fresh DB load), refuse a bad-scheme URL and a bad-signature feed with
no DB change, then enable the scheduled loop and apply a rotated signed feed
after the configured interval. It then drives repeated refusals against the
loopback bad-signature feed to assert the exact exponential-with-cap schedule
and a reset on the next successful recovery feed (settings keys persisted).
It is also part of `make test_optin_crypto`.

`make test_hosted_tls` runs `tools/hosted_tls_gate.sh`: exact build evidence
for the no-TLS default hosted recipe and the `HOSTED_TLS=1` recipe, plus a
loopback TLS fixture proving the mbedTLS-backed HTTPS path authenticates the
server. It is also part of `make test_optin_crypto`.
