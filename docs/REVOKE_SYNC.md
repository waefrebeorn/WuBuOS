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
redirect-following.  Two additional sources feed the same bounded set:
the `WUBU_PKGMGR_SYNC_URLS` environment variable (comma-separated) and the CLI
`--trusted` flag.  An URL that is not in the set is refused before any network
or database activity.

## Wire format

* `<url>` — the exact NDJSON feed bytes documented in
  `src/gui/wubu_pkgmgr_revoke_feed.c`.
* `<url>.sig` — detached signature: 128 lowercase hex chars, an Ed25519
  signature over the domain-separated `WUBU-PKG-REVOCATION-v1/feed` hash of the
  exact feed bytes.

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
fragment/dot-dot in the path, TLS verify failure, HTTP redirect or error,
feed or sidecar over the byte cap, missing/malformed signature, unreachable
endpoint, bad signature, tampered feed.

## Gate

`make test_pkg_revoke_sync` runs `tools/pkg_revoke_sync_gate.sh`: ephemeral
keys, a loopback HTTP fixture, a valid sync that persists across fresh
processes, tampered/wrong-key/unsigned/oversize/redirect/untrusted/TLS/unreachable
refusals, and a pre-change negative control.  It is also part of
`make test_optin_crypto`.
