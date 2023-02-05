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
  - More RAM is allocated to the smart contract account through the RAM purchase process.

## How To Purchase RAM

The RAM resource must be bought using the system token. The price of RAM is calculated according to the unique Bancor liquidity algorithm which is implemented in the system contract [here](https://docs.eosnetwork.com/system-contracts/latest/reference/Classes/structeosiosystem_1_1exchange__state).

The quickest way to calculate the price of RAM is the following:

1. Run the following cleos command, make sure you run it against the mainnet or the testnet of your choice:

  ```shell
  cleos get table eosio eosio rammarket
  ```

2. Observe the output which should look like the following:

  ```text
  {
    "supply": "10000000000.0000 RAMCORE",
    "base": {
      "balance": "35044821247 RAM",
      "weight": "0.50000000000000000"
    },
    "quote": {
      "balance": "3158350.8754 EOS",
      "weight": "0.50000000000000000"
    }
  }
  ```

3. Make note of the `base balance`, in this case is 35044821247
4. Make note of the `quote balance`, in this case is 3158350.8754
5. Calculate the price of 1Kib of RAM as `quote balance` * 1024 / `base balance` = 0.0922 EOS

### Buy RAM With Command Line Interface

You can buy RAM through cleos command line interface tool. And you can buy either an explicit amount of RAM expressed in bytes or an amount of RAM worth of an explicit amount of EOS.

### Buy RAM In EOS

For example the below command buys for account `bob` 0.1 EOS worth of RAM at the current market RAM price. The cost for the RAM and the execution of this transaction is covered by the `alice` account and the transaction is authorized by the `active` key of the `alice` account.

```shell
cleos system buyram alice bob "0.1 EOS" -p alice@active
```

### Buy RAM In Bytes

For example the below command buys for account `bob` 1000 RAM bytes at the current market RAM price. The cost for the RAM and the execution of this transaction is covered by the `alice` account and the transaction is authorized by the `active` key of the `alice` account.

```shell
cleos system buyrambytes alice bob "1000" -p alice@active
```

### Buy RAM With EOS Wallet

Another way to buy RAM is through an EOS wallet that supports this feature.

## How Is RAM Calculated

The necessary RAM needed for a smart contract to store its data is calculated from the used blockchain state.

As a developer when you attempt to understand the amount of RAM your smart contract needs you have to pay attention to the data structure underlying the multi-index tables your smart contract instantiates and uses. The data structure underlying one multi-index table defines a row in the table. Each data member of the data structure corresponds with a row cell of the table. Summing the type size of each data member and adding to it the overheads, defined by EOS code, will give you an approximate amount of RAM one multi-index row needs to be stored on the blockchain. On top of this, you will have to also take in consideration if there are any indexes defined on your multi-index table. The overheads defined by the EOS code for multi-index tables, indexes and data structures can be consulted below:

- [Multi-index RAM bytes overhead](https://github.com/AntelopeIO/leap/blob/f6643e434e8dc304bba742422dd036a6fbc1f039/libraries/chain/include/eosio/chain/contract_table_objects.hpp#L240)
- [Overhead per row per index RAM bytes](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L109)
- [Fixed overhead shared vector RAM bytes](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L108)
- [Overhead per account RAM bytes](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L110)
- [Setcode RAM bytes multiplier](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L111)
- [RAM usage update function](https://github.com/AntelopeIO/leap/blob/9f0679bd0a42d6c24a966bb79de6d8c0591872a5/libraries/chain/apply_context.cpp#L725)

## Related documentation articles

- Multi-index table [reference documentation page](http://docs.eosnetwork.com/cdt/latest/reference/Modules/group__multiindex)
- Multi-index table [how to documentation page](https://docs.eosnetwork.com/cdt/latest/how-to-guides/multi-index/how-to-define-a-primary-index/)
- Singleton [reference documentation page](https://docs.eosnetwork.com/cdt/latest/reference/Classes/classeosio_1_1singleton)
- Singleton [how to documentation page](https://docs.eosnetwork.com/cdt/latest/how-to-guides/multi-index/how-to-define-a-singleton)
