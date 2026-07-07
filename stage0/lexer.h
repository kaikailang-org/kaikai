#ifndef KAI_LEXER_H
#define KAI_LEXER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Token kinds for kaikai-minimal.
 *
 * Keep in sync with tk_name() in lexer.c.
 */
typedef enum {
    TK_EOF,
    TK_NEWLINE,

    /* Keywords */
    TK_AND, TK_AS, TK_ASSERT, TK_ELSE, TK_FALSE,
    TK_FN, TK_IF, TK_IMPORT, TK_LET, TK_MATCH,
    TK_NOT, TK_OR, TK_PUB, TK_TEST, TK_TRUE, TK_TYPE,

    /* Literals */
    TK_INT,                  /* 42, 1_000_000 */
    TK_REAL,                 /* 3.14, 1e10, 2.5e-3 */
    TK_CHAR,                 /* 'a', '\n' */
    TK_STRING,               /* "..." or """...""" */

    /* Identifiers (snake_case or PascalCase; parser decides by context) */
    TK_IDENT,
    TK_UNDERSCORE,           /* the lone `_` pattern wildcard */

    /* Punctuation */
    TK_LPAREN, TK_RPAREN,
    TK_LBRACKET, TK_RBRACKET,
    TK_LBRACE, TK_RBRACE,
    TK_COMMA, TK_COLON, TK_SEMI,
    TK_DOT, TK_DOTDOT, TK_ELLIPSIS,    /* . .. ... */

    /* Operators */
    TK_EQ,                   /* = */
    TK_ARROW,                /* -> */
    TK_FAT_ARROW,            /* => */
    TK_PIPE,                 /* | (used only in type declarations for sum variants) */
    TK_PIPE_APPLY,           /* |> */
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_SLASH_SLASH, TK_PERCENT,
    TK_EQEQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_BANG,                 /* ! reserved; no use in minimal, emitted for the parser to reject */

    /* Error token, carries a message via kai_lex_errors() */
    TK_ERROR
} TokenKind;

typedef struct {
    TokenKind kind;
    int32_t   line;          /* 1-based */
    int32_t   col;           /* 1-based, in bytes */
    size_t    start;         /* byte offset into source */
    size_t    length;        /* in bytes */
} Token;

/*
 * Tokenize a source buffer.
 *
 *  - `file` is the display name used in error messages.
 *  - `src`  must remain valid for the lifetime of the returned Token array
 *           (Tokens reference `src` by offset only; no copies).
 *  - `len`  is the byte length of `src` (not counting a trailing NUL).
 *  - On return, `*out_n` holds the number of tokens (always includes a
 *    trailing TK_EOF).
 *
 * The returned array is malloc()-allocated. The caller owns it and must
 * free() it when done.
 *
 * Lexing is error-tolerant: an invalid input produces one or more
 * TK_ERROR tokens with a message retrievable via kai_lex_error_message();
 * lexing continues past them. The caller should check for any TK_ERROR
 * before advancing to parsing.
 */
Token *kai_lex(const char *file, const char *src, size_t len, size_t *out_n);

/* Human-readable name of a token kind, for debug and error output. */
const char *tk_name(TokenKind k);

/* Error message attached to a TK_ERROR token (static storage, do not free). */
const char *kai_lex_error_message(size_t idx);

/* Print tokens to stdout, one per line (for debugging). */
void kai_lex_dump(const char *file, const char *src, const Token *toks, size_t n);

#endif /* KAI_LEXER_H */
