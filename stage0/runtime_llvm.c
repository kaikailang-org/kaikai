/*
 * LLVM-backend shim over the header-only kaikai-minimal runtime.
 *
 * `runtime.h` defines the whole runtime as `static inline` so the
 * stage 0 / stage 1 / stage 2 C backends can emit one big translation
 * unit and not worry about duplicate-symbol linkage. That trick does
 * not work for the LLVM backend: `llc` produces an object file that
 * needs externally linkable runtime symbols, and `static` functions
 * live inside a different TU with no visible name.
 *
 * This file is that visible name. It includes the header once, then
 * re-exports each runtime entry point the LLVM backend calls into as
 * a thin `kaix_*` wrapper. It also provides `main`, wired to the
 * `kai_main` symbol the LLVM output defines.
 *
 * Link line:
 *   clang -I stage0 foo.ll stage0/runtime_llvm.c -o foo
 */

#include "runtime.h"

/* ---------- value constructors ---------- */
KaiValue *kaix_str(const char *s)              { return kai_str(s); }
KaiValue *kaix_int(int64_t i)                  { return kai_int(i); }
KaiValue *kaix_bool(int b)                     { return kai_bool(b); }

/* ---------- binops ---------- */
KaiValue *kaix_add(KaiValue *a, KaiValue *b)   { return kai_add(a, b); }
KaiValue *kaix_sub(KaiValue *a, KaiValue *b)   { return kai_sub(a, b); }
KaiValue *kaix_mul(KaiValue *a, KaiValue *b)   { return kai_mul(a, b); }
KaiValue *kaix_div(KaiValue *a, KaiValue *b)   { return kai_div(a, b); }
KaiValue *kaix_idiv(KaiValue *a, KaiValue *b)  { return kai_idiv(a, b); }
KaiValue *kaix_mod(KaiValue *a, KaiValue *b)   { return kai_mod(a, b); }
KaiValue *kaix_eq(KaiValue *a, KaiValue *b)    { return kai_eq_v(a, b); }
KaiValue *kaix_ne(KaiValue *a, KaiValue *b)    { return kai_ne_v(a, b); }
KaiValue *kaix_lt(KaiValue *a, KaiValue *b)    { return kai_lt(a, b); }
KaiValue *kaix_gt(KaiValue *a, KaiValue *b)    { return kai_gt(a, b); }
KaiValue *kaix_le(KaiValue *a, KaiValue *b)    { return kai_le(a, b); }
KaiValue *kaix_ge(KaiValue *a, KaiValue *b)    { return kai_ge(a, b); }
KaiValue *kaix_neg(KaiValue *a)                { return kai_neg(a); }
KaiValue *kaix_not(KaiValue *a)                { return kai_bool(!kai_truthy(a)); }

/* ---------- control helpers ---------- */
int kaix_truthy(KaiValue *v)                   { return kai_truthy(v); }

/* ---------- prelude subset used by M3b ---------- */
KaiValue *kaix_prelude_print(KaiValue *v)          { return kai_prelude_print(v); }
KaiValue *kaix_prelude_int_to_string(KaiValue *v)  { return kai_prelude_int_to_string(v); }

/* Entry point: the LLVM output defines kai_main. Match what the C
   backend's emit_main_wrapper does. */
extern KaiValue *kai_main(void);

int main(int argc, char **argv) {
    kai_set_args(argc, argv);
    KaiValue *result = kai_main();
    kai_decref(result);
    return 0;
}
