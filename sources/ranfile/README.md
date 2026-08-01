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

| Flag | Description |
|------|-------------|
| `-w, --words N` | Generate N space-separated random words |
| `-s, --sentences N` | Generate N sentences |
| `-p, --paragraphs N` | Generate N paragraphs (separated by blank lines) |
| `-o, --output FILE` | Write to file instead of stdout |
| `--seed U` | 64-bit seed for reproducible output |
| `-h, --help` | Show help |

Exactly one of `-w`, `-s`, or `-p` is required.

Paragraphs are separated by two newlines (`\n\n`).

## Examples

```bash
ranfile -w 500
ranfile -p 10 -o lorem.txt
ranfile --sentences 25 --seed 42
```
