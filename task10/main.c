#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum token_kind {
  TOK_END = 0,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_VAR,   /* single letter A..Z */
  TOK_AND,   /* keyword AND */
  TOK_OR,    /* keyword OR */
  TOK_NOT,   /* keyword NOT */
  TOK_ERROR, /* invalid character/word */
};

struct token {
  enum token_kind kind;
  char var; /* valid only for TOK_VAR */
};

struct lexer {
  const char *s;
  size_t i;
  struct token cur;
  int err; /* 0 ok, nonzero = lex error */
};

static void lex_skip_ws(struct lexer *lx) {
  while (lx->s[lx->i] && isspace((unsigned char)lx->s[lx->i])) {
    lx->i++;
  }
}

static bool lex_match_word(const char *w, const char *s, size_t *consumed) {
  size_t n = strlen(w);
  if (strncmp(s, w, n) != 0) {
    return false;
  }
  /* Keywords must be standalone tokens: ANDX is not AND + X. */
  if (s[n] >= 'A' && s[n] <= 'Z') {
    return false;
  }
  *consumed = n;
  return true;
}

static void lex_next(struct lexer *lx) {
  lex_skip_ws(lx);
  char c = lx->s[lx->i];
  if (!c) {
    lx->cur = (struct token){.kind = TOK_END, .var = 0};
    return;
  }

  if (c == '(') {
    lx->i++;
    lx->cur = (struct token){.kind = TOK_LPAREN, .var = 0};
    return;
  }
  if (c == ')') {
    lx->i++;
    lx->cur = (struct token){.kind = TOK_RPAREN, .var = 0};
    return;
  }

  if (c >= 'A' && c <= 'Z') {
    /* Could be a variable (single letter) or start of a keyword. */
    size_t consumed = 0;
    if (lex_match_word("AND", lx->s + lx->i, &consumed)) {
      lx->i += consumed;
      lx->cur = (struct token){.kind = TOK_AND, .var = 0};
      return;
    }
    if (lex_match_word("OR", lx->s + lx->i, &consumed)) {
      lx->i += consumed;
      lx->cur = (struct token){.kind = TOK_OR, .var = 0};
      return;
    }
    if (lex_match_word("NOT", lx->s + lx->i, &consumed)) {
      lx->i += consumed;
      lx->cur = (struct token){.kind = TOK_NOT, .var = 0};
      return;
    }

    /* Variable must be exactly one uppercase letter. */
    lx->i++;
    lx->cur = (struct token){.kind = TOK_VAR, .var = c};
    return;
  }

  lx->err = 1;
  lx->cur = (struct token){.kind = TOK_ERROR, .var = 0};
}

enum node_kind {
  NODE_VAR = 0,
  NODE_NOT,
  NODE_AND,
  NODE_OR,
};

struct node {
  enum node_kind kind;
  char var; /* for NODE_VAR */
  struct node *a;
  struct node *b; /* for binary ops */
};

static void node_free(struct node *n) {
  if (!n) return;
  node_free(n->a);
  node_free(n->b);
  free(n);
}

static struct node *node_new(enum node_kind k, char var, struct node *a, struct node *b) {
  struct node *n = (struct node *)calloc(1, sizeof(*n));
  if (!n) return NULL;
  n->kind = k;
  n->var = var;
  n->a = a;
  n->b = b;
  return n;
}

struct parser {
  struct lexer lx;
  int err; /* 0 ok, nonzero = parse/alloc error */
  bool used[26];
};

static void parse_advance(struct parser *p) {
  lex_next(&p->lx);
  if (p->lx.cur.kind == TOK_ERROR) {
    p->err = 1;
  }
}

static bool parse_accept(struct parser *p, enum token_kind k) {
  if (p->lx.cur.kind != k) return false;
  parse_advance(p);
  return true;
}

static bool parse_expect(struct parser *p, enum token_kind k) {
  if (parse_accept(p, k)) return true;
  p->err = 1;
  return false;
}

/*
 * Grammar with precedence (highest to lowest):
 *   primary := VAR | '(' expr ')'
 *   unary   := 'NOT' unary | primary
 *   and     := unary ('AND' unary)*
 *   or      := and ('OR' and)*
 *   expr    := or
 */
static struct node *parse_expr(struct parser *p);
static struct node *parse_or(struct parser *p);
static struct node *parse_and(struct parser *p);
static struct node *parse_unary(struct parser *p);
static struct node *parse_primary(struct parser *p);

static struct node *parse_expr(struct parser *p) { return parse_or(p); }

static struct node *parse_or(struct parser *p) {
  struct node *lhs = parse_and(p);
  if (!lhs) return NULL;

  while (p->lx.cur.kind == TOK_OR && !p->err) {
    parse_advance(p);
    struct node *rhs = parse_and(p);
    if (!rhs) {
      node_free(lhs);
      return NULL;
    }
    struct node *tmp = node_new(NODE_OR, 0, lhs, rhs);
    if (!tmp) {
      node_free(lhs);
      node_free(rhs);
      p->err = 1;
      return NULL;
    }
    lhs = tmp;
  }
  return lhs;
}

static struct node *parse_and(struct parser *p) {
  struct node *lhs = parse_unary(p);
  if (!lhs) return NULL;

  while (p->lx.cur.kind == TOK_AND && !p->err) {
    parse_advance(p);
    struct node *rhs = parse_unary(p);
    if (!rhs) {
      node_free(lhs);
      return NULL;
    }
    struct node *tmp = node_new(NODE_AND, 0, lhs, rhs);
    if (!tmp) {
      node_free(lhs);
      node_free(rhs);
      p->err = 1;
      return NULL;
    }
    lhs = tmp;
  }
  return lhs;
}

static struct node *parse_unary(struct parser *p) {
  if (p->lx.cur.kind == TOK_NOT) {
    parse_advance(p);
    struct node *inner = parse_unary(p);
    if (!inner) return NULL;
    struct node *tmp = node_new(NODE_NOT, 0, inner, NULL);
    if (!tmp) {
      node_free(inner);
      p->err = 1;
      return NULL;
    }
    return tmp;
  }
  return parse_primary(p);
}

static struct node *parse_primary(struct parser *p) {
  if (p->lx.cur.kind == TOK_VAR) {
    char v = p->lx.cur.var;
    if (v >= 'A' && v <= 'Z') {
      p->used[v - 'A'] = true;
    }
    parse_advance(p);
    struct node *n = node_new(NODE_VAR, v, NULL, NULL);
    if (!n) {
      p->err = 1;
    }
    return n;
  }

  if (parse_accept(p, TOK_LPAREN)) {
    struct node *inside = parse_expr(p);
    if (!inside) return NULL;
    if (!parse_expect(p, TOK_RPAREN)) {
      node_free(inside);
      return NULL;
    }
    return inside;
  }

  p->err = 1;
  return NULL;
}

static int node_eval(const struct node *n, const bool val[26]) {
  switch (n->kind) {
    case NODE_VAR:
      return val[n->var - 'A'] ? 1 : 0;
    case NODE_NOT:
      return node_eval(n->a, val) ? 0 : 1;
    case NODE_AND:
      return (node_eval(n->a, val) && node_eval(n->b, val)) ? 1 : 0;
    case NODE_OR:
      return (node_eval(n->a, val) || node_eval(n->b, val)) ? 1 : 0;
    default:
      return 0;
  }
}

static size_t collect_vars_sorted(const bool used[26], char out[26]) {
  size_t n = 0;
  for (int i = 0; i < 26; i++) {
    if (used[i]) {
      out[n++] = (char)('A' + i);
    }
  }
  return n;
}

int main(void) {
  char *line = NULL;
  size_t cap = 0;
  ssize_t gl = getline(&line, &cap, stdin);
  if (gl < 0) {
    free(line);
    return 1;
  }

  /* Trim trailing newline(s). */
  while (gl > 0 && (line[gl - 1] == '\n' || line[gl - 1] == '\r')) {
    line[--gl] = '\0';
  }

  struct parser p;
  memset(&p, 0, sizeof(p));
  p.lx.s = line;
  p.lx.i = 0;
  parse_advance(&p); /* prime first token */

  if (p.lx.cur.kind == TOK_END) {
    free(line);
    return 2;
  }

  struct node *root = parse_expr(&p);
  if (!root || p.err) {
    node_free(root);
    free(line);
    return 2;
  }

  /* Ensure the entire line was consumed. */
  if (p.lx.cur.kind != TOK_END) {
    node_free(root);
    free(line);
    return 2;
  }

  char vars[26];
  size_t nvars = collect_vars_sorted(p.used, vars);

  /* Header: variables in sorted order, then Result. */
  for (size_t i = 0; i < nvars; i++) {
    if (i) putchar(' ');
    putchar(vars[i]);
  }
  if (nvars) putchar(' ');
  printf("Result\n");

  const uint64_t rows = (nvars == 0) ? 1ULL : (1ULL << nvars);
  for (uint64_t mask = 0; mask < rows; mask++) {
    bool val[26] = {0};

    /*
     * Row order matches a binary counter over the sorted variables:
     * the leftmost variable changes the slowest.
     */
    for (size_t j = 0; j < nvars; j++) {
      int bit = (int)((mask >> (nvars - 1 - j)) & 1ULL);
      val[vars[j] - 'A'] = (bit != 0);

      if (j) putchar(' ');
      putchar(bit ? '1' : '0');
    }

    if (nvars) putchar(' ');
    int r = node_eval(root, val);
    putchar(r ? '1' : '0');
    putchar('\n');
  }

  node_free(root);
  free(line);
  return 0;
}

