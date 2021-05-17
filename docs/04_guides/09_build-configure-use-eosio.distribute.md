---
content_title: How to build, configure, and use eosio.distribute
link_text: How to build, configure, and use eosio.distribute
---
# Build, Configure, and Use eosio.distribute

## Overview

The `eosio.distribute` contract is a general contract used to distribute tokens sent to an account. The distribution is decided by an array of accounts and percentages. Accounts can use the action `claimdistrib` to receive accumulated tokens. A special action `donatetorex` is called if the distribution account is `eosio.rex`. This is to enable the distribution of tokens to be given out to REX participants. The `eosio.system` contract can be enabled to send inflation to the `eosio.dist` account by compiling with the `USE_INFLATION_DISTRIBUTE` flag.

## Build
The following steps will compile the `eosio.system` contract with inflation directed to `eosio.dist`. Assumes the [eosio.system](https://github.com/eosio/eosio.system) repository has been cloned. 

```bash
mkdir build
cd build
cmake -DUSE_INFLATION_DISTRIBUTE=true ..
make -j4 
```

Or run the `build.sh`:

```bash
build.sh -d
```

## Configuration

The `setdistrib` action takes a vector of `distribute_account` objects, which is an account name and the percentage the account will receive.

```cpp
struct distribute_account {
        name        account;
        uint16_t    percent;
    };
```

Example call for `setdistrib`

The below command will distribute 50% of the tokens received to `eosio.rex` and 50% to `eosio.saving`

```bash
cleos push action eosio.dist setdistrib '[[{"account":"eosio.rex", "percent":5000}, {"account":"eosio.saving", "percent":5000}]]' -p eosio.dist
```

Example call to `claimdistrib`

The below command will claim rewards and remove the table entry in `claimers` for `eosio.saving`

```bash
cleos push action eosio.dist claimdistrib "[eosio.saving]" -p eosio.saving
```
```output
executed transaction: ddfffc3fa29377ed346ed8a5f45502270a588cc9b20d94927469980f602c5e84  104 bytes  217 us
#    eosio.dist <= eosio.dist::claimdistrib     {"claimer":"eosio.saving"}
#   eosio.token <= eosio.token::transfer        {"from":"eosio.dist","to":"eosio.saving","quantity":"315184.4357 SYS","memo":"distribution claim"}
#    eosio.dist <= eosio.token::transfer        {"from":"eosio.dist","to":"eosio.saving","quantity":"315184.4357 SYS","memo":"distribution claim"}
#  eosio.saving <= eosio.token::transfer        {"from":"eosio.dist","to":"eosio.saving","quantity":"315184.4357 SYS","memo":"distribution claim"}
```

Example token transfer to `eosio.dist`

Any transfer of `eosio.token` to `eosio.dist` will distribute tokens as described in the `state` singleton. In this case because the special case `eosio.rex` is a recipient there is a call to `donatetorex` to update REX appropriately.

```bash
cleos transfer eosio eosio.dist "100.0000 SYS" "random transfer" -p eosio@owner
```
```output
executed transaction: 39abce44320f5921dc810baad7642060dd4bd7c9f6a64595e183418df07f022a  144 bytes  499 us
#   eosio.token <= eosio.token::transfer        {"from":"eosio","to":"eosio.dist","quantity":"100.0000 SYS","memo":"random transfer"}
#         eosio <= eosio.token::transfer        {"from":"eosio","to":"eosio.dist","quantity":"100.0000 SYS","memo":"random transfer"}
#    eosio.dist <= eosio.token::transfer        {"from":"eosio","to":"eosio.dist","quantity":"100.0000 SYS","memo":"random transfer"}
#         eosio <= eosio::donatetorex           {"from":"eosio.dist","amount":"50.0000 SYS","memo":"donation from eosio to eosio.rex"}
#   eosio.token <= eosio.token::transfer        {"from":"eosio.dist","to":"eosio.rex","quantity":"50.0000 SYS","memo":"donation from eosio to eosio....
#    eosio.dist <= eosio.token::transfer        {"from":"eosio.dist","to":"eosio.rex","quantity":"50.0000 SYS","memo":"donation from eosio to eosio....
#     eosio.rex <= eosio.token::transfer        {"from":"eosio.dist","to":"eosio.rex","quantity":"50.0000 SYS","memo":"donation from eosio to eosio....
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```