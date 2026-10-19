/* Port of cli/cert.bend (the mech_cert program): read SOURCE, check it from the initial globals
 * with an unlimited budget, refuse a source that declares axioms, then print CHECKED axioms=0 and one
 * NAME<TAB>VALUE line per NAT_EXPORT whose whnf value is a natural number literal. Every failure dies
 * with exit 1 and its message; the first failing export wins and nothing is printed before it. The
 * binding lists are Vecs of CertBinding pointers in Bend list order. */
#include "cli.h"
#include "surface.h"

static CertBindingResult binding_done(const CertBinding *binding) {
  CertBindingResult result = {IO_DONE, binding, str_empty()};
  return result;
}

static CertBindingResult binding_fail(Str message) {
  CertBindingResult result = {IO_FAIL, NULL, message};
  return result;
}

static CertBindingsResult bindings_done(Vec bindings) {
  CertBindingsResult result = {IO_DONE, bindings, str_empty()};
  return result;
}

static CertBindingsResult bindings_fail(Heap *heap, Str message) {
  CertBindingsResult result = {IO_FAIL, vec_new(heap->arena), message};
  return result;
}

/* Binding{name, value}; NULL (heap->oom) when the cell fails. */
static const CertBinding *binding_new(Heap *heap, Str name, Str value) {
  CertBinding *binding = heap_alloc(heap, sizeof(CertBinding));
  if (binding == NULL) return NULL;
  binding->name = name;
  binding->value = value;
  return binding;
}

/* natural (17-24) */
CertBindingResult cert_natural(Heap *heap, const Value *value, Str name) {
  if (value->kind != VALUE_LIT) return binding_fail(heap_str_cat(heap, str_lit("not a literal: "), name));
  switch (value->lit.kind) {
  case LITERAL_INT: return binding_done(binding_new(heap, name, big_to_string(heap, value->lit.integer)));
  case LITERAL_STRING: return binding_fail(heap_str_cat(heap, str_lit("not a natural number: "), name));
  }
  return binding_fail(heap_str_cat(heap, str_lit("not a literal: "), name));
}

/* normal (26-31) */
CertBindingResult cert_normal(Heap *heap, Result result, Str name) {
  switch (result.kind) {
  case RESULT_DONE: return cert_natural(heap, (const Value *)result.value, name);
  case RESULT_FAIL: return binding_fail(error_to_string(heap, result.error));
  }
  return binding_fail(error_to_string(heap, result.error));
}

/* evaluated (33-38) */
CertBindingResult cert_evaluated(Heap *heap, Result result, Globals globals, Str name) {
  switch (result.kind) {
  case RESULT_DONE: return cert_normal(heap, eval_whnf(heap, globals, (const Value *)result.value), name);
  case RESULT_FAIL: return binding_fail(error_to_string(heap, result.error));
  }
  return binding_fail(error_to_string(heap, result.error));
}

/* prepend (40-45): Con{binding, rest} */
CertBindingsResult cert_prepend(Heap *heap, CertBindingsResult result, const CertBinding *binding) {
  switch (result.status) {
  case IO_DONE: {
    Vec head = vec_new(heap->arena);
    if (binding == NULL || !vec_push(&head, (void *)binding)) heap->oom = true;
    Vec bindings = vec_concat(heap->arena, &head, &result.bindings);
    if (bindings.len != head.len + result.bindings.len) heap->oom = true;
    return bindings_done(bindings);
  }
  case IO_FAIL: return bindings_fail(heap, result.message);
  }
  return bindings_fail(heap, result.message);
}

/* next (52-57) */
CertBindingsResult cert_next(Heap *heap, CertBindingResult result, Globals globals, const Vec *exports, size_t index) {
  switch (result.status) {
  case IO_FAIL: return bindings_fail(heap, result.message);
  case IO_DONE: return cert_prepend(heap, cert_values(heap, globals, exports, index), result.binding);
  }
  return bindings_fail(heap, result.message);
}

/* values (59-64): the exports evaluate in order and the first failure stops the list. */
CertBindingsResult cert_values(Heap *heap, Globals globals, const Vec *exports, size_t index) {
  VecGet got = vec_get(exports, index);
  switch (got.status) {
  case VEC_GET_OUT_OF_RANGE: return bindings_done(vec_new(heap->arena));
  case VEC_GET_OK: break;
  }
  if (heap->oom) return bindings_fail(heap, str_empty());
  Str name = *(const Str *)got.value;
  const Term *term = term_global(heap, name);
  if (term == NULL) return bindings_fail(heap, str_empty());
  Result value = eval_eval(heap, globals, vec_new(heap->arena), term);
  return cert_next(heap, cert_evaluated(heap, value, globals, name), globals, exports, index + 1);
}

/* String.join(names, ", ") */
static Str join_names(Heap *heap, const Vec *names) {
  Str joined = str_empty();
  size_t i = 0;
  for (; i < names->len; i = i + 1) {
    VecGet got = vec_get(names, i);
    Str name = got.status == VEC_GET_OK ? *(const Str *)got.value : str_empty();
    joined = heap_str_cat(heap, i == 0 ? joined : heap_str_cat(heap, joined, str_lit(", ")), name);
  }
  return joined;
}

/* allowed (66-71) */
CertBindingsResult cert_allowed(Heap *heap, const Vec *axioms, Globals globals, const Vec *exports, size_t index) {
  if (axioms->len == 0) return cert_values(heap, globals, exports, index);
  return bindings_fail(heap, heap_str_cat(heap, str_lit("certificate declares axioms: "), join_names(heap, axioms)));
}

/* checked (73-78) */
CertBindingsResult cert_checked(Heap *heap, Result result, const Vec *exports, size_t index) {
  switch (result.kind) {
  case RESULT_FAIL: return bindings_fail(heap, error_to_string(heap, result.error));
  case RESULT_DONE: {
    const ElabChecked *checked = (const ElabChecked *)result.value;
    Vec axioms = elab_axiom_names(heap, &checked->rows);
    return cert_allowed(heap, &axioms, checked->globals, exports, index);
  }
  }
  return bindings_fail(heap, error_to_string(heap, result.error));
}

/* show (80-87): IO.print(name ++ "\t" ++ value) per binding. */
int cert_show(const Vec *bindings) {
  size_t i = 0;
  for (; i < bindings->len; i = i + 1) {
    VecGet got = vec_get(bindings, i);
    if (got.status == VEC_GET_OK) {
      const CertBinding *binding = got.value;
      Str parts[] = {binding->name, str_lit("\t"), binding->value};
      cli_print_parts(parts, 3);
    }
  }
  return 0;
}

/* report (89-96) */
int cert_report(Heap *heap, CertBindingsResult result) {
  if (heap->oom) return cli_host_failure();
  switch (result.status) {
  case IO_FAIL: return cli_die(1, result.message);
  case IO_DONE:
    cli_print(str_lit("CHECKED axioms=0"));
    return cert_show(&result.bindings);
  }
  return cli_die(1, result.message);
}

/* read_result (98-103) */
int cert_read_result(Heap *heap, IoResult result, const Vec *exports, size_t index) {
  if (heap->oom) return cli_host_failure();
  switch (result.status) {
  case IO_FAIL: return cli_die(1, result.value);
  case IO_DONE:
    return cert_report(heap, cert_checked(heap, elab_check_in_compact(heap, global_initial(heap), result.value), exports, index));
  }
  return cli_die(1, result.value);
}

/* arguments (105-112) */
int cert_arguments(Heap *heap, const Vec *args) {
  VecGet source = vec_get(args, 0);
  switch (source.status) {
  case VEC_GET_OUT_OF_RANGE: return cli_die(1, str_lit("usage: mech_cert SOURCE [NAT_EXPORT ...]"));
  case VEC_GET_OK: return cert_read_result(heap, io_read_file(heap, *(const Str *)source.value), args, 1);
  }
  return cli_die(1, str_lit("usage: mech_cert SOURCE [NAT_EXPORT ...]"));
}
