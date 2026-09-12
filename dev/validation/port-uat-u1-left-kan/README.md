# Left Kan extension validation

Base: 7c6d50d.  Coverage combines the complete battery and scoped
rechecks of its two timed-out source suites.  All 43 checks other
than TRUSTED-LINES pass.  The inherited trusted-line failure is
kernel=4208/3000 and encoder=246/900.

gates.log retains the initial battery: 41 PASS legs, two source
watchdog expiries at 300 s and the trusted-line failure.  The
category and functor source suites then pass with 900 s ceilings,
in 206.417 s and 215.939 s.  rechecks.log and rechecks.json retain
those results.  watchdogs.diff shows the subsequent ceiling changes.
Every runtime gate passed in the original battery.

gates.json records the initial and final coverage, measurements,
52 source hashes, checker hashes and evidence-file hashes.  It
retains every previous gate and adds the two left Kan gates.
left-kan-mutations.json records six detected controls, passing
baseline and restored suites, input hashes and complete stream
hashes.  mutations.log is the replay transcript.  Full control
streams are emitted into the replay's NEW_DIRECTORY.

Staging removed one terminal blank line from left-kan-identity.mech.
gates.json records both hashes and the exact whitespace operation.
The mutation report retains the original tested-input hash.

The source and tests are the replay contract.  After a build:

```sh
zsh dev/gates.sh
python3 -I dev/left-kan-mutations.py NEW_DIRECTORY
```

The battery still exits 1 for the trusted-line ruling.  The mutation
replay exits 0.  The larger category group requires higher watchdog
ceilings; template checking and erasure performance remain open.
