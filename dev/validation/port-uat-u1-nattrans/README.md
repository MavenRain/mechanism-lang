# Natural-transformation validation

Base: bb215e4.  The gate battery reports 41 PASS legs and the
inherited TRUSTED-LINES failure at kernel=4208/3000 and
encoder=246/900.  No watchdog expired.

gates.log is the complete battery output.  gates.json records its
measurements, the changed checker-input hashes and the hashes of
both evidence files.  It verifies that every previous gate and the
two new gates are present.  nattrans-mutations.json records five
detected controls, passing baseline and restored suites, source
hashes and the checker binary hash.

The source and tests are the replay contract.  After a build, run:

```sh
zsh dev/gates.sh
python3 -I dev/nattrans-mutations.py NEW_DIRECTORY
```

The battery exits 1 because the trusted-line ruling remains open.
The mutation replay exits 0.  The source and runtime gates cover
identity, vertical composition and both whiskering operations.
Runtime checks use eight exports, two payloads and three hosts.
