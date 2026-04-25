# structured concurrency

Adopted for stage 2. Makes the "fibers + effects" model concretely
composable: every fiber lives inside a scope (`nursery`) that waits
for its children and propagates cancellation. No fiber can outlive
the block that spawned it.

## Motivation

Without structure, fibers are like raw `go` statements or threads:
they leak, they survive scope exits, they need manual join/cancel
plumbing. The *"Go statement considered harmful"* argument
(Nathaniel Smith, 2018) showed that fiber lifetimes are easier to
reason about when tied to lexical scopes. Trio, Kotlin coroutines,
Swift structured concurrency, and OCaml 5's Eio all converged on the
same answer.

kaikai takes it further by making the scope **the handler for a
`Spawn` effect capability**. `spawn`/`await`/`select` are not
built-in primitives; they are operations you can only invoke inside
an active nursery. `nursery` itself is a stdlib helper that
installs the `Spawn` handler — also not a built-in: the language
core has no scheduling concepts, only the effect system.

## Syntax

```kai
nursery { n ->
  let a = n.spawn { task_a() }
  let b = n.spawn { task_b() }
  combine(n.await(a), n.await(b))
}
```

- `nursery { n -> ... }` opens a scope. `n` is the `Spawn`
  capability binding (the trailing-lambda + `as`-style cap form
  pinned in `docs/syntax-sugars.md`).
- `n.spawn(f: () -> T / e) : Fiber[T] / Spawn + Cancel`
  creates a child fiber.
- `n.await(f: Fiber[T]) : T / Spawn + Cancel` suspends until
  the child finishes, returns its value.
- `n.select(fs: [Fiber[T]]) : T / Spawn + Cancel` returns when
  **any** listed child finishes; the others are cancelled.
- `n.cancel(f: Fiber[T]) : Unit / Spawn` requests cancellation
  of one specific child. The child receives `Cancel.raise()`
  at its next yield point.
- `n.cancel_all() : Unit / Spawn` requests cancellation of every
  child of the nursery. Same delivery mechanism as `cancel`,
  applied to all live children.
- Leaving the nursery block **waits for all pending children**.
- If any child crashes (unhandled effect or panic), the nursery
  cancels the remaining children, drains them, and re-raises the
  original cause.

`spawn`, `await`, and `select` carry `Cancel` because each is a
yield point: a fiber blocked in any of them can be cancelled by
the scheduler.

## Type system

- `Fiber[T]` is a region-branded handle, not a value. It
  **cannot** escape the nursery that produced it: the scope
  type tags `Fiber` with a region brand and the return type
  checker rejects carrying it out. The same brand machinery is
  shared with `Pid[Msg]` from `docs/actors.md` — both are
  scope-bound handles to runtime objects, not first-class
  movable values.
- `spawn(f: () -> T / e)` makes the **effect set `e`** part of the
  nursery's own row. A nursery that spawns only pure tasks has no
  additional effect; a nursery that spawns `Io` tasks has `Io`.
- Cancellation is an effect, `Cancel`. A task can `handle` it to
  release resources. Unhandled `Cancel` unwinds the fiber cleanly.

```kai
# Polymorphic over whatever effect the worker carries.
pub fn pmap[A, B, e](xs: [A], f: (A) -> B / e) : [B] / e + Spawn + Cancel {
  nursery { n ->
    xs | (x) => n.spawn { f(x) } | n.await
  }
}
```

If the caller is in a pure context, `e` resolves to the empty set
and the row collapses to `/ Spawn + Cancel`. If the caller is
already under `Console`, `e = Console` and the signature becomes
`/ Console + Spawn + Cancel`.

## Cancellation

`Cancel` is delivered when:

- A sibling fiber in the same nursery crashes.
- The nursery is cancelled wholesale via `n.cancel_all()` (cancels
  every child) or a child is cancelled individually via
  `Spawn.cancel(fiber)`.
- The surrounding fiber is itself cancelled (propagation).

A fiber that wants to clean up on cancellation wraps the affected
region in a `Cancel` handler. The handler runs the cleanup and
does not call `resume`; the fiber unwinds out of the wrapped
block:

```kai
fn worker(m: ActorCap[Job]) : Unit / Actor[Job] + Console + Mutable + Cancel {
  let counter = Mutable.ref_make(0)
  handle {
    forever {
      process(counter, m.receive())
    }
  } with Cancel {
    raise(resume) -> {
      let final = Mutable.ref_get(counter)
      Console.eprint("worker: cancelled after #{final} jobs")
      # resume is intentionally not called — the fiber unwinds
    }
  }
}
```

If a fiber has no `Cancel` handler, the runtime delivers
`Cancel.raise()` at the next yield point and the fiber unwinds
cleanly. There are no silent survivors. Doc B §`Cancel`
*Handling for cleanup* and *Unwind through nested handlers*
pin the wider semantics.

## Patterns

### Race (first result wins)

```kai
pub fn race[T, e](options: [() -> T / e]) : T / e + Spawn + Cancel {
  nursery { n ->
    let fibers = options | (f) => n.spawn(f)
    n.select(fibers)
  }
}
```

Polymorphic in `e` so the candidate functions can carry any
effect set (`Console`, `File`, `Io`, etc.).

### Timeout

```kai
type Outcome[T] =
  | Value(value: T)
  | Timeout

pub fn with_timeout[T, e](
  ms:   Int,
  task: () -> T / e
) : Option[T] / e + Spawn + Cancel + Time {
  nursery { n ->
    let winner = n.spawn { Value(task()) }
    let timer  = n.spawn { Time.sleep(ms); Timeout }
    match n.select([winner, timer]) {
      Value(v) -> Some(v)
      Timeout  -> None
    }
  }
}
```

`Time.sleep(ms)` is an op of a `Time` effect (deferred to a
later doc; for v1, treat `Time` as a placeholder for whatever
clock capability the runtime exposes — likely `Spawn` or a
dedicated `Time` effect).

### Actor supervision

```kai
pub fn run_pipeline(
  s: Pid[Source], w: [Pid[Work]], k: Pid[Collected]
) : Unit / Spawn + Cancel {
  nursery { n ->
    let source = n.spawn { producer(s) }
    each([1..4]) { _ -> n.spawn { worker(s, w) } }
    let sink   = n.spawn { collector(w, k) }
    n.await(sink)
  }
}
```

If any worker crashes, the producer and sink are cancelled
before `run_pipeline` returns. For the actor surface (mailbox
policies, link/monitor supervision, `spawn_actor`,
`with_mailbox`), see `docs/actors.md`.

## Root nursery

`main` runs inside an implicit root nursery, so `spawn` is
usable at the top level without ceremony. Any `main` whose
effect row contains `Spawn` triggers the root nursery
installation (Doc B §`main` and the runtime *Installation
order*):

```kai
fn main() : Unit / Console + Spawn + Cancel {
  let processed = pmap([1..100], heavy)
  Console.print("processed #{processed.length} items")
}
# In this example pmap closes its internal nursery before
# returning, so there is nothing left to wait on. The implicit
# root nursery matters when main spawns fibers directly at the
# top level (without an inner nursery) — those fibers are
# joined before the program exits with 0.
```

## Non-goals

- **Unbounded concurrency**. There is no `detach` / "fire and
  forget". Every fiber is owned by exactly one nursery.
- **Pre-emptive cancellation**. A fiber stuck in a tight CPU
  loop with no effect ops is not interrupted until it yields
  (e.g. via `Spawn.yield()`). The runtime does not insert
  async safety points or signal-driven preemption —
  cancellation is cooperative by design. See §Cancellation for
  delivery rules and `Spawn.yield()` placement.
- **Priority schedulers, fairness guarantees**. Post-MVP.
- **Distributed supervision**. Phase 5+ at the earliest, when
  distribution lands.

## Implementation notes

Stage 2 work, milestone **m8** — the scheduler and `Spawn` /
`Cancel` runtime support land after m7a (effects mechanics) and
m7b (sugars). Doc B §*Next steps* tracks the m7a/m7b split;
this doc's deliverables sit on top of both.

Requires:

- The effects + handlers machinery from m7a (row unification,
  CPS transform, handler-stack runtime).
- `Fiber[T]` as a region-branded handle; the region check is
  a small extension to the existing type checker, sharing the
  brand machinery with `Pid[Msg]` from `docs/actors.md`.
- A scheduler in the runtime: per-fiber stacks (segmented or
  heap-allocated), a cooperative scheduler loop, and a
  cancellation flag every yield-point op checks.
- `nursery` as a stdlib helper installed as an effect handler;
  the trailing-lambda form `nursery { n -> ... }` desugars (per
  `docs/syntax-sugars.md` §1) to a call passing the body to
  the handler.

## References

- Nathaniel Smith, *Notes on structured concurrency, or: Go
  statement considered harmful* (2018).
- BEAM / Erlang, the original "isolated fibers with private
  heap, messages copied" model that kaikai inherits at the
  runtime level.
- Trio, Python structured-concurrency library.
- Kotlin `coroutineScope`; Swift `async let` / `TaskGroup`.
- OCaml 5 Eio.
