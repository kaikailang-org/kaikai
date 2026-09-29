#ifndef KAI_CHECK_H
#define KAI_CHECK_H

#include "ast.h"

/*
 * Minimal name-resolver for stage 0.
 *
 * Walks the program and reports identifiers that are not in scope. Does
 * not perform full static type checking: because the runtime uses uniform
 * boxing, the emitter does not need exact types to produce correct C.
 * The checker's job here is limited to surfacing undefined names and
 * obviously-wrong arities on prelude calls.
 *
 * Returns 0 on success, non-zero on any error (messages printed to stderr).
 */
int kai_check(Node *program, const char *file, const char *src);

#endif /* KAI_CHECK_H */
