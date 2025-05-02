---
title: How To Configure The PowerUp Resource Model
---

## Configure and Use the PowerUp Resource Model

### Overview

The PowerUp Resource Model creates a new NET and CPU marketplace which renders obsolete the existing staking mechanism and REX market. Under the old model, XYZ token holders own NET and CPU and may choose to use it themselves, delegate it to others, or make it available for others to rent using the REX market. With the new model, the chain owns almost all NET and CPU resources and the only way to access these resources is through the new `powerup` action. Equally important, the PowerUp Resource Model channels fees to the REX pool to enable token holders to profit off the new market.

### Configuration

```c++
// configure the `powerup` market. The market becomes available the first time this action is invoked
void cfgpowerup( powerup_config& args );

struct powerup_config_resource {
    std::optional<int64_t>        current_weight_ratio;   // Immediately set weight_ratio to this amount. 1x = 10^15. 0.01x = 10^13.                                                
                                                          //    Do not specify to preserve the existing setting or use the default;
                                                          //    this avoids sudden price jumps. For new chains which don't need
                                                          //    to gradually phase out staking and REX, 0.01x (10^13) is a good
                                                          //    value for both current_weight_ratio and target_weight_ratio.
    std::optional<int64_t>        target_weight_ratio;    // Linearly shrink weight_ratio to this amount. 1x = 10^15. 0.01x = 10^13.
                                                          //    Do not specify to preserve the existing setting or use the default.
    std::optional<int64_t>        assumed_stake_weight;   // Assumed stake weight for ratio calculations. Use the sum of total
                                                          //    staked and total rented by REX at the time the power market
                                                          //    is first activated. Do not specify to preserve the existing
                                                          //    setting (no default exists); this avoids sudden price jumps.
                                                          //    For new chains which don't need to phase out staking and REX,
                                                          //    10^12 is probably a good value.
    std::optional<time_point_sec> target_timestamp;       // Stop automatic weight_ratio shrinkage at this time. Once this
                                                          //    time hits, weight_ratio will be target_weight_ratio. Ignored
                                                          //    if current_weight_ratio == target_weight_ratio. Do not specify
                                                          //    this to preserve the existing setting (no default exists).
    std::optional<double>         exponent;               // Exponent of resource price curve. Must be >= 1. Do not specify
                                                          //    to preserve the existing setting or use the default.
    std::optional<uint32_t>       decay_secs;             // Number of seconds for the gap between adjusted resource
                                                          //    utilization and instantaneous resource utilization to shrink
                                                          //    by 63%. Do not specify to preserve the existing setting or
                                                          //    use the default.
    std::optional<asset>          min_price;              // Fee needed to reserve the entire resource market weight at the
                                                          //    minimum price. For example, this could be set to 0.005% of
                                                          //    total token supply. Do not specify to preserve the existing
                                                          //    setting or use the default.
    std::optional<asset>          max_price;              // Fee needed to reserve the entire resource market weight at the
                                                          //    maximum price. For example, this could be set to 10% of total
                                                          //    token supply. Do not specify to preserve the existing
                                                          //    setting (no default exists).

   };

struct powerup_config {
    powerup_config_resource  net;             // NET market configuration
    powerup_config_resource  cpu;             // CPU market configuration
    std::optional<uint32_t> powerup_days;     // `powerup` `days` argument must match this. Do not specify to preserve the
                                              //    existing setting or use the default.
    std::optional<asset>    min_powerup_fee;  // Powerup fees below this amount are rejected. Do not specify to preserve the
                                              //    existing setting (no default exists).
};
```

#### Definitions

Definitions useful to help understand the configuration, including defaults:

```c++
inline constexpr int64_t powerup_frac = 1'000'000'000'000'000ll;  // 1.0 = 10^15

struct powerup_state_resource {
    static constexpr double   default_exponent   = 2.0;                  // Exponent of 2.0 means that the price to reserve a
                                                                         //    tiny amount of resources increases linearly
                                                                         //    with utilization.
    static constexpr uint32_t default_decay_secs = 1 * seconds_per_day;  // 1 day; if 100% of bandwidth resources are in a
                                                                         //    single loan, then, assuming no further powerup usage,
                                                                         //    1 day after it expires the adjusted utilization
                                                                         //    will be at approximately 37% and after 3 days
                                                                         //    the adjusted utilization will be less than 5%.

    uint8_t        version                 = 0;
    int64_t        weight                  = 0;                  // resource market weight. calculated; varies over time.
                                                                 //    1 represents the same amount of resources as 1
                                                                 //    satoshi of XYZ staked.
    int64_t        weight_ratio            = 0;                  // resource market weight ratio:
                                                                 //    assumed_stake_weight / (assumed_stake_weight + weight).
                                                                 //    calculated; varies over time. 1x = 10^15. 0.01x = 10^13.
    int64_t        assumed_stake_weight    = 0;                  // Assumed stake weight for ratio calculations.
    int64_t        initial_weight_ratio    = powerup_frac;       // Initial weight_ratio used for linear shrinkage.
    int64_t        target_weight_ratio     = powerup_frac / 100; // Linearly shrink the weight_ratio to this amount.
    time_point_sec initial_timestamp       = {};                 // When weight_ratio shrinkage started
    time_point_sec target_timestamp        = {};                 // Stop automatic weight_ratio shrinkage at this time. Once this
                                                                 //    time hits, weight_ratio will be target_weight_ratio.
    double         exponent                = default_exponent;   // Exponent of resource price curve.
    uint32_t       decay_secs              = default_decay_secs; // Number of seconds for the gap between adjusted resource
                                                                 //    utilization and instantaneous utilization to shrink by 63%.
    asset          min_price               = {};                 // Fee needed to reserve the entire resource market weight at
                                                                 //    the minimum price (defaults to 0).
    asset          max_price               = {};                 // Fee needed to reserve the entire resource market weight at
                                                                 //    the maximum price.
    int64_t        utilization             = 0;                  // Instantaneous resource utilization. This is the current
                                                                 //    amount sold. utilization <= weight.
    int64_t        adjusted_utilization    = 0;                  // Adjusted resource utilization. This is >= utilization and
                                                                 //    <= weight. It grows instantly but decays exponentially.
    time_point_sec utilization_timestamp   = {};                 // When adjusted_utilization was last updated
};

struct powerup_state {
    static constexpr uint32_t default_powerup_days = 30; // 30 day resource powerups

    uint8_t                    version           = 0;
    powerup_state_resource     net               = {};                     // NET market state
    powerup_state_resource     cpu               = {};                     // CPU market state
    uint32_t                   powerup_days      = default_powerup_days;   // `powerup` `days` argument must match this.
    asset                      min_powerup_fee   = {};                     // fees below this amount are rejected

    uint64_t primary_key()const { return 0; }
};
```

### Preparation for Upgrade

1. Build [system-contracts](https://github.com/VaultaFoundation/system-contracts) with `powerup` code.
2. Deploy eosio.system contract to `eosio`.
3. Create account `eosio.reserv` and ensure the account has enough RAM, at least 4 KiB.
4. Deploy `powup.results.abi` to `eosio.reserv` account using `setabi`. The ABI can be found in the `build/contracts/eosio.system/.powerup/` directory.
5. Enable the REX system (if not enabled).

#### Configure PowerUp

##### Config File

**config.json**

```json
{
    "net": {
        "assumed_stake_weight": 944076307,
        "current_weight_ratio": 1000000000000000,
        "decay_secs": 86400,
        "exponent": 2,
        "max_price": "10000000.0000 XYZ",
        "min_price": "0.0000 XYZ",
        "target_timestamp": "2022-01-01T00:00:00.000",
        "target_weight_ratio": 10000000000000
    },
    "cpu": {
        "assumed_stake_weight": 3776305228,
        "current_weight_ratio": 1000000000000000,
        "decay_secs": 86400,
        "exponent": 2,
        "max_price": "10000000.0000 XYZ",
        "min_price": "0.0000 XYZ",
        "target_timestamp": "2022-01-01T00:00:00.000",
        "target_weight_ratio": 10000000000000
    },
    "min_powerup_fee": "0.0001 XYZ",
    "powerup_days": 1
}
```

##### cfgpowerup Action Call

```sh
# call to `cfgpowerup`
cleos push action eosio cfgpowerup "[`cat ./config.json`]" -p eosio
```

#### Check state

```sh
cleos get table eosio 0 powup.state
```

```json
{
  "rows": [{
      "version": 0,
      "net": {
        "version": 0,
        "weight": 0,
        "weight_ratio": "1000000000000000",
        "assumed_stake_weight": 944076307,
        "initial_weight_ratio": "1000000000000000",
        "target_weight_ratio": "10000000000000",
        "initial_timestamp": "2020-11-16T19:52:50",
        "target_timestamp": "2022-01-01T00:00:00",
        "exponent": "2.00000000000000000",
        "decay_secs": 3600,
        "min_price": "0.0000 XYZ",
        "max_price": "10000000.0000 XYZ",
        "utilization": 0,
        "adjusted_utilization": 0,
        "utilization_timestamp": "2020-11-16T19:52:50"
      },
      "cpu": {
        "version": 0,
        "weight": 0,
        "weight_ratio": "1000000000000000",
        "assumed_stake_weight": 3776305228,
        "initial_weight_ratio": "1000000000000000",
        "target_weight_ratio": "10000000000000",
        "initial_timestamp": "2020-11-16T19:52:50",
        "target_timestamp": "2022-01-01T00:00:00",
        "exponent": "2.00000000000000000",
        "decay_secs": 3600,
        "min_price": "0.0000 XYZ",
        "max_price": "10000000.0000 XYZ",
        "utilization": 0,
        "adjusted_utilization": 0,
        "utilization_timestamp": "2020-11-16T19:52:50"
      },
      "powerup_days": 1,
      "min_powerup_fee": "0.0001 XYZ"
    }
  ],
  "more": false,
  "next_key": ""
}
```
