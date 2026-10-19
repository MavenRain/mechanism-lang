/* conv (kernel/conv.bend): the budgeted conversion entry points. */
#include "kernel.h"

Budget conv_budget_of(Ctx ctx) { return ctx.budget; }

/* Result<Error.T, Bool> from the generic Result whose payload is the kernel bool box. */
static BoolResult bool_result(Result result) {
  BoolResult out;
  out.kind = result.kind;
  out.value = false;
  out.error = result.error;
  switch (result.kind) {
  case RESULT_DONE:
    out.value = kernel_bool_unbox(result.value);
    return out;
  case RESULT_FAIL:
    return out;
  }
  return out;
}

BoolResult conv_conv(Heap *heap, Ctx ctx, const Value *ty, const Value *a, const Value *b) {
  return bool_result(budget_run(heap, check_engine_conv(heap, ctx, ty, a, b), conv_budget_of(ctx)));
}

BoolResult conv_conv_type(Heap *heap, Ctx ctx, const Value *a, const Value *b) {
  return bool_result(budget_run(heap, check_engine_conv_type(heap, ctx, a, b), conv_budget_of(ctx)));
}
