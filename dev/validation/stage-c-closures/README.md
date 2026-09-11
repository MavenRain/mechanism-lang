# Dependent closure validation

The gate battery has 37 passing legs.  It exits 1 because the inherited
TRUSTED-LINES bound remains exceeded: kernel=4208/3000 and
encoder=246/900.  The gate output records each leg and its timing.

The compiler directory records four killed controls, plus passing
baseline and restored suites.  Each control builds without warnings,
fails its designated export on both WASM hosts and keeps the kernel
checks passing.  Run `dev/closure-mutations.py NEW_DIRECTORY` to replay.

The goldens directory records two killed gate controls and a passing
restored suite.  After a build, run
`dev/wasm-golden-mutations.py NEW_DIRECTORY` to replay.  Both scripts
require a new directory outside the repository.

The receipt binds the tested source files and saved outputs by SHA-256.
The four compiler controls also record their own source hashes.  These
records describe this checkpoint, and later source changes require new
validation evidence.
