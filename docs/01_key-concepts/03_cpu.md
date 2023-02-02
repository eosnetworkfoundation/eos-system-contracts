---
title: CPU as system resource
---

CPU, as NET and RAM, is a very important system resource in the EOS blockchain. The system resource CPU provides processing power to blockchain accounts. When the blockchain executes a transaction it consumes CPU and NET, therefore sufficient CPU must be allocated to the payer account for transactions to complete. The amount of CPU an account has is measured in microseconds and it is referred to as `cpu bandwidth` on the `cleos get account` command output.

## How Is CPU Calculated

Before a transaction is prepared for execution, the blockchain makes sure the payer account has enough CPU to cover for the transaction execution. The necessary CPU is calculated by measuring the time for executing it on the node that is actively building the current block. If the account has enough CPU resources the transaction can be executed otherwise it is rejected. For technical details please refer to the following sources:

- [The defined CPU configuration variables](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L66)
- [The transaction initialization](https://github.com/AntelopeIO/leap/blob/e55669c42dfe4ac112e3072186f3a449936c0c61/libraries/chain/controller.cpp#L1559)
- [The transaction CPU billing](https://github.com/AntelopeIO/leap/blob/e55669c42dfe4ac112e3072186f3a449936c0c61/libraries/chain/controller.cpp#L1577)
- [The check of CPU usage for a transaction](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/transaction_context.cpp#L381)

## Subjective CPU Billing



## How To Allocated CPU

For details on how to allocated CPU resources refer to the [Resource Allocation](./05_system_resource_allocation.md) documentation.
