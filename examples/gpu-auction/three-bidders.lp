\ One bundle of eight slices. Priority A, B, C. Credits are integers.
\ Ranked objective = 3 * reported welfare + fixed winner priority.
Maximize
 ranked_welfare: 38 x_A + 28 x_B + 21 x_C
Subject To
 one_bundle: x_A + x_B + x_C = 1
 mig_capacity: 8 x_A + 8 x_B + 8 x_C <= 8
Bounds
 0 <= x_A <= 1
 0 <= x_B <= 1
 0 <= x_C <= 1
Binary
 x_A x_B x_C
End
