---
title: CPU as system resource
---

As NET and RAM, the CPU resource is a very important system resource in the Vaulta blockchain. The CPU system resource provides processing power to blockchain accounts. When the blockchain executes a transaction, it consumes CPU and NET. For transactions to complete, sufficient CPU must be allocated to the payer account. The amount of CPU an account has is measured in microseconds and it is referred to as `cpu bandwidth` on the `cleos get account` command output.

## How Is CPU Calculated

Transactions executed by the blockchain contain one or more actions. Each transaction must consume an amount of CPU within the limits predefined by the minimum and maximum transaction CPU usage values. For Vaulta blockchain these limits are set in the blockchain's configuration. You can find out these limits by running the following command and consult the `min_transaction_cpu_usage` and the `max_transaction_cpu_usage` which are expressed in microseconds:

```shell
cleos get consensus_parameters
```

For accounts that execute transactions, the blockchain calculates and updates the remaining resources with each block before each transaction is executed. When a transaction is prepared for execution, the blockchain determines whether the payer account has enough CPU to cover the transaction execution. To calculate the necessary CPU, the node that actively builds the current block measures the time to execute the transaction. If the account has enough CPU, the transaction is executed; otherwise it is rejected. For technical details please refer to the following links:

* [The CPU configuration variables](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/include/eosio/chain/config.hpp#L69-L73)
* [The transaction initialization](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/controller.cpp#L3012)
* [The transaction CPU billing](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/controller.cpp#L3030)
* [The check of CPU usage for a transaction](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/libraries/chain/transaction_context.cpp#L457)

## Subjective CPU Billing

Subjective billing is an optional feature of the Vaulta blockchain. It allows nodes to bill account resources locally in their own node without sharing the billing with the rest of the network. Since its introduction, subjective billing benefited the nodes that adopted it because it reduced the node CPU usage by almost 90%. But it can result in failed transactions or lost transactions. Subjective billing can trigger transaction failure when a smart contract code uses a "check" function, like `assert()` or `check()` command to verify data. When this situation occurs, assert or check earlier in the system contract execution to reduce the applied billing. If the lack of an error message does not affect the user experience, a system contract may benefit by replacing some asserts and checks with a return statement. This replacement ensures their transactions succeed and are billed objectively on-chain.

Find more details about subjective billing in the [Introduction to subjective billing and lost transactions](https://eosnetwork.com/blog/api-plus-an-introduction-to-subjective-billing-and-lost-transactions/) article.

## How To Rent CPU

For details on how to rent CPU resources refer to the [Account Power Up](./07_powerup_model.md#power-up-your-account) section.
