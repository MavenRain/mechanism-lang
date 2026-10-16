#include "driver.h"
#include "import.h"

/* A projection adds its owner only to its own summary, even when its subject is cached. */
static bool projection_summary_isolated(Heap *heap) {
  Str source = str_lit("{\"meta\":{\"exporter\":{\"name\":\"lean4export\",\"version\":\"3.1.0\"},\"format\":{\"version\":\"3.1.0\"},\"lean\":{\"githash\":\"68218e876d2a38b1985b8590fff244a83c321783\",\"version\":\"4.31.0\"}}}\n{\"in\":1,\"str\":{\"pre\":0,\"str\":\"R\"}}\n{\"in\":2,\"str\":{\"pre\":0,\"str\":\"X\"}}\n{\"in\":3,\"str\":{\"pre\":0,\"str\":\"P\"}}\n{\"in\":4,\"str\":{\"pre\":0,\"str\":\"Q\"}}\n{\"il\":1,\"succ\":0}\n{\"ie\":1,\"sort\":1}\n{\"ie\":2,\"const\":{\"name\":2,\"us\":[]}}\n{\"ie\":3,\"proj\":{\"typeName\":1,\"idx\":0,\"struct\":2}}\n{\"axiom\":{\"name\":1,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":2,\"levelParams\":[],\"type\":1,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":3,\"levelParams\":[],\"type\":3,\"isUnsafe\":false}}\n{\"axiom\":{\"name\":4,\"levelParams\":[],\"type\":2,\"isUnsafe\":false}}\n");
  ExportResult exported = export_read_string(heap, source);
  if (exported.kind == JSON_FAIL) return false;
  TranslateResult translated = translate_translate(heap, &exported.value);
  if (translated.kind == JSON_FAIL) return false;
  TableGet cached = table_get(translate_summaries(translated.value), str_lit("2"));
  TableGet projected = table_get(translate_summaries(translated.value), str_lit("3"));
  if (!cached.present || !projected.present) return false;
  TranslateSummary subject = *(TranslateSummary *)cached.value;
  TranslateSummary projection = *(TranslateSummary *)projected.value;
  Vec subject_keys = translate_set_keys(heap, &subject.constants);
  Vec projection_keys = translate_set_keys(heap, &projection.constants);
  return subject_keys.len == 1 && projection_keys.len == 2 &&
         !translate_set_has(&subject.constants, str_lit("1")) &&
         translate_set_has(&projection.constants, str_lit("1"));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  const char *invalid[] = {
      "\"\x80\"", "\"\xc0\xaf\"", "\"\xe2\x82\"", "\"\xe0\x80\x80\"",
      "\"\xf4\x90\x80\x80\"", "\"\xff\"", "\"\xed\xa0\x80\"",
      "{\"\x80\":null}"};
  int failures = 0;
  if (!projection_summary_isolated(&heap)) {
    fprintf(stderr, "FAIL projection mutated a cached dependency summary\n");
    ++failures;
  }
  for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
    if (json_parse(&heap, str_lit(invalid[i])).kind != JSON_FAIL) {
      fprintf(stderr, "FAIL invalid UTF-8 case %zu was accepted\n", i);
      ++failures;
    }
  }
  Str valid = str_lit("\"\xc2\x80\xe0\xa0\x80\xef\xbf\xbf\xf4\x8f\xbf\xbf\"");
  JsonResult parsed = json_parse(&heap, valid);
  if (parsed.kind != JSON_DONE || !str_eq(json_to_string(&heap, parsed.value), valid)) {
    fprintf(stderr, "FAIL valid UTF-8 did not round-trip\n");
    ++failures;
  }
  int code = heap.oom ? driver_host_failure() : (failures == 0 ? driver_print(str_lit("PASS import_regression")) : 1);
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
