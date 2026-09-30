import { runReactor, writeStream } from './reactor.mjs';

const usage = 'Usage: node runtime/run.mjs MODULE.wasm [ARG ...]\n';
const [modulePath, ...args] = process.argv.slice(2);

// Every failure here leaves the same trace: one `kanon reactor:` line on
// stderr and exit 2. A closed output pipe makes even the usage write fail,
// so the two usage writes run inside the same guard as the reactor run.
try {
  if (modulePath === '--help' && args.length === 0) {
    await writeStream(process.stdout, usage);
  } else if (!modulePath) {
    await writeStream(process.stderr, usage);
    process.exitCode = 64;
  } else {
    // A POSIX status holds eight bits, and the host truncates a larger
    // number, so exit 256 would be reported as success. An out-of-range
    // code is a runtime error instead.
    const code = await runReactor(modulePath, args);
    if (!Number.isInteger(code) || code < 0 || code > 255) throw new RangeError(`exit code ${code} out of range`);
    process.exitCode = code;
  }
} catch (error) {
  const message = error instanceof Error ? error.message : String(error);
  const line = `kanon reactor: ${message.replace(/[\r\n]+/g, ' ')}\n`;
  await writeStream(process.stderr, line).catch(() => {});
  process.exitCode = 2;
}
