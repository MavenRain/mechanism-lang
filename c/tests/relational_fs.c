/* Port of bend2/tests/relational_fs.bend: relational_list_dir(path), the directory listing effect
 * of the relational test protocol (bend2/tests/relational_fs.c is the Bend effect body). */
#include "relational_fs.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>

static RelationalDir dir_fail(Heap *heap, int code) { return (RelationalDir){code, vec_new(heap->arena)}; }

static bool has_nul(Str path) { return path.len > 0 && memchr(path.data, 0, path.len) != NULL; }

/* relational_list_dir(path) */
RelationalDir relational_list_dir(Heap *heap, Str path) {
  if (has_nul(path)) return dir_fail(heap, EILSEQ);
  char *cpath = heap_alloc(heap, path.len + 1);
  if (cpath == NULL) return dir_fail(heap, ENOMEM);
  if (path.len > 0) memcpy(cpath, path.data, path.len);
  cpath[path.len] = 0;
  DIR *directory = opendir(cpath);
  if (directory == NULL) return dir_fail(heap, errno);
  RelationalDir out = {0, vec_new(heap->arena)};
  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(directory);
    if (entry == NULL) {
      out.code = errno;
      break;
    }
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
    Str name = str_copy(heap->arena, str_lit(entry->d_name));
    Str *boxed = heap_alloc(heap, sizeof *boxed);
    bool name_lost = name.len > 0 && name.data == NULL;
    if (boxed == NULL || name_lost) {
      out.code = ENOMEM;
      break;
    }
    *boxed = name;
    if (!vec_push(&out.names, boxed)) {
      out.code = ENOMEM;
      break;
    }
  }
  if (closedir(directory) != 0 && out.code == 0) out.code = errno;
  return out;
}
