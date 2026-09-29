# Native kernel contracts

All datatypes are reusable (`Data`). Lists stored in kernel records use `List<&2, A>`, optional fields use `Maybe<&2, A>`. Expected failures use `Result<&2, &2, Error.T, A>`.

- `quantity.bend`: `T = QZero{} | QOne{} | QMany{}`. One is linear, requiring exactly one use on each returning path.
- `bignum.bend`: `T = Big{negative: Bool, digits: List<&2, U32>}`. Canonical base 32768 limbs, least significant first. Empty digits is zero. `of_decimal(String) -> Maybe<&2,T>`, `of_u32(U32)->T`, `to_string(T)->String`, `add/mul/sub(T,T)->T`. `sub` truncates at zero. `compare(T,T)->Cmp`.
- `literal.bend`: `T = LString{value:String} | LInt{value:Bignum.T}`.
- `level_var.bend`: `T = LZero{} | LSucc{amount:Bignum.T,base:T} | LMax{left:T,right:T} | LIMax{left:T,right:T} | LVar{index:Nat}`. `level.bend` reexports the type and level operations.
- `shape.bend`: generic `T<A:Data>` with `SPi{quantity:Quantity.T,name:String,domain:A}`, `SColl{count:Bignum.T}`, `SPar{left:A,right:A}`, `SMu{name:String,args:List<&2,A>}`, `SNu{name:String,args:List<&2,A>}`, `SZk{quantity:Quantity.T,name:String,domain:A}`, `SFhc{level:A}`, `SMpc{parties:A,access:A}`.
- `term.bend`: generic `Addr<A>` constructors `APt{quantity:Quantity.T,arg:A}`, `ALeg{index:Bignum.T}`, `ACtor{name:String}`. `Leg<A> = Leg{binders:List<&2,Common.Pair<Quantity.T,String>>,body:A}`. `Motive<A> = Motive{ind:Maybe<&2,String>,indices:List<&2,String>,self:String,body:A}`. `ElimData<A> = ElimData{shape:Shape.T<A>,scrut:A,scrut_q:Quantity.T,motive:Maybe<&2,Motive<A>>,branches:List<&2,Common.Pair<Addr<A>,Leg<A>>>}`.
- `term.bend`: `T = Var{index:Nat} | Univ{level:Level.T} | Lan{shape:Shape.T<T>,body:T} | Ran{shape:Shape.T<T>,body:T} | In{shape:Shape.T<T>,addr:Addr<T>,args:List<&2,T>} | Elim{data:ElimData<T>} | Sec{shape:Shape.T<T>,legs:List<&2,Leg<T>>} | Out{shape:Shape.T<T>,addr:Addr<T>,head:T} | Let{name:String,ty:T,value:T,body:T} | Ann{body:T,ty:T} | Global{name:String} | Lit{value:Literal.T} | Auto{}`.
- De Bruijn index zero is the innermost binder. Telescope order is outermost first. Motive body is scoped under indices then self. Leg binders are declaration order.
- `error.bend`: all original error variants retained; `Parse{message:String,line:Nat,column:Nat}`, every other variant has one String field. `message` and `to_string` preserve diagnostic decoration.
- `global.bend`: reusable association lists of entries and positivity families. `find(name:String,globals:T)->Maybe<&2,Entry>`, `find_def` and `find_family` analogous. Entries `Def{value:DefEntry}`, `Axiom{ty:Term.T}`, `Prim{ty:Term.T,prim:Prim.T}`. `DefEntry = DefEntry{ty:Term.T,body:Term.T,reducible:Bool,rec_arg:Maybe<&2,Nat>,partial:Bool}`. Only checked declarations may extend a trusted environment.

Progress: shared contracts fixed; implementation and validation in progress. No full-checker completeness claim yet.


`common.bend` supplies reusable `Pair<A,B> = Pair{first:A,second:B}`. Native Bend `A & B` is affine and is not stored in these reusable records.

`check.bend` context helpers are implemented: `make(globals,budget,level_arity)`, `globals_of`, `env_of`, `size_of`, `names_of`, `bind(name,q,ty,ctx)`, `define(name,q,ty,value,ctx)`. Public checking signatures are `infer(ctx,Quantity.T,Term.T)->Result<Error.T,Value.T>` and `check(ctx,Quantity.T,Term.T,Value.T)->Result<Error.T,Unit>`, with reusable Result quantity arguments. Inference, checking, conversion, all active shape rules, declarations and families are implemented and check.

`budget.bend`: `T = Unlimited{} | Polls{remaining:Nat}`. `Comp(A)` is a lazy affine computation in continuation-passing form: `forall R:Data, (Result<Error.T,A> -> Step<R>) -> Step<R>`, where a step is `Finished{result}` or `Poll{resume:Bool -> Step<R>}`. Composing result continuations avoids revisiting every pending bind when a poll resumes. A poll answer of true means exhausted, matching the original callback convention. `tick()` requests one poll and `tick_with(message)` preserves caller-specific diagnostics. `run(A,action,budget)` returns the result; `run_state(A,action,budget)` returns `Common.Pair{remaining,result}`. Use these runners instead of applying a computation directly.

Implemented and checked: all kernel modules, including Check/Rules/Conv, declaration and family installation, Order/Totality, Pp, and full erasure. The internal mutually recursive checker resides in check_engine.bend; public wrappers remain in check.bend and conv.bend.

Execution tests passed: kernel_numbers, kernel_eval, kernel_quantity, kernel_check, kernel_declarations, kernel_conversion, kernel_recursive, kernel_spec_count, levels_exact, levels_budget, levels_regression, levels_checker. Full integration and original-fixture differential validation are ongoing. No complete validation claim yet.


Poll state belongs to the runner and survives failures and recovery. `bind`, `recover`, and `attempt` suspend at every poll; a refused poll resumes with an ordinary `Budget_exhausted` result, so recovery sees it without resetting state. Conversion's proof probe propagates Budget_exhausted while recovering other errors. `Totality.guard_group_comp(globals,members)` shares the runner with frontend catalog traversal.

Caller-supplied polling uses an affine successor callback:

- `of_poll(Unit -> Callback & Bool) -> Callback` constructs a pure callback.
- `run_poll_state(A,action,callback) -> Callback & Result<Error.T,A>` returns its successor on success or failure; `run_poll` discards that successor.
- `of_poll_io(Unit -> IO(IOCallback & Bool)) -> IOCallback` constructs an effectful callback.
- `run_io_state(A,action,callback) -> IO(IOCallback & Result<Error.T,A>)` preserves the successor; `run_io` returns just the result in IO.

Every `Poll` invokes its callback exactly once. Pure results and non-polling failures invoke none. Callbacks may change their answers after refusal; the runner does not make exhaustion sticky. Returning a successor allows private counters, cancellation sources, throttled clock reads and other driver-owned state without copying affine closures. Existing `do Budget.Comp` bodies and finite-budget Result wrappers remain usable. Callback clients run the corresponding `_comp` operation through a callback runner.

`bend2/tests/kernel_budget_callbacks.bend` supplies executable scripted and clock-backed examples. Its `deadline(expires)` reads the installed `IO.now()` at each requested poll and answers `expires <= now`. The clock uses monotonic milliseconds. This is cooperative cancellation at existing poll points, not preemption or an upper bound on elapsed time between polls. Clock policy and reads remain in the caller, outside kernel checking.

Declaration APIs: `check_decl(globals,budget,decl)->Result<Entry>`, `check_decl_at(globals,budget,arity,decl)`, `check_scheme(globals,budget,arity,decl)->Result<Unit>`, `check_decls(globals,budget,decls)->Result<List<Pair<String,Entry>>>`. Family APIs: `declare_family(globals,budget,decl)->Result<Global.T>`, `define_ctors(globals,budget,group,name,ctors)->Result<Global.T>`, and corresponding `_at` variants adding `arity` after budget. `check_family_scheme(globals,budget,arity,decl,ctors)->Result<Unit>`. All have `_comp` cores with no budget argument and explicit arity where needed. DeclKind is Definition or Postulate.

Structural equality: `Term.equal` compares every raw field including names and universe syntax. `Positivity.same_family` is exact; `same_family_except(a,b,ignore_level,ignore_self_rec)` supports template reuse checks.

Raw checking: `Check.infer_node(ctx, mode, term)` preserves the original scoped raw inference entry point and returns `Engine.Inferred`. It checks universe scope without adding the outer usage-checking poll. Universe conversion and width-zero collection annotation equality run the budgeted level comparison, including internal comparison polls. Unequal former shapes skip annotation comparison.

Erasure APIs: `Erase.program(globals,rows)->Result<List<Pair<String,Erase.Entry>>>`, `program_budget(globals,budget,rows)`, `decl(globals,budget,name,entry)`, `entry(globals,name,entry)`, and `print(rows)`. Entry remains Dropped, Postulate{repr}, Code{decls}. `erase_repr.bend` supplies representation and nominal layout computations; `erase_util.bend` supplies free-variable, runtime-index and capture-pruning utilities. Original declaration order and printed erased representation are preserved. Public signed invalid indices are excluded by Nat types; source parsers still diagnose malformed syntax.

Collection widths and leg addresses preserve signed arbitrary precision values. `Shape.coll(A,n)`, `Term.aleg(A,n)` and `Value.valeg(A,n)` accept small `Nat` values produced by actual list lengths or positions. `Bignum.to_nat_bounded(value,upper)` narrows only after proving `0 <= value <= upper`; `Bignum.at(A,items,index)` uses the concrete list bound. Raw injection and projection shape metadata remains ignored where the original checker ignored it.

`Rules.coll_leg_ty(ctx,legs,index)` accepts a `Bignum.T` index. `Rules.coll_legs_of(ctx,closure)` still returns the actual leg list. `Rules.mpc_parties(ctx,parties)` returns `Result<Error.T,Bignum.T>` and preserves the declared signed width. `Rules.mpc_sub_ty(count)` and `mpc_pred_ty(count)` accept `Bignum.T` and return `Result<Error.T,Term.T>`. They construct nonnegative counts without a Nat ceiling. A negative constructor width returns `Mismatch("an mpc subset width must be nonnegative")`; the original helper raised the host `List.init` exception. Collection eta traversal similarly reports a negative width as `Mismatch("a collection width must be nonnegative")`, replacing the original host exception. These helpers add no budget polls.

`CheckEngine.close_valid` uses explicit cases for outer variables, erased checking, unrestricted binders, and affine versus exact usage. Preserve that case split: Bend 2.0.25 native C miscomputed the former composed boolean predicate for a dynamic linear identity. The permanent `kernel_usage_native` matrix checks all 216 combinations at caller-provided levels; the native replay also retains real positive and negative quantity fixtures.
