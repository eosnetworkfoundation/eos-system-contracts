# Reference contracts

Reference contracts are a collection of contracts deployable to an [Antelope](https://github.com/AntelopeIO) blockchain which implements a lot of critical functionality that goes beyond what is provided by the base Antelope protocol.

The Antelope protocol includes capabilities such as:
* an accounts and permissions system which enables a flexible permission system that allows authorization authority over specific actions in a transaction to be satisfied by the appropriate combination of signatures;
* a consensus algorithm to propose and finalize blocks by a set of active block producers that can be arbitrarily selected by privileged smart contracts running on the blockchain;
* a basic resource management system that tracks usage of CPU/NET and RAM per account and enforces limits based on per-account quotas that can be adjusted by privileged smart contracts.

However, the Antelope protocol itself does not immediately provide:
* a mechanism for multiple accounts to reach consensus on authorization of a proposed transaction on-chain before executing it;
* a consensus mechanism that goes beyond the consensus algorithm to determine how block producers are selected and to align incentives by providing appropriate rewards and punishments to block producers or the entities that get them into that position;
* more sophisticated resource management systems that create markets for users to acquire resource rights;
* or, even something as seemingly basic as the concept of tokens (whether fungible or non-fungible).

The reference contracts in this repository provide all of the above and more by building higher-level features or abstractions on top of the primitive mechanisms provided by the Antelope protocol.

The collection of reference contracts consists of the following individual contracts:

* [boot contract](contracts/eosio.boot/include/eosio.boot/eosio.boot.hpp): A minimal contract that only serves the purpose of activating protocol features which enables other more sophisticated contracts to be deployed onto the blockchain. (Note: this contract must be deployed to the privileged `eosio` account.)
* [bios contract](contracts/eosio.bios/include/eosio.bios/eosio.bios.hpp): A simple alternative to the core contract which is suitable for test chains or perhaps centralized blockchains. (Note: this contract must be deployed to the privileged `eosio` account.)
* [token contract](contracts/eosio.token/include/eosio.token/eosio.token.hpp): A contract enabling fungible tokens.
* [core contract](contracts/eosio.system/include/eosio.system/eosio.system.hpp): A monolithic contract that includes a variety of different functions which enhances a base Antelope blockchain for use as a public, decentralized blockchain in an opinionated way. (Note: This contract must be deployed to the privileged `eosio` account. Additionally, this contract requires that the token contract is deployed to the `eosio.token` account and has already been used to setup the core token.) The functions contained within this monolithic contract include (non-exhaustive):
   + Delegated Proof of Stake (DPoS) consensus mechanism for selecting and paying (via core token inflation) a set of block producers that are chosen through delegation of the staked core tokens.
   + Allocation of CPU/NET resources based on core tokens in which the core tokens are either staked for an indefinite allocation of some fraction of available CPU/NET resources, or they are paid as a fee in exchange for a time-limited allocation of CPU/NET resources via REX or via PowerUp.
   + An automated market maker enabling a market for RAM resources which allows users to buy or sell available RAM allocations.
   + An auction for bidding for premium account names.
* [multisig contract](contracts/eosio.msig/include/eosio.msig/eosio.msig.hpp): A contract that enables proposing Antelope transactions on the blockchain, collecting authorization approvals for many accounts, and then executing the actions within the transaction after authorization requirements of the transaction have been reached. (Note: this contract must be deployed to a privileged account.)
* [wrap contract](contracts/eosio.wrap/include/eosio.wrap/eosio.wrap.hpp): A contract that wraps around any Antelope transaction and allows for executing its actions without needing to satisfy the authorization requirements of the transaction. If used, the permissions of the account hosting this contract should be configured to only allow highly trusted parties (e.g. the operators of the blockchain) to have the ability to execute its actions. (Note: this contract must be deployed to a privileged account.)

## Repository organization

The `main` branch contains the latest state of development; do not use this for production. Refer to the [releases page](https://github.com/AntelopeIO/reference-contracts/releases) for current information on releases, pre-releases, and obsolete releases as well as the corresponding tags for those releases.

## Supported Operating Systems

[CDT](https://github.com/AntelopeIO/cdt) is required to build contracts. Any operating systems supported by CDT is sufficient to build the reference contracts.

To build and run the tests as well, [Leap](https://github.com/AntelopeIO/leap) is also required as a dependency, which may have its further restrictions on supported operating systems.
## Building

The build guide below will assume you are running Ubuntu 20.04. However, as mentioned above, other operating systems may also be supported.

### Build or install CDT dependency

The CDT dependency is required with a minimum version of 3.0.

The easiest way to satisfy this dependency is to install CDT on your system through a package. Find the release of a compatible version of CDT from its [releases page](https://github.com/AntelopeIO/cdt/releases), download the package file appropriate for your OS from the attached assets, and install the package.

Alternatively, you can build CDT from source. Please refer to the guide in the [CDT README](https://github.com/AntelopeIO/cdt#building-from-source) for instructions on how to do this. If you choose to go with building CDT from source, please keep the path to the build directory in the shell environment variable `CDT_BUILD_PATH` for later use when building the reference contracts.

### Optionally build Leap dependency

The Leap dependency is optional. It is only needed if you wish to also build the tests using the `BUILD_TESTS` CMake flag.

Unfortunately, it is not currently possible to satisfy the contract testing dependencies through the Leap packages made available from the [Leap releases page](https://github.com/AntelopeIO/leap/releases). So if you want to build the contract tests, you will first need to build Leap from source.

Please refer to the guide in the [Leap README](https://github.com/AntelopeIO/leap#building-from-source) for instructions on how to do this. If you choose to go with building Leap from source, please keep the path to the build directory in the shell environment variable `LEAP_BUILD_PATH` for later use when building the reference contracts.

### Build reference contracts

Beyond CDT and optionally Leap (if also building the tests), no additional dependencies are required to build the reference contracts.

The instructions below assume you are building the reference contracts with tests, have already built Leap from source, and have the CDT dependency installed on your system. For some other configurations, expand the hidden panels placed lower within this section.

For all configurations, you should first `cd` into the directory containing cloned reference contracts repository.

Build reference contracts with tests using Leap built from source and with installed CDT package:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -Dleap_DIR="${LEAP_BUILD_PATH}/lib/cmake/leap" ..
make -j $(nproc)
```

**Note:** `CMAKE_BUILD_TYPE` has no impact on the WASM files generated for the contracts. It only impacts how the test binaries are built. Use `-DCMAKE_BUILD_TYPE=Debug` if you want to create test binaries that you can debug.

<details>
<summary>Build reference contracts with tests using Leap and CDT both built from source</summary>

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" -Dleap_DIR="${LEAP_BUILD_PATH}/lib/cmake/leap" ..
make -j $(nproc)
```
</details>

<details>
<summary>Build reference contracts without tests and with CDT build from source</summary>

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -Dcdt_DIR="${CDT_BUILD_PATH}/lib/cmake/cdt" ..
make -j $(nproc)
```

</details>

#### Supported CMake options

The following is a list of custom CMake options supported in building the reference contracts (default values are shown below):

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