---
title: RAM as system resource
---

## What is RAM

RAM is the memory, the storage space, where the blockchain stores data. If your contract needs to store data on the blockchain, like in a database, then it can store it in the blockchain's RAM using either a `multi-index table` or a `singleton`.

## RAM High Performance

The EOS blockchain is known for its high performance, which is achieved also because the data stored on the blockchain is using RAM as the storage medium, and thus access to blockchain data is very fast, helping the performance benchmarks to reach levels very few blockchains has been able to.

## RAM Importance

RAM is a very important system resource because of the following reasons:

- It is a limited resource, the public EOS blockchain started with 64GB of RAM and after that the block producers decided to increase the memory with 1KB per block, thus increasing constantly the supply of RAM for its price to not grow too high due to the increased demand from blockchain applications.

- RAM is used in executing many actions sent to the blockchain; creating a new account action, for example, it needs to store in the blockchain memory the new account's information; also when an account accepts a new type of token a new record has to be created, somewhere in the blockchain memory, that holds the balance of the new token accepted, and that memory, the storage space on the blockchain, has to be purchased either by the account that transfers the token or by the account that accepts the new token type.

- The smart contract can not store any additional information if it consumes all its allocated RAM. To continue to save data in the blockchain database, one, or both of the following conditions must be met:

  - A portion of the occupied RAM is freed by the smart contract.
  - More RAM is allocated to the smart contract account through the RAM buying process.

The price o RAM is calculated according to the unique Bancor liquidity algorithm which is implemented in the system contract [here](https://docs.eosnetwork.com/system-contracts/latest/reference/Classes/structeosiosystem_1_1exchange__state).

The RAM system resource must be purchased using the system token. Refer to the [cleos manual](https://docs.eosnetwork.com/leap/latest/cleos/how-to-guides/how-to-buy-ram) to learn how to buy RAM via the command line interface.

## How Is RAM Calculated

The necessary RAM needed for a smart contract to store its data is calculated from the used blockchain state. There are also extra amounts added in for various things so it is not an exact bill of used computer RAM. For more details consult the following pointers in the source code:

- [Fixed overhead shared vector RAM bytes](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L108)
- [Overhead per row per index RAM bytes](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L109)
- [Overhead per account RAM bytes](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L110)
- [Setcode RAM bytes multiplier](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L111)
- [RAM usage update function](https://github.com/AntelopeIO/leap/blob/9f0679bd0a42d6c24a966bb79de6d8c0591872a5/libraries/chain/apply_context.cpp#L725)

## Related documentation articles

- Multi-index table [reference documentation page](http://docs.eosnetwork.com/cdt/latest/reference/Modules/group__multiindex)
- Multi-index table [how to documentation page](https://docs.eosnetwork.com/cdt/latest/how-to-guides/multi-index)
- Singleton [reference documentation page](https://docs.eosnetwork.com/cdt/latest/reference/Classes/classeosio_1_1singleton)
- Singleton [how to documentation page](https://docs.eosnetwork.com/cdt/latest/how-to-guides/multi-index/how-to-define-a-singleton)
