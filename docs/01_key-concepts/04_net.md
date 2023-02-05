---
title: NET as system resource
---

NET, as CPU and RAM, is a very important system resource in the EOS blockchain. When the blockchain executes a transaction it consumes CPU and NET, therefore sufficient NET must be allocated to the payer account for transactions to complete. NET is referred to as `net bandwidth` on the `cleos get account` command output.

## How Is NET Calculated

When an account uses the allocated NET, the amount that can be used in one transaction is limited by predefine [maximum NET](https://docs.eosnetwork.com/cdt/latest/reference/Classes/structeosio_1_1blockchain__parameters#variable-max-transaction-net-usage) limit. Transactions executed by the blockchain contain one or more actions, and each transaction must consume an amount of NET which is in the limits defined by the aforementioned blockchain settings.

The blockchain calculates and updates the remaining resources, for the accounts which execute transactions, with each block, before each transaction is executed. When a transaction is prepared for execution, the blockchain makes sure the payer account has enough NET to cover for the transaction execution. The necessary NET is calculated based on the transaction size, that is, the size of the packed transaction as it is stored in the blockchain. If the account has enough NET resources the transaction can be executed otherwise it is rejected. For technical details please refer to the following sources:

- [The NET configuration variables](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/include/eosio/chain/config.hpp#L57)
- [The transaction initialization](https://github.com/AntelopeIO/leap/blob/e55669c42dfe4ac112e3072186f3a449936c0c61/libraries/chain/controller.cpp#L1559)
- [The transaction NET billing](https://github.com/AntelopeIO/leap/blob/e55669c42dfe4ac112e3072186f3a449936c0c61/libraries/chain/controller.cpp#L1577)
- [The check of NET usage for a transaction](https://github.com/AntelopeIO/leap/blob/a4c29608472dd195d36d732052784aadc3a779cb/libraries/chain/transaction_context.cpp#L376)

## How To Rent NET

For details on how to rent NET resources refer to the [PowerUp Model](./07_powerup_model.md) documentation.
