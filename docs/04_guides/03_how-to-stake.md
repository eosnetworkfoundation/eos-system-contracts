---
content_title: How to stake
link_text: How to stake
---

## Goal

Stake resources for your account to participate in the on-chain voting and governance.

## Before you begin

* Install the currently supported version of dune

* Ensure the reference system contracts from `eos-system-contracts` repository is deployed and used to manage system resources

* Understand the following:
  * What is an account
  * What is network bandwidth
  * What is CPU bandwidth

## Steps

Stake 10 SYS network bandwidth for `alice`

```shell
dune -- cleos system delegatebw alice alice "0 SYS" "10 SYS"
```

Stake 10 SYS CPU bandwidth for `alice`:

```shell
dune -- cleos system delegatebw alice alice "10 SYS" "0 SYS"
```
