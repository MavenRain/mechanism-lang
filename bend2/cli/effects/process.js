function host_process(launcher, runner, wasm, exported, outPath, errPath) {
  const fs = require('node:fs');
  const child = require('node:child_process');
  let out, err;
  try {
    out = fs.openSync(outPath, 'w');
    err = fs.openSync(errPath, 'w');
    const result = child.spawnSync(launcher, [runner, wasm, exported], {
      stdio: ['inherit', out, err],
    });
    if (result.error) {
      fs.writeSync(err, `invalid: ${result.error.message}\n`);
      return 127;
    }
    if (result.signal) {
      const signals = require('node:os').constants.signals;
      return (128 + (signals[result.signal] || 0)) >>> 0;
    }
    return result.status === null ? 127 : result.status >>> 0;
  } catch (error) {
    if (err !== undefined) fs.writeSync(err, `invalid: ${error.message}\n`);
    return 127;
  } finally {
    if (out !== undefined) fs.closeSync(out);
    if (err !== undefined) fs.closeSync(err);
  }
}
