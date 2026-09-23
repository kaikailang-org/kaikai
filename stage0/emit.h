#ifndef KAI_EMIT_H
#define KAI_EMIT_H

#include <stdio.h>

#include "ast.h"

/*
 * Emit portable C for a checked kaikai-minimal AST.
 *
 * The output is a single translation unit that expects to be compiled
 * together with `runtime.h` (the kaikai runtime) sitting on the include
 * path:
 *     ./kaic0 hello.kai > hello.c
 *     cc hello.c -I stage0 -o hello
 *     ./hello
 *
 * Returns 0 on success. Prints diagnostics to stderr for AST shapes
 * that are not supported yet.
 */
/* If `test_mode` is non-zero, the emitter wires `test`/`assert` blocks
   into a test-runner main instead of the ordinary main wrapper. */
int kai_emit(Node *program, FILE *out, int test_mode);

#endif /* KAI_EMIT_H */
