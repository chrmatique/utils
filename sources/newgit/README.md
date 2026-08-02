# newgit

<img src="https://chrmserve.b-cdn.net/pb-img/newgit.png" width="250" height="250">

Create a `.gitignore` skeleton, optionally initialize a git repository, and install a secret-scanning pre-commit hook.

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

Skip the pre-commit hook installation when running `git init`:

```bash
./newgit.py --git-init --no-precommit
```

## Default ignore patterns

The generated `.gitignore` includes common build, IDE, and macOS artifacts, plus secret- and credential-related files:

```
# Editor / IDE
.cursor/

# Build / debug artifacts
.build/
.debug/

# macOS
.DS_Store

# Secrets and credentials
.env
.env.*
!.env.example
*.pem
*.key
*.p12
*.pfx
*.keystore
*.jks
id_rsa
id_rsa.pub
id_dsa
id_dsa.pub
id_ecdsa
id_ecdsa.pub
id_ed25519
id_ed25519.pub
secrets/
credentials/
private/
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

## Secret-scanning pre-commit hook

When `git init` is run (or when you run `newgit` inside an existing repository), it offers to install a pre-commit hook in `.git/hooks/pre-commit`. The hook scans staged files for:

- Private key blocks (`-----BEGIN ... PRIVATE KEY-----`)
- AWS access key IDs (`AKIA...`)
- Lines containing `api_key`, `secret_key`, `private_key`, `auth_token`, or `access_token` followed by a value
- Sensitive filenames such as `.env`, `.pem`, `.key`, `.p12`, `.pfx`, `.keystore`, and `.jks`

If it finds any of these, the commit is blocked and the offending lines are printed. The hook is written in Python and requires `python3` to be available in the environment where commits are made.

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
