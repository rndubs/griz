"""Tests for FieldAPI's resolve-with-passthrough helper.

`_resolve_with_passthrough` is the small layer between the curated
ResultsMap (which is strict) and the server (which accepts any Mili
command-language name). It lets callers pass either a curated
``(family, component)`` pair or a raw Griz/Mili name like ``sx``.
"""

from __future__ import annotations

import pytest

from griz.exceptions import UnknownFieldError
from griz.field import _resolve_with_passthrough


class TestResolveWithPassthrough:
    def test_curated_family_with_component_resolves_via_map(self):
        assert _resolve_with_passthrough("stress", "von_mises") == "seff"
        assert _resolve_with_passthrough("stress", "xx") == "sx"

    def test_curated_alias_resolves(self):
        assert _resolve_with_passthrough("stress", "vm") == "seff"

    def test_scalar_family_no_component(self):
        assert _resolve_with_passthrough("temperature", None) == "temp"

    def test_raw_mili_name_passes_through(self):
        # `sx`, `seff`, `eps`, `dispmag` are all things `list_fields()`
        # returns directly. Single-arg form must accept them as-is.
        for raw in ("sx", "seff", "eps", "dispmag", "evol"):
            assert _resolve_with_passthrough(raw, None) == raw

    def test_unknown_family_with_component_still_raises(self):
        # `(typo, "xx")` is unambiguously a typo — don't pass through.
        with pytest.raises(UnknownFieldError):
            _resolve_with_passthrough("foo", "xx")

    def test_known_family_unknown_component_still_raises(self):
        # `("stress", "nope")` should still surface the curated error.
        with pytest.raises(UnknownFieldError):
            _resolve_with_passthrough("stress", "nope")

    def test_known_family_without_component_still_raises(self):
        # `("stress", None)` is ambiguous — preserve the strict failure.
        with pytest.raises(UnknownFieldError):
            _resolve_with_passthrough("stress", None)
