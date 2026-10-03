// Execute the project's own C++ rules test with the existing Node runtime.
const { readFile } = require('node:fs/promises');
const { WASI } = require('node:wasi');

async function main() {
  const wasi = new WASI({ version: 'preview1', args: [], env: {}, preopens: {}, returnOnExit: true });
  const module = await WebAssembly.compile(await readFile(process.argv[2] || 'build/host-tests/core_test.wasm'));
  const instance = await WebAssembly.instantiate(module, { wasi_snapshot_preview1: wasi.wasiImport });
  process.exitCode = wasi.start(instance);
}
main().catch(error => { console.error(error.message); process.exitCode = 1; });
