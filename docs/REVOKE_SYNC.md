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

## Gate

`make test_pkg_revoke_sync` runs `tools/pkg_revoke_sync_gate.sh`: ephemeral
keys and an ephemeral local CA, a loopback HTTP + HTTPS fixture, a valid signed
generation that persists across fresh processes, replay of N-1/N refused,
N+1 applied, HTTPS trusted-CA success with untrusted-CA and wrong-host
refusals, GUI settings save/load read-path survival, tampered/wrong-key/
unsigned/oversize/redirect/untrusted/TLS/unreachable refusals, and a
pre-change negative control.  It is also part of `make test_optin_crypto`.
