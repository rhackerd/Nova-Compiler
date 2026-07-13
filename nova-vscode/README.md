# Nova Language — VSCode Extension

Syntax highlighting for the Nova programming language.

## Supported file extensions
- `.nl` — Nova source files
- `.nh` — Nova header files

## Features
- Keywords: `func`, `ret`, `let`, `mut`, `ext`
- Types: `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`, `i64`, `f32`, `f64`
- Function definitions and calls highlighted separately
- Line comments (`//`)
- String literals with escape sequences
- Numeric literals (integer, float, hex)
- Operators: `->`, arithmetic, comparison, assignment
- Auto-closing brackets and quotes

## Installation (development)

1. Copy this folder to `~/.vscode/extensions/nova-lang`
2. Restart VSCode
3. Open any `.nl` file — syntax highlighting applies automatically

## Manual install without vsce

```bash
cp -r nova-vscode ~/.vscode/extensions/nova-lang
```

No build step required — this is a pure grammar extension.
