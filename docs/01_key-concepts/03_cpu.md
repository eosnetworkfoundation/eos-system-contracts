---
content_title: CPU as system resource
---

The system resource CPU provides processing power to blockchain accounts. The amount of CPU an account has is measured in microseconds and it is referred to as `cpu bandwidth` on the `dune -- cleos get account` command output. The `cpu bandwidth` represents the amount of processing time an account has at its disposal when actions sent to its contract are executed by the blockchain. When a transaction is executed by the blockchain it consumes CPU and NET, therefore sufficient CPU must be allocated to the account in order for transactions to complete.

For more details about resource allocation on the EOS blockchain refer to the [Resource Allocation](./05_system_resource_allocation.md) documentation.
