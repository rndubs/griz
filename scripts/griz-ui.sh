#!/bin/bash
# griz-ui.sh — idempotent Qt UI lifecycle wrapper
#
# Usage:
#   griz-ui.sh up [database]     bring a UI up (or report the existing one)
#   griz-ui.sh status            print {ui_pid, server_pid, rendezvous} or none
#   griz-ui.sh down              kill any live UIs + clean stale rendezvous files
#
# Output is line-oriented `key=value` for easy capture by an LLM or shell.
# Exit codes: 0 success, 1 nothing to do / no live UI, 2 preflight/launch error.
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RV_DIR="${HOME}/.griz/rendezvous"
DEFAULT_DB="Src/test/image/bar71/bar71.pltA"

# --- helpers ----------------------------------------------------------------

err() { echo "error: $*" >&2; }
note() { echo "$*" >&2; }

is_pid_alive() {
    local pid=$1
    [[ -n "$pid" && "$pid" -gt 0 ]] && kill -0 "$pid" 2>/dev/null
}

# Reads server_pid from a rendezvous JSON file (no jq dependency).
rv_field() {
    local file=$1 key=$2
    [[ -r "$file" ]] || return 1
    python3 -c "
import json, sys
try:
    with open(sys.argv[1]) as f:
        data = json.load(f)
    val = data.get(sys.argv[2])
    if val is None:
        sys.exit(1)
    print(val)
except Exception:
    sys.exit(1)
" "$file" "$key" 2>/dev/null
}

# Find UI client PIDs whose grandchild server matches a live rendezvous.
# More direct: walk rendezvous files, return entries whose server_pid is alive.
# Returns lines: "rv_path|server_pid".
live_rendezvous() {
    [[ -d "$RV_DIR" ]] || return 0
    shopt -s nullglob
    local f pid
    for f in "$RV_DIR"/ui-*.json; do
        pid=$(rv_field "$f" server_pid) || continue
        if is_pid_alive "$pid"; then
            echo "${f}|${pid}"
        fi
    done
    shopt -u nullglob
}

# Discover the parent (client) PID of a server PID. The client launches
# griz-server as a child and dies with it on Qt window close; ppid()
# walks the chain.
ppid_of() {
    local pid=$1
    ps -o ppid= -p "$pid" 2>/dev/null | tr -d ' '
}

find_client_bin() {
    local cand="$REPO_ROOT/client/build/linux-release/src/griz-client"
    [[ -x "$cand" ]] && { echo "$cand"; return 0; }
    cand="$REPO_ROOT/client/build/linux-debug/src/griz-client"
    [[ -x "$cand" ]] && { echo "$cand"; return 0; }
    return 1
}

find_server_bin() {
    local f
    for f in "$REPO_ROOT"/Src/GRIZ4-*/bin_server_opt/griz-server; do
        [[ -x "$f" ]] && { echo "$f"; return 0; }
    done
    return 1
}

# --- subcommands ------------------------------------------------------------

cmd_status() {
    local entry rv_path server_pid client_pid
    entry=$(live_rendezvous | head -n1)
    if [[ -z "$entry" ]]; then
        echo "ui=none"
        return 1
    fi
    rv_path="${entry%|*}"
    server_pid="${entry##*|}"
    client_pid=$(ppid_of "$server_pid")
    echo "ui=running"
    echo "rendezvous=$rv_path"
    echo "server_pid=$server_pid"
    echo "ui_pid=${client_pid:-unknown}"
    return 0
}

cmd_up() {
    local db="${1:-$DEFAULT_DB}"

    # Reuse-or-launch: if any UI is already up, print its rendezvous and exit.
    local entry rv_path server_pid client_pid
    entry=$(live_rendezvous | head -n1)
    if [[ -n "$entry" ]]; then
        rv_path="${entry%|*}"
        server_pid="${entry##*|}"
        client_pid=$(ppid_of "$server_pid")
        echo "status=reused"
        echo "rendezvous=$rv_path"
        echo "server_pid=$server_pid"
        echo "ui_pid=${client_pid:-unknown}"
        return 0
    fi

    # Preflight.
    local client_bin server_bin
    client_bin=$(find_client_bin) || {
        err "griz-client not found. Build with: ./build_client.sh release"
        return 2
    }
    server_bin=$(find_server_bin) || {
        err "griz-server not found. Build with: ./build.sh server"
        return 2
    }
    [[ -e "$REPO_ROOT/$db" || -e "$db" ]] || {
        err "database not found: $db"
        return 2
    }
    if [[ -z "${DISPLAY:-}" ]]; then
        err "DISPLAY is unset; the Qt UI needs an X server."
        return 2
    fi
    if ! command -v xdpyinfo >/dev/null 2>&1; then
        note "xdpyinfo not on PATH — skipping X reachability check"
    elif ! xdpyinfo >/dev/null 2>&1; then
        err "xdpyinfo failed — DISPLAY=$DISPLAY is not reachable."
        return 2
    fi

    mkdir -p "$RV_DIR"

    # Resolve db to a stable absolute path (qt client should accept either,
    # but the user's launch invocation is more reproducible this way).
    local db_abs="$db"
    if [[ ! "$db_abs" = /* ]]; then
        db_abs="$REPO_ROOT/$db"
    fi

    local log
    log="/tmp/${USER:-user}/griz-client-$$.log"
    mkdir -p "$(dirname "$log")" 2>/dev/null
    setsid nohup "$client_bin" --db "$db_abs" \
        </dev/null >"$log" 2>&1 & disown

    # Poll for a new live rendezvous to appear.
    local deadline=$((SECONDS + 30))
    while (( SECONDS < deadline )); do
        entry=$(live_rendezvous | head -n1)
        if [[ -n "$entry" ]]; then
            rv_path="${entry%|*}"
            server_pid="${entry##*|}"
            client_pid=$(ppid_of "$server_pid")
            echo "status=launched"
            echo "rendezvous=$rv_path"
            echo "server_pid=$server_pid"
            echo "ui_pid=${client_pid:-unknown}"
            echo "log=$log"
            return 0
        fi
        sleep 0.25
    done

    err "UI did not produce a rendezvous file within 30s. See: $log"
    return 2
}

cmd_down() {
    local entries any=0 entry rv_path server_pid client_pid
    while IFS= read -r entry; do
        [[ -z "$entry" ]] && continue
        any=1
        rv_path="${entry%|*}"
        server_pid="${entry##*|}"
        client_pid=$(ppid_of "$server_pid")
        if [[ -n "$client_pid" && "$client_pid" -gt 1 ]]; then
            kill -TERM "$client_pid" 2>/dev/null && \
                echo "killed ui_pid=$client_pid (server_pid=$server_pid)"
        else
            kill -TERM "$server_pid" 2>/dev/null && \
                echo "killed server_pid=$server_pid (no ui parent found)"
        fi
    done < <(live_rendezvous)

    # Wait briefly, then clean stale files and SIGKILL if anything lingers.
    sleep 1
    while IFS= read -r entry; do
        [[ -z "$entry" ]] && continue
        rv_path="${entry%|*}"
        server_pid="${entry##*|}"
        if is_pid_alive "$server_pid"; then
            kill -KILL "$server_pid" 2>/dev/null
            echo "force-killed server_pid=$server_pid"
        fi
    done < <(live_rendezvous)

    # Reap stale rendezvous files (any ui-*.json whose server_pid is dead).
    if [[ -d "$RV_DIR" ]]; then
        shopt -s nullglob
        local f pid
        for f in "$RV_DIR"/ui-*.json; do
            pid=$(rv_field "$f" server_pid) || { rm -f "$f"; continue; }
            if ! is_pid_alive "$pid"; then
                rm -f "$f" && echo "removed stale rendezvous=$f"
            fi
        done
        shopt -u nullglob
    fi

    if (( any == 0 )); then
        echo "ui=none"
        return 1
    fi
    return 0
}

# --- main -------------------------------------------------------------------

case "${1:-}" in
    up)     shift; cmd_up "$@" ;;
    down)   shift; cmd_down ;;
    status) shift; cmd_status ;;
    ""|-h|--help|help)
        cat <<EOF
Usage: griz-ui.sh <command> [args]

Commands:
  up [database]   Launch the Qt UI (or reuse a running one).
                  Default database: $DEFAULT_DB
  status          Print the live UI/server PIDs and rendezvous path,
                  or 'ui=none' if nothing is running. Exit 1 if none.
  down            Kill running UIs and clean stale rendezvous files.

Output is line-oriented key=value pairs.
EOF
        ;;
    *)
        err "unknown command: $1"
        exit 2
        ;;
esac
