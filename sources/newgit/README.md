# newgit

Create a `.gitignore` skeleton and optionally initialize a git repository.

## Quick usage

If you have the Python script:

```bash
./newgit.py [path]
```

If you have the compiled binary:

```bash
./newgit [path]
```

If `path` is omitted, it uses the current directory.

## Examples

Create `.gitignore` in the current directory and prompt for `git init`:

```bash
./newgit.py
```

Create `.gitignore` in a new project directory:

```bash
./newgit.py ~/projects/my-app
```

Override the ignore patterns for a single run. Put the path before `-i` if you also pass a directory:

```bash
./newgit.py -i .venv/ node_modules/ __pycache__/
./newgit.py ~/projects/my-app -i .venv/ node_modules/
```

Skip the `git init` prompt and always run it:

```bash
./newgit.py --git-init
```

Skip the `git init` prompt and never run it:

```bash
./newgit.py --no-git-init
```

Force the built-in defaults even if a config file exists:

```bash
./newgit.py --defaults
```

## Default ignore patterns

```
.cursor/
.build/
.debug/
.DS_Store
```

## Configuration

You can provide a persistent list of ignore patterns in a config file:

- `$XDG_CONFIG_HOME/newgit/ignore`
- `~/.config/newgit/ignore` (fallback)

The file uses one pattern per line. Lines starting with `#` and blank lines are ignored. If the file exists, it replaces the built-in defaults. Use `--defaults` to ignore the config file.

## Existing `.gitignore` behavior

If the target directory already contains a `.gitignore`, the tool prompts:

```
.gitignore already exists. [o]verwrite / [a]ppend / [q]uit:
```

Append mode merges the resolved patterns into the existing file, skipping duplicates.

## Building a standalone binary

You can build a single-file executable for Linux or macOS with PyInstaller.

Install PyInstaller (once):

```bash
pip install pyinstaller
```

Build the binary:

```bash
make
```

The resulting executable is written to `dist/newgit`. You can run it directly or copy it to a directory on your PATH:

```bash
./dist/newgit --help
cp dist/newgit /usr/local/bin/newgit
```

To clean build artifacts:

```bash
make clean
```

Notes:
- PyInstaller binaries are platform-specific. Run `make` on each platform where you need a binary (e.g., macOS for macOS, Linux for Linux).
- `make install` copies `dist/newgit` to `/usr/local/bin/newgit`.
