First, [build `libmatoya`](https://github.com/matoya/libmatoya/wiki/Building). The following compiler commands assume that your working directory is this test directory.

#### Windows
```
nmake
```

#### Linux / macOS
```
make
````

#### Coverage
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

#### JSON

For additional edge case testing, you can put the `.json` files from [this repo](https://github.com/nst/JSONTestSuite/tree/master/test_parsing) in the `json` subdirectory.
