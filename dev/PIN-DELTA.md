# PIN delta

Every mechanism-lang source file that overlays a file of the vendored
kanon tree at PIN has one row here.  dev/pin-delta.sh diffs each file
against `git -C vendor/kanon show 936a43a92dd59a04698648f24fa5ae94cdb532df:PATH`
and compares the changed line count with the expected column.  The count
is the line count of the `diff` output.  The overlay scope is lib/,
wasm/, surface/ and bin/ (S0-D6).

At Stage 0 the table holds no row, because lib/ is empty until Stage A.
Stage A adds the first rows with the D3 level overlay.

| file | expected |
| --- | --- |
