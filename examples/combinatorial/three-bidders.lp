\ Integer welfare; equal optima use the documented verifier tie rule.
Maximize
 welfare: 12 x_0 + 9 x_1 + 7 x_2
Subject To
 tenant_0: 1 x_0 <= 1
 tenant_1: 1 x_1 <= 1
 tenant_2: 1 x_2 <= 1
 slice_0_0: 1 x_0 + 1 x_1 <= 1
 slice_1_0: 1 x_0 + 1 x_1 <= 1
 slice_2_0: 1 x_0 + 1 x_1 <= 1
 slice_3_0: 1 x_0 + 1 x_1 <= 1
 slice_4_0: 1 x_0 + 1 x_2 <= 1
 slice_5_0: 1 x_0 + 1 x_2 <= 1
 slice_6_0: 1 x_0 + 1 x_2 <= 1
 slice_7_0: 1 x_0 + 1 x_2 <= 1
 cpu_0_0: 8 x_0 + 4 x_1 + 4 x_2 <= 8
Bounds
 0 <= x_0 <= 1
 0 <= x_1 <= 1
 0 <= x_2 <= 1
Binary
 x_0
 x_1
 x_2
End
