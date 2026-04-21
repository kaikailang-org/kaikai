/*
 * stage 0: kaikai-minimal bootstrap compiler.
 *
 * Milestone 1 skeleton: reads a .kai source file into memory and hands
 * the buffer to the (currently stub) lexer. Later milestones fill in
 * the rest of the pipeline: parser -> type checker -> emitter.
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
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: cannot seek '%s'\n", path);
        fclose(fp);
        return NULL;
    }
    long n = ftell(fp);
    if (n < 0) {
        fprintf(stderr, "error: cannot tell size of '%s'\n", path);
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: cannot rewind '%s'\n", path);
        fclose(fp);
        return NULL;
    }
    char *buf = (char *) malloc((size_t) n + 1);
    if (!buf) {
        fprintf(stderr, "error: out of memory reading '%s'\n", path);
        fclose(fp);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t) n, fp);
    fclose(fp);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s <file.kai>\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        usage(argv[0]);
        return 2;
    }

    size_t len = 0;
    char *src = read_file(argv[1], &len);
    if (!src) return 1;

    kai_lex(argv[1], src, len);

    free(src);
    return 0;
}
