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

char* append(char* str1, char* str2)
{
  char *result = tor_malloc(strlen(str1)+strlen(str2)+1);
  if (result == NULL) return result;
  result[0] = '\0';
  strcat(result, str1);
  strcat(result, str2);
  return result;
}

void generate_diff(smartlist_t *dest, int **Size,
    smartlist_t *orig, smartlist_t *new, int i, int j)
{
  if (i > 0 && j > 0 && line_eq(orig, new, i-1, j-1)) {
    generate_diff(dest, Size, orig, new, i-1, j-1);
    smartlist_add(dest, append(" ", smartlist_get(orig, i-1)));
  } else if (j > 0 && (i == 0 || Size[i][j-1] >= Size[i-1][j])) {
    generate_diff(dest, Size, orig, new, i, j-1);
    smartlist_add(dest, append("+", smartlist_get(new, j-1)));
  } else if (i > 0 && (j == 0 || Size[i][j-1] < Size[i-1][j])) {
    generate_diff(dest, Size, orig, new, i-1, j);
    smartlist_add(dest, append("-", smartlist_get(orig, i-1)));
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

int read_file(char **buffer, char *path)
{
  long size;
  FILE *fh = fopen(path, "r");
  if (fh != NULL)
  {
    fseek(fh, 0L, SEEK_END);
    size = ftell(fh);
    rewind(fh);
    *buffer = tor_malloc(size);
    if (*buffer != NULL)
    {
      fread(*buffer, size, 1, fh);
    }
    if (fh != NULL) fclose(fh);
  }
  return (int)size;
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
  char *buf1, *buf2;
  int size1 = read_file(&buf1, argv[1]);
  int size2 = read_file(&buf2, argv[2]);
  tor_split_lines(orig, buf1, size1);
  tor_split_lines(new, buf2, size2);

  diff(result, orig, new);

  SMARTLIST_FOREACH_BEGIN(result, char *, cp) {
    printf("%s\n", cp);
  } SMARTLIST_FOREACH_END(cp);

  tor_free(buf1);
  tor_free(buf2);
  SMARTLIST_FOREACH_BEGIN(result, char *, cp) {
    tor_free(cp);
  } SMARTLIST_FOREACH_END(cp);

  smartlist_free(orig);
  smartlist_free(new);
  smartlist_free(result);

  return 0;
}

// vim: et sw=2
