Quick Start
-----------

With emscripten installed:

```text
$ emcc -s ASM_JS=1 raycast.c -o raycast.js --preload-file textures
$ python -m SimpleHTTPServer
```

Open up `localhost:8000` in your asm.js-enabled browser.
