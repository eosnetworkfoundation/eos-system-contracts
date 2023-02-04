---
title: System Resource Allocation
---

## System Resources

EOS Blockchain work with three system resources:

* [RAM](02_ram.md)
* [CPU](03_cpu.md)
* [NET](04_net.md)

## How To Allocate System Resources

The EOS accounts need sufficient system resources, RAM, CPU and NET, to interact with the smart contracts deployed on the blockchain.

### Rent NET and CPU

The CPU and NET system resources are rented by the account owner through the [PowerUp model](./07_powerup_model.md).

When an account uses the rented resources, the amount that can be used in one transaction is limited by predefine [maximum CPU](https://docs.eosnetwork.com/cdt/latest/reference/Classes/structeosio_1_1blockchain__parameters#variable-max-transaction-cpu-usage), [minimum CPU](https://docs.eosnetwork.com/cdt/latest/reference/Classes/structeosio_1_1blockchain__parameters#variable-min-transaction-cpu-usage), and [maximum NET](https://docs.eosnetwork.com/cdt/latest/reference/Classes/structeosio_1_1blockchain__parameters#variable-max-transaction-net-usage) limits. Transactions executed by the blockchain contain one or more actions, and each transaction must consume an amount of CPU and NET which is in the limits defined by the aforementioned blockchain settings.

[[info]]
| The blockchain calculates and updates the remaining resources, for the accounts which execute transactions, with each block, before each transaction is executed.

### Buy RAM

The RAM resource must be bought using the system token.

Refer to the [cleos manual](https://docs.eosnetwork.com/leap/latest/cleos/how-to-guides/how-to-buy-ram) to learn how to do it via the command line interface. When an account consumes all its allocated RAM can not store any additional information on the blockchain database until it frees some of the occupied RAM or more RAM is allocated to the account through the RAM buying process.

Another way to buy RAM is through an EOS wallet that supports this feature.
