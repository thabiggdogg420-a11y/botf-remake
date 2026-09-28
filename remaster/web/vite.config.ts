import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

// Static, relocatable build: one self-contained index.html (JS, CSS, fonts and the
// WebAssembly core are all inlined), so the dist/ folder can be published anywhere.
export default defineConfig({
  base: './',
  plugins: [preact(), viteSingleFile({ removeViteModuleLoader: true })],
  build: {
    target: 'es2022',
    outDir: 'dist',
    assetsInlineLimit: 100_000_000,
    chunkSizeWarningLimit: 4096,
    cssCodeSplit: false,
    reportCompressedSize: false,
  },
  server: { port: 5173 },
});
