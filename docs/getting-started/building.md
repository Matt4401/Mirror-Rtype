# Building the Project

Thanks to `xmake`, building the project is incredibly straightforward. You do not need to manually install Raylib, Asio, or GTest on your system; `xmake` will download and compile them automatically.

## 1. Configure the Build

To configure the project (it detects your platform and compiler automatically):

```bash
xmake f -c
```

## 2. Compile

To compile all targets (the engine library, the server, the client, and the unit tests):

```bash
xmake build
```

This will generate the binaries directly in the project root directory.
