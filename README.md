# kaikai

A **functional**, **statically typed** programming language that compiles to
**native code** via LLVM.

- **Algebraic effects** as a first-class primitive (Effekt-style + inference).
- **Elixir-style pipelines**.
- **Memory** without a global GC or borrow checker: Perceus (compile-time
  optimised RC) + isolated fibers (BEAM-style).
- **Portable bootstrap**: 3-stage compiler (C → kaikai-minimal → full kaikai).

## Status

MVP in progress. Phase 1–3 landed the kaikai-minimal language and a
self-hosting compiler (`stage1/kaic1`); phase 4 adds a small stdlib and
the `kai` driver so programs can be built with a single command. LLVM
backend, effect inference, fibers, and tooling like `kai fmt` / `kai
repl` / `kai lsp` are post-MVP — see `docs/design.md` for the full roadmap.

## Build

Everything builds from the repo root with `make`:

```sh
make all       # stage 0 (C), stage 1 (kaikai-minimal), bin/kai
make test      # runs stage 0, stage 1, and phase 4 demo suites
make selfhost  # proves kaic1 compiled by kaic1 is a fixed point
```

On a fresh checkout, only a C compiler is required:

```sh
cc stage0/*.c -o stage0/kaic0
./stage0/kaic0 stage1/compiler.kai > /tmp/stage1.c
cc /tmp/stage1.c -I stage0 -o stage1/kaic1
bin/kai run examples/phase4/hello.kai
```

## Usage

The `bin/kai` driver wraps `kaic1` + `cc`. Run a program:

```sh
kai run examples/phase4/collatz.kai
#  longest collatz in 1..100 is n=97 with length 119
```

Build to a standalone native binary:

```sh
kai build examples/phase4/euler1.kai -o euler1
./euler1
#  sum = 233168
```

Run the inline test blocks in a file:

```sh
kai test examples/phase4/factorials.kai
#    ok   factorial base cases
#    ok   factorial small values
#
#  2/2 tests passed
```

The driver auto-builds `stage0/kaic0` and `stage1/kaic1` on first use
and prepends `stdlib/core.kai` to every compilation (set
`KAI_NO_STDLIB=1` to turn this off).

## Layout

```
stage0/          C bootstrap compiler for kaikai-minimal.
stage1/          kaikai-minimal compiler (self-hosted).
stdlib/          Core stdlib in kaikai-minimal (List/String/Option/...).
bin/             Shell driver (`kai build/run/test`).
examples/minimal/  Canonical minimal examples used for regression.
examples/phase4/   Small demos against stdlib.
demos/           Pre-redesign sketches (do not compile today).
docs/            Design docs and specs.
tests/           Reserved for future .kai-level test suites.
runtime/         Reserved for future stage 2 runtime (Perceus, fibers).
```

## License

To be defined.
