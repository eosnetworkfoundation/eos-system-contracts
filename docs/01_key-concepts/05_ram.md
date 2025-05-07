---
title: RAM as system resource
---

The RAM resource is the memory, the storage space, where the blockchain stores data. If your contract needs to store data on the blockchain, it can store it in the blockchain's RAM as a `multi-index table` or a `singleton`.

## RAM High Performance

Data stored on the Vaulta blockchain uses RAM as its storage medium. Therefore, access to the blockchain data is highly performant and fast. Its performance benchmarks can reach levels rarely achieved by other blockchains. This ability elevates the Vaulta blockchain to be a contender for the fastest blockchain worldwide.

## RAM Importance

RAM is an important system resource because of the following reasons:

* Ram is a limited resource. The public Vaulta blockchain started with 64GB of total RAM. After a brief period the block producers decided to increase the memory with 1KB per block. In this way the supply of RAM constantly increases, yet its price accelerates at a slower pace.

* The execution of many actions by the blockchain uses RAM. For example, when you create a new account, the new account information is stored in the blockchain's RAM. Also, when an account accepts a token which it did not hold before, a new record has to be created, that holds the balance of the newly accepted token. The blockchain memory has to be purchased either by the account that transfers the tokens or by the account that accepts the new token type.

* If a smart contract consumes all of its allocated RAM, it cannot store any additional information. To continue to save data in the blockchain database, one or both of the following conditions must be met:

  * A portion of the occupied RAM is freed by the smart contract.
  * More RAM is allocated to the smart contract account through the RAM purchase process.

## How To Purchase RAM

The RAM resource must be bought with the `Vaulta` system token. The price of RAM is calculated according to the unique Bancor liquidity algorithm that is implemented in the system contract.

The quickest way to calculate the price of RAM:

1. Run the following command: (**Note:** Make sure you run it against the mainnet or the testnet of your choice.)

    ```shell
    cleos get table eosio eosio rammarket
    ```

2. Observe the output which should look like the sample below:  

    ```text
    {
        "supply": "10000000000.0000 RAMCORE",
        "base": {
            "balance": "35044821247 RAM",
            "weight": "0.50000000000000000"
        },
        "quote": {
            "balance": "3158350.8754 EOS ",
            "weight": "0.50000000000000000"
        }
    }
    ```

3. Make note of the `base balance`, in this case it is 35044821247.
4. Make note of the `quote balance`, in this case it is 3158350.8754.
5. Calculate the price of 1Kib of RAM as `quote balance` * 1024 / `base balance` = 0.0922 EOS .

### Buy RAM With Command Line Interface

You can buy RAM through the command line interface tool. You can buy either an explicit amount of RAM expressed in bytes or an amount of RAM worth an explicit amount of Vaulta.

### Buy RAM In EOS 

For example, the command below buys for account `bob` 0.1 EOS  worth of RAM at the current market RAM price. The cost for the RAM and the execution of this transaction is covered by the `alice` account and the transaction is authorized by the `active` key of the `alice` account.

```shell
cleos system buyram alice bob "0.1 EOS " -p alice@active
```

### Buy RAM In Bytes

For example, the command below buys for account `bob` 1000 RAM bytes at the current market RAM price. The cost for the RAM and the execution of this transaction is covered by the `alice` account and the transaction is authorized by the `active` key of the `alice` account.

```shell
cleos system buyrambytes alice bob "1000" -p alice@active
```

## How Is RAM Calculated

The necessary RAM needed for a smart contract to store its data is calculated from the used blockchain state.

As a developer, to understand the amount of RAM your smart contract needs, pay attention to the data structure underlying the multi-index tables your smart contract instantiates and uses. The data structure underlying one multi-index table defines a row in the table. Each data member of the data structure corresponds with a row cell of the table.
To approximate the amount of RAM one multi-index row needs to store on the blockchain, you have to add the size of the type of each data member and the memory overheads for each of the defined indexes, if any. Find below the overheads defined by the Vaulta code for multi-index tables, indexes, and data types:

* [Multi-index RAM bytes overhead](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/contract_table_objects.hpp#L242-L285)
* [Overhead per row per index RAM bytes](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/config.hpp#L114)
* [Fixed overhead shared vector RAM bytes](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/config.hpp#L113)
* [Overhead per account RAM bytes](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/config.hpp#L115)
* [Setcode RAM bytes multiplier](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/config.hpp#L116)
* [RAM usage update function](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/apply_context.cpp#L734)
