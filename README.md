# System Contracts

System contracts are a collection of contracts deployable to an EOSIO blockchain that implement a lot of critical functionality that is expected from blockchains. For example, the concept of the core token of the blockchain is introduced through the system contracts. In addition, the system contracts can build upon primitives provided by the base EOSIO protocol to enable more useful capabilities, such as:
* decentralized, open-access resource management through staking tokens or paying fees which builds upon the EOSIO protocol's primitive mechanism of assigning resource quotas to accounts;
* higher-level consensus mechanisms (e.g. Delegated Proof of Stake or Proof of Authority) which build upon the EOSIO protocol's base consensus algorithm that advances finality of blocks based on confirmations from a selected set of block producers.

The collection of system contracts consists of the following individual contracts:

* [boot contract](contracts/eosio.boot/include/eosio.boot/eosio.boot.hpp): A minimal contract that only serves the purpose of activating protocol features which enables other more sophisticated contracts to be deployed onto the blockchain. (Note: this contract must be deployed to the privileged `eosio` account.)
* [bios contract](contracts/eosio.bios/include/eosio.bios/eosio.bios.hpp): A simple alternative to the core contract which is suitable for test chains or perhaps centralized blockchains. (Note: this contract must be deployed to the privileged `eosio` account.)
* [token contract](contracts/eosio.token/include/eosio.token/eosio.token.hpp): A contract enabling fungible tokens.
* [core contract](contracts/eosio.system/include/eosio.system/eosio.system.hpp): A monolithic contract that includes a variety of different functions which enhances a base EOSIO blockchain for use as a public, decentralized blockchain in an opinionated way. (Note: This contract must be deployed to the privileged `eosio` account. Additionally, this contract requires that the token contract is deployed to the `eosio.token` account and has already been used to setup the core token.) The functions contained within this monolithic contract include (non-exhaustive):
   + Delegated Proof of Stake (DPoS) consensus mechanism for selecting and paying (via core token inflation) a set of block producers that are chosen through delegation of the staked core tokens.
   + Allocation of CPU/NET resources based on core tokens in which the core tokens are either staked for an indefinite allocation of some fraction of available CPU/NET resources, or they are paid as a fee in exchange for a time-limited allocation of CPU/NET resources via REX or via PowerUp.
   + An automated market maker enabling a market for RAM resources which allows users to buy or sell available RAM allocations.
   + An auction for bidding for premium account names.
* [multisig contract](contracts/eosio.msig/include/eosio.msig/eosio.msig.hpp): A contract that enables proposing EOSIO transactions on the blockchain, collecting authorization approvals for many accounts, and then executing the actions within the transaction after authorization requirements of the transaction have been reached. (Note: this contract must be deployed to a privileged account.)
* [wrap contract](contracts/eosio.wrap/include/eosio.wrap/eosio.wrap.hpp): A contract that wraps around any EOSIO transaction and allows for executing its actions without needing to satify the authorization requirements of the transaction. If used, the permissions of the account hosting this contract should be configured to only allow highly trusted parties (e.g. the operators of the blockchain) to have the ability to execute its actions. (Note: this contract must be deployed to a privileged account.)

## Repository organization

The `main` branch contains the latest state of development; do not use this for production. Refer to the [releases page](https://github.com/eosnetworkfoundation/mandel-contracts/releases) for current information on releases, pre-releases, and obsolete releases as well as the corresponding tags for those releases.
## Supported Operating Systems

[CDT](https://github.com/eosnetworkfoundation/mandel.cdt) is required to build contracts. Any operating systems supported by CDT is sufficient to build the system contracts.

To build and run the tests as well, [EOSIO](https://github.com/eosnetworkfoundation/mandel) is also required as a dependency, which may have its further restrictions on supported operating systems.
## Building

The build guide below will assume you are running Ubuntu 20.04 though, as mentioned above, other operating systems may also be supported.

### Build or install CDT dependency

The CDT dependency is required. This release of the system contracts requires at least version 3.0 of CDT. 

The easiest way to satisfy this dependency is to install CDT on your system through a package. Find the release of a compatible version of CDT from its [releases page](https://github.com/eosnetworkfoundation/mandel.cdt/releases), download the package file appropriate for your OS from the attached assets, and install the package.

Alternatively, you can build CDT from source. Please refer to the guide in the [CDT README](https://github.com/eosnetworkfoundation/mandel.cdt#building) for instructions on how to do this. If you choose to go with building CDT from source, please keep the path to the build directory in the shell environment variable `CDT_BUILD_PATH` for later use when building the system contracts.

### Build or install EOSIO dependency

The EOSIO dependency is optional. It is only needed if you wish to also build the tests using the `BUILD_TESTS` CMake flag.

The easiest way to satisfy this dependency is to install EOSIO on your system through a package. Find the release of a compatible version of EOSIO from its [releases page](https://github.com/eosnetworkfoundation/mandel/releases), download the package file appropriate for your OS from the attached assets, and install the package.

Alternatively, you can build EOSIO from source. Please refer to the guide in the [EOSIO README](https://github.com/eosnetworkfoundation/mandel/#building-from-source) for instructions on how to do this. If you choose to go with building EOSIO from source, please keep the path to the build directory in the shell environment variable `EOSIO_BUILD_PATH` for later use when building the system contracts.

### Build system contracts

Beyond CDT and optionally EOSIO (if also building the tests), no additional dependencies are required to build the system contracts.

The instructions below assuming you are building the system contracts with tests and with both the CDT and EOSIO dependencies installed. For some other configurations, expand the hidden panels placed lower within this section.

For all configurations, you should first `cd` into the directory containing cloned system contracts repository.

Build system contracts with tests and installed CDT and EOSIO packages:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON ..
make -j $(nproc)
```

**Note:** `CMAKE_BUILD_TYPE` has no impact on the WASM files generated for the contracts. It only impacts how the test binaries are built. Use `-DCMAKE_BUILD_TYPE=Debug` if you want to create test binaries that you can debug.

<details>
<summary>Build system contracts with tests and manually built CDT and EOSIO</summary>

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" -Deosio_DIR="${EOSIO_BUILD_PATH}/lib/cmake/eosio" ..
make -j $(nproc)
```
</details>

<details>
<summary>Build system contracts without tests and with manually built CDT</summary>

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" ..
make -j $(nproc)
```

</details>

#### Supported CMake options

The following is a list of custom CMake options supported in building the system contracts (default values are shown below):

```
-DBUILD_TESTS=OFF                       Do not build the tests

-DSYSTEM_CONFIGURABLE_WASM_LIMITS=ON    Enable use of the CONFIGURABLE_WASM_LIMITS
                                        protocol feature

-DSYSTEM_BLOCKCHAIN_PARAMETERS=ON       Enable use of the BLOCKCHAIN_PARAMETERS
                                        protocol feature
```

### Running tests

Assuming you built with `BUILD_TESTS=ON`, you can run the tests.

```
cd build/tests
ctest -j $(nproc)
```

## License

[MIT](LICENSE)