---
title: NET as system resource
---

As CPU and RAM, the NET resource is an important system resource in the EOS blockchain. When the blockchain executes a transaction, it consumes CPU and NET. Sufficient NET must be allocated to the payer account for transactions to complete. NET is referred to as `net bandwidth` on the `dune -- cleos get account` command output.

## How Is NET Calculated

Each transaction must consume an amount of NET which can not exceed the predefined maximum transaction NET usage value. For EOS blockchain this limit is set in the blockchain's configuration. You can find out this limit by running the following command and consult the `max_transaction_net_usage` which is expressed in bytes.

```shell
dune -- cleos get consensus_parameters
```

For the accounts that execute transactions, the blockchain calculates and updates the remaining resources for each block before each transaction is executed. When a transaction is prepared for execution, the blockchain determines whether the payer account has enough NET to cover the transaction execution. The necessary NET is calculated based on the transaction size, which is the size of the packed transaction as it is stored in the blockchain. If the account has enough NET resources, the transaction can be executed; otherwise it is rejected. For technical details please refer to the following sources:

* [The NET configuration variables](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/config.hpp#L60-L67)
* [The transaction initialization](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/controller.cpp#L3012)
* [The transaction NET billing](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/controller.cpp#L3030)
* [The check of NET usage for a transaction](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/transaction_context.cpp#L416)

## How To Rent NET

For details on how to rent NET resources refer to the [Account Power Up](./07_powerup_model.md#power-up-your-account) section.
