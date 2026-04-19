# TLS / Cert Setup Issue

## Symptom

`uv sync` (and any other tool that uses `rustls`) fails against pypi.org with:

```
error: Failed to fetch: `https://pypi.org/simple/<pkg>/`
  Caused by: client error (Connect)
  Caused by: invalid peer certificate: UnknownIssuer
```

## Root cause

The shell environment exports

```sh
export SSL_CERT_FILE=$HOME/.certs/llnl/llnl-ca-bundle-full.pem
export REQUESTS_CA_BUNDLE=$HOME/.certs/llnl/llnl-ca-bundle-full.pem
export CURL_CA_BUNDLE=$HOME/.certs/llnl/llnl-ca-bundle-full.pem
```

so that `npm` / `git` / `requests` trust the LLNL internal CAs.

When `SSL_CERT_FILE` is set, `rustls`-based tools (uv, cargo, rustup, etc.)
use **only** that file as their trust store — it is not merged with the
system roots. The LLNL bundle does not include the public CAs that
pypi.org's certificate chains to (currently `GlobalSign Atlas R3 DV TLS CA
2025 Q4`), so verification fails.

`--native-tls` alone does not fix it: uv still consults `SSL_CERT_FILE`
when it is set in the environment, even with `tool.uv.native-tls = true`
in `pyproject.toml`.

Confirmed with:

```sh
echo | openssl s_client -showcerts -servername pypi.org -connect pypi.org:443
# subject = CN = pypi.org
# issuer  = C = BE, O = GlobalSign nv-sa, CN = GlobalSign Atlas R3 DV TLS CA 2025 Q4
# Verification error: unable to get local issuer certificate
```

while `curl https://pypi.org/simple/` works once `SSL_CERT_FILE` is unset
(curl falls back to `/etc/pki/tls/cert.pem`, which trusts GlobalSign).

## Workarounds (current)

Pick one when running `uv` from this repo:

1. **Per-invocation unset** (what the smoke test used):

   ```sh
   unset SSL_CERT_FILE && uv sync --extra test
   ```

2. **Redirect to the system trust store** (also unsets the LLNL roots, so
   internal services may break for tools that read `SSL_CERT_FILE`):

   ```sh
   SSL_CERT_FILE=/etc/pki/tls/cert.pem uv sync --extra test
   ```

Neither is acceptable as a long-term default — option 1 breaks LLNL
internal access; option 2 affects every other tool in the shell.

## Proper fixes (to evaluate)

In rough order of preference:

1. **Extend the LLNL bundle** so `llnl-ca-bundle-full.pem` concatenates the
   LLNL intermediates **plus** the public root store
   (`/etc/pki/tls/cert.pem`). One bundle that satisfies both sets of TLS
   peers; nothing in the user's bashrc has to change.

   ```sh
   cat /etc/pki/tls/cert.pem $HOME/.certs/llnl/llnl-only.pem \
       > $HOME/.certs/llnl/llnl-ca-bundle-full.pem
   ```

2. **Split the env vars** so `SSL_CERT_FILE` keeps the system roots while
   only the LLNL-specific tools read the LLNL bundle. For example, leave
   `SSL_CERT_FILE` unset (rustls/openssl use system defaults) and rely on
   `NODE_EXTRA_CA_CERTS`, `GIT_SSL_CAINFO`, and `REQUESTS_CA_BUNDLE` to
   inject LLNL roots only where each tool actually needs them.

3. **Per-project unset** via a `direnv` / `.envrc` hook that drops
   `SSL_CERT_FILE` whenever the shell enters the griz repo. Cheapest to
   set up but solves the problem only for this one tree.

## What's already in place

- `pygriz/pyproject.toml` sets `tool.uv.native-tls = true` so that, once
  the trust-store conflict is resolved, uv will keep using the system
  store without needing `--native-tls` on every invocation.
