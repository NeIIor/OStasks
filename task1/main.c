#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int cmp_int(const void *a, const void *b) {
  const int ia = *(const int *)a;
  const int ib = *(const int *)b;
  return (ia > ib) - (ia < ib);
}

struct sort_job {
  int *base;
  size_t len;
};

static void *sort_thread(void *arg) {
  struct sort_job *job = (struct sort_job *)arg;
  qsort(job->base, job->len, sizeof(int), cmp_int);
  return NULL;
}

struct run_cursor {
  size_t pos;
  size_t end;
};

struct heap_node {
  int value;
  size_t run_idx;
};

struct min_heap {
  struct heap_node *a;
  size_t n;
};

/*
 * Minimal min-heap for k-way merge:
 * heap_node.value is the current "head" element of a sorted run,
 * heap_node.run_idx points to the run it comes from.
 */
static void heap_sift_up(struct min_heap *h, size_t i) {
  while (i > 0) {
    size_t p = (i - 1) / 2;
    if (h->a[p].value <= h->a[i].value) {
      break;
    }
    struct heap_node tmp = h->a[p];
    h->a[p] = h->a[i];
    h->a[i] = tmp;
    i = p;
  }
}

static void heap_sift_down(struct min_heap *h, size_t i) {
  for (;;) {
    size_t l = 2 * i + 1;
    size_t r = 2 * i + 2;
    size_t m = i;
    if (l < h->n && h->a[l].value < h->a[m].value) {
      m = l;
    }
    if (r < h->n && h->a[r].value < h->a[m].value) {
      m = r;
    }
    if (m == i) {
      break;
    }
    struct heap_node tmp = h->a[m];
    h->a[m] = h->a[i];
    h->a[i] = tmp;
    i = m;
  }
}

static void heap_push(struct min_heap *h, struct heap_node x) {
  h->a[h->n++] = x;
  heap_sift_up(h, h->n - 1);
}

static struct heap_node heap_pop(struct min_heap *h) {
  struct heap_node out = h->a[0];
  h->a[0] = h->a[--h->n];
  if (h->n) heap_sift_down(h, 0);
  return out;
}

static int parse_threads(const char *s, size_t *out) {
  errno = 0;
  char *end = NULL;
  unsigned long v = strtoul(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0') {
    return -1;
  }
  if (v == 0 || v > SIZE_MAX) {
    return -1;
  }
  *out = (size_t)v;
  return 0;
}

static int read_all_ints(int **out_a, size_t *out_n) {
  size_t cap = 1024;
  size_t n = 0;
  int *a = (int *)malloc(cap * sizeof(*a));
  if (!a) {
    return -1;
  }

  for (;;) {
    int x;
    int rc = scanf("%d", &x);
    if (rc == 1) {
      if (n == cap) {
        if (cap > SIZE_MAX / 2 / sizeof(*a)) {
          free(a);
          errno = ENOMEM;
          return -1;
        }
        cap *= 2;
        int *na = (int *)realloc(a, cap * sizeof(*a));
        if (!na) {
          free(a);
          return -1;
        }
        a = na;
      }
      a[n++] = x;
      continue;
    }
    if (rc == EOF) {
      break;
    }
    free(a);
    errno = EINVAL;
    return -1;
  }

  *out_a = a;
  *out_n = n;
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <threads>\n", argv[0]);
    return 2;
  }

  size_t threads = 0;
  if (parse_threads(argv[1], &threads) != 0) {
    fprintf(stderr, "invalid threads: %s\n", argv[1]);
    return 2;
  }

  int *a = NULL;
  size_t n = 0;
  if (read_all_ints(&a, &n) != 0) {
    if (errno == EINVAL) {
      fprintf(stderr, "failed to read integer input\n");
    } else {
      perror("input");
    }
    return 1;
  }

  if (n == 0) {
    free(a);
    return 0;
  }

  if (threads > n) {
    threads = n;
  }
  if (threads == 0) {
    threads = 1;
  }

  pthread_t *tids = (pthread_t *)calloc(threads, sizeof(*tids));
  struct sort_job *jobs = (struct sort_job *)calloc(threads, sizeof(*jobs));
  if (!tids || !jobs) {
    perror("calloc");
    free(tids);
    free(jobs);
    free(a);
    return 1;
  }

  /*
   * Split the array into 'threads' contiguous runs with lengths that sum to N.
   * Each run is sorted independently in its own thread.
   */
  size_t base = 0;
  for (size_t i = 0; i < threads; i++) {
    size_t rem = n - base;
    size_t chunk = rem / (threads - i);
    jobs[i].base = a + base;
    jobs[i].len = chunk;
    base += chunk;
    int prc = pthread_create(&tids[i], NULL, sort_thread, &jobs[i]);
    if (prc != 0) {
      fprintf(stderr, "pthread_create failed\n");
      for (size_t j = 0; j < i; j++) {
        pthread_join(tids[j], NULL);
      }
      free(tids);
      free(jobs);
      free(a);
      return 1;
    }
  }

  for (size_t i = 0; i < threads; i++) {
    pthread_join(tids[i], NULL);
  }

  struct run_cursor *runs = (struct run_cursor *)calloc(threads, sizeof(*runs));
  struct heap_node *heap_buf = (struct heap_node *)malloc(threads * sizeof(*heap_buf));
  if (!runs || !heap_buf) {
    perror("alloc");
    free(runs);
    free(heap_buf);
    free(tids);
    free(jobs);
    free(a);
    return 1;
  }

  base = 0;
  for (size_t i = 0; i < threads; i++) {
    runs[i].pos = base;
    runs[i].end = base + jobs[i].len;
    base = runs[i].end;
  }

  /*
   * k-way merge:
   * - keep the current head element of each run in a min-heap
   * - pop the smallest, output it, then advance that run and push its next head
   */
  struct min_heap h = {.a = heap_buf, .n = 0};
  for (size_t i = 0; i < threads; i++) {
    if (runs[i].pos < runs[i].end) {
      heap_push(&h, (struct heap_node){.value = a[runs[i].pos], .run_idx = i});
      runs[i].pos++;
    }
  }

  size_t out_i = 0;
  while (h.n) {
    struct heap_node node = heap_pop(&h);
    if (out_i++ > 0) putchar(' ');
    printf("%d", node.value);

    struct run_cursor *r = &runs[node.run_idx];
    if (r->pos < r->end) {
      heap_push(&h, (struct heap_node){.value = a[r->pos], .run_idx = node.run_idx});
      r->pos++;
    }
  }
  putchar('\n');

  free(runs);
  free(heap_buf);
  free(tids);
  free(jobs);
  free(a);
  return 0;
}

