---
title: How To Use The PowerUp Model
---

## Execute An Order

The action to power up an account is `powerup`. It takes a `payer` of the fee and a `receiver` of the resources. The `days` must always match `state.powerup_days`. The `net_frac` and `cpu_frac` are the percentage of the resources that you need. The easiest way to calculate the percentage is to multiple 10^15 (100%) by the desired percentage. For example: 10^15 * 0.01 = 10^13.

```sh
cleos push action eosio powerup '[user, user, 1, 10000000000000, 10000000000000, "1000.0000 TST"]' -p user
```

```console
executed transaction: 82b7124601612b371b812e3bf65cf63bb44616802d3cd33a2c0422b58399f54f  144 bytes  521 us
#         eosio <= eosio::powerup               {"payer":"user","receiver":"user","days":1,"net_frac":"10000000000000","cpu_frac":"10000000000000","...
#   eosio.token <= eosio.token::transfer        {"from":"user","to":"eosio.rex","quantity":"999.9901 TST","memo":"transfer from user to eosio.rex"}
#  eosio.reserv <= eosio.reserv::powupresult    {"fee":"999.9901 TST","powup_net_weight":"16354","powup_cpu_weight":"65416"}
#          user <= eosio.token::transfer        {"from":"user","to":"eosio.rex","quantity":"999.9901 TST","memo":"transfer from user to eosio.rex"}
#     eosio.rex <= eosio.token::transfer        {"from":"user","to":"eosio.rex","quantity":"999.9901 TST","memo":"transfer from user to eosio.rex"}
```

You can see how much NET and CPU weight was received as well as the fee by looking at the `eosio.reserv::powupresult` informational action.

## Process Expired Orders

The resources in loans that expire do not automatically get reclaimed by the system. The expired loans sit in a queue that must be processed. Anyone calling the `powerup` action will help with processing this queue (limited to processing at most two expired loans at a time) so that normally the expired loans will be automatically processed in a timely manner. However, in some cases it may be necessary to manual process expired loans in the queue to make resources available to the system again and thus make prices cheaper. In such a scenario, any account may process up to an arbitrary number of expired loans by calling the `powerupexec` action.

The orders table `powup.order` can be viewed by calling:

```sh
cleos get table eosio 0 powup.order
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
cleos push action eosio powerupexec '[user, 2]' -p user
```

```console
executed transaction: 93ab4ac900a7902e4e59e5e925e8b54622715328965150db10774aa09855dc98  104 bytes  363 us
#         eosio <= eosio::powerupexec           {"user":"user","max":2}
warning: transaction executed locally, but may not be confirmed by the network yet         ]
```

## Alternative Ways To Use The PowerUp Model

You can also use the PowerUp model to power your account with CPU and NET, using an EOS wallet that supports the PowerUp function.
