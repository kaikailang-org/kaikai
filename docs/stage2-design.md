# stage 2 design

Architectural decisions for the **definitive compiler** — the one written in
full kaikai, compiled by stage 1, targeting LLVM directly, with effects,
fibers, and all the things the MVP promised.

Stage 2 is the end state. Stage 0 is throwaway; stage 1 is transitional;
stage 2 is what the rest of the project lives on.

## Relationship to stages 0 and 1

- Stage 0 (`kaic0`) keeps existing as the C-only ingress. It is the only
  binary a user needs to install from source.
- Stage 1 (`kaic1`) keeps existing as the kaikai-minimal compiler and
  serves as the **bootstrap target** for stage 2 — the first time stage
  2 is compiled, it is by stage 1, not by itself.
- Stage 2 (`kaic2`) is the final compiler. Once it compiles itself,
  stage 1 is retired for dev work (kept in the repo for reproducible
  bootstraps from C, but not maintained).

## Non-goals

- **Backwards compatibility with stage 1's output**. Stage 2 may emit
  wildly different C/LLVM IR; what matters is that programs run.
- **Full Haskell-level type inference polymorphism**. Stage 2 sticks to
  the plan: decidable, predictable, fast. Principal types where
  possible, explicit annotations at module boundaries.
- **Dependent types, refinement types, linear types as the core**.
  Affine types are internal (Perceus needs linearity analysis); they
  do not surface to the user yet.
- **Package manager**. Separate project, explicitly post-stage-2.

## What stage 2 adds over stage 1

Grouped by subsystem:

### 1. LLVM backend

- Emit **textual LLVM IR** (`.ll`) as the canonical output.
  `kaic2 foo.kai -o foo.ll`; `kaic2 foo.kai -o foo` links via
  `llc` + `clang`.
- A thin C fallback (`--emit=c`) kept for bootstrapping from stage 1
  and for platforms where LLVM is absent.
- Target triples: `x86_64-linux-gnu`, `aarch64-apple-darwin`, and
  `aarch64-linux-gnu` for the MVP+1 set. WASM and Windows stay
  post-stage-2.

### 2. Full Perceus memory management

- **Reuse analysis**: the same reuse-specialised Koka paper, applied
  as a separate pass over the typed IR. In-place update when the
  compiler can prove no other live reference exists.
- **Drop specialisation**: decref chains generated per type, unboxed
  inline instead of going through a dispatch table.
- **Unboxing** of `Int`, `Real`, `Bool`, `Char` into native machine
  registers inside each fiber. Heap boxing only for compound
  immutable values. Messages across fiber boundaries copy.
- **Region regions** for specific cases where RC is demonstrably
  worse (intermediate parser buffers, say) — opt-in via attribute.

### 3. Effects system — full

Inherits the skeleton from stage 1 and fills it in.

- **Capability-passing Effekt + inference**. Effects are first-class
  rows on function types; the checker infers them in private
  bodies and requires them on public signatures.
- `perform Effect.op(args)` does the call; `handle { body } with {
  op(x, k) -> ... }` installs a handler; `resume(v)` continues.
- The one-shot/multi-shot distinction is a runtime property, not a
  type: continuations are represented by a stack segment pointer,
  multi-shot pays copy cost on `resume`, one-shot is free.
- Coarse-grained polymorphism over effect rows: `map[A, B, e](xs:
  [A], f: (A) -> B / e) : [B] / e`.
- **Actor capability** is a handler in the stdlib, not a language
  built-in: `Actor` defines `spawn`, `send`, `receive`; the default
  scheduler provides its implementation.

### 4. Fibers + scheduler

- Every fiber owns a private, movable stack segment (CPS or
  segmented-stack representation — picked at implementation time).
- Per-fiber heap (Perceus RC-scoped); messages are **deep-copied**
  across fiber boundaries and there is no aliasing between fibers.
  Matches BEAM's isolation guarantee.
- Cooperative scheduler in the runtime. Pre-emption via effect-op
  check-points (every `perform` is a pre-emption point).
- `Fiber[T]` is a capability tagged with the region brand of the
  enclosing nursery (see structured concurrency below); it cannot
  escape its scope.

### 5. Structured concurrency

Design already adopted in `docs/structured-concurrency.md`. Stage 2
is where it lands.

- `nursery (n) => { ... }` as a macro-like expansion to `handle Spawn
  with (n) => { ... }`.
- `Spawn`, `Cancel` effects defined in the stdlib.
- Region-branding of `Fiber[T]` lives in the type checker.

### 6. Typed holes

Design already adopted in `docs/typed-holes.md`. Stage 2 is where it
lands.

- `?` and `?name` as first-class expressions/patterns.
- Reports emitted as diagnostics and as `--holes-json`.
- Synthesis bounded to one function-application level; everything
  deeper is the job of the LLM on the other side of the JSON.

### 7. Monomorphised generics

- Every call site of a generic function picks its instantiation; the
  emitter generates one specialised copy per distinct instantiation.
- Stage 1's uniform-boxing approach is retired here. Registers are
  typed; generics are specialised.
- Instantiation keys share structure across compilation (cache),
  keeping binary size reasonable.

### 8. Diagnostics at Elm/Rust quality

Commitment, not a feature: every diagnostic in stage 2 must

- Name the expected type vs the actual type with a visual diff when
  they disagree structurally.
- Suggest a concrete fix where possible ("did you mean `list_map`?",
  "wrap this in `Some(...)`?").
- Point at the root cause, not the symptom; unify propagation
  failures walk back to the origin.
- Include a one-line "why" that explains the rule violated, not the
  mechanism.

This is enforced by treating each new diagnostic as a design item
that lands with its message text, not a TODO.

### 9. Tooling

- `kai fmt`: canonical formatter. Single style, no options. LLM-
  friendly deterministic output.
- `kai repl`: online session with module reload and `?`-hole
  completion.
- `kai lsp`: LSP server talking diagnostics + hover + completion +
  go-to-definition. `--holes-json` doubles as completion source.
- `kai doc`: extract `pub` signatures and doc comments; emit HTML.
- `kai bench`: alongside `kai test`; reuses the test syntax but
  measures time.

## Compilation pipeline

```
.kai source
  → lex        (shared with stage 1 — subset-compatible)
  → parse      (extended grammar: effects, handlers, nursery, holes)
  → resolve    (module-aware: imports become edges)
  → infer      (HM-extended with effect rows, region branding)
  → monomorph  (instantiate generics, specialise drops)
  → perceus    (reuse analysis, insert incref/decref)
  → lower      (typed IR → LLVM IR or C)
  → link       (ld / clang wrapper)
```

Each pass is a pure function of the previous AST/IR. A debug dump
between any two passes is writable via `--dump=<pass>`.

## Module system

Phase 4's single-file `--prelude` retires in favour of real imports:

```kai
import math.vector
import math.vector as V
import math.vector.{dot, cross}
```

- Module resolution: a module name `a.b.c` maps to `a/b/c.kai` under
  the project root (or the standard library root).
- Dependency graph is built; compilation is topologically sorted.
  Circular imports are a hard error, explained with a suggestion.
- Each module is compiled to its own translation unit in the output
  (either a `.c` or a `.ll`); the linker glues them.
- Stdlib shipped as a compiled archive plus sources for hacking.

## Test and bench

- `test "..."` — exactly as in stage 1.
- `check "..." with a: Int, b: [Int]` — property test; the runner
  generates random inputs, shrinks failures. The generator is
  derivable for any type whose declarations allow it (all sum/record
  types do by default).
- `bench "..."` — same syntax as test, but the runner times the body
  and reports ns/iter + outlier-robust stats.
- `kai test`, `kai check`, `kai bench` — three subcommands; `kai
  test` runs `test` blocks, `kai check` runs `check` blocks, `kai
  bench` runs `bench` blocks.

## FFI

- `extern "C" fn name(args...) : T / Ffi` — declaration.
- Calling an extern is an op of `Ffi`. Pure kaikai code cannot
  reach it without a handler (in practice, `main` installs a trivial
  pass-through handler at program start; libraries opt in).
- `kai bindgen foo.h` — reads a C header, emits an `extern` module
  against the project's conventions. Post-stage-2 deliverable but
  keep the door open.

## Bootstrapping

```sh
# fresh machine, only cc
cc stage0/*.c -o kaic0                         # existing
./kaic0 stage1/compiler.kai > /tmp/s1.c
cc /tmp/s1.c -I stage0 -o kaic1                # existing

# stage 2 comes online
./kaic1 stage2/compiler.kai > /tmp/s2.c        # first cross-compile
cc /tmp/s2.c -I stage0 -o kaic2
./kaic2 stage2/compiler.kai > /tmp/s2b.c       # self-compile
diff /tmp/s2.c /tmp/s2b.c                      # optional: fixed-point sanity

# then kaic2 becomes the compiler for everything
./kaic2 demos/my_actor_system.kai -o actor-sys
./actor-sys
```

Once stage 2 is compiled with itself and produces a fixed point,
stage 1 is frozen. Any language-level change lands only in stage 2.

## Milestones within stage 2

High level — each item will get its own sub-design-doc when it comes
up.

1. **Stage 2 skeleton**: `stage2/compiler.kai`, CLI, file IO, minimal
   pipeline that calls stage 1's existing pieces (lex + parse +
   check) and emits the same C stage 1 does. Proves the wiring.
2. **HM-extended type checker**: replace stage 1's name-resolution-
   only check with a real inference pass that returns a typed AST.
3. **LLVM IR backend (no optimisations)**: emit `.ll` that produces
   the same output as the existing C backend. All four minimal
   examples round-trip.
4. **Monomorphisation**: retire uniform boxing; emit specialised
   functions per generic instantiation. Verify perf on
   phase-4-demo workloads.
5. **Basic Perceus**: reuse analysis + drop insertion in the typed
   IR pass. Measure allocation reduction vs stage 1.
6. **Module resolution**: cross-file imports, topological
   compilation, standard library loaded from a search path.
7. **Effects + handlers**: `perform` / `handle` / `resume`, effect
   inference for private bodies, annotation at module boundaries.
8. **Fibers + scheduler**: CPS-transformed emission, cooperative
   runtime, `Actor` effect handler in the stdlib.
9. **Structured concurrency**: `nursery` sugar, `Spawn` / `Cancel`
   effects, region-branding of `Fiber[T]`.
10. **Typed holes**: `?` / `?name` expressions and patterns, text
    and JSON reports.
11. **Diagnostics quality pass**: every error message reviewed,
    rewritten, and tested against an Elm/Rust bar.
12. **Self-hosting checkpoint**: `kaic2 stage2/compiler.kai`
    produces a byte-identical output. Stage 1 retired from the dev
    loop.
13. **Property testing + bench**: `check` and `bench` blocks,
    matching runners.
14. **Stdlib expansion**: stage-2-native stdlib, module-organised.
15. **`kai fmt`** using the stage 2 parser.
16. **`kai lsp`** using the stage 2 pipeline.
17. **`kai repl`** using the stage 2 pipeline + holes.

Order is indicative; some items can land in parallel once 1–4 are
in.

## What stage 2 deliberately does not ship

- Gradual typing, dependent types, refinement types.
- A macro system beyond `nursery`-style sugar that desugars in the
  parser.
- Garbage collection as the default memory strategy.
- Thread-level parallelism that is not fiber-based.
- Distributed runtime. Remote actors are a post-stage-2 research
  project, if at all.
- Self-hosted LLVM backend in pure kaikai; we use the C API from
  kaikai via `Ffi`, same as every other compiler.

The list exists so every new feature request has a clear "yes/no/
later" anchor.
