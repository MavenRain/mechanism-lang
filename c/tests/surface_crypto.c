/* Port of tests/surface_crypto.bend (oracle label unit-surface-crypto): checks the seven veil fixture
 * sources (vendor/veil/test, comment lines removed) against the initial globals and prints PASS. */
#include "driver.h"
#include "surface.h"

/* One Common.Pair{name, source} of cases(). */
typedef struct {
  const char *name;
  const char *source;
} Case;

/* cases(): the string table is transcribed verbatim from the Bend driver (same escapes). */
static const Case CASES[] = {
  {"host/zk-pack.kan",
   "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef preimageR : (y : Nat) -> (w : Nat) -> Prop :=\n  fun (y : Nat) (w : Nat) => isTrue (natEq (natMul w w) y)\n\ndef sqProof : (y : Nat) -> (1 w : Nat) -> (r : preimageR y w) ->\n    zk (0 w : Nat) * preimageR y w :=\n  fun (y : Nat) (1 w : Nat) (r : preimageR y w) => prove y w r\n\ndef main : Nat := natMul 3 3"},
  {"host/fhc-pack.kan",
   "\ndef Pk : Type 0 := Nat\ndef Sk : Type 0 := Nat\ndef pk : Pk := 0\ndef sk : Sk := 0\ndef seal : (t : Nat) -> fhc 0 Nat := fun (t : Nat) => enc pk t\ndef addEnc : (c : fhc 0 Nat) -> fhc 1 Nat :=\n  fun (c : fhc 0 Nat) => eval ((fun (x : Nat) => natAdd x 1) : (y : Nat) -> Nat) c\ndef reveal : (1 k : Sk) -> (c : fhc 1 Nat) -> Nat :=\n  fun (1 k : Sk) (c : fhc 1 Nat) => dec k c\n\ndef main : Nat := reveal sk (addEnc (seal 7))"},
  {"host/mpc-pack.kan",
   "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef Party : Prop := sum (prod (), prod ())\ndef alice : Party := inj 0 of 2 ()\ndef bob : Party := inj 1 of 2 ()\ndef allParties : prod (Nat, Nat) := tuple (1, 1)\n\ndef both : (q : prod (Nat, Nat)) -> Prop :=\n  fun (q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)\n\ndef secretA : Nat := 1\ndef secretB : Nat := 1\n\ndef jointSum : (1 a : Nat) -> (1 b : Nat) -> mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat :=\n  fun (1 a : Nat) (1 b : Nat) =>\n    mpc allParties (natAdd : (x : Nat) -> (y : Nat) -> Nat)\n      ((input alice a) : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat)\n      ((input bob b) : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat)\n\ndef joint : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat := jointSum secretA secretB\n\ndef reveal : (0 h : both allParties) -> (1 c : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat) -> Nat :=\n  fun (0 h : both allParties) (1 c : mpc[Party, fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)] Nat) =>\n    open allParties h c\n\ndef opened : Nat := reveal (tuple ()) joint\n\ndef main : Nat := opened"},
  {"shapes/zk-pack.kan",
   "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef preimageR : (y : Nat) -> (w : Nat) -> Prop :=\n  fun (y : Nat) (w : Nat) => isTrue (natEq (natMul w w) y)\n\ndef sqProof : (y : Nat) -> (1 w : Nat) -> (r : preimageR y w) ->\n    zk (0 w : Nat) * preimageR y w :=\n  fun (y : Nat) (1 w : Nat) (r : preimageR y w) => prove y w r\n\ndef stmt : (y : Nat) -> (p : zk (0 w : Nat) * preimageR y w) -> Prop :=\n  fun (y : Nat) (p : zk (0 w : Nat) * preimageR y w) => (h : verify y p) -> prod ()\n\ndef betaConv : (y : Nat) -> (w : Nat) -> (r : preimageR y w) ->\n    (h : verify y (prove y w r : zk (0 w : Nat) * preimageR y w)) -> preimageR y w :=\n  fun (y : Nat) (w : Nat) (r : preimageR y w)\n      (h : verify y (prove y w r : zk (0 w : Nat) * preimageR y w)) => h\n\ndef ok : (y : Nat) -> (p : zk (0 w : Nat) * preimageR y w) -> Prop :=\n  fun (y : Nat) => fun (p : zk (0 w : Nat) * preimageR y w) => verify y p"},
  {"shapes/fhc-pack.kan",
   "\naxiom Pk : Type\naxiom Sk : Type\naxiom pk : Pk\naxiom sk : Sk\ndef seal : (t : Nat) -> fhc 0 Nat := fun (t : Nat) => enc pk t\ndef addEnc : (c : fhc 2 Nat) -> fhc 3 Nat :=\n  fun (c : fhc 2 Nat) => eval (fun (x : Nat) => natAdd x 1) c\ndef reveal : (1 k : Sk) -> (c : fhc 3 Nat) -> Nat :=\n  fun (1 k : Sk) (c : fhc 3 Nat) => dec k c"},
  {"shapes/mpc-pack.kan",
   "\ndef Bool : Type 0 := sum ((prod () : Type 0), (prod () : Type 0))\n\ndef isTrue : (b : Bool) -> Prop :=\n  fun (b : Bool) =>\n    case b as z return Prop with\n    | 0 (x : prod ()) => sum ()\n    | 1 (y : prod ()) => prod ()\n\ndef Party : Prop := sum (prod (), prod ())\ndef alice : Party := inj 0 of 2 ()\ndef bob : Party := inj 1 of 2 ()\ndef allParties : prod (Nat, Nat) := tuple (1, 1)\n\ndef both : (0 q : prod (Nat, Nat)) -> Prop :=\n  fun (0 q : prod (Nat, Nat)) => isTrue (natEq (natAdd q.0 q.1) 2)\n\naxiom secretA : Nat\naxiom secretB : Nat\n\ndef jointSum : (1 a : Nat) -> (1 b : Nat) -> mpc[Party, both] Nat :=\n  fun (1 a : Nat) (1 b : Nat) =>\n    mpc allParties (natAdd : (x : Nat) -> (y : Nat) -> Nat)\n      ((input alice a) : mpc[Party, both] Nat)\n      ((input bob b) : mpc[Party, both] Nat)\n\ndef joint : mpc[Party, both] Nat := jointSum secretA secretB\n\ndef reveal : (0 h : both allParties) -> (1 c : mpc[Party, both] Nat) -> Nat :=\n  fun (0 h : both allParties) (1 c : mpc[Party, both] Nat) =>\n    open allParties h c\n\ndef opened : Nat := reveal (tuple ()) joint"},
  {"fixtures/circuit-fhc.kan",
   "mu FhcLeaf : Type 0 :=\n| fhcLeaf : FhcLeaf\n\ndef namedLeaf : FhcLeaf := fhcLeaf\ndef leafAlias : FhcLeaf := namedLeaf\n\ndef leafCircuit (n : Nat) : Nat :=\n  match leafAlias as x in FhcLeaf return Nat with\n  | fhcLeaf => natMul (natAdd n 1) 2\n\ndef lift (c : fhc 0 Nat) : fhc 2 Nat := eval leafCircuit c"},
};

/* checked(Elab.check_text(Global.initial(), Budget.unlimited(), source), name): a failure becomes
 * Mismatch{name ++ ": " ++ Error.to_string(error)}, which report() dies with. */
static int run_case(Heap *heap, Case c) {
  Result result = elab_check_text(heap, global_initial(heap), budget_unlimited(), str_lit(c.source));
  if (result.kind == RESULT_DONE) return 0;
  Str tail = str_cat(heap->arena, str_lit(": "), error_to_string(heap, result.error));
  Error mismatch = error_make(ERROR_MISMATCH, str_cat(heap->arena, str_lit(c.name), tail));
  return driver_die_text(1, error_to_string(heap, mismatch));
}

/* run(cases) then report(result) */
static int run(Heap *heap) {
  size_t count = sizeof CASES / sizeof CASES[0];
  int code = 0;
  for (size_t i = 0; i < count && code == 0; i++) code = run_case(heap, CASES[i]);
  if (code != 0) return code;
  return driver_print(str_lit("PASS seven original ZK, FHC, MPC and circuit frontend fixtures"));
}

static int program(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Arena arena = arena_new(1 << 20);
  Heap heap = heap_new(&arena);
  int code = run(&heap);
  if (heap.oom) code = driver_host_failure();
  arena_free(&arena);
  return code;
}

int main(int argc, char **argv) { return rt_run_main(argc, argv, program); }
