---
title: How To Use The PowerUp Model
---

## Power Up Your Account

To power up an account is a technique to rent CPU and NET resources from the PowerUp resource model. A smart contract implements this model on the blockchain and allocates these resources to the account of your choice. The action to power up an account is `powerup`. It takes as parameters:

* The `payer` of the fee, must be a valid Vaulta account.
* The `receiver` of the resources, must be a valid Vaulta account.
* The `days` which must always match `state.powerup_days` specified in the [PowerUp configuration settings](https://github.com/VaultaFoundation/system-contracts/blob/7cec470b17bd53b8c78465d4cbd889dbaf1baffb/contracts/eosio.system/include/eosio.system/eosio.system.hpp#L588).
* The `net_frac`, and the `cpu_frac` are the percentage of the resources that you need. The easiest way to calculate the percentage is to multiple 10^15 (100%) by the desired percentage. For example: 10^15 * 0.01 = 10^13.
* The `max_payment`, must be expressed in XYZ and is the maximum amount the `payer` is willing to pay.

```sh
cleos push action eosio powerup '[user, user, 1, 10000000000000, 10000000000000, "1000.0000 XYZ"]' -p user
```

To view the received NET and CPU weight as well as the amount of the fee, check the `eosio.reserv::powupresult` returned by the action, which should look similar to the one below:

```console
executed transaction: 82b7124601612b371b812e3bf65cf63bb44616802d3cd33a2c0422b58399f54f  144 bytes  521 us
#         eosio <= eosio::powerup               {"payer":"user","receiver":"user","days":1,"net_frac":"10000000000000","cpu_frac":"10000000000000","...
#   eosio.token <= eosio.token::transfer        {"from":"user","to":"eosio.rex","quantity":"999.9901 XYZ","memo":"transfer from user to eosio.rex"}
#  eosio.reserv <= eosio.reserv::powupresult    {"fee":"999.9901 XYZ","powup_net_weight":"16354","powup_cpu_weight":"65416"}
#          user <= eosio.token::transfer        {"from":"user","to":"eosio.rex","quantity":"999.9901 XYZ","memo":"transfer from user to eosio.rex"}
#     eosio.rex <= eosio.token::transfer        {"from":"user","to":"eosio.rex","quantity":"999.9901 XYZ","memo":"transfer from user to eosio.rex"}
```

The PowerUp resource model on the Vaulta blockchain is initialized with `"powerup_days": 1,`. This setting permits the maximum period to rent CPU and NET for 24 hours. If you do not use the resources within the 24 hour interval, the rented CPU and NET expires.

### Process Expired Orders

The resources in loans that expire are not automatically reclaimed by the system. The expired loans remain in a queue that must be processed.

Any calls to the `powerup` action does process also this queue (limited to two expired loans at a time). Therefore, the expired loans are automatically processed in a timely manner. Sometimes, it may be necessary to manually process expired loans in the queue to release resources back to the system, which reduces prices. Therefore, any account may process up to an arbitrary number of expired loans if it calls the `powerupexec` action.

To view the orders table `powup.order` execute the following command:

```sh
dune -- cleos get table eosio 0 powup.order
```

```json
{
  "rows": [{
      "version": 0,
      "id": 0,
      "owner": "user",
      "net_weight": 16354,
      "cpu_weight": 65416,
      "expires": "2020-11-18T13:04:33"
    }
  ],
  "more": false,
  "next_key": ""
}
```

Example `powerupexec` call:

```sh
dune -- cleos push action eosio powerupexec '[user, 2]' -p user
```

```console
executed transaction: 93ab4ac900a7902e4e59e5e925e8b54622715328965150db10774aa09855dc98  104 bytes  363 us
#         eosio <= eosio::powerupexec           {"user":"user","max":2}
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```
