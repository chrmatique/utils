import argparse
import importlib.util
import os
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory


MODULE_PATH = Path(__file__).parents[1] / "newgit.py"
spec = importlib.util.spec_from_file_location("newgit", MODULE_PATH)
newgit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(newgit)


def run_generated_hook(text: str) -> subprocess.CompletedProcess[str]:
    with TemporaryDirectory() as directory:
        repo = Path(directory)
        subprocess.run(["git", "init", "-q"], cwd=repo, check=True)
        staged = repo / "config.txt"
        staged.write_text(text, encoding="utf-8")
        subprocess.run(["git", "add", "config.txt"], cwd=repo, check=True)
        hook = repo / ".git" / "hooks" / "pre-commit"
        hook.write_text(newgit.PRE_COMMIT_HOOK, encoding="utf-8")
        hook.chmod(0o755)
        return subprocess.run([str(hook)], cwd=repo, text=True, capture_output=True)


def test_detects_supported_provider_api_key_formats():
    text = "\n".join(
        [
            "sk-" + "proj-abcdefghijklmnopqrstuvwxyz123456",
            "sk-" + "ant-api03-abcdefghijklmnopqrstuvwxyz123456",
            "key_abcdefghijklmnopqrstuvwxyz123456",
            "sk-" + "abcdefghijklmnopqrstuvwxyz123456",
            "sk-" + "abcdefghijklmnopqrstuvwxyz123456",
            "0123456789abcdef0123" + "." + "secretabcdefghijklmnopqrstuvwxyz123456",
        ]
    )

    result = run_generated_hook(text)

    assert result.returncode == 1
    assert "possible secrets" in result.stderr


def test_does_not_flag_provider_names_or_documentation_examples_without_key_values():
    text = "Supported providers: OpenAI, Anthropic, Cursor, DeepSeek, Kimi (Moonshot), and Z.ai."

    result = run_generated_hook(text)

    assert result.returncode == 0


# ---------------------------------------------------------------------------
# Inline YAML parser tests
# ---------------------------------------------------------------------------


def test_parse_yaml_simple_scalars_and_lists():
    text = """
path: /tmp/project
ignore:
  - .venv/
  - node_modules/
  - __pycache__/
defaults: true
no_precommit: false
git_init: yes
no_git_init: no
"""
    result = newgit.parse_yaml_simple(text)
    assert result["path"] == "/tmp/project"
    assert result["ignore"] == [".venv/", "node_modules/", "__pycache__/"]
    assert result["defaults"] is True
    assert result["no_precommit"] is False
    assert result["git_init"] is True
    assert result["no_git_init"] is False


def test_parse_yaml_simple_ignores_comments_and_blank_lines():
    text = """
# Project-level newgit configuration
path: .  # current directory

ignore:
  - build/  # build output
  - dist/
"""
    result = newgit.parse_yaml_simple(text)
    assert result["path"] == "."
    assert result["ignore"] == ["build/", "dist/"]


def test_parse_yaml_simple_preserves_quoted_hashes():
    text = 'ignore:\n  - "foo#bar"\n  - \'baz#qux\'\n'
    result = newgit.parse_yaml_simple(text)
    assert result["ignore"] == ["foo#bar", "baz#qux"]


def test_parse_yaml_simple_parses_numbers_and_null():
    text = "threshold: 42\nratio: 3.14\nempty: null\n"
    result = newgit.parse_yaml_simple(text)
    assert result["threshold"] == 42
    assert result["ratio"] == 3.14
    assert result["empty"] is None


def test_parse_yaml_simple_normalizes_hyphenated_keys():
    text = "no-precommit: true\ngit-init: false\n"
    result = newgit.parse_yaml_simple(text)
    assert result["no_precommit"] is True
    assert result["git_init"] is False


def test_parse_yaml_simple_empty_document():
    assert newgit.parse_yaml_simple("") == {}
    assert newgit.parse_yaml_simple("# only comments\n\n") == {}


# ---------------------------------------------------------------------------
# Argument resolution tests
# ---------------------------------------------------------------------------


def _namespace(**kwargs) -> argparse.Namespace:
    defaults = {
        "path": None,
        "ignore": None,
        "defaults": None,
        "no_precommit": None,
        "git_init": None,
        "no_git_init": None,
    }
    defaults.update(kwargs)
    return argparse.Namespace(**defaults)


def test_resolve_cli_args_uses_config_when_cli_is_absent():
    config = {
        "path": "/config/path",
        "ignore": [".venv/"],
        "defaults": True,
        "no_precommit": True,
        "git_init": True,
    }
    args = newgit.resolve_cli_args(_namespace(), config)
    assert args.path == "/config/path"
    assert args.ignore is None
    assert args.config_ignore == [".venv/"]
    assert args.defaults is True
    assert args.no_precommit is True
    assert args.git_init is True
    assert args.no_git_init is False


def test_resolve_cli_args_cli_overrides_config():
    config = {
        "path": "/config/path",
        "ignore": [".venv/"],
        "defaults": True,
        "git_init": True,
    }
    args = newgit.resolve_cli_args(
        _namespace(path="/cli/path", ignore=["node_modules/"], defaults=False, no_git_init=True),
        config,
    )
    assert args.path == "/cli/path"
    assert args.ignore == ["node_modules/"]
    assert args.config_ignore == [".venv/"]
    assert args.defaults is False
    assert args.git_init is False
    assert args.no_git_init is True


def test_resolve_cli_args_applies_hardcoded_defaults():
    args = newgit.resolve_cli_args(_namespace(), {})
    assert args.path == "."
    assert args.ignore is None
    assert args.config_ignore is None
    assert args.defaults is False
    assert args.no_precommit is False
    assert args.git_init is False
    assert args.no_git_init is False


def test_resolve_cli_args_rejects_both_git_flags():
    try:
        newgit.resolve_cli_args(_namespace(), {"git_init": True, "no_git_init": True})
    except ValueError as exc:
        assert "cannot both be enabled" in str(exc)
    else:
        raise AssertionError("Expected ValueError")


# ---------------------------------------------------------------------------
# Config loading and ignore resolution tests
# ---------------------------------------------------------------------------


def test_load_yaml_config_reads_target_directory_file():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_file = target / "newgit.yml"
        config_file.write_text("path: /foo\ndefaults: true\n", encoding="utf-8")
        assert newgit.load_yaml_config(target) == {"path": "/foo", "defaults": True}


def test_load_yaml_config_falls_back_to_global_config():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_dir = target / "global" / "newgit"
        config_dir.mkdir(parents=True)
        config_file = config_dir / "newgit.yml"
        config_file.write_text("no_precommit: true\n", encoding="utf-8")
        old_xdg = os.environ.get("XDG_CONFIG_HOME")
        os.environ["XDG_CONFIG_HOME"] = str(target / "global")
        try:
            assert newgit.load_yaml_config(target) == {"no_precommit": True}
        finally:
            if old_xdg is None:
                os.environ.pop("XDG_CONFIG_HOME", None)
            else:
                os.environ["XDG_CONFIG_HOME"] = old_xdg


def test_resolve_entries_cli_ignore_wins():
    assert newgit.resolve_entries(["a/"], True, ["b/"]) == ["a/"]


def test_resolve_entries_defaults_flag_wins_over_config():
    assert newgit.resolve_entries(None, True, ["b/"]) == newgit.DEFAULT_IGNORES


def test_resolve_entries_config_ignore_wins_over_builtin():
    assert newgit.resolve_entries(None, False, ["b/"]) == ["b/"]


def test_resolve_entries_falls_back_to_builtin_when_nothing_provided():
    assert newgit.resolve_entries(None, False, None) == newgit.DEFAULT_IGNORES


# ---------------------------------------------------------------------------
# End-to-end config file behavior
# ---------------------------------------------------------------------------


def test_config_file_drives_gitignore_creation():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_file = target / "newgit.yml"
        config_file.write_text(
            "ignore:\n  - .venv/\n  - node_modules/\nno_git_init: true\nno_precommit: true\n",
            encoding="utf-8",
        )
        result = subprocess.run(
            ["python3", str(MODULE_PATH), str(target)],
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr
        gitignore = target / ".gitignore"
        assert gitignore.exists()
        content = gitignore.read_text(encoding="utf-8")
        assert ".venv/" in content
        assert "node_modules/" in content
        assert not (target / ".git").exists()


def test_cli_overrides_config_file_ignore():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_file = target / "newgit.yml"
        config_file.write_text("ignore:\n  - .venv/\n", encoding="utf-8")
        result = subprocess.run(
            ["python3", str(MODULE_PATH), str(target), "-i", "build/", "--no-git-init"],
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr
        content = (target / ".gitignore").read_text(encoding="utf-8")
        assert "build/" in content
        assert ".venv/" not in content


def test_parse_yaml_simple_handles_escaped_quotes_and_unescaped_hashes():
    text = r'''
path: "foo#bar"
ignore:
  - 'it''s#ok'
  - "quote\"inside#ok"
url: http://example.com#fragment
'''
    result = newgit.parse_yaml_simple(text)
    assert result["path"] == "foo#bar"
    assert result["ignore"] == ["it's#ok", 'quote"inside#ok']
    assert result["url"] == "http://example.com"


def test_parse_yaml_simple_filters_bare_list_items():
    text = "ignore:\n  - \n  - build/\n  - \n"
    result = newgit.parse_yaml_simple(text)
    assert newgit._normalize_ignore(result["ignore"]) == ["build/"]


# ---------------------------------------------------------------------------
# Integration tests for audit findings
# ---------------------------------------------------------------------------


def test_config_path_is_applied():
    with TemporaryDirectory() as directory:
        cwd = Path(directory)
        other = cwd / "other"
        other.mkdir()
        config_file = cwd / "newgit.yml"
        config_file.write_text(
            f"path: {other}\nignore:\n  - .venv/\nno_git_init: true\nno_precommit: true\n",
            encoding="utf-8",
        )
        result = subprocess.run(
            ["python3", str(MODULE_PATH)],
            cwd=cwd,
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr
        assert (other / ".gitignore").exists()
        assert not (cwd / ".gitignore").exists()


def test_empty_config_ignore_writes_empty_gitignore():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_file = target / "newgit.yml"
        config_file.write_text("ignore: []\nno_git_init: true\n", encoding="utf-8")
        result = subprocess.run(
            ["python3", str(MODULE_PATH), str(target), "--no-git-init"],
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr
        content = (target / ".gitignore").read_text(encoding="utf-8")
        assert content == "# Generated by newgit\n"


def test_cli_no_git_init_overrides_config_git_init():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_file = target / "newgit.yml"
        config_file.write_text("git_init: true\nignore:\n  - .venv/\n", encoding="utf-8")
        result = subprocess.run(
            ["python3", str(MODULE_PATH), str(target), "--no-git-init"],
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr
        assert (target / ".gitignore").exists()
        assert not (target / ".git").exists()


def test_cli_git_init_overrides_config_no_git_init():
    with TemporaryDirectory() as directory:
        target = Path(directory)
        config_file = target / "newgit.yml"
        config_file.write_text("git_init: false\nignore:\n  - .venv/\nno_precommit: true\n", encoding="utf-8")
        result = subprocess.run(
            ["python3", str(MODULE_PATH), str(target), "--git-init"],
            text=True,
            capture_output=True,
        )
        assert result.returncode == 0, result.stderr
        assert (target / ".gitignore").exists()
        assert (target / ".git").exists()
