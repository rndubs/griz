# 07 — Launch (SSH + SLURM)

## Scope

How a user goes from opening `griz-client` to a running `griz-server` on the HPC, over SSH, possibly inside a SLURM allocation. Covers host profiles, SSH integration, launch methods, rendezvous, tunneling, session reporting, reconnect, and credential handling. Modeled on VisIt's launch flow.

Out of scope: what happens once connected (see [01-architecture](01-architecture.md)), protocol details (see [02-protocol](02-protocol.md)), feature-level UI (see [04-client](04-client.md) §8 for host-profile storage).

## Related

- `../UI.md` §6 (Launch model)
- [01-architecture](01-architecture.md) §3 (names, rendezvous format), §5 (deployment variants), §7 (failure recovery)
- [02-protocol](02-protocol.md) §2.3 (token auth), §2.4 (keepalive)
- [03-server](03-server.md) §7.3 (shutdown)
- [`../shared/server-binary.md`](../shared/server-binary.md) (RPC-specific CLI flags)

## 1. Current state (2026-04)

No launch code exists in `client/` (the client doesn't exist yet).

What **does** exist and is a useful precedent for the subprocess-lifecycle parts of this plan:

- **Local subprocess spawn** for the MCP stack: `pygriz/src/griz/worker.py:95–138` spawns `griz-server --transport=stdio`, waits for `ready`, performs handshake, runs the command loop. The same state machine — spawn, wait-for-ready, handshake, background reader, graceful drain — applies to the **local** variant (variant A in [01-architecture](01-architecture.md) §5) of the UI client.
- **Auto-discovery of the server binary** so the Python layer works in-tree: `pygriz/src/griz/worker.py:_find_griz_server()`. The Qt client's host-profile "server path" default should follow the same convention for in-tree development.

No SSH, no SLURM, no rendezvous on disk yet. All new in the UI effort.

## 2. Host profiles

### 2.1 Storage

Location: `$XDG_CONFIG_HOME/griz/hosts.toml` (fallback `~/.config/griz/hosts.toml`). TOML over JSON/YAML: human-editable, supports comments, `ini`-like rigor. File owned 0600.

### 2.2 Schema

```toml
# ~/.config/griz/hosts.toml

# Common fields per host profile.
[[host]]
nickname          = "dane"                     # UI label, unique
hostname          = "dane.llnl.gov"            # what ssh sees
username          = "whitmore"                 # optional; falls back to ssh_config
auth_hint         = "agent"                    # agent | key | prompt
ssh_identity_file = "~/.ssh/id_dane"           # optional; key path for auth_hint=key
ssh_extra_args    = ["-o", "ServerAliveInterval=30"]  # passed through to ssh
jump_hosts        = ["bastion.llnl.gov"]       # ProxyJump chain; optional

# Remote environment
griz_server_path  = "/usr/apps/griz/bin/griz-server"
env_setup         = """
module load intel-classic/2021.6.0-magic
module load mdg
"""

# Launch strategy
launch_method     = "slurm"                    # direct | slurm | preallocated
forbid_login_node_compute = true               # reject method=direct on login

# Default SLURM parameters (used when method=slurm)
[host.slurm]
account    = "engphys"
partition  = "pbatch"
nodes      = 1
ntasks     = 1
walltime   = "02:00:00"
memory     = "32G"
extra_args = ["--gres=none"]

# Rendezvous location
[host.rendezvous]
dir_template = "$HOME/.griz/rendezvous"        # must be on shared FS for slurm/direct
mode         = 0o600
```

### 2.3 Import / export

- **Starter profiles.** Site admins can ship `/etc/griz/hosts.d/*.toml`; the client merges these on startup under their own section. Precedence: user file overrides site file by nickname.
- **Export button** in the UI dumps a single `[[host]]` to the clipboard or a file for sharing.

### 2.4 UI

Per [04-client](04-client.md) §3: a **Host** menu with a host picker, **Connect…** dialog, and a **Host → Manage hosts…** dialog for edit/import/export. Dialog field-mirrored from the TOML schema above.

## 3. Launch methods

Three supported flavors, selected per profile:

### 3.1 `direct` — run on the login node

```
ssh <host> sh -c '<env_setup>; exec griz-server --transport=rpc \
                   --bind=127.0.0.1:0 --rendezvous=<path> -i <db>'
```

- No SLURM. The server runs on the login node.
- Intended for small DBs and quick inspection.
- Site policy guard: `forbid_login_node_compute = true` rejects this method and shows a modal.

### 3.2 `slurm` — fresh allocation (primary)

```
ssh <host> sh -c '<env_setup>; sbatch --parsable --wrap "srun \
                   griz-server --transport=rpc --bind=127.0.0.1:0 \
                   --rendezvous=<path> -i <db>" \
                 -A <acct> -p <part> -N <nodes> -t <wall> \
                 --mem=<mem> <extra_args>'
```

- `sbatch --parsable` returns just the job id on stdout.
- `srun` inside the sbatch script puts the server on a compute node.
- Client stores the job id for the SLURM UI (§6).

### 3.3 `preallocated` — attach to an existing allocation

User has an existing `salloc` and supplies a job id in the connect dialog:

```
ssh <host> sh -c '<env_setup>; srun --jobid=<job> \
                   griz-server --transport=rpc --bind=127.0.0.1:0 \
                   --rendezvous=<path> -i <db>'
```

Same rendezvous and tunnel flow as `slurm` afterward. Useful for debugging or long-lived sessions with pre-negotiated walltime.

## 4. Rendezvous

[01-architecture](01-architecture.md) §3 fixes the rendezvous layout. Summary: the server writes `$HOME/.griz/rendezvous/{session-id}.json` on the HPC shared filesystem immediately after `bind()`, containing `{version, session_id, host, port, token, server_pid, started_at}`. Client reads this over SSH.

### 4.1 Client read flow

After `sbatch`/`srun` returns:

1. Wait for the sbatch job to reach `RUNNING` (poll `sacct`/`squeue` over SSH) — for `slurm` method only.
2. Poll `ssh <host> cat <rendezvous_path>` on a 500 ms cadence until it returns a well-formed JSON.
3. Timeout: 60 s. If timed out, try `ssh <host> sacct -j <job> --format State`. If job failed, surface job log via `ssh cat <slurm_out>`; if still pending, keep waiting up to the configured limit.

### 4.2 Token

32-byte urandom generated server-side, base64-encoded in the rendezvous. Client presents it on the first frame (see [02-protocol](02-protocol.md) §2.3). Server validates in constant time. On mismatch: server emits `error.code = "protocol_mismatch"` and closes.

### 4.3 Alternative: stdout handoff

For `direct` on a login node, the server can print the rendezvous JSON to stdout and let the launching SSH channel relay it. Cleaner than filesystem coordination. Recommend: **still use the file**, but also print a one-line summary for debugability. Keeps a single path.

### 4.4 Sweep

Server deletes its own rendezvous on clean shutdown ([03-server](03-server.md) §7.3). Client also sweeps stale rendezvous on successful connect (best-effort cleanup of any siblings of the one it used). Safety-net for crashed servers.

## 5. SSH driver

Two paths to consider; **recommend: system SSH**.

### 5.1 System SSH (default, recommend)

- Spawn `/usr/bin/ssh` (or `%PATH% ssh`) as a `QProcess`.
- Honors `~/.ssh/config`, key agents (gnome-keyring, macOS Keychain), known_hosts, MFA prompts, certificate auth.
- The client sets `SSH_ASKPASS` to its own helper binary to pipe prompts into an in-app modal — optional; users without it fall back to terminal prompting if one is attached.
- Tunnel via `ssh -N -L <local_port>:127.0.0.1:<remote_port> <host>`.

Pros: no embedded SSH maintenance burden; full auth-feature fidelity; matches how users already connect.

Cons: Windows doesn't always ship a usable SSH. Mitigation: recent Windows ships OpenSSH; document it as the path for v1, and include `libssh2` as a fallback in phase 3.

### 5.2 `libssh2` embedded (phase 3 fallback)

For Windows-first installs without system SSH. Links into the Qt client; exposes same driver interface as the system-SSH path. Implementation burden: key + agent handling, MFA prompt plumbing. Not worth it for v1.

### 5.3 Jump hosts / bastions

Supported via `ssh_extra_args = ["-J", "bastion.llnl.gov,user@secondhop.llnl.gov"]` or `jump_hosts = [...]` in the profile (the client expands to `-J`). Nothing Griz-specific; system SSH handles it.

## 6. SLURM UI

A dialog that surfaces the SLURM job state live, at the user's attention level:

- **Queue phase** (`PENDING`): "Queued: 5 min remaining (est.)" — derive from `squeue --start`. Cancel button.
- **Startup phase** (`RUNNING`, rendezvous not yet readable): "Allocating node, starting Griz…" — 1 s poll on rendezvous.
- **Ready phase**: status bar shows remaining walltime, continuously updated via `squeue` once per 30 s. If `walltime_remaining < 5 min`, promote to a modal warning.
- **Log tail** (optional): a collapsible "diagnostics" panel showing the tail of `$HOME/.griz/logs/{session-id}.log` via `ssh tail -f`. Off by default (bandwidth), toggle-able.

End-of-walltime: server emits `session_ending(reason="slurm_walltime", seconds_remaining=N)` (see [03-server](03-server.md) §7.3), client shows the modal.

## 7. Tunneling

- **Tunnel start.** `ssh -N -L <local_port>:127.0.0.1:<server_port> <host>` after rendezvous read. Local port: kernel-ephemeral from `bind(... , 0)`.
- **Tunnel lifecycle.** Owned by the network thread. Tied 1:1 to the session; killed on disconnect.
- **Tunnel death** → UI shows a reconnect banner (see [01-architecture](01-architecture.md) §7 SSH drop). Default retry policy: 3 attempts over 30 s. If reconnect succeeds and the server is still alive, resume against the saved session.

## 8. Session reporting

The UI should make "what's going on" observable without the user opening a terminal:

- **Status bar.** Session state (Connecting / Queued / Starting / Running / Disconnected), host nickname, remaining walltime, current FPS (from frame seq deltas), stream bitrate.
- **Session menu.** "Show server log" opens a window that `ssh tail`s the remote log file. "Cancel job" triggers `scancel` over SSH.
- **Notifications.** Non-blocking toast for "Job started", "5 min walltime remaining", "Tunnel dropped — reconnecting".

## 9. Reconnect and extend

- **Reconnect.** If the SSH tunnel dropped but the server is still running (same session id, rendezvous file still present): client retries SSH; on success, re-opens the tunnel, re-auths with the stored token, sends `q_state` to resync, resumes. No new sbatch.
- **Extend walltime.** `scontrol update job=<id> TimeLimit=+01:00:00` is the mechanism; many sites restrict this. Expose as a menu item; fail gracefully with the site error message. Not a v1 focus.
- **Detach / reattach across client restarts.** Out of scope for v1 — closing the client tears down the server. If reattach becomes a requirement, `sessions.toml` already holds the reconnect hints (host, job id, rendezvous path); the architecture supports it.

## 10. Credentials

- **No credential caching.** The client does not store passwords, keys, or tokens outside of `ssh_identity_file` paths (which are just pointers). Everything flows through the user's SSH agent or an `ssh-askpass` prompt.
- **Rendezvous token.** Ephemeral, 32 bytes, per-session, never persisted on the client side except in memory for the lifetime of the connection.
- **known_hosts.** Use the user's `~/.ssh/known_hosts` via system SSH; no client-side duplicate. If the user has never ssh'd to the host before, the first connection does the key prompt as usual.

## 11. Schedulers other than SLURM

LSF, PBS, and Flux exist. The client's scheduler integration is narrow enough (one-shot submit, get job id, poll job state, cancel) to abstract. For v1: **SLURM only**. Scheduler abstraction deferred until a second site needs it.

## 12. Failure modes (reference)

Mirrors [01-architecture](01-architecture.md) §7 briefly:

| Event | User-visible | Action |
|-------|--------------|--------|
| SSH auth failure | Modal with stderr from ssh | "Retry" / "Edit profile" |
| Rendezvous timeout | Modal | "Cancel job" / "Keep waiting" |
| Token mismatch | Modal | "Disconnect" |
| `session_ending(slurm_walltime)` | Modal countdown | "Save & quit" / "Extend walltime" / "Relaunch" |
| Server crash (EOF w/o session_ending) | Modal + server-log tail | "Relaunch" |
| SSH drop | Banner + auto-reconnect | — |

## Open questions

- **Bastion + MFA prompts in-app.** System SSH's `SSH_ASKPASS` works on Linux; less clean on macOS. If a large fraction of users are on macOS with MFA, we may need an in-app prompt path beyond ASKPASS. Defer until we see the distribution.
- **Shared filesystem assumption.** A minority of clusters do not expose `$HOME` on compute nodes. Fallback: server writes the rendezvous to a site-agreed shared path via `scp` over the login node. Adds complexity; defer until a site demands it (same open question in [01-architecture](01-architecture.md)).
- **Rendezvous sweep scope.** On successful connect, do we sweep only our own stale rendezvous, or *all* rendezvous older than 24h? First is safer; latter is cleaner. Lean first.
- **`module load` portability.** The env-setup heredoc assumes `/bin/sh` environment modules. If a site uses a different mechanism (e.g. spack env, conda activate), the profile should support it via free-form shell snippet; already does.
