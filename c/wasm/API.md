# Native backend API

The C files in this directory port the Bend 2.0.25 modules of commit 8c90b64 and keep their definition names in snake_case, in the same order; the signatures below use the Bend notation of those modules. Binary output is a byte list with one byte per element (`List<&2,U32>` in that notation), preserving arbitrary bytes independently of Unicode strings.

- `Gc_encode.encode(module:Gc_encode.T) -> List<&2,U32>` writes Wasm GC modules, including recursive type groups, typed function references, tail calls, imports, exports and declared function references.
- `Emit.program(rows:List<&2,Common.Pair<String,Erase.Entry>>, export:String) -> Result<&2,&2,Error.T,List<&2,U32>>` implements the existing no-argument Nat entry ABI.
- `Emit.reactor(rows, exports:List<&2,String>) -> Result<&2,&2,Error.T,List<&2,U32>>` implements reusable exports. Nat crosses as checked i32. Aggregate values cross as immutable typed envelopes.
- `Link.build(rows) -> Result<&2,&2,Error.T,Link.T>` collects concrete type and function indices, nominal family recursive groups, closure application helpers and runtime functions.
- `Nat_runtime.runtime_func(layout:Link.T,name:String) -> Result<&2,&2,Error.T,Gc_encode.Func>` emits exact base-32768 natural arithmetic.
- `Circuit.depth(globals:List<&2,Common.Pair<String,Term.T>>,term:Term.T) -> Result<&2,&2,String,Bignum.T>` computes circuit depth with captured closure environments and bounded nominal elimination.
- `Eterm.print_ktm`, `print_decl`, and `print_repr` preserve erased syntax output.

`Erase.Entry` is `Dropped{}` / `Postulate{repr:Eterm.Repr}` / `Code{decls:List<&2,Eterm.Kdecl>}`. `Erase.program(globals,rows)` produces these declarations from the checked kernel rows.

The original M0/M1 restrictions on strings, thunks, and unsupported host function values retain explicit errors. These are source behavior, not migration fallbacks.
