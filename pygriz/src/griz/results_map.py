"""Load and query the shared results map.

The canonical YAML lives at ``Src/data/results_map.yaml`` and is
symlinked into ``griz/data/results_map.yaml`` so the wheel ships the
real file. Schema: see ``planning/shared/results-map.md``.

Resolution semantics:

- Scalar fields (``scalar: true`` with a top-level ``griz`` name) accept
  no component. Calling with a component raises ``UnknownFieldError``.
- Fielded fields use their ``components`` map. ``component=None`` is
  ambiguous → raises ``UnknownFieldError`` listing the valid components.
- Aliases declared on a component resolve to the same griz name as the
  primary component key.
"""

from __future__ import annotations

import os
from importlib.resources import files
from pathlib import Path
from typing import Any

import yaml

from griz.exceptions import UnknownFieldError


_PACKAGE_DATA = files("griz.data").joinpath("results_map.yaml")


class ResultsMap:
    """Human-readable field names → Griz command-language tokens."""

    def __init__(self, data: dict | None = None) -> None:
        if data is None:
            data = self._load_default()
        self._schema_version = int(data.get("schema_version", 0))
        self._fields: dict[str, dict[str, Any]] = data.get("fields") or {}
        self._alias_index = self._build_alias_index(self._fields)

    @staticmethod
    def _load_default() -> dict:
        override = os.environ.get("GRIZ_RESULTS_MAP")
        if override:
            path = Path(override)
            if not path.is_file():
                raise FileNotFoundError(
                    f"GRIZ_RESULTS_MAP points at missing file: {path}"
                )
            text = path.read_text()
        else:
            text = _PACKAGE_DATA.read_text()
        loaded = yaml.safe_load(text)
        if not isinstance(loaded, dict):
            raise ValueError("results_map.yaml must be a mapping at the top level")
        return loaded

    @staticmethod
    def _build_alias_index(
        fields: dict[str, dict[str, Any]]
    ) -> dict[tuple[str, str], str]:
        index: dict[tuple[str, str], str] = {}
        for field_name, field_cfg in fields.items():
            components = field_cfg.get("components") or {}
            for comp_name, comp_cfg in components.items():
                for alias in comp_cfg.get("aliases", []) or []:
                    index[(field_name, alias)] = comp_name
        return index

    @property
    def schema_version(self) -> int:
        return self._schema_version

    def fields(self) -> list[str]:
        """Primary field names, in declaration order."""
        return list(self._fields.keys())

    def components(self, field: str) -> list[str]:
        """Primary component names for a field (empty for scalar fields)."""
        cfg = self._fields.get(field)
        if cfg is None:
            raise UnknownFieldError(
                f"unknown field {field!r}; known fields: {self.fields()}"
            )
        return list((cfg.get("components") or {}).keys())

    def resolve(self, field: str, component: str | None = None) -> str:
        """Return the griz command-language name for (field, component)."""
        cfg = self._fields.get(field)
        if cfg is None:
            raise UnknownFieldError(
                f"unknown field {field!r}; known fields: {self.fields()}"
            )

        if cfg.get("scalar"):
            if component is not None:
                raise UnknownFieldError(
                    f"field {field!r} is a scalar — pass component=None"
                )
            griz_name = cfg.get("griz")
            if not isinstance(griz_name, str) or not griz_name:
                raise UnknownFieldError(
                    f"field {field!r} has no griz name configured"
                )
            return griz_name

        components = cfg.get("components") or {}
        if component is None:
            raise UnknownFieldError(
                f"field {field!r} requires a component; "
                f"available: {list(components.keys())}"
            )

        comp_cfg = components.get(component)
        if comp_cfg is None:
            primary = self._alias_index.get((field, component))
            if primary is None:
                raise UnknownFieldError(
                    f"unknown component {component!r} for field {field!r}; "
                    f"available: {list(components.keys())}"
                )
            comp_cfg = components.get(primary)
            if comp_cfg is None:
                raise UnknownFieldError(
                    f"alias {component!r} references missing primary "
                    f"{primary!r} in field {field!r}"
                )

        griz_name = comp_cfg.get("griz")
        if not isinstance(griz_name, str) or not griz_name:
            raise UnknownFieldError(
                f"field {field!r} component {component!r} missing griz name"
            )
        return griz_name

    def label(self, field: str, component: str | None = None) -> str:
        """Return the human-readable label for a resolved (field, component)."""
        cfg = self._fields.get(field)
        if cfg is None:
            raise UnknownFieldError(f"unknown field {field!r}")
        if cfg.get("scalar"):
            return cfg.get("label") or field
        components = cfg.get("components") or {}
        comp_cfg = components.get(component)
        if comp_cfg is None and component is not None:
            primary = self._alias_index.get((field, component))
            if primary is not None:
                comp_cfg = components.get(primary)
        if comp_cfg is None:
            raise UnknownFieldError(
                f"no label for field={field!r} component={component!r}"
            )
        return comp_cfg.get("label") or f"{field}.{component}"


_default_map: ResultsMap | None = None


def default_map() -> ResultsMap:
    """Return a lazily-initialised module-level ResultsMap."""
    global _default_map
    if _default_map is None:
        _default_map = ResultsMap()
    return _default_map
