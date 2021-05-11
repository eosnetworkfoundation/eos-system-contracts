---
content_title: How to build, configure, and use eosio.distribute
link_text: How to build, configure, and use eosio.distribute
---
# Build, Configure, and Use eosio.distribute

## Overview

The `eosio.distribute` contract is a general contract used to distribute tokens sent to an account. The distribution is decided by an array of accounts and percentages. Accounts can use the action `claimdistrib` to receive accumulated tokens. A special action `donatetorex` is called if the distribution account is `eosio.rex`. This is to enable the distribution of tokens to be given out to REX participants. The `eosio.system` contract can be enabled to send inflation to the `eosio.dist` account by compiling with the `USE_INFLATION_DISTRIBUTE` flag.

## Build
The following steps will compile the `eosio.system` contract with inflation directed to `eosio.dist`. Assumes the [eosio.system](https://github.com/eosio/eosio.system) repository has been cloned. 

```
mkdir build
cd build
cmake -DUSE_INFLATION_DISTRIBUTE=true ..
make -j4 
```

Or run the `build.sh`:

```
build.sh -d
```

## Configuration

The `setdistrib` action takes a vector of `distribute_account` objects, which is an account name and the percentage the account will receive.

```
struct distribute_account {
        name        account;
        uint16_t    percent;
    };
```

Example call for `setdistrib`

```
cleos push action eosio.dist setdistrib '[[{"account":"eosio.rex", "percent":5000}, {"account":"eosio.saving", "percent":5000}]]' -p eosio.dist
```
