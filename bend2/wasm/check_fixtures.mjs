import fs from 'node:fs';
import assert from 'node:assert/strict';

// Validate byte arrays produced by native Bend fixture executables.
const [kind, path] = process.argv.slice(2);
const lines = fs.readFileSync(path, 'utf8').trim().split('\n').map(JSON.parse);
const instantiate = bytes => new WebAssembly.Instance(new WebAssembly.Module(Uint8Array.from(bytes))).exports;
let checks = 0;
if (kind === 'encode') {
  assert.equal(lines.length, 5);
  const x = instantiate(lines[0]);
  for (const name of ['i31', 'struct', 'array', 'if', 'tail', 'closure']) {
    assert.equal(x[name](), 42, name);
    checks++;
  }
  const effect = new WebAssembly.Instance(new WebAssembly.Module(Uint8Array.from(lines[1])),
    {host: {effect: value => value + 2}}).exports;
  assert.equal(effect.run(), 42);
  assert.deepEqual(lines[2], [255, 255, 255, 255, 15]);
  assert.deepEqual(lines[3], [128, 128, 128, 128, 120]);
  assert.deepEqual(lines[4], [127]);
  checks += 4;
} else if (kind === 'oracle') {
  const frozen = JSON.parse(fs.readFileSync(new URL('./oracle_bytes.json', import.meta.url), 'utf8')).fixtures;
  assert.deepEqual(lines.map(x => x.name), frozen.map(x => x.name), 'oracle fixture inventory');
  const expected = new Map(frozen.map(x => [x.name, x]));
  for (const fixture of lines) {
    assert.deepEqual(fixture.bytes, expected.get(fixture.name).bytes, `${fixture.name}: original Wasm bytes`);
    assert.deepEqual(fixture.expected, expected.get(fixture.name).expected, `${fixture.name}: original outcome`);
    const x = instantiate(fixture.bytes);
    if (fixture.expected.trap) {
      assert.throws(() => x.main(), WebAssembly.RuntimeError, fixture.name);
    } else {
      assert.equal(x.main(), fixture.expected.value, fixture.name);
    }
    checks++;
  }
} else if (kind === 'emit') {
  for (const bytes of lines) {
    assert.equal(instantiate(bytes).main(), 42);
    checks++;
  }
} else if (kind === 'nat') {
  const x = instantiate(lines[0]);
  const inputs = [0, 1, 2, 32767, 32768, 65535, 536870911, 1073741822, 1073741823];
  for (const a of inputs) for (const b of inputs) {
    const aa = BigInt(a), bb = BigInt(b);
    const results = { natAdd: aa + bb, natSub: aa < bb ? 0n : aa - bb,
      natMul: aa * bb, natEq: BigInt(a === b), natLt: BigInt(a < b) };
    for (const [op, result] of Object.entries(results)) for (let limb = 0; limb < 5; limb++) {
      const expected = Number((result >> BigInt(limb * 15)) & 32767n);
      assert.equal(x[op](a, b, limb), expected, `${op}(${a},${b}) limb ${limb}`);
      checks++;
    }
  }
} else if (kind === 'reactor') {
  const x = instantiate(lines[0]);
  assert.equal(x.add(40, 2), 42);
  assert.equal(x.sum(x.pair(40, 2)), 42);
  assert.equal(x.unbool(x.bool()), 1);
  assert.throws(() => x.add(1073741824, 0));
  assert.throws(() => x.add(-1, 0));
  assert.throws(() => x.big());
  assert.throws(() => x.sum(x.bool()));
  checks = 7;
} else if (kind === 'layout') {
  for (const bytes of lines) {
    new WebAssembly.Module(Uint8Array.from(bytes));
    checks++;
  }
} else {
  throw new Error('Expected oracle, emit, nat, reactor, or layout fixture kind');
}
console.log(JSON.stringify({ kind, modules: lines.length, checks }));
