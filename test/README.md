### Usage
First, [build `libmatoya`](https://github.com/matoya/libmatoya/wiki/Building). The makefiles assume that your working directory is this test directory and the `libmatoya` static library resides in the `../bin` directory.

Use `make` or `nmake` with different targets to compile the unit test suite or the examples. For example, `make test` compiles the test suite, `make 0-minimal` compiles the first example.

### Targets

| Target       | Description                                                     |
| ------------ | --------------------------------------------------------------- |
| `test`       | `libmatoya` test suite.                                         |
| `0-minimal`  | The most basic `libmatoya` app and event loop.                  |
| `1-draw`     | Building on `0-minimal`, fetches and renders a PNG image.       |
| `2-threaded` | Buidling on `1-draw`, uses a thread for non-blocking rendering. |

### Test Coverage
- Crypto
- File
- JSON
- Log
- Memory
- Net
- Struct
- System
- TLS (via Net)
- Thread
- Time
- Version

### JSON

For additional edge case testing, you can put the `.json` files from [this repo](https://github.com/nst/JSONTestSuite/tree/master/test_parsing) in the `json` subdirectory.
