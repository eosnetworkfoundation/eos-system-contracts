---
title: How to stake
link_text: How to stake
---

## Goal

Stake resources for your account to participate in the on-chain voting and governance.

## Before you begin

* Install the currently supported version of cleos

* Ensure the reference system contracts from `system-contracts` repository is deployed and used to manage system resources

* Understand the following:
  * What is an account
  * What is network bandwidth
  * What is CPU bandwidth

## Steps

Stake 10 A network bandwidth for `alice`

```shell
cleos system delegatebw alice alice "0 A" "10 A"
```

Stake 10 A CPU bandwidth for `alice`:

```shell
cleos system delegatebw alice alice "10 A" "0 A"
```
