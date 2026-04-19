"""Unit tests for griz.results_map."""

from __future__ import annotations

import pytest

from griz.exceptions import UnknownFieldError
from griz.results_map import ResultsMap, default_map


def test_default_map_loads_canonical_yaml():
    m = default_map()
    assert "stress" in m.fields()
    assert m.schema_version == 1


def test_fields_roundtrip():
    m = ResultsMap()
    assert m.resolve("stress", "xx") == "sx"
    assert m.resolve("stress", "yy") == "sy"
    assert m.resolve("stress", "von_mises") == "seff"
    assert m.resolve("temperature") == "temp"
    assert m.resolve("displacement", "magnitude") == "umag"


def test_aliases_resolve_to_primary_griz_name():
    m = ResultsMap()
    assert m.resolve("stress", "vm") == "seff"
    assert m.resolve("stress", "mises") == "seff"
    assert m.resolve("displacement", "mag") == "umag"


def test_unknown_field_raises():
    m = ResultsMap()
    with pytest.raises(UnknownFieldError):
        m.resolve("not_a_field")


def test_unknown_component_raises_and_lists_available():
    m = ResultsMap()
    with pytest.raises(UnknownFieldError) as excinfo:
        m.resolve("stress", "nope")
    assert "xx" in str(excinfo.value)


def test_scalar_with_component_raises():
    m = ResultsMap()
    with pytest.raises(UnknownFieldError):
        m.resolve("temperature", "xx")


def test_fielded_without_component_raises():
    m = ResultsMap()
    with pytest.raises(UnknownFieldError) as excinfo:
        m.resolve("stress")
    assert "requires a component" in str(excinfo.value)


def test_components_listing_for_scalar_is_empty():
    m = ResultsMap()
    assert m.components("temperature") == []


def test_components_listing_for_fielded_contains_primary_names():
    m = ResultsMap()
    comps = m.components("displacement")
    assert "magnitude" in comps
    # Aliases are not listed as primary components.
    assert "mag" not in comps


def test_components_unknown_field_raises():
    m = ResultsMap()
    with pytest.raises(UnknownFieldError):
        m.components("not_a_field")


def test_label_for_scalar_falls_back_to_field_name():
    m = ResultsMap()
    assert m.label("temperature") == "temperature"


def test_label_for_fielded_uses_configured_label():
    m = ResultsMap()
    assert m.label("stress", "von_mises") == "von Mises"


def test_label_honors_aliases():
    m = ResultsMap()
    assert m.label("stress", "vm") == "von Mises"


def test_custom_data_overrides_default():
    data = {
        "schema_version": 2,
        "fields": {
            "foo": {
                "components": {
                    "bar": {"griz": "fbar", "label": "F.bar"},
                }
            },
            "scalar_only": {"scalar": True, "griz": "so"},
        },
    }
    m = ResultsMap(data=data)
    assert m.schema_version == 2
    assert m.resolve("foo", "bar") == "fbar"
    assert m.resolve("scalar_only") == "so"


def test_env_override_reads_alternate_file(tmp_path, monkeypatch):
    alt = tmp_path / "alt.yaml"
    alt.write_text(
        """
schema_version: 9
fields:
  mystery:
    scalar: true
    griz: myst
"""
    )
    monkeypatch.setenv("GRIZ_RESULTS_MAP", str(alt))
    m = ResultsMap()
    assert m.schema_version == 9
    assert m.resolve("mystery") == "myst"


def test_env_override_missing_file_raises(tmp_path, monkeypatch):
    monkeypatch.setenv("GRIZ_RESULTS_MAP", str(tmp_path / "does-not-exist.yaml"))
    with pytest.raises(FileNotFoundError):
        ResultsMap()
