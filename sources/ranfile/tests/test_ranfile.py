"""Tests for the ranfile C binary."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path
from typing import List

import pytest

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "ranfile"


def run(args: List[str], stdin: str | None = None, cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [str(BINARY), *args],
        cwd=cwd,
        capture_output=True,
        text=True,
        input=stdin,
    )
    return result


@pytest.fixture(scope="session", autouse=True)
def ensure_binary() -> None:
    if not BINARY.exists():
        subprocess.run(["make"], cwd=ROOT, check=True)


# ---------------------------------------------------------------------------
# Count-based generation
# ---------------------------------------------------------------------------


def test_words() -> None:
    result = run(["-w", "10", "--seed", "1"])
    assert result.returncode == 0
    words = result.stdout.strip().split()
    assert len(words) == 10


def test_sentences() -> None:
    result = run(["-s", "5", "--seed", "1"])
    assert result.returncode == 0
    assert sum(1 for s in result.stdout.strip().split(" ") if s and s[-1] in ".?!") == 5


def test_paragraphs() -> None:
    result = run(["-p", "3", "--seed", "1"])
    assert result.returncode == 0
    paragraphs = result.stdout.strip().split("\n\n")
    assert len(paragraphs) == 3


def test_seed_reproducibility() -> None:
    a = run(["-p", "4", "--seed", "12345"]).stdout
    b = run(["-p", "4", "--seed", "12345"]).stdout
    assert a == b


# ---------------------------------------------------------------------------
# Size-based generation
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "spec,expected",
    [
        ("10B", 10),
        ("2.5MB", int(2.5 * 1024 * 1024)),
        ("1GB", 1024 * 1024 * 1024),
        (" 1 gb ", 1024 * 1024 * 1024),
    ],
)
def test_size_parsing(spec: str, expected: int) -> None:
    result = run(["--size", spec, "--no-confirm", "--seed", "1"])
    assert result.returncode == 0
    assert len(result.stdout.encode("utf-8")) <= expected


@pytest.mark.parametrize("spec", ["10", "10KB", "-5MB", "0B", "abc"])
def test_size_parsing_invalid(spec: str) -> None:
    result = run(["--size", spec, "--no-confirm"])
    assert result.returncode == 2


def test_size_output_to_file(tmp_path: Path) -> None:
    out = tmp_path / "sized.txt"
    result = run(["--size", "200B", "--no-confirm", "--seed", "8", "-o", str(out)])
    assert result.returncode == 0
    data = out.read_text(encoding="utf-8")
    assert data.endswith("\n")
    assert len(data.encode("utf-8")) <= 200


# ---------------------------------------------------------------------------
# Compression
# ---------------------------------------------------------------------------


def test_compress_collapse_to_one_line() -> None:
    result = run(["-p", "3", "--seed", "1", "--compress"])
    assert result.returncode == 0
    stripped = result.stdout.rstrip("\n")
    assert "\n" not in stripped
    assert "  " not in stripped


# ---------------------------------------------------------------------------
# Markdown generation
# ---------------------------------------------------------------------------


def test_markdown_starts_with_title() -> None:
    result = run(["--markdown", "5", "--seed", "1"])
    assert result.returncode == 0
    assert result.stdout.startswith("# ")


def test_markdown_contains_expected_elements() -> None:
    result = run(["--markdown", "200", "--seed", "99"])
    assert result.returncode == 0
    md = result.stdout
    assert md.startswith("# ")
    assert re.search(r"^##+ ", md, flags=re.MULTILINE)
    assert "- " in md or "1." in md
    assert "```" in md
    assert "|" in md
    assert "> " in md


def test_markdown_seed_reproducibility() -> None:
    a = run(["--markdown", "10", "--seed", "4242"]).stdout
    b = run(["--markdown", "10", "--seed", "4242"]).stdout
    assert a == b


def test_markdown_section_count_zero() -> None:
    result = run(["--markdown", "0", "--seed", "3"])
    assert result.returncode == 0
    assert result.stdout.startswith("# ")
    # Only the title's trailing blank line remains; no extra sections.
    assert result.stdout.count("\n\n") == 1


def test_markdown_paragraph_formatting() -> None:
    result = run(["--markdown", "20", "--seed", "5"])
    assert result.returncode == 0
    in_code = False
    for line in result.stdout.splitlines():
        if line.startswith("```"):
            in_code = not in_code
            continue
        if in_code or not line:
            continue
        if not line.startswith(("#", "-", "|", ">", "1.")):
            assert line[0].isalnum() or line[0] in "*_` "


# ---------------------------------------------------------------------------
# Large-file confirmation
# ---------------------------------------------------------------------------


def test_large_file_confirmation_aborted() -> None:
    result = run(["--size", "1GB", "--seed", "1"], stdin="\n")
    assert result.returncode == 2
    assert "aborted" in result.stderr.lower()


def test_large_file_confirmed(tmp_path: Path) -> None:
    out = tmp_path / "big.txt"
    # Use 1MB to exercise the confirmation path without slowing CI.
    result = run(
        ["--size", "1MB", "--seed", "1", "-o", str(out)],
        stdin="y\n",
    )
    assert result.returncode == 0
    size = out.stat().st_size
    assert size <= 1024 * 1024
    assert size > 1024 * 1024 * 0.5


# ---------------------------------------------------------------------------
# CLI validation
# ---------------------------------------------------------------------------


def test_no_mode_errors() -> None:
    result = run([])
    assert result.returncode == 2


def test_conflicting_modes() -> None:
    result = run(["-w", "5", "-s", "3"])
    assert result.returncode == 2


def test_help() -> None:
    result = run(["--help"])
    assert result.returncode == 0
    assert "--size" in result.stdout
    assert "--markdown" in result.stdout
    assert "--compress" in result.stdout


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
