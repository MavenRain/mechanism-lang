/* Port of bend2/kernel/level_scope.bend: universe levels stay inside the global parameter scope. */
#include "kernel.h"

static void push(Heap *heap, Vec *xs, void *item) {
  if (!vec_push(xs, item)) heap->oom = true;
}

static const Term *term_at(const Vec *xs, size_t index) {
  VecGet got = vec_get(xs, index);
  return got.status == VEC_GET_OK ? (const Term *)got.value : NULL;
}

bool level_scope_valid_node(const Term *term, Nat arity) {
  switch (term->kind) {
  case TERM_UNIV:
    return level_in_scope(arity, term->level);
  case TERM_VAR:
    return true;
  case TERM_LAN:
    return true;
  case TERM_RAN:
    return true;
  case TERM_IN:
    return true;
  case TERM_ELIM:
    return true;
  case TERM_SEC:
    return true;
  case TERM_OUT:
    return true;
  case TERM_LET:
    return true;
  case TERM_ANN:
    return true;
  case TERM_GLOBAL:
    return true;
  case TERM_LIT:
    return true;
  case TERM_AUTO:
    return true;
  }
  return true;
}

/* The Bend worklist recursion (children prepended to the rest) run as a loop over an explicit
 * stack: the same pre-order visit, false as soon as one node fails. */
bool level_scope_valid_walk(Heap *heap, Vec todo, Nat arity) {
  Vec stack = vec_reverse(heap->arena, &todo);
  if (stack.len != todo.len) {
    heap->oom = true;
    return false;
  }
  while (stack.len > 0) {
    const Term *term = term_at(&stack, stack.len - 1);
    stack.len -= 1;
    if (term == NULL) return false;
    if (!level_scope_valid_node(term, arity)) return false;
    {
      Vec kids = term_children(heap, term);
      size_t i;
      for (i = kids.len; i > 0; i--) push(heap, &stack, (void *)term_at(&kids, i - 1));
    }
    if (heap->oom) return false;
  }
  return true;
}

Result level_scope_check(Heap *heap, Nat arity, const Term *term) {
  Vec todo = vec_new(heap->arena);
  push(heap, &todo, (void *)term);
  if (level_scope_valid_walk(heap, todo, arity)) return budget_result_done(kernel_unit());
  return budget_result_fail(
      error_make(ERROR_UNIVERSE, str_lit("universe level is outside the global parameter scope")));
}
