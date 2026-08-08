use std::env;
use std::fs::File;
use std::io::{self, Read, Write};
use std::process;
use std::time::{SystemTime, UNIX_EPOCH};

// ---------------------------------------------------------------------------
// Configuration constants (matches src/config.h)
// ---------------------------------------------------------------------------

const SENTENCE_WORDS_MIN: usize = 8;
const SENTENCE_WORDS_MAX: usize = 15;
const PARAGRAPH_SENTENCES_MIN: usize = 3;
const PARAGRAPH_SENTENCES_MAX: usize = 7;
const SIZE_GB_THRESHOLD: usize = 512 * 1024 * 1024;

// ---------------------------------------------------------------------------
// Static word dictionary (matches src/words.h)
// ---------------------------------------------------------------------------

const WORDS: &[&str] = &[
    "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing",
    "elit", "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore",
    "et", "dolore", "magna", "aliqua", "enim", "ad", "minim", "veniam",
    "quis", "nostrud", "exercitation", "ullamco", "laboris", "nisi", "aliquip",
    "ex", "ea", "commodo", "consequat", "duis", "aute", "irure", "in",
    "reprehenderit", "voluptate", "velit", "esse", "cillum", "fugiat", "nulla",
    "pariatur", "excepteur", "sint", "occaecat", "cupidatat", "non", "proident",
    "sunt", "culpa", "qui", "officia", "deserunt", "mollit", "anim", "id",
    "est", "laborum", "at", "vero", "eos", "accusamus", "iusto", "odio",
    "dignissimos", "ducimus", "blanditiis", "praesentium", "voluptatum", "deleniti",
    "atque", "corrupti", "quos", "dolores", "quas", "molestias", "excepturi",
    "obcaecati", "cupiditate", "provident", "similique", "architecto", "beatae",
    "vitae", "dicta", "explicabo", "nemo", "ipsam", "voluptatem", "quia",
    "voluptas", "aspernatur", "aut", "odit", "fugit", "numquam", "eius",
    "modi", "tempora", "magnam", "quaerat", "voluptatem", "sequi", "nesciunt",
    "neque", "porro", "quisquam", "dolorem", "adipisci", "numquam", "eius",
    "modi", "tempora", "incidunt", "magnam", "aliquam", "quaerat", "voluptatem",
];

// ---------------------------------------------------------------------------
// PRNG (matches src/rng.h — Xorshift64*)
// ---------------------------------------------------------------------------

struct Rng {
    state: u64,
}

impl Rng {
    fn next(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        self.state = x;
        x.wrapping_mul(0x2545F4914F6CDD1D)
    }

    fn bounded(&mut self, n: u64) -> u64 {
        self.next() % n
    }
}

// ---------------------------------------------------------------------------
// Generation types
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, Debug, PartialEq)]
enum GenMode {
    Words,
    Sentences,
    Paragraphs,
    Size,
    Markdown,
}

struct GenOptions {
    mode: GenMode,
    count: usize,
    seed: u64,
    target_size: usize,
}

// ---------------------------------------------------------------------------
// Text generation helpers
// ---------------------------------------------------------------------------

fn append_word(buf: &mut Vec<u8>, word_idx: usize, capitalize: bool) {
    let word = WORDS[word_idx].as_bytes();
    if capitalize && !word.is_empty() {
        let mut first = word[0];
        if first.is_ascii_lowercase() {
            first = first - b'a' + b'A';
        }
        buf.push(first);
        buf.extend_from_slice(&word[1..]);
    } else {
        buf.extend_from_slice(word);
    }
}

fn sentence_punct(rng: &mut Rng) -> u8 {
    let roll = rng.bounded(100);
    if roll < 85 {
        b'.'
    } else if roll < 95 {
        b'?'
    } else {
        b'!'
    }
}

fn append_sentence(buf: &mut Vec<u8>, rng: &mut Rng) {
    let word_count = SENTENCE_WORDS_MIN
        + rng.bounded((SENTENCE_WORDS_MAX - SENTENCE_WORDS_MIN + 1) as u64) as usize;
    for i in 0..word_count {
        if i > 0 {
            buf.push(b' ');
        }
        let idx = rng.bounded(WORDS.len() as u64) as usize;
        append_word(buf, idx, i == 0);
    }
    buf.push(sentence_punct(rng));
}

fn generate_words(rng: &mut Rng, count: usize) -> Vec<u8> {
    let mut buf = Vec::with_capacity(count * 8 + 1);
    for i in 0..count {
        if i > 0 {
            buf.push(b' ');
        }
        let idx = rng.bounded(WORDS.len() as u64) as usize;
        append_word(&mut buf, idx, false);
    }
    buf.push(b'\n');
    buf
}

fn generate_sentences(rng: &mut Rng, count: usize) -> Vec<u8> {
    let mut buf = Vec::with_capacity(count * 80 + 1);
    for i in 0..count {
        if i > 0 {
            buf.push(b' ');
        }
        append_sentence(&mut buf, rng);
    }
    buf.push(b'\n');
    buf
}

fn generate_paragraphs(rng: &mut Rng, count: usize) -> Vec<u8> {
    let mut buf = Vec::with_capacity(count * 400 + 1);
    for p in 0..count {
        if p > 0 {
            buf.extend_from_slice(b"\n\n");
        }
        let sentence_count = PARAGRAPH_SENTENCES_MIN
            + rng.bounded((PARAGRAPH_SENTENCES_MAX - PARAGRAPH_SENTENCES_MIN + 1) as u64) as usize;
        for s in 0..sentence_count {
            if s > 0 {
                buf.push(b' ');
            }
            append_sentence(&mut buf, rng);
        }
    }
    buf.push(b'\n');
    buf
}

fn generate_size(rng: &mut Rng, target: usize) -> Vec<u8> {
    let mut buf = Vec::with_capacity(target + 1);
    if target == 0 {
        return buf;
    }
    while buf.len() + 2 < target {
        let idx = rng.bounded(WORDS.len() as u64) as usize;
        let add = WORDS[idx].len() + if buf.is_empty() { 0 } else { 1 };
        if buf.len() + add + 1 > target {
            break;
        }
        if !buf.is_empty() {
            buf.push(b' ');
        }
        append_word(&mut buf, idx, false);
    }
    if buf.last() == Some(&b' ') {
        buf.pop();
    }
    if !buf.is_empty() && buf.len() < target {
        buf.push(b'\n');
    }
    buf
}

// ---------------------------------------------------------------------------
// Markdown generation
// ---------------------------------------------------------------------------

fn append_inline_format(buf: &mut Vec<u8>, rng: &mut Rng) {
    let roll = rng.bounded(100);
    let wrap = if roll < 20 {
        "**"
    } else if roll < 40 {
        "_"
    } else if roll < 55 {
        "`"
    } else {
        ""
    };
    if !wrap.is_empty() {
        buf.extend_from_slice(wrap.as_bytes());
    }
    let idx = rng.bounded(WORDS.len() as u64) as usize;
    append_word(buf, idx, false);
    if !wrap.is_empty() {
        buf.extend_from_slice(wrap.as_bytes());
    }
}

fn append_md_paragraph(buf: &mut Vec<u8>, rng: &mut Rng) {
    let words = SENTENCE_WORDS_MIN + rng.bounded((20 - SENTENCE_WORDS_MIN + 1) as u64) as usize;
    for i in 0..words {
        if i > 0 {
            buf.push(b' ');
        }
        if rng.bounded(100) < 30 {
            append_inline_format(buf, rng);
        } else {
            let idx = rng.bounded(WORDS.len() as u64) as usize;
            append_word(buf, idx, false);
        }
    }
    buf.push(b'\n');
}

fn append_md_heading(buf: &mut Vec<u8>, rng: &mut Rng) {
    let level = 1 + rng.bounded(6) as usize;
    for _ in 0..level {
        buf.push(b'#')
    }
    buf.push(b' ');
    let words = 2 + rng.bounded(5) as usize;
    for i in 0..words {
        if i > 0 {
            buf.push(b' ');
        }
        let idx = rng.bounded(WORDS.len() as u64) as usize;
        append_word(buf, idx, i == 0);
    }
    buf.push(b'\n');
}

fn append_md_unordered_list(buf: &mut Vec<u8>, rng: &mut Rng) {
    let items = 2 + rng.bounded(4) as usize;
    for _ in 0..items {
        buf.extend_from_slice(b"- ");
        let words = 2 + rng.bounded(6) as usize;
        for j in 0..words {
            if j > 0 {
                buf.push(b' ');
            }
            let idx = rng.bounded(WORDS.len() as u64) as usize;
            append_word(buf, idx, false);
        }
        buf.push(b'\n');
    }
}

fn append_md_ordered_list(buf: &mut Vec<u8>, rng: &mut Rng) {
    let items = 2 + rng.bounded(4) as usize;
    for _ in 0..items {
        buf.extend_from_slice(b"1. ");
        let words = 2 + rng.bounded(6) as usize;
        for j in 0..words {
            if j > 0 {
                buf.push(b' ');
            }
            let idx = rng.bounded(WORDS.len() as u64) as usize;
            append_word(buf, idx, false);
        }
        buf.push(b'\n');
    }
}

fn append_md_code_block(buf: &mut Vec<u8>, rng: &mut Rng) {
    const LANGS: &[&str] = &["c", "python", "js", "sh", "text"];
    let lang_idx = rng.bounded(LANGS.len() as u64) as usize;
    buf.extend_from_slice(b"```");
    buf.extend_from_slice(LANGS[lang_idx].as_bytes());
    buf.push(b'\n');
    let lines = 2 + rng.bounded(5) as usize;
    for _ in 0..lines {
        buf.extend_from_slice(b"    ");
        let words = 2 + rng.bounded(5) as usize;
        for j in 0..words {
            if j > 0 {
                buf.push(b' ');
            }
            let idx = rng.bounded(WORDS.len() as u64) as usize;
            append_word(buf, idx, false);
        }
        buf.push(b'\n');
    }
    buf.extend_from_slice(b"```\n");
}

fn append_md_table(buf: &mut Vec<u8>, rng: &mut Rng) {
    let cols = 2 + rng.bounded(3) as usize;
    let rows = 2 + rng.bounded(4) as usize;

    for c in 0..cols {
        if c == 0 {
            buf.push(b'|');
        }
        let idx = rng.bounded(WORDS.len() as u64) as usize;
        buf.push(b' ');
        append_word(buf, idx, false);
        buf.extend_from_slice(b" |");
    }
    buf.push(b'\n');

    for c in 0..cols {
        if c == 0 {
            buf.push(b'|');
        }
        buf.extend_from_slice(b" --- |");
    }
    buf.push(b'\n');

    for _ in 0..rows {
        for c in 0..cols {
            if c == 0 {
                buf.push(b'|');
            }
            let idx = rng.bounded(WORDS.len() as u64) as usize;
            buf.push(b' ');
            append_word(buf, idx, false);
            buf.extend_from_slice(b" |");
        }
        buf.push(b'\n');
    }
}

fn append_md_blockquote(buf: &mut Vec<u8>, rng: &mut Rng) {
    let lines = 1 + rng.bounded(3) as usize;
    for _ in 0..lines {
        buf.extend_from_slice(b"> ");
        let words = 2 + rng.bounded(6) as usize;
        for j in 0..words {
            if j > 0 {
                buf.push(b' ');
            }
            let idx = rng.bounded(WORDS.len() as u64) as usize;
            append_word(buf, idx, false);
        }
        buf.push(b'\n');
    }
}

fn append_md_section(buf: &mut Vec<u8>, rng: &mut Rng) {
    let roll = rng.bounded(100);
    if roll < 25 {
        append_md_heading(buf, rng);
    } else if roll < 50 {
        append_md_unordered_list(buf, rng);
    } else if roll < 65 {
        append_md_ordered_list(buf, rng);
    } else if roll < 80 {
        append_md_code_block(buf, rng);
    } else if roll < 90 {
        append_md_table(buf, rng);
    } else if roll < 96 {
        append_md_blockquote(buf, rng);
    } else {
        append_md_paragraph(buf, rng);
    }
}

fn generate_markdown(rng: &mut Rng, count: usize) -> Vec<u8> {
    let mut buf = Vec::with_capacity(count * 600 + 256);
    buf.push(b'#');
    buf.push(b' ');
    let title_words = 2 + rng.bounded(5) as usize;
    for i in 0..title_words {
        if i > 0 {
            buf.push(b' ');
        }
        let idx = rng.bounded(WORDS.len() as u64) as usize;
        append_word(&mut buf, idx, i == 0);
    }
    buf.extend_from_slice(b"\n\n");
    for i in 0..count {
        if i > 0 {
            buf.push(b'\n');
        }
        append_md_section(&mut buf, rng);
    }
    buf
}

// ---------------------------------------------------------------------------
// Public generation dispatch
// ---------------------------------------------------------------------------

fn generate(opts: &GenOptions) -> Vec<u8> {
    let seed = if opts.seed == 0 { 1 } else { opts.seed };
    let mut rng = Rng { state: seed };
    match opts.mode {
        GenMode::Words => generate_words(&mut rng, opts.count),
        GenMode::Sentences => generate_sentences(&mut rng, opts.count),
        GenMode::Paragraphs => generate_paragraphs(&mut rng, opts.count),
        GenMode::Size => generate_size(&mut rng, opts.target_size),
        GenMode::Markdown => generate_markdown(&mut rng, opts.count),
    }
}

fn compress(data: &mut Vec<u8>) {
    if data.is_empty() {
        return;
    }
    let mut j = 0;
    let mut in_space = true;
    for i in 0..data.len() {
        let c = data[i];
        let is_space = c == b' ' || c == b'\t' || c == b'\n' || c == b'\r';
        if is_space {
            if !in_space {
                data[j] = b' ';
                j += 1;
                in_space = true;
            }
        } else {
            data[j] = c;
            j += 1;
            in_space = false;
        }
    }
    if j > 0 && data[j - 1] == b' ' {
        j -= 1;
    }
    data[j] = b'\n';
    j += 1;
    data.truncate(j);
    data.shrink_to_fit();
}

// ---------------------------------------------------------------------------
// CLI parsing and I/O
// ---------------------------------------------------------------------------

fn usage() -> &'static str {
    "Usage: ranfile [OPTIONS]\n\
     \n\
     Generate random lorem-style text.\n\
     \n\
     Options:\n\
       -w, --words N         Generate N words\n\
       -s, --sentences N     Generate N sentences\n\
       -p, --paragraphs N    Generate N paragraphs\n\
           --size SIZE       Generate text to reach SIZE bytes (B, MB, GB)\n\
           --markdown N      Generate N random Markdown sections\n\
       -c, --compress        Collapse output to a single line\n\
       -y, --no-confirm      Skip the large-file confirmation prompt\n\
       -o, --output FILE     Write output to FILE (default: stdout)\n\
           --seed U          64-bit seed for reproducible output\n\
       -h, --help            Show this help message\n\
     \n\
     Exactly one of -w, -s, -p, --size, or --markdown is required.\n"
}

fn parse_u64(s: &str) -> Option<u64> {
    s.parse().ok()
}

fn parse_count(s: &str) -> Option<usize> {
    parse_u64(s).and_then(|v| if v > 0 { Some(v as usize) } else { None })
}

fn parse_md_count(s: &str) -> Option<usize> {
    parse_u64(s).map(|v| v as usize)
}

fn parse_data_size(s: &str) -> Option<usize> {
    let s = s.trim();
    let mut num_end = 0;
    let mut has_dot = false;
    for (i, c) in s.char_indices() {
        if c.is_ascii_digit() {
            num_end = i + c.len_utf8();
        } else if c == '.' && !has_dot {
            has_dot = true;
            num_end = i + 1;
        } else {
            break;
        }
    }
    if num_end == 0 {
        return None;
    }
    let value: f64 = s[..num_end].parse().ok()?;
    if value <= 0.0 {
        return None;
    }
    let unit = s[num_end..].trim();
    let mult: f64 = match unit.to_ascii_lowercase().as_str() {
        "b" => 1.0,
        "mb" => 1024.0 * 1024.0,
        "gb" => 1024.0 * 1024.0 * 1024.0,
        _ => return None,
    };
    let bytes = value * mult;
    if bytes > usize::MAX as f64 {
        return None;
    }
    Some(bytes as usize)
}

fn default_seed() -> u64 {
    if let Ok(mut f) = File::open("/dev/urandom") {
        let mut buf = [0u8; 8];
        if f.read_exact(&mut buf).is_ok() {
            let seed = u64::from_ne_bytes(buf);
            if seed != 0 {
                return seed;
            }
        }
    }
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(1)
}

fn confirm_large_file(bytes: usize) -> bool {
    eprint!(
        "You are about to generate {:.2} GB of data. Continue? [y/N] ",
        bytes as f64 / (1024.0 * 1024.0 * 1024.0)
    );
    let _ = io::stderr().flush();
    let mut line = String::new();
    if io::stdin().read_line(&mut line).is_err() {
        return false;
    }
    matches!(line.chars().next(), Some('y' | 'Y'))
}

struct CliConfig {
    mode: GenMode,
    count: usize,
    target_size: usize,
    output: Option<String>,
    seed: Option<u64>,
    compress: bool,
    no_confirm: bool,
}

fn mode_err() -> String {
    "specify only one of -w, -s, -p, --size, or --markdown".to_string()
}

fn take_value<'a>(
    arg: &'a str,
    args: &'a [String],
    i: &mut usize,
    short: &str,
    long: &str,
) -> Option<&'a str> {
    if arg == short || arg == long {
        *i += 1;
        args.get(*i).map(|s| s.as_str())
    } else if arg.starts_with(short) && arg.len() > short.len() {
        Some(&arg[short.len()..])
    } else if let Some(rest) = arg.strip_prefix(&format!("{}=", long)) {
        Some(rest)
    } else {
        None
    }
}

fn parse_cli_args(args: &[String]) -> Result<CliConfig, String> {
    let mut mode: Option<GenMode> = None;
    let mut count: usize = 0;
    let mut target_size: usize = 0;
    let mut output: Option<String> = None;
    let mut seed: Option<u64> = None;
    let mut compress = false;
    let mut no_confirm = false;

    let mut i = 0;
    while i < args.len() {
        let arg = &args[i];

        if arg == "-h" || arg == "--help" {
            print!("{}", usage());
            process::exit(0);
        } else if arg == "-c" || arg == "--compress" {
            compress = true;
        } else if arg == "-y" || arg == "--no-confirm" {
            no_confirm = true;
        } else if let Some(val) = take_value(arg, args, &mut i, "-w", "--words") {
            if mode.is_some() {
                return Err(mode_err());
            }
            count = parse_count(val).ok_or_else(|| format!("invalid word count '{}'", val))?;
            mode = Some(GenMode::Words);
        } else if let Some(val) = take_value(arg, args, &mut i, "-s", "--sentences") {
            if mode.is_some() {
                return Err(mode_err());
            }
            count = parse_count(val).ok_or_else(|| format!("invalid sentence count '{}'", val))?;
            mode = Some(GenMode::Sentences);
        } else if let Some(val) = take_value(arg, args, &mut i, "-p", "--paragraphs") {
            if mode.is_some() {
                return Err(mode_err());
            }
            count = parse_count(val).ok_or_else(|| format!("invalid paragraph count '{}'", val))?;
            mode = Some(GenMode::Paragraphs);
        } else if let Some(val) = take_value(arg, args, &mut i, "--size", "--size") {
            if mode.is_some() {
                return Err(mode_err());
            }
            target_size = parse_data_size(val).ok_or_else(|| format!("invalid size '{}'", val))?;
            mode = Some(GenMode::Size);
        } else if let Some(val) = take_value(arg, args, &mut i, "--markdown", "--markdown") {
            if mode.is_some() {
                return Err(mode_err());
            }
            count = parse_md_count(val).ok_or_else(|| format!("invalid markdown section count '{}'", val))?;
            mode = Some(GenMode::Markdown);
        } else if let Some(val) = take_value(arg, args, &mut i, "-o", "--output") {
            output = Some(val.to_string());
        } else if let Some(val) = take_value(arg, args, &mut i, "--seed", "--seed") {
            seed = Some(parse_u64(val).ok_or_else(|| format!("invalid seed '{}'", val))?);
        } else {
            return Err(format!("unknown option '{}'", arg));
        }

        i += 1;
    }

    let mode = mode.ok_or_else(|| {
        eprintln!("ranfile: one of -w, -s, -p, --size, or --markdown is required");
        eprint!("{}", usage());
        "no mode specified".to_string()
    })?;

    Ok(CliConfig {
        mode,
        count,
        target_size,
        output,
        seed,
        compress,
        no_confirm,
    })
}

fn run() -> i32 {
    let args: Vec<String> = env::args().skip(1).collect();
    let config = match parse_cli_args(&args) {
        Ok(c) => c,
        Err(msg) => {
            eprintln!("ranfile: {}", msg);
            return 2;
        }
    };

    let seed = config.seed.unwrap_or_else(default_seed);
    let opts = GenOptions {
        mode: config.mode,
        count: config.count,
        seed,
        target_size: config.target_size,
    };

    if opts.mode == GenMode::Size && opts.target_size >= SIZE_GB_THRESHOLD && !config.no_confirm {
        if !confirm_large_file(opts.target_size) {
            eprintln!("ranfile: aborted");
            return 2;
        }
    }

    let mut data = generate(&opts);
    if config.compress {
        compress(&mut data);
    }

    if let Some(path) = config.output {
        match File::create(&path) {
            Ok(mut f) => {
                if let Err(e) = f.write_all(&data) {
                    eprintln!("ranfile: cannot write '{}': {}", path, e);
                    return 1;
                }
                if let Err(e) = f.flush() {
                    eprintln!("ranfile: close error on '{}': {}", path, e);
                    return 1;
                }
            }
            Err(e) => {
                eprintln!("ranfile: cannot open '{}': {}", path, e);
                return 1;
            }
        }
    } else if let Err(e) = io::stdout().write_all(&data) {
        eprintln!("ranfile: write error on stdout: {}", e);
        return 1;
    }

    0
}

fn main() {
    process::exit(run());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    fn rng(seed: u64) -> Rng {
        Rng { state: seed }
    }

    #[test]
    fn test_words_count() {
        let data = generate_words(&mut rng(1), 10);
        let text = String::from_utf8(data).unwrap();
        assert_eq!(text.trim().split_whitespace().count(), 10);
    }

    #[test]
    fn test_sentences_count() {
        let data = generate_sentences(&mut rng(1), 5);
        let text = String::from_utf8(data).unwrap();
        let count = text
            .split_whitespace()
            .filter(|s| s.ends_with('.') || s.ends_with('?') || s.ends_with('!'))
            .count();
        assert_eq!(count, 5);
    }

    #[test]
    fn test_paragraphs_count() {
        let data = generate_paragraphs(&mut rng(1), 3);
        let text = String::from_utf8(data).unwrap();
        let paras = text.trim().split("\n\n").count();
        assert_eq!(paras, 3);
    }

    #[test]
    fn test_seed_reproducibility() {
        let a = generate_paragraphs(&mut rng(12345), 4);
        let b = generate_paragraphs(&mut rng(12345), 4);
        assert_eq!(a, b);
    }

    #[test]
    fn test_size_parsing() {
        assert_eq!(parse_data_size("10B"), Some(10));
        assert_eq!(
            parse_data_size("2.5MB"),
            Some((2.5 * 1024.0 * 1024.0) as usize)
        );
        assert_eq!(parse_data_size("1GB"), Some(1024 * 1024 * 1024));
        assert_eq!(parse_data_size(" 1 gb "), Some(1024 * 1024 * 1024));
    }

    #[test]
    fn test_size_parsing_invalid() {
        assert!(parse_data_size("10").is_none());
        assert!(parse_data_size("10KB").is_none());
        assert!(parse_data_size("-5MB").is_none());
        assert!(parse_data_size("0B").is_none());
        assert!(parse_data_size("abc").is_none());
    }

    #[test]
    fn test_size_output_bounds() {
        let data = generate_size(&mut rng(8), 200);
        assert!(data.len() <= 200);
        assert!(data.ends_with(b"\n"));
    }

    #[test]
    fn test_compress_collapse() {
        let mut data = generate_paragraphs(&mut rng(1), 3);
        compress(&mut data);
        let text = String::from_utf8(data.clone()).unwrap();
        let stripped = text.trim_end_matches('\n');
        assert!(!stripped.contains('\n'));
        assert!(!stripped.contains("  "));
        assert!(data.ends_with(b"\n"));
    }

    #[test]
    fn test_markdown_starts_with_title() {
        let data = generate_markdown(&mut rng(1), 5);
        assert!(data.starts_with(b"# "));
    }

    #[test]
    fn test_markdown_elements() {
        let data = generate_markdown(&mut rng(99), 300);
        let text = String::from_utf8(data).unwrap();
        assert!(text.starts_with("# "));
        assert!(text.contains("##") || text.contains("###"));
        assert!(text.contains("- ") || text.contains("1."));
        assert!(text.contains("```"));
        assert!(text.contains("|"));
        assert!(text.contains("> "));
    }

    #[test]
    fn test_markdown_seed_reproducibility() {
        let a = generate_markdown(&mut rng(4242), 10);
        let b = generate_markdown(&mut rng(4242), 10);
        assert_eq!(a, b);
    }

    #[test]
    fn test_markdown_zero() {
        let data = generate_markdown(&mut rng(3), 0);
        let text = String::from_utf8(data).unwrap();
        assert!(text.starts_with("# "));
        assert_eq!(text.matches("\n\n").count(), 1);
    }

    #[test]
    fn test_parse_count_positive() {
        assert_eq!(parse_count("5"), Some(5));
        assert!(parse_count("0").is_none());
        assert!(parse_count("abc").is_none());
    }

    #[test]
    fn test_parse_md_count_zero_allowed() {
        assert_eq!(parse_md_count("0"), Some(0));
        assert_eq!(parse_md_count("5"), Some(5));
    }
}
