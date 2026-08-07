# ranfile

Small, fast random text generator written in C.

## Build

```bash
make
```

## Install (optional)

```bash
make install PREFIX=$HOME/.local
```

## Usage

```bash
./ranfile [OPTIONS]
```

| Flag | Description |
|------|-------------|
| `-w, --words N` | Generate N space-separated random words |
| `-s, --sentences N` | Generate N sentences |
| `-p, --paragraphs N` | Generate N paragraphs (separated by blank lines) |
| `--size SIZE` | Generate text up to SIZE bytes (`10B`, `2.5MB`, `1GB`) |
| `--markdown N` | Generate N random Markdown sections |
| `-c, --compress` | Collapse whitespace/newlines into a single line |
| `-y, --no-confirm` | Skip the large-file confirmation prompt |
| `-o, --output FILE` | Write to file instead of stdout |
| `--seed U` | 64-bit seed for reproducible output |
| `-h, --help` | Show help |

Exactly one of `-w`, `-s`, `-p`, `--size`, or `--markdown` is required.

When using `--size` with a value of `0.5GB` or larger, a `Y/N` prompt is shown
unless `-y`/`--no-confirm` is passed.

Paragraphs are separated by two newlines (`\n\n`).

## Examples

```bash
./ranfile -w 500
./ranfile -p 10 -o lorem.txt
./ranfile --sentences 25 --seed 42
./ranfile --size 1MB -o lorem.txt
./ranfile --markdown 5 --compress
```

## Tests

```bash
make test
```

Tests live in `tests/` and exercise the compiled `ranfile` binary via Python.
