/* Port of tests/surface_family.bend (oracle label unit-surface-family): Family.instance_names
 * derives the specialised family, companion and member names, honours exports and reuse, refuses
 * incomplete or colliding exports, and map_member refuses postulate members. */
#include "driver.h"
#include "surface.h"

static Vec one_item(Heap *heap, void *item) {
  Vec items = vec_new(heap->arena);
  if (item == NULL) heap->oom = true;
  else if (!vec_push(&items, item)) heap->oom = true;
  return items;
}

static Decl *decl_box(Heap *heap, Str name, DeclKind kind, const Term *ty, bool has_body, const Term *body) {
  Decl *decl = heap_alloc(heap, sizeof(Decl));
  if (decl == NULL) return NULL;
  decl->name = name;
  decl->kind = kind;
  decl->ty = ty;
  decl->body.some = has_body;
  decl->body.value = body;
  return decl;
}

static CtorDecl *ctor_box(Heap *heap, const char *name) {
  CtorDecl *ctor = heap_alloc(heap, sizeof(CtorDecl));
  if (ctor == NULL) return NULL;
  ctor->name = str_lit(name);
  ctor->args = vec_new(heap->arena);
  ctor->res_params = vec_new(heap->arena);
  ctor->res_idx = vec_new(heap->arena);
  return ctor;
}

static FamilyDecl family_decl(Heap *heap, const char *name, const LevelVar *level) {
  FamilyDecl decl;
  decl.name = str_lit(name);
  decl.params = vec_new(heap->arena);
  decl.indices = vec_new(heap->arena);
  decl.level = level;
  return decl;
}

static FamilyRaw *raw_box(Heap *heap, FamilyDecl family, Vec ctors) {
  FamilyRaw *raw = heap_alloc(heap, sizeof(FamilyRaw));
  if (raw == NULL) return NULL;
  raw->family = family;
  raw->ctors = ctors;
  return raw;
}

static PolyRename *rename_box(Heap *heap, const char *source, const char *target) {
  PolyRename *rename = heap_alloc(heap, sizeof(PolyRename));
  if (rename == NULL) return NULL;
  rename->source = str_lit(source);
  rename->target = str_lit(target);
  return rename;
}

static FamilyMaybeVec no_exports(Heap *heap) {
  FamilyMaybeVec exports;
  exports.some = false;
  exports.value = vec_new(heap->arena);
  return exports;
}

static FamilyMaybeVec some_exports(Vec renames) {
  FamilyMaybeVec exports;
  exports.some = true;
  exports.value = renames;
  return exports;
}

/* scheme() */
static FamilyScheme scheme(Heap *heap) {
  FamilyScheme scheme;
  scheme.arity = heap_nat_of_u64(heap, 1);
  scheme.family = family_decl(heap, "Box", level_var(heap, nat_zero()));
  scheme.ctors = one_item(heap, ctor_box(heap, "box"));
  scheme.companions = one_item(heap, raw_box(heap, family_decl(heap, "Tag", level_var(heap, nat_zero())), vec_new(heap->arena)));
  scheme.members = one_item(heap, decl_box(heap, str_lit("unwrap"), DECL_DEFINITION, term_univ(heap, level_var(heap, nat_zero())),
                                           true, term_auto()));
  scheme.origin = global_empty();
  return scheme;
}

static const Str *name_at(const Vec *names, size_t index) {
  VecGet got = vec_get(names, index);
  if (got.status != VEC_GET_OK) return NULL;
  return (const Str *)got.value;
}

static bool name_is(const Vec *names, size_t index, const char *expected) {
  const Str *name = name_at(names, index);
  return name != NULL && str_eq(*name, str_lit(expected));
}

/* expected_names(result, family, companion, member) */
static bool expected_names(MaybeFamilyInstanceNames result, const char *family, const char *companion, const char *member) {
  if (!result.some || result.value.families.len != 2 || result.value.members.len != 1) return false;
  return name_is(&result.value.families, 0, family) && name_is(&result.value.families, 1, companion) &&
         name_is(&result.value.members, 0, member);
}

/* reused(result) */
static bool reused(MaybeFamilyInstanceNames result) {
  if (!result.some || result.value.families.len != 1 || result.value.members.len != 1) return false;
  return name_is(&result.value.families, 0, "Small") && name_is(&result.value.members, 0, "Small_unwrap");
}

/* refused(result) */
static bool refused(MaybeFamilyInstanceNames result) { return !result.some; }

/* postulate_refused(result) */
static bool postulate_refused(Result result) {
  return result.kind == RESULT_FAIL && result.error.kind == ERROR_NOT_YET &&
         str_eq(result.error.message, str_lit("family schema members must be definitions"));
}

/* main() */
static bool suite(Heap *heap) {
  FamilyRow row;
  FamilyCatalog catalog = &row;
  Str box = str_lit("Box");
  Str small = str_lit("Small");
  Vec no_reuse = vec_new(heap->arena);
  Vec reuse_tag = one_item(heap, rename_box(heap, "Tag", "ExistingTag"));
  Decl bad;
  row.name = box;
  row.scheme = scheme(heap);
  row.rest = NULL;
  bad.name = str_lit("bad");
  bad.kind = DECL_POSTULATE;
  bad.ty = term_univ(heap, level_var(heap, heap_nat_of_u64(heap, 9)));
  bad.body.some = false;
  bad.body.value = NULL;
  return expected_names(family_instance_names(heap, catalog, box, small, &no_reuse, no_exports(heap)), "Small", "Small_Tag",
                        "Small_unwrap") &&
         expected_names(family_instance_names(heap, catalog, box, small, &no_reuse,
                                              some_exports(one_item(heap, rename_box(heap, "unwrap", "get")))),
                        "Small", "Small_Tag", "get") &&
         reused(family_instance_names(heap, catalog, box, small, &reuse_tag, no_exports(heap))) &&
         refused(family_instance_names(heap, catalog, box, small, &no_reuse, some_exports(vec_new(heap->arena)))) &&
         refused(family_instance_names(heap, catalog, box, small, &no_reuse,
                                       some_exports(one_item(heap, rename_box(heap, "unwrap", "box"))))) &&
         refused(family_instance_names(heap, catalog, box, small, &no_reuse,
                                       some_exports(one_item(heap, rename_box(heap, "unwrap", "Small_Tag"))))) &&
         refused(family_instance_names(heap, catalog, box, small, &no_reuse,
                                       some_exports(one_item(heap, rename_box(heap, "missing", "get"))))) &&
         postulate_refused(budget_run(heap,
                                      family_map_member(heap, bad,
                                                        poly_action(poly_scope_levels(nat_zero()), vec_new(heap->arena),
                                                                    vec_new(heap->arena))),
                                      budget_unlimited()));
}

/* The bend_surface.js harness prints PASS <name> on true and dies with 1 and FAIL <name> on false. */
static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  bool ok = suite(&heap);
  int code = ok ? driver_print(str_lit("PASS surface-family")) : driver_die(1, "FAIL surface-family");
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
