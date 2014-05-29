#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

typedef struct {
  const char* content;
  size_t len;
} line_t;

INLINE int max(int a, int b)
{
  return (a > b) ? a : b;
}

INLINE int line_eq(smartlist_t *orig, smartlist_t *new, int i1, int i2)
{
  char *line1 = smartlist_get(orig, i1);
  char *line2 = smartlist_get(new, i2);
  int r = strcmp(line1, line2) == 0;
  return r;
}

void generate_diff(smartlist_t *dest, int **Size,
    smartlist_t *orig, smartlist_t *new, int i, int j)
{
  char *line;
  if (i > 0 && j > 0 && line_eq(orig, new, i-1, j-1)) {
    generate_diff(dest, Size, orig, new, i-1, j-1);
    tor_asprintf(&line, " %s", smartlist_get(orig, i-1));
    smartlist_add(dest, line);
  } else if (j > 0 && (i == 0 || Size[i][j-1] >= Size[i-1][j])) {
    generate_diff(dest, Size, orig, new, i, j-1);
    tor_asprintf(&line, "+%s", smartlist_get(new, j-1));
    smartlist_add(dest, line);
  } else if (i > 0 && (j == 0 || Size[i][j-1] < Size[i-1][j])) {
    generate_diff(dest, Size, orig, new, i-1, j);
    tor_asprintf(&line, "-%s", smartlist_get(orig, i-1));
    smartlist_add(dest, line);
  }
}

void diff(smartlist_t *result, smartlist_t *orig, smartlist_t *new)
{
  int m = smartlist_len(orig);
  int n = smartlist_len(new);
  int i, j;
  int **Size = tor_malloc(sizeof(int*) * (m+1));
  for (i=0; i<=m; ++i)
  {
    Size[i] = tor_malloc(sizeof(int) * (n+1));
  }

  for (i=0; i<=m; i++) {
    for (j=0; j<=n; j++) {
      if (i == 0 || j == 0) {
        Size[i][j] = 0;
      } else if (line_eq(orig, new, i-1, j-1)) {
        Size[i][j] = Size[i-1][j-1] + 1;
      } else {
        Size[i][j] = max(Size[i-1][j], Size[i][j-1]);
      }
    }
  }

  /*printf("Length of LCS is %d\n", Size[m][n]);*/
  generate_diff(result, Size, orig, new, m, n);
  for (i=0; i<=m; ++i)
  {
    tor_free(Size[i]);
  }
  tor_free(Size);
}

int main(int argc, char **argv)
{
  smartlist_t *orig = smartlist_new();
  smartlist_t *new = smartlist_new();
  smartlist_t *result = smartlist_new();
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s file1 file2\n", argv[0]);
    return 1;
  }
  char *cons1 = read_file_to_str(argv[1], 0, NULL);
  char *cons2 = read_file_to_str(argv[2], 0, NULL);
  tor_split_lines(orig, cons1, strlen(cons1));
  tor_split_lines(new, cons2, strlen(cons2));

  diff(result, orig, new);

  SMARTLIST_FOREACH_BEGIN(result, char *, cp) {
    printf("%s\n", cp);
  } SMARTLIST_FOREACH_END(cp);

  tor_free(cons1);
  tor_free(cons2);
  SMARTLIST_FOREACH_BEGIN(result, char *, cp) {
    tor_free(cp);
  } SMARTLIST_FOREACH_END(cp);

  smartlist_free(orig);
  smartlist_free(new);
  smartlist_free(result);

  return 0;
}

// vim: et sw=2
