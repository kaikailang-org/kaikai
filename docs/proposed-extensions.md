# proposed extensions

Catalogue of extensions for kaikai, grouped into two families:

1. **LLM-friendly diagnostics** — extensions in the same family as
   `docs/typed-holes.md`: the compiler emits structured,
   machine-consumable information that an LLM (or LSP) can act on.
2. **Language-surface features** — additions to the core language
   itself. These are candidates for *closing* `design.md`'s open
   decision on concrete-syntax consolidation, not for opening new
   ones.

None of these are adopted yet. They are catalogued here so that,
when the core stabilises, we have a short list of coherent moves
rather than a pile of ad-hoc additions.

Each entry states what it buys, what it costs, and what it depends
on.

## Status summary

### LLM-friendly diagnostics family

| Extension                                  | Status   | Depends on              |
|--------------------------------------------|----------|-------------------------|
| `todo!(msg) : T`                           | proposed | typed holes             |
| `kai type <pos> --json`                    | proposed | stage-2 type checker    |
| Counterexample JSON for exhaustiveness     | proposed | match exhaustiveness    |
| `axiom name : T`                           | proposed | stage-2 type checker    |
| `kai effects <target> --json`              | proposed | effect inference        |
| `?e` — effect holes                        | proposed | typed holes + effects   |
| `import ?name` — dependency holes          | proposed | module resolution       |
| `kai lint --json` — canonical-form rules   | proposed | canonical style guide   |

### Language-surface family

| Extension                                  | Status   | Depends on              |
|--------------------------------------------|----------|-------------------------|
| Tuples `(T1, T2, ...)`                     | proposed | syntax consolidation    |
| Record punning `{ x, y }`                  | proposed | parser                  |
| `variants[T]()` builtin                    | proposed | monomorphisation        |
| Sum types with constant attributes         | proposed | parser + resolution     |
| `!` postfix — `Option` / `Result` propagation | proposed (reserved) | `Option` / `Result` in prelude |
| `@` as-pattern in `match`                  | proposed | parser                  |
| `?.` optional chaining                     | proposed | parser + type checker   |
| Bitwise operators (`&`, `~`, `^`, `<<`, `>>`) | deferred | demand-driven        |

## 1. `todo!(msg) : T` — principled unimplemented

```kai
fn parse_expr(tokens: [Token]) : Expr = todo!("pending binary ops")
```

`todo!(msg)` is an expression of any type. Structurally a sibling
of `?`:

- Type-checks as the expected type at its position.
- At runtime, aborts via `kai_prelude_panic("todo: #{msg}")`.
- Reported in `--holes-json` with `"kind": "todo"` and the message.

The distinction with `?` is intent. `?` means *I don't know yet*;
`todo!` means *I know, not yet*. Persists through reformats and is
grep-able, replacing informal `// TODO` comments with a typed marker
that the checker tracks.

**Cost**: low. One new token, reuses the hole runtime.
**Depends on**: typed holes.

## 2. `kai type <pos> --json` — queryable type-at-position

```
$ kai type foo.kai:10:5 --json
{
  "file": "foo.kai", "line": 10, "col": 5,
  "expression": "xs |> filter(. > 0)",
  "type": "[Int]",
  "effects": [],
  "in_scope": [
    { "name": "xs", "type": "[Int]" }
  ]
}
```

Exposes what the checker already computes at every AST node. Covers
any position, not just holes. The schema deliberately overlaps with
`--holes-json` so tools learn one format.

**Cost**: low. The checker annotates every node with its inferred
type; the query walks to the cursor position.
**Depends on**: stage-2 type checker.

## 3. Counterexample JSON for exhaustiveness

When a `match` is non-exhaustive, the compiler already knows which
patterns are missing. Expose them:

```json
{
  "kind": "non_exhaustive_match",
  "at": "foo.kai:14:3",
  "counterexamples": [
    { "pattern": "Rect(0.0, _)", "reason": "not covered" },
    { "pattern": "Triangle(_, _, _)", "reason": "not covered" }
  ],
  "suggested_arms": [
    "Rect(0.0, h) -> ?",
    "Triangle(a, b, c) -> ?"
  ]
}
```

The suggested arms contain `?` holes, so the LLM can paste the
completion and receive hole reports for the bodies. This closes
the loop: error → concrete fix with holes → LLM fills → compile.

**Cost**: medium. The match-check already computes missing inhabitants
for its current error text; formalising the output is a refactor
plus a schema.
**Depends on**: pattern-match exhaustiveness check (already planned).

## 4. `axiom name : T` — postulated symbols

```kai
axiom unsafe_cast[A, B] : (A) -> B
axiom db_layer_exists : Database / Io
```

An `axiom` declares a symbol with a type but no body. The compiler
accepts it, type-checks its uses, and lists it in `--axioms-json`.

- Unlike `todo!`, axioms live at top level and can be called from
  anywhere. They declare *this will never have an implementation
  here* (FFI boundary, architectural placeholder, library seam).
- By default, calling an axiom at runtime aborts via
  `kai_prelude_panic`. Axioms can be bound to an external symbol
  via `Ffi` to receive a real implementation at link time.
- Every binary carries a manifest of which axioms it was built
  against, so an auditor (human or LLM) can see the trust surface.

**Cost**: low to medium. One new top-level form, a side list, a
manifest pass.
**Depends on**: stage-2 type checker.

## 5. `kai effects <target> --json` — effect graph as data

```
$ kai effects src/ --json
[
  { "fn": "greet",
    "effects": ["Io"],
    "handled_in": null },
  { "fn": "main",
    "effects": [],
    "handlers_installed": [
      { "effect": "Io", "at": "main.kai:9" }
    ] }
]
```

Summary of which effects each function performs and where handlers
are installed across the call graph. Complements `kai type` for
effect-row-level queries.

**Cost**: low once effect inference is working — it walks the
inferencer's output.
**Depends on**: stage-2 effect inference.

## 6. `?e` — effect holes

```kai
fn run() : Int / ?e {
  perform Io.read_line() |> string_to_int |> unwrap
}
```

The compiler infers `?e` and reports it like a regular hole: `?e`
resolves to `Io + Fail`. Useful when the user does not yet know which
effect row a function belongs to — common during exploratory work
and when an LLM is drafting a signature.

**Cost**: low. Effect rows are already unification variables during
inference; this exposes one as a named hole.
**Depends on**: typed holes + effects.

## 7. `import ?name` — dependency holes

```kai
import ?parse_expr
```

When the user knows the symbol they need but not the module, the
compiler searches the stdlib and project modules and reports
candidates:

```
foo.kai:1:8: import hole

  looking for a module that exports `parse_expr`:
    - syntax.parser           (pub fn parse_expr(...))
    - experimental.pratt      (pub fn parse_expr(...))

  replace `?parse_expr` with one of those paths.
```

**Cost**: low. Needs a symbol index over the loaded modules; the
resolver already visits them.
**Depends on**: module resolution (stage 2).

## 8. `kai lint --json` — canonical style as data

```json
[
  {
    "at": "foo.kai:3:5",
    "kind": "pipe_simplification",
    "message": "this could be a pipe chain",
    "suggestion": "xs |> filter(. > 0) |> map(. * 2)"
  }
]
```

A small, hand-curated set of canonical rewrites emitted by the
compiler — not a pluggable linter. The rules nudge code toward
*few forms, each with clear intent* (the principle that replaces the
retired *one canonical form per construct*). They are the cultural
scaffold for that standard, not an optional plugin.

**Cost**: medium. The rules must be defined and maintained. The
output pipeline is trivial once the rules exist.
**Depends on**: a canonical-form style guide (cultural design work,
not technical).

## 9. Tuples — anonymous products

```kai
pub fn player_turn(hand: [Card], deck: [Card]) : ([Card], [Card]) {
  if hand_score(hand) >= 21 { (hand, deck) }
  else { ... }
}

let (new_hand, new_deck) = player_turn(hand, deck)
```

`kaikai-minimal.md` excludes tuples for the minimal subset ("No
tuples (use records; or use sum-type variants with positional
fields)"). For **full kaikai** the status is open: it falls under
`design.md`'s *"Concrete syntax consolidation: eliminate redundancies
— collections `[]`/`()`/`{}`"*, which is explicitly deferred.

This entry is here so the debate can be closed concretely rather
than by default.

### What they buy

- **Anonymous products** where field names are noise. Pairs like
  `(hand, deck)`, `(key, value)`, `(found, rest)` are semantically
  pairs; inventing two field names is ceremony that obscures intent.
- **Positional destructuring** for small fixed products. List
  patterns already support `[a, b, ...rest]`; tuples extend the same
  idea to finite-arity products.
- **Shorter signatures**. `: ([Card], [Card])` vs
  `: { hand: [Card], deck: [Card] }` saves both the field names in
  the return type and the field-access chatter at every use site.

### What they cost

- **Second form of product type** alongside records. Violates "one
  canonical form" unless justified by a crisp split: tuples for
  anonymous products, records for named ones.
- **Syntactic disambiguation**: `(x)` is grouping. `(x,)` for
  1-tuples (Python-style) is the mandatory wart.
- **Parser bookkeeping**: a parenthesised expression list must be
  distinguished from function-call argument lists. Local cost.

### Decision posture

Two coherent positions:

1. **Reject**: keep records, keep the surface small. Record
   punning (proposal 10 below) covers most of the ergonomic gap
   without adding a second product form.
2. **Accept**: give anonymous products their own form. Reserve
   records for cases where field names carry meaning. This matches
   how kaikai already distinguishes `[...]` (ordered) from records
   (named) — tuples complete the matrix for finite arity.

Neither is obviously right. A concrete measurement: rewriting the
blackjack example (≈176 lines of kaikai) to use tuples where it uses
records for anonymous pairs produces **zero line-count savings** —
every `{ x: x, y: y }` expression already fit on its own line, and
`(x, y)` takes the same line. The gain is per-line readability, not
fewer lines. That is a real but smaller win than one might guess.

Line-count impact may be larger in code that uses many multi-return
helper functions, where `let (a, b) = ...` collapses two
binding-lines into one. Blackjack does not exercise that pattern
heavily.

**Cost**: low-to-medium. Parser changes are local; type-system
extension is one product rule per arity.
**Depends on**: closing *syntax consolidation* in `design.md`.

## 10. Record punning `{ x, y }` — additive sugar

```kai
# Today:
{ hand: hand, deck: deck }

# With punning:
{ hand, deck }
```

When the field name matches the variable in scope, repeating both is
noise. Punning desugars `{ x, y }` to `{ x: x, y: y }`. Not a new
product form; purely a parser-level rewrite. Same shape as the
placeholder `.` lambda (`. < 5` desugars to `(x) => x < 5`).

Applies symmetrically in patterns: `let { hand, deck } = pair`
already works when field names are explicit; punning lets it be
written `let { hand, deck } = pair` without the `: hand` / `: deck`
suffix.

Aligns with all principles — doesn't add a construct, only shortens
a common pattern. Independent of the tuples decision: even if
tuples ship, records still benefit.

**Cost**: trivial. A desugar at parse time.
**Depends on**: nothing.

## 11. `variants[T]()` — enumerate sum-type constructors

```kai
type Rank = Two | Three | ... | Ace

let all_ranks = variants[Rank]()
# [Two, Three, Four, Five, Six, Seven, Eight, Nine, Ten,
#  Jack, Queen, King, Ace]
```

A builtin (not a macro) that, for a sum type `T` with
*nullary* constructors only, returns the list of all constructors.
The compiler has this information after resolution; exposing it is
mechanical.

Restricted to nullary constructors on purpose: once any variant
takes arguments, "list all inhabitants" is ill-defined.

**Cost**: low. A prelude entry, resolved at monomorphisation.
**Depends on**: stage-2 monomorphisation pass.

## 12. Sum types with constant attributes

```kai
type Rank with { value: Int, label: String } {
  Two   = { value: 2,  label: "2" }
  Three = { value: 3,  label: "3" }
  Four  = { value: 4,  label: "4" }
  Five  = { value: 5,  label: "5" }
  Six   = { value: 6,  label: "6" }
  Seven = { value: 7,  label: "7" }
  Eight = { value: 8,  label: "8" }
  Nine  = { value: 9,  label: "9" }
  Ten   = { value: 10, label: "10" }
  Jack  = { value: 10, label: "J" }
  Queen = { value: 10, label: "Q" }
  King  = { value: 10, label: "K" }
  Ace   = { value: 11, label: "A" }
}

# Auto-generated at resolution time, as top-level functions:
#   pub fn value(r: Rank) : Int { match r { ... } }
#   pub fn label(r: Rank) : String { match r { ... } }
```

A sum type whose variants each carry **constant** attribute values
known at declaration. For each attribute field, the compiler
generates a top-level projection function (same name as the field)
by expanding the declaration into a `match`. No dispatch, no search,
no polymorphism — pure desugaring.

### What it buys

- Eliminates the *parallel match* pattern — where a single sum type
  has two or three separate `fn_value`, `fn_label`, `fn_short_code`
  functions, each a full match over the same variants. Those matches
  drift: adding a new variant requires touching N functions. This
  proposal co-locates the info.
- Domain code with enums like HTTP status, currencies, priorities,
  months, locales — any closed set of named constants with a few
  associated attributes — shrinks linearly in the number of attribute
  functions.
- LLM benefit (Tier 3): parallel matches are exactly the kind of
  near-duplicate code LLMs desynchronise. One declaration removes
  the duplication.

### What it costs

- A third form of `type` declaration alongside alias, record, sum,
  and the existing sum-with-payload. Counts against "few visible
  concepts".
- Parser additions: `type X with { fields } { Variant = { ... } }`.
  Local to type declarations; no effect on expressions or patterns.
- The generated functions occupy the top-level namespace. Naming
  collisions with other top-level functions become possible —
  resolvable via module scoping but must be detected.

### Constraints (explicit, to keep this safe)

1. **Variants must remain nullary.** Mixing
   `Foo { ... } | Bar(Int) { ... }` is banned — too much going on in
   one declaration.
2. **Attributes are constants.** No function values, no effects, no
   references to other declarations. A literal goes in; a literal
   comes out.
3. **No uniqueness requirement.** Blackjack needs `Jack = { value:
   10, ... }` and `King = { value: 10, ... }`. Rust-style distinct
   discriminators are *rejected* — they would rule out this case,
   which is common enough to matter.
4. **Accessors are top-level functions, not methods.** `value(r)`
   works; `r.value` does **not** (no dot-access for sum-variant
   attributes — that would step toward methods and implicit
   dispatch).
5. **No typeclass drift.** This extension generates *one* function
   per attribute, at *one* type. No ad-hoc polymorphism, no instance
   resolution. If someone later proposes "extend this to derive
   across types," that is a separate, much bigger proposal.

### Alternative available today

The refactor `fn rank_info(r) -> { value, label }` + two accessors
achieves the same effect in 19 lines vs the 15 lines of the attribute
declaration. For a single enum, savings are modest. The case for
this proposal rests on codebases with many small enums carrying a
couple of constants each — the multiplier is in breadth, not depth.

### Decision posture

Land only when:
- Two or more enums in the standard library or common user code
  exhibit the parallel-match pattern.
- Review confirms the constraints above are stable (no drift toward
  methods, typeclasses, non-constant attributes).

Until then, the `fn_info + accessors` refactor is the canonical
pattern.

**Cost**: medium. New type-declaration syntax, small resolution-pass
addition to emit the projection functions.
**Depends on**: parser + resolve pass.

## 13. `!` postfix — `Option` / `Result` propagation

```kai
fn parse_config() : Result[Config, Error] {
  let raw = read_file("config.toml")!
  let parsed = parse_toml(raw)!
  validate(parsed)
}
```

The `!` suffix on an expression of type `Option[T]` or `Result[E, T]`
unwraps the success case into a `T` and short-circuits the enclosing
function on the failure case. Equivalent to Rust's `?`, renamed
because `?` is already taken by typed holes.

**Status**: already reserved in `docs/kaikai-minimal.md` ("`!` is
reserved (post-minimal uses for `Option`/`Result` propagation)").
This entry formalises the semantics.

### Semantics

- `expr!` on `Option[T]`:
  - `Some(x) -> x`
  - `None` → the enclosing function returns `None`.
  - The enclosing function's return type must be `Option[U]` for
    some `U`.
- `expr!` on `Result[E, T]`:
  - `Ok(x) -> x`
  - `Err(e)` → the enclosing function returns `Err(e)`.
  - The enclosing function's return type must be `Result[E, U]`
    for the same error type `E` (no `From`-style coercion — that
    would be a typeclass).
- Type error if the enclosing function's return type is incompatible.
  The error message must name both types and point at both the `!`
  and the function signature.

### What it buys

- Linear success paths: the happy case reads top-to-bottom, without
  match pyramids.
- Errors propagate with one character, not a five-line match.
- The `!` is **visible at the call site** — it is not implicit
  propagation. Readers see where the early returns are.

### What it costs

- Adds a postfix operator. Parser work is local.
- Fixes the semantics to `Option` and `Result` only. No user-defined
  "monad-like" types can participate — that would require typeclass
  resolution, which Tier 1 forbids.

### Explicit non-goals

- **No `From`-style error conversion.** If the function returns
  `Result[AppError, T]` and the callee returns `Result[IoError, T]`,
  `callee()!` is a type error. The user must convert explicitly.
  Rust added `From` to its `?` and paid with typeclass complexity;
  kaikai does not.
- **No user-defined `!`.** It is not a trait method. It is compiler
  sugar bound to two specific types.

**Cost**: low. Parser + two desugar rules (one for `Option`, one for
`Result`) in the typed-IR pass.
**Depends on**: `Option` and `Result` in prelude (present since
kaikai-minimal).

## 14. `@` as-patterns in `match`

```kai
match xs {
  all@[h, ...t] if needs_original(h) -> {
    log(all)                    # the full list, not rebuilt
    process(h, t)
  }
  [] -> default()
}
```

`name@pattern` binds `name` to the whole scrutinee when `pattern`
matches, *and* destructures as `pattern` for the remaining pattern
variables. Same semantics as Haskell and Rust.

### What it buys

- Access to both the destructured pieces and the whole value in one
  arm, without rebuilding `[h, ...t]` into a new list or introducing
  a `let all = xs; match all { ... }` wrapping.
- Common in linting, logging, and audit code where the original
  structure must be referenced alongside its parts.

### What it costs

- Adds one pattern form. The grammar gains `Ident "@" Pattern` as a
  pattern alternative.
- No runtime cost — the binding is already computed.

### Constraints

- Only in `match` arms and `let` bindings, both already established
  pattern contexts.
- The bound name must be a fresh identifier (no shadowing of an
  existing binding in scope — diagnostic required).

### Symbol reuse: `@` in two roles

`docs/effects-stdlib.md` (Doc B / m7b) uses `@cap` as a prefix
unary operator on expressions — `@counter` means `counter.get()`
on a `State[T]` capability binding, `@config` means
`config.ask()` on a `Reader[T]` one. The contexts are disjoint:
this proposal's `@` is *infix*, between an identifier and a
pattern, and appears only where patterns are legal (`match`
arms, `let` bindings). Doc B's `@` is *prefix*, on expressions.

No grammatical ambiguity — the parser can tell which role applies
from the surrounding production. If this proposal lands, the
documentation for both should cross-reference the other so readers
know `@` plays two disjoint roles.

**Cost**: trivial. One grammar rule plus one resolve-pass case.
**Depends on**: parser.

## 15. `?.` optional chaining

```kai
# user: Option[User]; u.profile: Option[Profile]; p.display_name: String
let name: Option[String] = user?.profile?.display_name
```

Safe navigation over `Option`. Desugars to nested `opt_and_then`
calls that short-circuit on `None`. The semantics matches
Swift / Kotlin, except restricted to `Option` (no reference nullability
in kaikai).

### Semantics

- `x?.field` where `x : Option[T]` and `T` has a field `field : F`:
  - `None -> None`
  - `Some(v) -> Some(v.field)` if `F` is not `Option[...]`
  - `Some(v) -> v.field`        if `F` is already `Option[U]`
    (flattens — same as Swift)
- Chainable: `x?.a?.b?.c` short-circuits at the first `None`.
- Does **not** short-circuit the enclosing function. `!` does that;
  `?.` is local.

### What it buys

- Linear, readable navigation through optional fields.
- Complements `!`: `?.` builds an `Option[T]` at the current
  expression; `!` propagates the `None` out. Both are needed for
  different patterns.

### What it costs

- One new postfix-then-field-access syntactic form.
- Parser must distinguish `?.` from separate `?` and `.` tokens.
  Easy with LL(1) + lookahead.
- Type checker needs one rule per invocation site. The flattening
  case (`Option[Option[T]]` → `Option[T]`) mirrors `opt_and_then`.

### Constraints

- Works on `Option[T]` only — not on `Result`, not on user-defined
  sum types. Same reasoning as `!`: generalising would require
  typeclasses.
- No `?[i]` for optional list indexing in the first pass —
  `list_nth` + `opt_and_then` covers it for now. Reconsider if
  demand appears.

**Cost**: low. Parser addition plus one desugar in the typed-IR
pass (to nested `opt_and_then`).
**Depends on**: parser + type checker.

## 16. Bitwise operators — deferred

Not a full proposal — a placeholder so the decision is not made by
default.

`&`, `~`, `^`, `<<`, `>>` on `Int` are standard bitwise operators in
Rust, Go, C. kaikai has not adopted them because:

- No code in the project currently needs bit manipulation.
- The arithmetic family (`+ - * / //`) is already wide; adding five
  more symbols without pull from real code would grow the surface
  for no gain.
- `|` is taken (map pipe + variant separator); offering `&`, `~`,
  `^`, `<<`, `>>` without `|` for bitwise OR is asymmetric.

### When to reconsider

Land when any of these is true:
- Stage-2 compiler internals need bit manipulation (likely for
  efficient tag/layout computations in the emitter).
- A stdlib module (hashing, encoding, random PRNG, networking) is
  written and is obviously clearer with operators than with named
  functions.
- A user submits a concrete module where `bit_and`, `bit_or`,
  `bit_shl` make ≥30% of expressions and hurt readability.

### Interim

Provide named functions in the prelude: `bit_and`, `bit_or`,
`bit_xor`, `bit_not`, `bit_shl`, `bit_shr`. If the functions get
heavy use, that is the signal to add operators. If they do not,
the language stays smaller.

**Cost**: zero for now (stdlib functions). Medium if/when operators
ship (lexer, parser, precedence rules, new AST node).
**Depends on**: demonstrated need.

## Deliberately not on this list

These were considered and rejected for the same reasons they are
rejected elsewhere in the design:

- **Macros / reflection**: break the *regular, predictable syntax*
  and *fast compilation* principles.
- **Refinement holes** (`?x : { n: Int | n > 0 }`): require a
  constraint solver. That violates the decidable-and-predictable
  commitment of the type system.
- **Gradual-typing holes** (`?: Dyn`): introduce dynamic typing into
  a language whose central promise is that typed effects cannot
  escape unhandled.
- **Rust-style enum discriminators** (`type T : Int { A = 0, B = 1 }`,
  each variant with a *unique* integer tag): the uniqueness rule
  rules out useful cases (e.g. blackjack, where `Jack`, `Queen`,
  `King` all want the same value `10`). The broader proposal #12
  (sum types with constant attributes) covers the same use-case
  without the uniqueness constraint, which is why this narrower
  form is not on the list.
- **`deriving Show` / typeclass-style auto-derivation**: directly
  conflicts with the principle *no costly type-class resolution*.
  If automatic stringification is ever desired, ship it as a `kai`
  tooling codegen, not as a typeclass. Note that proposal #12
  (attribute sums) handles the label case for *closed* sum types
  without needing typeclasses — that is the intended solution.
- **Early `return` statement**: kaikai is expression-based; *last
  expression is the value* is canonical. Adding `return` would
  create a second exit form and nest imperative control flow into
  an otherwise expression-oriented surface.
- **`&&` / `||` for Bool**: duplicate `and` / `or` without distinct
  intent. `and` / `or` stay as the canonical boolean operators.
- **`<-` as monadic bind / effect shorthand**: `!` (proposal #13)
  already covers the propagation pattern for `Option` / `Result`;
  kaikai's direct-style effects do not need a bind operator. Adding
  `<-` would be a second mechanism for the same flow.
- **`$` low-precedence apply (Haskell-style)**: `|>` and `|` already
  avoid paren pyramids. `$` would be a third way to write the same
  chain.
- **`::` path or type ascription**: `.` already separates module
  paths (`math.vector.dot`) and `:` annotates types. `::` would
  collide with one of them without adding intent.

## Adoption criteria

### For LLM-friendly diagnostics (sections 1–8)

Each extension lands only when:

1. Typed holes have shipped and been used in anger. They are the
   prototype for this family; the others inherit their shape
   (expected type, in-scope bindings, candidates, text + JSON, stable
   schema).
2. A concrete need has shown up in practice. *LLMs might like this*
   is not enough. A concrete interaction with an LLM or LSP that
   currently fails or is awkward is.
3. The feature fits in ≤500 lines on top of the stage-2 checker.
   Anything larger gets its own design doc first.

### For language-surface features (sections 9–16)

These land only alongside the closing of *"Concrete syntax
consolidation"* in `design.md`. Any decision on tuples or punning
should be made as part of that one conversation — not drip-fed.

- **Tuples**: ship only if the decision is to accept them as the
  canonical form for anonymous products, simultaneously reserving
  records for named-field use. A split rule is required, not both
  forms as interchangeable.
- **Record punning**: safe to land independently of the tuples
  decision — it's strictly additive sugar.
- **`variants[T]()`**: safe to land alongside stage-2
  monomorphisation.
- **Sum types with constant attributes**: land only after confirming
  (a) the pattern appears in ≥3 independent enums in stdlib or user
  code, and (b) the constraint set in section 12 has survived at
  least one design review without drifting toward methods,
  typeclasses, or non-constant attributes. Until then, prefer the
  `fn_info + accessors` refactor.
- **`!` postfix (`Option` / `Result` propagation)**: safe to land
  once `Option` and `Result` are fully in the stdlib and a small
  corpus of error-handling code exists to verify the ergonomics.
  The syntax is reserved today (`kaikai-minimal.md`); formalisation
  just fills in the semantics.
- **`@` as-pattern**: strictly additive to the pattern grammar,
  independent of everything else. Land whenever a case study shows
  it avoiding a meaningful amount of `let x = scrutinee; match x { ... }`
  boilerplate.
- **`?.` optional chaining**: land after `!` has been used enough
  to confirm the two do not overlap in practice. `?.` is local;
  `!` propagates. If the code base mostly wants propagation, `?.`
  may not be worth the complexity — re-evaluate then.
- **Bitwise operators**: deferred. Stays as named functions
  (`bit_and`, `bit_or`, …) until there is code that demonstrably
  suffers. This is the "wait for demand" case, not the "principle
  block" case.

The goal is to keep the surface small. A handful of orthogonal,
well-integrated extensions is worth more than a pile of clever
features.
