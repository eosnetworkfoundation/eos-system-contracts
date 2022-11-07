---
content_title: RAM as system resource
---

## What is RAM

RAM is the memory, the storage space, where the blockchain stores data. If your contract needs to store data on the blockchain, like in a database, then it can store it in the blockchain's RAM using either a `multi-index table` or a `singleton`.

### Related documentation articles

- Multi-index table [explainer documentation page](https://github.com/AntelopeIO/cdt/blob/main/libraries/eosiolib/contracts/eosio/multi_index.hpp)

- Multi-index table [how to documentation page](https://github.com/AntelopeIO/cdt/tree/main/docs/06_how-to-guides/40_multi-index)

- Singleton [reference documentation page](https://github.com/AntelopeIO/cdt/blob/main/libraries/eosiolib/contracts/eosio/singleton.hpp) 

- Singleton [how to documentation page](https://github.com/AntelopeIO/cdt/blob/main/docs/06_how-to-guides/40_multi-index/how-to-define-a-singleton.md)

## RAM High Performance

The Antelope-based blockchains are known for their high performance, which is achieved also because the data stored on the blockchain is using RAM as the storage medium, and thus access to blockchain data is very fast, helping the performance benchmarks to reach levels no other blockchain has been able to.

## RAM Importance

RAM is a very important system resource because of the following reasons:

- It is a limited resource, each Antelope-based blockchain can have different policies and rules around RAM; for example the public EOS blockchain started with 64GB of RAM and after that the block producers decided to increase the memory with 1KB per block, thus increasing constantly the supply of RAM for its price to not grow too high due to the increased demand from blockchain applications.

- RAM is used in executing many actions sent to the blockchain; creating a new account action, for example, it needs to store in the blockchain memory the new account's information; also when an account accepts a new type of token a new record has to be created, somewhere in the blockchain memory, that holds the balance of the new token accepted, and that memory, the storage space on the blockchain, has to be purchased either by the account that transfers the token or by the account that accepts the new token type.

- The smart contract can not store any additional information if it consumes all its allocated RAM. To continue to save data in the blockchain database, one, or both of the following conditions must be met:

  - A portion of the occupied RAM is freed by the smart contract.
  - More RAM is allocated to the smart contract account through the RAM buying process.

RAM is a scarce resource priced according to the unique Bancor liquidity algorithm which is implemented in the system contract [here](https://github.com/AntelopeIO/reference-contracts/blob/main/contracts/eosio.system/include/eosio.system/exchange_state.hpp).

The RAM system resource must be purchased using the system token. Refer to the [cleos manual](https://github.com/AntelopeIO/leap/blob/main/docs/02_cleos/02_how-to-guides/how-to-buy-ram.md) to learn how to buy RAM via the command line interface.
