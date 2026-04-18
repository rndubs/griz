# Shared — Results Map

## Scope

Fixes the single source of truth for the mapping from human-readable result names (`stress.xx`, `displacement.magnitude`, `temperature`) to the terse names Griz's command language actually accepts (`sx`, `umag`, `temp`). Both front ends need this:

- The MCP Python package uses it to implement `g.field.show("stress", component="xx")` — what `MCP.md` §5.4 called the hand-curated `RESULTS` dict.
- The Qt UI client uses it to populate field/component menus with the same labels and to translate user selections into the same underlying commands.

Having the map in one place prevents the two front ends from drifting in what they expose or what they call things.

## Related

- [`../MCP.md`](../MCP.md) §5.4 (original hand-curated dict).
- [`query-commands.md`](query-commands.md) — the `results.available` block in the state schema is generated from this map.
- [`../ui-design/04-client.md`](../ui-design/04-client.md) (stub; will consume this map).

## Decision

The map lives as a **data file** checked into the source tree:

```
Src/data/results_map.yaml
```

YAML is chosen over JSON/TOML because the file benefits from comments explaining unit conventions and alias history, and over an embedded C table because the Qt client and the Python package each need to load it and we do not want to regenerate code for both.

## File format

```yaml
# Src/data/results_map.yaml
# Maps human-readable (field, component) pairs to Griz command-language names.
# Single source of truth; consumed by Src/python/griz/ and the Qt client.

schema_version: 1

fields:

  stress:
    description: "Cauchy stress tensor components and invariants."
    unit: "stress"
    components:
      xx:         { griz: "sx",    label: "σxx" }
      yy:         { griz: "sy",    label: "σyy" }
      zz:         { griz: "sz",    label: "σzz" }
      xy:         { griz: "sxy",   label: "σxy" }
      yz:         { griz: "syz",   label: "σyz" }
      zx:         { griz: "szx",   label: "σzx" }
      von_mises:  { griz: "seff",  label: "σvm",    aliases: ["vm", "mises"] }
      pressure:   { griz: "spres", label: "p" }

  strain:
    description: "Infinitesimal strain tensor."
    unit: "dimensionless"
    components:
      xx: { griz: "exx", label: "εxx" }
      yy: { griz: "eyy", label: "εyy" }
      zz: { griz: "ezz", label: "εzz" }
      xy: { griz: "exy", label: "εxy" }
      yz: { griz: "eyz", label: "εyz" }
      zx: { griz: "ezx", label: "εzx" }

  temperature:
    description: "Nodal temperature scalar."
    unit: "temperature"
    scalar: true
    griz: "temp"

  displacement:
    description: "Nodal displacement vector and magnitude."
    unit: "length"
    components:
      x:         { griz: "ux",   label: "ux" }
      y:         { griz: "uy",   label: "uy" }
      z:         { griz: "uz",   label: "uz" }
      magnitude: { griz: "umag", label: "|u|", aliases: ["mag"] }
```

Conventions:

- `scalar: true` fields have no `components` map; the top-level `griz` name applies. Consumers surface them as `g.field.show("temperature")` with no component.
- `aliases` is optional; any listed name resolves to the same `griz` command. The primary name is the dict key.
- `label` is the presentation string (may contain unicode / TeX-ish glyphs). MCP tool docs and Qt menus use it.
- `unit` is a free-form category; used for axis labels and min/max formatting. Not yet enumerated — phase 2 may pin a vocabulary.

## Loaders

### Python

`griz/results_map.py` (in the `griz` package) loads the YAML at import time:

```python
import importlib.resources as r
import yaml

_DATA = yaml.safe_load(r.files("griz.data").joinpath("results_map.yaml").read_text())

def resolve(field: str, component: str | None = None) -> str:
    """Return the griz command-language name for (field, component), or raise UnknownFieldError."""
    ...
```

The file is shipped inside the wheel (`package_data` or `importlib.resources`), so it ends up alongside the Python module and is version-locked with the package.

Users never edit the loaded dict; the Python `raw()` escape hatch remains available for unmapped fields.

### Qt client

The C++ client loads the same YAML at startup (`yaml-cpp` or similar) and keeps the resulting tree behind a `ResultsMap` service used by menu builders and the command translator.

For packaging, the client installer ships its own copy of `results_map.yaml` under the client's resource directory. The server does **not** read this file — the server only speaks the terse names, and the state schema ([`query-commands.md`](query-commands.md)) is what exposes the human-readable names to the server's clients.

### Server (optional)

The server does not need to load the YAML to function, but `q_results` currently has to report `results.available` in human-readable form. Two options:

1. **Ship the YAML alongside the server** and load it at startup so `q_results` can produce friendly names. Simple, small dep (yaml-cpp or a tiny YAML reader).
2. **Ship a generated C header** (`results_map.h`) produced from the YAML at build time, linked into the server.

Option 2 avoids a runtime dep on the server side. Recommended: **option 2** for the server, **option 1** for the Qt client (which already brings yaml-cpp for other reasons).

## Generation / CI

- A tiny script `Src/data/generate_results_map_header.py` produces `Src/data/results_map.h` from the YAML, run as part of the build.
- CI lints the YAML for: unique primary keys, unique alias resolution, every entry has a non-empty `griz` name, `scalar: true` entries have no `components`.

## Conflict resolution

`MCP.md` §5.4 placed the map inside the Python package as the authoritative home. That was fine in isolation but left the Qt client needing either its own copy or a runtime query against the Python package — both awkward. The conflict is resolved by:

- Moving the authoritative data out of Python and into `Src/data/results_map.yaml`, which ships with every front end.
- Keeping the `griz/results_map.py` loader for the Python API, but pointing it at the shared file.
- The `RESULTS` dict shape from MCP.md §5.4 is replaced by the richer YAML (labels, aliases, units, descriptions). The Python loader flattens it back into the same lookup semantics users had before.

## Open questions

- **Unit taxonomy.** Should `unit` be one of a fixed vocabulary (`length`, `stress`, `temperature`, ...) tied to a unit-system preference? Current plan: free-form for v1, revisit if the UI grows a units preferences panel.
- **Database-specific extensions.** Some Mili DBs expose custom fields. The current map is universal. Mechanism for a per-database overlay file is a phase-2 question.
- **i18n / localization.** `label` is English-only for now. Phase 3 if needed.
