#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "container.h"

typedef struct {
  const char* content;
  size_t len;
} line_t;

int max(int a, int b)
{
  return (a > b) ? a : b;
}

inline int line_eq(smartlist_t *orig, smartlist_t *new, int i1, int i2)
{
  /*printf("%d %d\n", i1, i2);*/
  char *line1 = smartlist_get(orig, i1);
  char *line2 = smartlist_get(new, i2);
  /*printf("ok\n");*/
  return strcmp(line1, line2) == 0;
}

void print_diff(int **C, smartlist_t *orig, smartlist_t *new, int i, int j)
{
  if (i > 0 && j > 0 && line_eq(orig, new, i, j)) {
    print_diff(C, orig, new, i-1, j-1);
    printf("  %s\n", smartlist_get(orig, i));
  } else if (j > 0 && (i == 0 || C[i][j-1] >= C[i-1][j])) {
    print_diff(C, orig, new, i, j-1);
    printf("+ %s\n", smartlist_get(new, j));
  } else if (i > 0 && (j == 0 || C[i][j-1] < C[i-1][j])) {
    print_diff(C, orig, new, i-1, j);
    printf("- %s\n", smartlist_get(orig, i));
  } else {
    printf("\n");
  }
}

void diff(smartlist_t *orig, smartlist_t *new)
{
  int m = smartlist_len(orig);
  int n = smartlist_len(new);
  int i, j;
  int **C = tor_malloc(sizeof(int*) * (m+1));
  for (i=0; i<m+1; ++i)
  {
    C[i] = tor_malloc(sizeof(int) * (n+1));
  }

  for (i=0; i<=m; i++) {
    for (j=0; j<=n; j++) {
      if (i == 0 || j == 0) {
        C[i][j] = 0;
      } else if (line_eq(orig, new, i-1, j-1)) {
        C[i][j] = C[i-1][j-1] + 1;
      } else {
        C[i][j] = max(C[i-1][j], C[i][j-1]);
      }
    }
  }

  printf("Length of LCS is %d\n", C[m][n]);
  print_diff(C, orig, new, m-1, n-1);
  for (i=0; i<m+1; ++i)
  {
    tor_free(C[i]);
  }
  tor_free(C);
}

int main()
{
  smartlist_t *orig = smartlist_new();
  smartlist_t *new = smartlist_new();
  int m = smartlist_split_string(orig, "AAA:GGG:GGG:TTT:AAA:BBB", ":", 0, 0);
  int n = smartlist_split_string(new, "GGG:XXX:TTT:XXX:AAA:YYY:BBB", ":", 0, 0);
  /*printf("%d\n", m);*/
  /*printf("%d\n", n);*/

  /*SMARTLIST_FOREACH_BEGIN(orig, char *, cp) {*/
    /*printf("%d: %s\n", cp_sl_idx, cp);*/
  /*} SMARTLIST_FOREACH_END(cp);*/

  /*SMARTLIST_FOREACH_BEGIN(new, char *, cp) {*/
    /*printf("%d: %s\n", cp_sl_idx, cp);*/
  /*} SMARTLIST_FOREACH_END(cp);*/

  diff(orig, new);

  smartlist_free(orig);
  smartlist_free(new);

  return 0;
}

// vim: et sw=2
