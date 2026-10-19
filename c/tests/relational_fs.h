/* Port of tests/relational_fs.bend (and its effect body relational_fs.c): list a directory
 * and keep the filenames as separate strings. A library module (no main): the relational drivers
 * link it through relational_common. */
#ifndef MECH_RELATIONAL_FS_H
#define MECH_RELATIONAL_FS_H

#include "kernel.h"

/* Result<U32 & String, List<String>>: code 0 is Done with the names (Str pointers in readdir order,
 * no sort, "." and ".." skipped); any other code is the errno of the failure (EILSEQ for a NUL byte
 * inside the path, ENOMEM when the heap is exhausted). */
typedef struct {
  int code;
  Vec names;
} RelationalDir;

RelationalDir relational_list_dir(Heap *heap, Str path);

#endif
