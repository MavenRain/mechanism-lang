# Pinned Veil fixture snapshot

`pinned/` contains byte-exact tracked fixture, negative, golden, and runner assets
from Veil at the commit recorded in `provenance.json`. No compiler sources or
generated Wasm binaries are included.

`test/` and `dev/` provide the active test root. They preserve the pinned data
except for the existing eight Mechanism WAT overlays from `test/golden/wasm`.
Both original and active bytes are recorded in the provenance manifest.
The canonical Wasm protocol argument is `test/veil/test`.
