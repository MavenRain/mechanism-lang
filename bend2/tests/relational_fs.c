#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *path; char **names; size_t used, capacity; } RelationalDir;

static void relational_dir_call(IoWork *w) {
  RelationalDir *s = (RelationalDir *)w->data;
  DIR *directory = opendir(s->path);
  if (!directory) { w->code = errno; return; }
  for (;;) {
    errno = 0;
    struct dirent *entry = readdir(directory);
    if (!entry) { if (errno) w->code = errno; break; }
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
    if (s->used == s->capacity) {
      size_t next = s->capacity ? s->capacity * 2 : 16;
      if (next < s->capacity || next > SIZE_MAX / sizeof(char *)) { w->code = EOVERFLOW; break; }
      char **grown = realloc(s->names, next * sizeof(char *));
      if (!grown) { w->code = ENOMEM; break; }
      s->names = grown; s->capacity = next;
    }
    char *name = strdup(entry->d_name);
    if (!name) { w->code = ENOMEM; break; }
    s->names[s->used++] = name;
  }
  if (closedir(directory) != 0 && !w->code) w->code = errno;
}

static Term relational_dir_pack(Env e, IoWork *w) {
  RelationalDir *s = (RelationalDir *)w->data;
  Term result;
  if (w->code) result = io_fail(e, w->code, NULL);
  else {
    Term rows = term_pak(CID_NIL, 0);
    for (size_t i = s->used; i > 0; --i)
      rows = io_node(e, CID_CON, io_str(e, s->names[i - 1], strlen(s->names[i - 1])), rows);
    result = io_done(e, rows);
  }
  for (size_t i = 0; i < s->used; ++i) free(s->names[i]);
  free(s->names); free(s->path); free(s);
  return result;
}

Term relational_list_dir_run(Env e, Term *f, IoWork *w) {
  w->code = 0;
  uint64_t length = 0;
  RelationalDir *s = io_mem(calloc(1, sizeof(*s)));
  s->path = io_cstr(e, f[0], &length);
  w->data = (char *)s;
  if (io_nul(s->path, length)) { w->code = EILSEQ; return relational_dir_pack(e, w); }
  return io_work(w, relational_dir_call, relational_dir_pack);
}

static void __attribute__((constructor)) relational_list_dir_use(void) {
  io_eff(CID_RELATIONAL_LIST_DIR, relational_list_dir_run, 0);
}
