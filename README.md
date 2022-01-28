# Mandel Contracts

## Repo organization

| branch                | description |
| ------                | ----------- |
| `main`                | Development for future releases |
| `release/3.0.x`       | 3.0.x-* series of releases |

## Supported Operating Systems

To speed up development, we're starting with **Ubuntu 20.04**. We'll support additional operating systems as time and personnel allow. In the mean time, they may break.

## Building

To speed up development and reduce support overhead, we're initially only supporting the following build approach. CMake options not listed here may break. Build scripts may break. Dockerfiles may break.

### Ubuntu 20.04 dependencies

```
apt-get update && apt-get install   \
        binaryen                    \
        build-essential             \
        ccache                      \
        cmake                       \
        curl                        \
        git                         \
        libboost-all-dev            \
        libcurl4-openssl-dev        \
        libgmp-dev                  \
        libssl-dev                  \
        libtinfo5                   \
        libusb-1.0-0-dev            \
        libzstd-dev                 \
        llvm-11-dev                 \
        npm                         \
        ninja-build                 \
        pkg-config                  \
        time

#
# Install CDT v1.7.0
#
curl -LO https://github.com/EOSIO/eosio.cdt/releases/download/v1.7.0/eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb
dpkg -i eosio.cdt_1.7.0-1-ubuntu-18.04_amd64.deb

#
# Build Mandel from sources. Not needed if you use
# -DBUILD_TESTS=no
#
mkdir -p ~/work
cd ~/work
git clone https://github.com/eosnetworkfoundation/mandel.git
cd mandel
git submodule update --init --recursive
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_WASM_SPEC_TESTS=yes ..
make -j $(nproc)
```

### Building

```
# Where you built Mandel
export PATH=~/work/mandel/build/bin:$PATH

# cd to this contracts repo  
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=yes ..
make -j $(nproc)
```

We support the following CMake options:
```
-DCMAKE_BUILD_TYPE=DEBUG                    Debug builds
-DBUILD_TESTS=no                            Don't build the tests; it
                                            builds much faster without them
-DEOSIO_SYSTEM_CONFIGURABLE_WASM_LIMITS=no  Disables use of the CONFIGURABLE_WASM_LIMITS
                                            protocol feature
-DEOSIO_SYSTEM_BLOCKCHAIN_PARAMETERS=no     Disables use of the BLOCKCHAIN_PARAMETERS
                                            protocol feature
```

### Running tests

```
cd build/tests
ctest -j $(nproc)
```
