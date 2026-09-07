(lang dune 3.24)

(name mechanism)

; S0-D1.  The package is declared at Stage 0, in the pin's form, so that
; Stage A adds source files only and no build file changes.  allow_empty
; holds while lib/ is empty.  zarith is the kernel's arbitrary precision
; host and it is pinned to the version the vendored tree needs.
(package
 (name mechanism)
 (allow_empty)
 (depends (zarith (= 1.14))))
