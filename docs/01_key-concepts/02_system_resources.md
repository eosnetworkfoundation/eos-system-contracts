---
title: System Resources
---

The Vaulta blockchain works with three system resources: CPU, NET and RAM. The Vaulta accounts need sufficient system resources to interact with the smart contracts deployed on the blockchain.

* [RAM Resource](./05_ram.md)
* [CPU Resource](./03_cpu.md)
* [NET Resource](./04_net.md)

To allocate RAM resources to an account you have to [purchase RAM](./05_ram.md#how-to-purchase-ram).
To allocate CPU and NET resources to an account you have to [power up the account](./07_powerup_model.md#power-up-your-account).

## Resource Cost Estimation

As a developer if you want to estimate how much CPU and NET a transaction requires for execution, you can employ one of the following methods:

* Use the `--dry-run` option for the `dune -- cleos push transaction` command.
* Use any tool that can pack a transaction and send it to the blockchain and specify the `--dry-run` option.
* Use the chain API endpoint [`compute_transaction`](https://github.com/AntelopeIO/spring/blob/7254bab917a17bcc0d82d23d03f4173176150239/plugins/chain_plugin/include/eosio/chain_plugin/chain_plugin.hpp#L557).

In all cases, when the transaction is processed, the blockchain node simulates the execution of the transaction and, as a consequence, the state of the blockchain is changed speculatively, which allows for the CPU and NET measurements to be done. However, the transaction is not sent to the blockchain and the caller receives the estimated CPU and NET costs in return.
