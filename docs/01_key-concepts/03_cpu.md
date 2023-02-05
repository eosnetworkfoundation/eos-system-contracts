---
title: CPU as system resource
---

CPU, as NET and RAM, is a very important system resource in the EOS blockchain. The system resource CPU provides processing power to blockchain accounts. When the blockchain executes a transaction it consumes CPU and NET, therefore sufficient CPU must be allocated to the payer account for transactions to complete. The amount of CPU an account has is measured in microseconds and it is referred to as `cpu bandwidth` on the `cleos get account` command output.

## How Is CPU Calculated

When an account uses the rented CPU, the amount that can be used in one transaction is limited by predefine [maximum CPU](https://docs.eosnetwork.com/cdt/latest/reference/Classes/structeosio_1_1blockchain__parameters#variable-max-transaction-cpu-usage) and [minimum CPU](https://docs.eosnetwork.com/cdt/latest/reference/Classes/structeosio_1_1blockchain__parameters#variable-min-transaction-cpu-usage) limits. Transactions executed by the blockchain contain one or more actions, and each transaction must consume an amount of CPU which is in the limits defined by the aforementioned blockchain settings.

The blockchain calculates and updates the remaining resources, for the accounts which execute transactions, with each block, before each transaction is executed. When a transaction is prepared for execution, the blockchain makes sure the payer account has enough CPU to cover for the transaction execution. The necessary CPU is calculated by measuring the time for executing the transaction on the node that is actively building the current block. If the account has enough CPU the transaction can be executed otherwise it is rejected. For technical details please refer to the following pointers:

- [The CPU configuration variables](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L66)
- [The transaction initialization](https://github.com/AntelopeIO/leap/blob/e55669c42dfe4ac112e3072186f3a449936c0c61/libraries/chain/controller.cpp#L1559)
- [The transaction CPU billing](https://github.com/AntelopeIO/leap/blob/e55669c42dfe4ac112e3072186f3a449936c0c61/libraries/chain/controller.cpp#L1577)
- [The check of CPU usage for a transaction](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/transaction_context.cpp#L381)

## Subjective CPU Billing

Subjective billing is an optional feature of the EOS blockchain that lets nodes bill account resources locally in their own node without sharing the billing with the rest of the network. It has, since its introduction, benefited the adopting nodes because it reduced node CPU usage by almost 90%. On the other side, sometimes, it results in failing transactions or lost transactions. As a developer you should be aware that when a smart contract code uses a "check" function, like `assert()` or `check()`, to verify data, it can trigger transaction failure and thus can lead to subjective billing issues. One alternative is to assert or check earlier in a contract’s execution to reduce the billing applied. Or as long as the lack of an error message does not affect user experience, some contracts may also benefit from replacing some asserts and checks with return statements to ensure their transactions succeed and are billed objectively on-chain.

More details about subjective billing can be found in [Introduction to subjective billing and lost transactions](https://eosnetwork.com/blog/api-plus-an-introduction-to-subjective-billing-and-lost-transactions/) article.

## How To Rent CPU

For details on how to rent CPU resources refer to the [PowerUp Model](./07_powerup_model.md) documentation.
