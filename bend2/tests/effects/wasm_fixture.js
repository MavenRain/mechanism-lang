function wasm_fixture_io(operation, first, second) {
  const fs = require('node:fs');
  const path = require('node:path');
  const child = require('node:child_process');
  try {
    if (operation === 0) {
      const parent = path.dirname(second);
      if (!fs.existsSync(parent)) fs.mkdirSync(parent);
      if (!fs.existsSync(second)) fs.mkdirSync(second);
      return '+' + fs.readdirSync(path.join(first, 'fixtures')).filter(x => x.endsWith('.kan'))
        .map(x => x.slice(0, -4)).sort().join('\n');
    }
    if (operation === 1) {
      const result = child.spawnSync('wasm-opt', [first + '.wasm', '-S', '-o', first + '.wat',
        '--enable-gc', '--enable-reference-types', '--enable-tail-call', '--enable-exception-handling'],
        {encoding: 'utf8'});
      fs.writeFileSync(first + '.err', result.stderr || '');
      if (result.error) return '-' + result.error.message;
      if (result.status !== 0) return '-' + (result.stderr || `wasm-opt exit ${result.status}`).split('\n')[0].trim();
      return '+';
    }
    return '-unknown Wasm fixture operation';
  } catch (error) {
    return '-' + error.message;
  }
}
