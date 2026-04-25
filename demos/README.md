# Demos

Programs written in **target kaikai syntax** as probes for compiler progress.
Some compile today, some don't. The Makefile reports both gracefully.

```sh
make -C demos verify    # compile + run every demo, print PASS/FAIL table
make -C demos <topic>   # try a single demo
make -C demos list
make -C demos clean
```

Assumes the compiler is built at `../bin/kai` (run `make` at the project
root first).

See [STATUS.md](STATUS.md) for the rationale and how to read the output.

## Layout

```
demos/
  Makefile
  README.md
  STATUS.md
  <topic>/
    main.kai                 # entry point (required)
    main.out.expected        # golden stdout (optional)
    *.kai                    # additional modules (optional)
    fixtures/                # input data (optional)
```

The pre-redesign Go-frontend sketches that used to live as flat `*.kai`
files at this level have all been either migrated into per-demo
subdirectories or deleted (when their syntax / concepts had no
recovery path in the redesigned language). The full migration log is in
[STATUS.md](STATUS.md); the originals are preserved in git history.
