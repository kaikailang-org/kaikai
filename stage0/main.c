/*
 * stage 0: kaikai-minimal bootstrap compiler.
 *
 * Current state: lexer wired in. Flags:
 *   --tokens   print the token stream and exit
 *   -h, --help show usage
 *
 * Parser, type checker, and emitter are filled in by later milestones.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

static char *read_file(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open '%s'\n", path);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long n = ftell(fp);
    if (n < 0) { fclose(fp); return NULL; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    char *buf = (char *) malloc((size_t) n + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t got = fread(buf, 1, (size_t) n, fp);
    fclose(fp);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

static void usage(const char *prog) {
    fprintf(stderr,
            "usage: %s [--tokens] <file.kai>\n"
            "  --tokens    print the token stream and exit\n"
            "  -h, --help  this help\n",
            prog);
}

int main(int argc, char **argv) {
    const char *path = NULL;
    int dump_tokens = 0;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "--tokens") == 0) { dump_tokens = 1; continue; }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) { usage(argv[0]); return 0; }
        if (a[0] == '-') { fprintf(stderr, "unknown flag: %s\n", a); usage(argv[0]); return 2; }
        if (path)      { fprintf(stderr, "only one input file supported\n"); return 2; }
        path = a;
    }

    if (!path) { usage(argv[0]); return 2; }

    size_t len = 0;
    char *src = read_file(path, &len);
    if (!src) return 1;

    size_t n = 0;
    Token *toks = kai_lex(path, src, len, &n);
    if (!toks) { free(src); return 1; }

    /* Report any lexer errors. */
    int had_errors = 0;
    for (size_t i = 0; i < n; ++i) {
        if (toks[i].kind == TK_ERROR) {
            had_errors = 1;
            fprintf(stderr, "%s:%d:%d: error: lex error in %.*s\n",
                    path, toks[i].line, toks[i].col,
                    (int) toks[i].length, src + toks[i].start);
        }
    }

    if (dump_tokens) {
        kai_lex_dump(path, src, toks, n);
    } else if (!had_errors) {
        printf("kaic0: %s: %zu tokens (parser not implemented yet)\n", path, n);
    }

    free(toks);
    free(src);
    return had_errors ? 1 : 0;
}
