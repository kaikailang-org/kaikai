# stage 1

Self-hosting compiler for kaikai, written in kaikai-minimal. Compiled by
stage 0 (`../stage0/kaic0`), emits portable C the same way stage 0 does.

## Build

```sh
cd stage1
make
```

The Makefile runs `../stage0/kaic0 compiler.kai > build/stage1.c` and then
`cc -I ../stage0 build/stage1.c -o kaic1`.

## Use

```sh
./kaic1 path/to/file.kai > out.c
cc out.c -I ../stage0 -o out
./out
```

Flags mirror stage 0: `--tokens`, `--ast`, `--test`, `-h/--help`.

## Status

Under construction. See `docs/stage1-design.md` for the milestones and
`docs/kaikai-minimal.md` for the subset the compiler is written in.

Current milestone: **skeleton** (CLI, file IO, driver). The compilation
pipeline is a placeholder; subsequent milestones land the lexer, parser,
checker, and emitter one step at a time.
