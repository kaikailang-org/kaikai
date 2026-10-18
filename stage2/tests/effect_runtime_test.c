/* m7a #5: smoke test for the handler-stack runtime primitives.
 *
 * Exercises kai_evidence_push / pop / lookup directly, without
 * any compiled kaikai code. m7a #6 (lowering) will provide the
 * end-to-end coverage; this test just confirms the C primitives
 * behave as Doc C §*Handler-stack runtime* describes.
 */

#include "runtime.h"
#include <stdio.h>

static int failed = 0;

static void check(const char *what, int cond) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", what);
        failed = 1;
    }
}

int main(void) {
    /* Empty stack: lookup of any label returns NULL. */
    check("empty lookup is NULL",
          kai_evidence_lookup("Io") == NULL);

    /* Push two evidence nodes; lookup must find the innermost. */
    KaiEvidence outer, inner;
    int outer_handler = 1;
    int inner_handler = 2;

    kai_evidence_push(&outer, "Io", &outer_handler);
    kai_evidence_push(&inner, "Io", &inner_handler);

    check("innermost wins",
          kai_evidence_lookup("Io") == &inner_handler);

    /* Different effects coexist; the matching one is returned. */
    KaiEvidence fail;
    int fail_handler = 99;
    kai_evidence_push(&fail, "Fail", &fail_handler);

    check("Io still resolves with Fail on top",
          kai_evidence_lookup("Io") == &inner_handler);
    check("Fail resolves to its own handler",
          kai_evidence_lookup("Fail") == &fail_handler);

    /* Pop unwinds back through the stack. */
    kai_evidence_pop();      /* drops Fail */
    check("Fail gone after pop",
          kai_evidence_lookup("Fail") == NULL);
    check("Io still found",
          kai_evidence_lookup("Io") == &inner_handler);

    kai_evidence_pop();      /* drops inner */
    check("after popping inner, outer wins",
          kai_evidence_lookup("Io") == &outer_handler);

    kai_evidence_pop();      /* drops outer */
    check("empty after popping outer",
          kai_evidence_lookup("Io") == NULL);

    /* strcmp fallback: lookup with a non-pointer-equal label. */
    char io_copy[3] = { 'I', 'o', 0 };
    KaiEvidence node;
    int handler = 7;
    kai_evidence_push(&node, "Io", &handler);
    check("strcmp fallback finds Io via copy",
          kai_evidence_lookup(io_copy) == &handler);
    kai_evidence_pop();

    if (failed) {
        fprintf(stderr, "effect_runtime_test: FAIL\n");
        return 1;
    }
    printf("effect_runtime_test: OK\n");
    return 0;
}
