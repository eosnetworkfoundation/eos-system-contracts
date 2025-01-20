#pragma once

#include <eosio/asset.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/privileged.hpp>
#include <eosio/producer_schedule.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/instant_finality.hpp>

#include <eosio.system/exchange_state.hpp>
#include <eosio.system/native.hpp>

#include <deque>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>

#ifdef CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX
#undef CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX
#endif
// CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX macro determines whether ramfee and namebid proceeds are
// channeled to REX pool. In order to stop these proceeds from being channeled, the macro must
// be set to 0.
#define CHANNEL_RAM_AND_NAMEBID_FEES_TO_REX 1

namespace eosiosystem {

   using eosio::asset;
   using eosio::binary_extension;
   using eosio::block_timestamp;
   using eosio::check;
   using eosio::const_mem_fun;
   using eosio::datastream;
   using eosio::indexed_by;
   using eosio::name;
   using eosio::same_payer;
   using eosio::symbol;
   using eosio::symbol_code;
   using eosio::time_point;
   using eosio::time_point_sec;
   using eosio::unsigned_int;

   inline constexpr int64_t powerup_frac = 1'000'000'000'000'000ll;  // 1.0 = 10^15

   template<typename E, typename F>
   static inline auto has_field( F flags, E field )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, bool>
   {
      return ( (flags & static_cast<F>(field)) != 0 );
   }

   template<typename E, typename F>
   static inline auto set_field( F flags, E field, bool value = true )
   -> std::enable_if_t< std::is_integral_v<F> && std::is_unsigned_v<F> &&
                        std::is_enum_v<E> && std::is_same_v< F, std::underlying_type_t<E> >, F >
   {
      if( value )
         return ( flags | static_cast<F>(field) );
      else
         return ( flags & ~static_cast<F>(field) );
   }

   static constexpr uint32_t seconds_per_year      = 52 * 7 * 24 * 3600;
   static constexpr uint32_t seconds_per_day       = 24 * 3600;
   static constexpr uint32_t seconds_per_hour      = 3600;
   static constexpr int64_t  useconds_per_year     = int64_t(seconds_per_year) * 1000'000ll;
   static constexpr int64_t  useconds_per_day      = int64_t(seconds_per_day) * 1000'000ll;
   static constexpr int64_t  useconds_per_hour     = int64_t(seconds_per_hour) * 1000'000ll;
   static constexpr uint32_t blocks_per_day        = 2 * seconds_per_day; // half seconds per day

   static constexpr int64_t  min_activated_stake   = 150'000'000'0000;
   static constexpr int64_t  ram_gift_bytes        = 1400;
   static constexpr int64_t  min_pervote_daily_pay = 100'0000;
   static constexpr uint32_t refund_delay_sec      = 3 * seconds_per_day;

   static constexpr int64_t  inflation_precision           = 100;     // 2 decimals
   static constexpr int64_t  default_annual_rate           = 500;     // 5% annual rate
   static constexpr int64_t  pay_factor_precision          = 10000;
   static constexpr int64_t  default_inflation_pay_factor  = 50000;   // producers pay share = 10000 / 50000 = 20% of the inflation
   static constexpr int64_t  default_votepay_factor        = 40000;   // per-block pay share = 10000 / 40000 = 25% of the producer pay

#ifdef SYSTEM_BLOCKCHAIN_PARAMETERS
   struct blockchain_parameters_v1 : eosio::blockchain_parameters
   {
      eosio::binary_extension<uint32_t> max_action_return_value_size;
      EOSLIB_SERIALIZE_DERIVED( blockchain_parameters_v1, eosio::blockchain_parameters,
                                (max_action_return_value_size) )
   };
   using blockchain_parameters_t = blockchain_parameters_v1;
#else
   using blockchain_parameters_t = eosio::blockchain_parameters;
#endif

  /**
   * The `eosio.system` smart contract is provided by `block.one` as a sample system contract, and it defines the structures and actions needed for blockchain's core functionality.
   *
   * Just like in the `eosio.bios` sample contract implementation, there are a few actions which are not implemented at the contract level (`newaccount`, `updateauth`, `deleteauth`, `linkauth`, `unlinkauth`, `canceldelay`, `onerror`, `setabi`, `setcode`), they are just declared in the contract so they will show in the contract's ABI and users will be able to push those actions to the chain via the account holding the `eosio.system` contract, but the implementation is at the EOSIO core level. They are referred to as EOSIO native actions.
   *
   * - Users can stake tokens for CPU and Network bandwidth, and then vote for producers or
   *    delegate their vote to a proxy.
   * - Producers register in order to be voted for, and can claim per-block and per-vote rewards.
   * - Users can buy and sell RAM at a market-determined price.
   * - Users can bid on premium names.
   * - A resource exchange system (REX) allows token holders to lend their tokens,
   *    and users to rent CPU and Network resources in return for a market-determined fee.
   */

   // A name bid, which consists of:
   // - a `newname` name that the bid is for
   // - a `high_bidder` account name that is the one with the highest bid so far
   // - the `high_bid` which is amount of highest bid
   // - and `last_bid_time` which is the time of the highest bid
   struct [[eosio::table, eosio::contract("eosio.system")]] name_bid {
     name            newname;
     name            high_bidder;
     int64_t         high_bid = 0; ///< negative high_bid == closed auction waiting to be claimed
     time_point      last_bid_time;

     uint64_t primary_key()const { return newname.value;                    }
     uint64_t by_high_bid()const { return static_cast<uint64_t>(-high_bid); }
   };

   // A bid refund, which is defined by:
   // - the `bidder` account name owning the refund
   // - the `amount` to be refunded
   struct [[eosio::table, eosio::contract("eosio.system")]] bid_refund {
      name         bidder;
      asset        amount;

      uint64_t primary_key()const { return bidder.value; }
   };
   typedef eosio::multi_index< "namebids"_n, name_bid,
                               indexed_by<"highbid"_n, const_mem_fun<name_bid, uint64_t, &name_bid::by_high_bid>  >
                             > name_bid_table;

   typedef eosio::multi_index< "bidrefunds"_n, bid_refund > bid_refund_table;

   // Defines new global state parameters.
   struct [[eosio::table("global"), eosio::contract("eosio.system")]] eosio_global_state : eosio::blockchain_parameters {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

      uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
      uint64_t             total_ram_bytes_reserved = 0;
      int64_t              total_ram_stake = 0;

      block_timestamp      last_producer_schedule_update;
      time_point           last_pervote_bucket_fill;
      int64_t              pervote_bucket = 0;
      int64_t              perblock_bucket = 0;
      uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
      int64_t              total_activated_stake = 0;
      time_point           thresh_activated_stake_time;
      uint16_t             last_producer_schedule_size = 0;
      double               total_producer_vote_weight = 0; /// the sum of all producer votes
      block_timestamp      last_name_close;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE_DERIVED( eosio_global_state, eosio::blockchain_parameters,
                                (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                (last_producer_schedule_update)(last_pervote_bucket_fill)
                                (pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)
                                (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close) )
   };

   // Defines new global state parameters added after version 1.0
   struct [[eosio::table("global2"), eosio::contract("eosio.system")]] eosio_global_state2 {
      eosio_global_state2(){}

      uint16_t          new_ram_per_block = 0;
      block_timestamp   last_ram_increase;
      block_timestamp   last_block_num; /* deprecated */
      double            total_producer_votepay_share = 0;
      uint8_t           revision = 0; ///< used to track version updates in the future.

      EOSLIB_SERIALIZE( eosio_global_state2, (new_ram_per_block)(last_ram_increase)(last_block_num)
                        (total_producer_votepay_share)(revision) )
   };

   // Defines new global state parameters added after version 1.3.0
   struct [[eosio::table("global3"), eosio::contract("eosio.system")]] eosio_global_state3 {
      eosio_global_state3() { }
      time_point        last_vpay_state_update;
      double            total_vpay_share_change_rate = 0;

      EOSLIB_SERIALIZE( eosio_global_state3, (last_vpay_state_update)(total_vpay_share_change_rate) )
   };

   // Defines new global state parameters to store inflation rate and distribution
   struct [[eosio::table("global4"), eosio::contract("eosio.system")]] eosio_global_state4 {
      eosio_global_state4() { }
      double   continuous_rate;
      int64_t  inflation_pay_factor;
      int64_t  votepay_factor;

      EOSLIB_SERIALIZE( eosio_global_state4, (continuous_rate)(inflation_pay_factor)(votepay_factor) )
   };

   // Defines the schedule for pre-determined annual rate changes.
   struct [[eosio::table, eosio::contract("eosio.system")]] schedules_info {
      time_point_sec start_time;
      double   continuous_rate;

      uint64_t primary_key() const { return start_time.sec_since_epoch(); }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( schedules_info, (start_time)(continuous_rate) )
   };

   inline eosio::block_signing_authority convert_to_block_signing_authority( const eosio::public_key& producer_key ) {
      return eosio::block_signing_authority_v0{ .threshold = 1, .keys = {{producer_key, 1}} };
   }

   // Defines `producer_info` structure to be stored in `producer_info` table, added after version 1.0
   struct [[eosio::table, eosio::contract("eosio.system")]] producer_info {
      name                                                     owner;
      double                                                   total_votes = 0;
      eosio::public_key                                        producer_key; /// a packed public key object
      bool                                                     is_active = true;
      std::string                                              url;
      uint32_t                                                 unpaid_blocks = 0;
      time_point                                               last_claim_time;
      uint16_t                                                 location = 0;
      eosio::binary_extension<eosio::block_signing_authority>  producer_authority; // added in version 1.9.0

      uint64_t primary_key()const { return owner.value;                             }
      double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); producer_authority.reset(); is_active = false; }

      eosio::block_signing_authority get_producer_authority()const {
         if( producer_authority.has_value() ) {
            bool zero_threshold = std::visit( [](auto&& auth ) -> bool {
               return (auth.threshold == 0);
            }, *producer_authority );
            // zero_threshold could be true despite the validation done in regproducer2 because the v1.9.0 eosio.system
            // contract has a bug which may have modified the producer table such that the producer_authority field
            // contains a default constructed eosio::block_signing_authority (which has a 0 threshold and so is invalid).
            if( !zero_threshold ) return *producer_authority;
         }
         return convert_to_block_signing_authority( producer_key );
      }

      // The unregprod and claimrewards actions modify unrelated fields of the producers table and under the default
      // serialization behavior they would increase the size of the serialized table if the producer_authority field
      // was not already present. This is acceptable (though not necessarily desired) because those two actions require
      // the authority of the producer who pays for the table rows.
      // However, the rmvproducer action and the onblock transaction would also modify the producer table in a similar
      // way and increasing its serialized size is not acceptable in that context.
      // So, a custom serialization is defined to handle the binary_extension producer_authority
      // field in the desired way. (Note: v1.9.0 did not have this custom serialization behavior.)

      template<typename DataStream>
      friend DataStream& operator << ( DataStream& ds, const producer_info& t ) {
         ds << t.owner
            << t.total_votes
            << t.producer_key
            << t.is_active
            << t.url
            << t.unpaid_blocks
            << t.last_claim_time
            << t.location;

         if( !t.producer_authority.has_value() ) return ds;

         return ds << t.producer_authority;
      }

      template<typename DataStream>
      friend DataStream& operator >> ( DataStream& ds, producer_info& t ) {
         return ds >> t.owner
                   >> t.total_votes
                   >> t.producer_key
                   >> t.is_active
                   >> t.url
                   >> t.unpaid_blocks
                   >> t.last_claim_time
                   >> t.location
                   >> t.producer_authority;
      }
   };

   // Defines new producer info structure to be stored in new producer info table, added after version 1.3.0
   struct [[eosio::table, eosio::contract("eosio.system")]] producer_info2 {
      name            owner;
      double          votepay_share = 0;
      time_point      last_votepay_share_update;

      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( producer_info2, (owner)(votepay_share)(last_votepay_share_update) )
   };

   // finalizer_key_info stores information about a finalizer key.
   struct [[eosio::table("finkeys"), eosio::contract("eosio.system")]] finalizer_key_info {
      uint64_t          id;                   // automatically generated ID for the key in the table
      name              finalizer_name;       // name of the finalizer owning the key
      std::string       finalizer_key;        // finalizer key in base64url format
      std::vector<char> finalizer_key_binary; // finalizer key in binary format in Affine little endian non-montgomery g1

      uint64_t    primary_key() const { return id; }
      uint64_t    by_fin_name() const { return finalizer_name.value; }
      // Use binary format to hash. It is more robust and less likely to change
      // than the base64url text encoding of it.
      // There is no need to store the hash key to avoid re-computation,
      // which only happens if the table row is modified. There won't be any
      // modification of the table rows of; it may only be removed.
      checksum256 by_fin_key()  const { return eosio::sha256(finalizer_key_binary.data(), finalizer_key_binary.size()); }

      bool is_active(uint64_t finalizer_active_key_id) const { return id == finalizer_active_key_id ; }
   };
   typedef eosio::multi_index<
      "finkeys"_n, finalizer_key_info,
      indexed_by<"byfinname"_n, const_mem_fun<finalizer_key_info, uint64_t, &finalizer_key_info::by_fin_name>>,
      indexed_by<"byfinkey"_n, const_mem_fun<finalizer_key_info, checksum256, &finalizer_key_info::by_fin_key>>
   > finalizer_keys_table;

   // finalizer_info stores information about a finalizer.
   struct [[eosio::table("finalizers"), eosio::contract("eosio.system")]] finalizer_info {
      name              finalizer_name;           // finalizer's name
      uint64_t          active_key_id;            // finalizer's active finalizer key's id in finalizer_keys_table, for fast finding key information
      std::vector<char> active_key_binary;        // finalizer's active finalizer key's binary format in Affine little endian non-montgomery g1
      uint32_t          finalizer_key_count = 0;  // number of finalizer keys registered by this finalizer

      uint64_t primary_key() const { return finalizer_name.value; }
   };
   typedef eosio::multi_index< "finalizers"_n, finalizer_info > finalizers_table;

   // finalizer_auth_info stores a finalizer's key id and its finalizer authority
   struct finalizer_auth_info {
      finalizer_auth_info() = default;
      explicit finalizer_auth_info(const finalizer_info& finalizer);

      uint64_t                   key_id;        // A finalizer's key ID in finalizer_keys_table
      eosio::finalizer_authority fin_authority; // The finalizer's finalizer_authority

      bool operator==(const finalizer_auth_info& other) const {
         // Weight and description can never be changed by a user.
         // They are not considered here.
         return key_id == other.key_id &&
                fin_authority.public_key == other.fin_authority.public_key;
      };

      EOSLIB_SERIALIZE( finalizer_auth_info, (key_id)(fin_authority) )
   };

   // A single entry storing information about last proposed finalizers.
   // Should avoid  using the global singleton pattern as it unnecessarily
   // serializes data at construction/desstruction of system_contract,
   // even if the data is not used.
   struct [[eosio::table("lastpropfins"), eosio::contract("eosio.system")]] last_prop_finalizers_info {
      std::vector<finalizer_auth_info> last_proposed_finalizers; // sorted by ascending finalizer key id

      uint64_t primary_key()const { return 0; }

      EOSLIB_SERIALIZE( last_prop_finalizers_info, (last_proposed_finalizers) )
   };

   typedef eosio::multi_index< "lastpropfins"_n, last_prop_finalizers_info >  last_prop_fins_table;

   // A single entry storing next available finalizer key_id to make sure
   // key_id in finalizers_table will never be reused.
   struct [[eosio::table("finkeyidgen"), eosio::contract("eosio.system")]] fin_key_id_generator_info {
      uint64_t next_finalizer_key_id = 0;
      uint64_t primary_key()const { return 0; }

      EOSLIB_SERIALIZE( fin_key_id_generator_info, (next_finalizer_key_id) )
   };

   typedef eosio::multi_index< "finkeyidgen"_n, fin_key_id_generator_info >  fin_key_id_gen_table;

   // Voter info. Voter info stores information about the voter:
   // - `owner` the voter
   // - `proxy` the proxy set by the voter, if any
   // - `producers` the producers approved by this voter if no proxy set
   // - `staked` the amount staked
   struct [[eosio::table, eosio::contract("eosio.system")]] voter_info {
      name                owner;     /// the voter
      name                proxy;     /// the proxy set by the voter, if any
      std::vector<name>   producers; /// the producers approved by this voter if no proxy set
      int64_t             staked = 0;

      //  Every time a vote is cast we must first "undo" the last vote weight, before casting the
      //  new vote weight.  Vote weight is calculated as:
      //  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
      double              last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

      // Total vote weight delegated to this voter.
      double              proxied_vote_weight= 0; /// the total vote weight delegated to this voter as a proxy
      bool                is_proxy = 0; /// whether the voter is a proxy for others


      uint32_t            flags1 = 0;
      uint32_t            reserved2 = 0;
      eosio::asset        reserved3;

      uint64_t primary_key()const { return owner.value; }

      enum class flags1_fields : uint32_t {
         ram_managed = 1,
         net_managed = 2,
         cpu_managed = 4
      };

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( voter_info, (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(flags1)(reserved2)(reserved3) )
   };


   typedef eosio::multi_index< "voters"_n, voter_info >  voters_table;


   typedef eosio::multi_index< "producers"_n, producer_info,
                               indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                             > producers_table;

   typedef eosio::multi_index< "producers2"_n, producer_info2 > producers_table2;

   typedef eosio::multi_index< "schedules"_n, schedules_info > schedules_table;

   typedef eosio::singleton< "global"_n, eosio_global_state >   global_state_singleton;

   typedef eosio::singleton< "global2"_n, eosio_global_state2 > global_state2_singleton;

   typedef eosio::singleton< "global3"_n, eosio_global_state3 > global_state3_singleton;

   typedef eosio::singleton< "global4"_n, eosio_global_state4 > global_state4_singleton;

   struct [[eosio::table, eosio::contract("eosio.system")]] user_resources {
      name          owner;
      asset         net_weight;
      asset         cpu_weight;
      int64_t       ram_bytes = 0;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0 && ram_bytes == 0; }
      uint64_t primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( user_resources, (owner)(net_weight)(cpu_weight)(ram_bytes) )
   };

   // Every user 'from' has a scope/table that uses every recipient 'to' as the primary key.
   struct [[eosio::table, eosio::contract("eosio.system")]] delegated_bandwidth {
      name          from;
      name          to;
      asset         net_weight;
      asset         cpu_weight;

      bool is_empty()const { return net_weight.amount == 0 && cpu_weight.amount == 0; }
      uint64_t  primary_key()const { return to.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( delegated_bandwidth, (from)(to)(net_weight)(cpu_weight) )

   };

   struct [[eosio::table, eosio::contract("eosio.system")]] refund_request {
      name            owner;
      time_point_sec  request_time;
      eosio::asset    net_amount;
      eosio::asset    cpu_amount;

      bool is_empty()const { return net_amount.amount == 0 && cpu_amount.amount == 0; }
      uint64_t  primary_key()const { return owner.value; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( refund_request, (owner)(request_time)(net_amount)(cpu_amount) )
   };


   struct [[eosio::table, eosio::contract("eosio.system")]] gifted_ram {
      name      giftee;
      name      gifter;
      int64_t   ram_bytes = 0;

      bool      is_empty()    const { return ram_bytes == 0; }
      uint64_t  primary_key() const { return giftee.value; } // unique as one giftee can only hold gifted ram from one gifter
   };


   typedef eosio::multi_index< "userres"_n, user_resources >      user_resources_table;
   typedef eosio::multi_index< "delband"_n, delegated_bandwidth > del_bandwidth_table;
   typedef eosio::multi_index< "refunds"_n, refund_request >      refunds_table;
   typedef eosio::multi_index< "giftedram"_n, gifted_ram >        gifted_ram_table;

   // `rex_pool` structure underlying the rex pool table. A rex pool table entry is defined by:
   // - `version` defaulted to zero,
   // - `total_lent` total amount of CORE_SYMBOL in open rex_loans
   // - `total_unlent` total amount of CORE_SYMBOL available to be lent (connector),
   // - `total_rent` fees received in exchange for lent  (connector),
   // - `total_lendable` total amount of CORE_SYMBOL that have been lent (total_unlent + total_lent),
   // - `total_rex` total number of REX shares allocated to contributors to total_lendable,
   // - `namebid_proceeds` the amount of CORE_SYMBOL to be transferred from namebids to REX pool,
   // - `loan_num` increments with each new loan
   struct [[eosio::table,eosio::contract("eosio.system")]] rex_pool {
      uint8_t    version = 0;
      asset      total_lent;
      asset      total_unlent;
      asset      total_rent;
      asset      total_lendable;
      asset      total_rex;
      asset      namebid_proceeds;
      uint64_t   loan_num = 0;

      uint64_t primary_key()const { return 0; }
   };

   typedef eosio::multi_index< "rexpool"_n, rex_pool > rex_pool_table;

   // `rex_return_pool` structure underlying the rex return pool table. A rex return pool table entry is defined by:
   // - `version` defaulted to zero,
   // - `last_dist_time` the last time proceeds from renting, ram fees, and name bids were added to the rex pool,
   // - `pending_bucket_time` timestamp of the pending 12-hour return bucket,
   // - `oldest_bucket_time` cached timestamp of the oldest 12-hour return bucket,
   // - `pending_bucket_proceeds` proceeds in the pending 12-hour return bucket,
   // - `current_rate_of_increase` the current rate per dist_interval at which proceeds are added to the rex pool,
   // - `proceeds` the maximum amount of proceeds that can be added to the rex pool at any given time
   struct [[eosio::table,eosio::contract("eosio.system")]] rex_return_pool {
      uint8_t        version = 0;
      time_point_sec last_dist_time;
      time_point_sec pending_bucket_time      = time_point_sec::maximum();
      time_point_sec oldest_bucket_time       = time_point_sec::min();
      int64_t        pending_bucket_proceeds  = 0;
      int64_t        current_rate_of_increase = 0;
      int64_t        proceeds                 = 0;

      static constexpr uint32_t total_intervals  = 30 * 144; // 30 days
      static constexpr uint32_t dist_interval    = 10 * 60;  // 10 minutes
      static constexpr uint8_t  hours_per_bucket = 12;
      static_assert( total_intervals * dist_interval == 30 * seconds_per_day );

      uint64_t primary_key()const { return 0; }
   };

   typedef eosio::multi_index< "rexretpool"_n, rex_return_pool > rex_return_pool_table;

   struct pair_time_point_sec_int64 {
      time_point_sec first;
      int64_t        second;

      EOSLIB_SERIALIZE(pair_time_point_sec_int64, (first)(second));
   };

   // `rex_return_buckets` structure underlying the rex return buckets table. A rex return buckets table is defined by:
   // - `version` defaulted to zero,
   // - `return_buckets` buckets of proceeds accumulated in 12-hour intervals
   struct [[eosio::table,eosio::contract("eosio.system")]] rex_return_buckets {
      uint8_t                                version = 0;
      std::vector<pair_time_point_sec_int64> return_buckets;  // sorted by first field

      uint64_t primary_key()const { return 0; }
   };

   typedef eosio::multi_index< "retbuckets"_n, rex_return_buckets > rex_return_buckets_table;

   // `rex_fund` structure underlying the rex fund table. A rex fund table entry is defined by:
   // - `version` defaulted to zero,
   // - `owner` the owner of the rex fund,
   // - `balance` the balance of the fund.
   struct [[eosio::table,eosio::contract("eosio.system")]] rex_fund {
      uint8_t version = 0;
      name    owner;
      asset   balance;

      uint64_t primary_key()const { return owner.value; }
   };

   typedef eosio::multi_index< "rexfund"_n, rex_fund > rex_fund_table;

   // `rex_balance` structure underlying the rex balance table. A rex balance table entry is defined by:
   // - `version` defaulted to zero,
   // - `owner` the owner of the rex fund,
   // - `vote_stake` the amount of CORE_SYMBOL currently included in owner's vote,
   // - `rex_balance` the amount of REX owned by owner,
   // - `matured_rex` matured REX available for selling
   struct [[eosio::table,eosio::contract("eosio.system")]] rex_balance {
      uint8_t version = 0;
      name    owner;
      asset   vote_stake;
      asset   rex_balance;
      int64_t matured_rex = 0;
      std::vector<pair_time_point_sec_int64> rex_maturities; /// REX daily maturity buckets

      uint64_t primary_key()const { return owner.value; }
   };

   typedef eosio::multi_index< "rexbal"_n, rex_balance > rex_balance_table;

   // `rex_loan` structure underlying the `rex_cpu_loan_table` and `rex_net_loan_table`. A rex net/cpu loan table entry is defined by:
   // - `version` defaulted to zero,
   // - `from` account creating and paying for loan,
   // - `receiver` account receiving rented resources,
   // - `payment` SYS tokens paid for the loan,
   // - `balance` is the amount of SYS tokens available to be used for loan auto-renewal,
   // - `total_staked` total amount staked,
   // - `loan_num` loan number/id,
   // - `expiration` the expiration time when loan will be either closed or renewed
   //       If payment <= balance, the loan is renewed, and closed otherwise.
   struct [[eosio::table,eosio::contract("eosio.system")]] rex_loan {
      uint8_t             version = 0;
      name                from;
      name                receiver;
      asset               payment;
      asset               balance;
      asset               total_staked;
      uint64_t            loan_num;
      eosio::time_point   expiration;

      uint64_t primary_key()const { return loan_num;                   }
      uint64_t by_expr()const     { return expiration.elapsed.count(); }
      uint64_t by_owner()const    { return from.value;                 }
   };

   typedef eosio::multi_index< "cpuloan"_n, rex_loan,
                               indexed_by<"byexpr"_n,  const_mem_fun<rex_loan, uint64_t, &rex_loan::by_expr>>,
                               indexed_by<"byowner"_n, const_mem_fun<rex_loan, uint64_t, &rex_loan::by_owner>>
                             > rex_cpu_loan_table;

   typedef eosio::multi_index< "netloan"_n, rex_loan,
                               indexed_by<"byexpr"_n,  const_mem_fun<rex_loan, uint64_t, &rex_loan::by_expr>>,
                               indexed_by<"byowner"_n, const_mem_fun<rex_loan, uint64_t, &rex_loan::by_owner>>
                             > rex_net_loan_table;

   struct [[eosio::table,eosio::contract("eosio.system")]] rex_order {
      uint8_t             version = 0;
      name                owner;
      asset               rex_requested;
      asset               proceeds;
      asset               stake_change;
      eosio::time_point   order_time;
      bool                is_open = true;

      void close()                { is_open = false;    }
      uint64_t primary_key()const { return owner.value; }
      uint64_t by_time()const     { return is_open ? order_time.elapsed.count() : std::numeric_limits<uint64_t>::max(); }
   };

   typedef eosio::multi_index< "rexqueue"_n, rex_order,
                               indexed_by<"bytime"_n, const_mem_fun<rex_order, uint64_t, &rex_order::by_time>>> rex_order_table;

   struct [[eosio::table("rexmaturity"),eosio::contract("eosio.system")]] rex_maturity {
      uint32_t num_of_maturity_buckets = 5;
      bool sell_matured_rex = false;
      bool buy_rex_to_savings = false;
   };

   typedef eosio::singleton<"rexmaturity"_n, rex_maturity> rex_maturity_singleton;

   struct rex_order_outcome {
      bool success;
      asset proceeds;
      asset stake_change;
   };

   struct action_return_sellram {
      name account;
      asset quantity;
      int64_t bytes_sold;
      int64_t ram_bytes;
      asset fee;
   };

   struct action_return_buyram {
      name payer;
      name receiver;
      asset quantity;
      int64_t bytes_purchased;
      int64_t ram_bytes;
      asset fee;
   };

   struct action_return_ramtransfer {
      name from;
      name to;
      int64_t bytes;
      int64_t from_ram_bytes;
      int64_t to_ram_bytes;
   };

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

      EOSLIB_SERIALIZE( powerup_config_resource, (current_weight_ratio)(target_weight_ratio)(assumed_stake_weight)
                                                (target_timestamp)(exponent)(decay_secs)(min_price)(max_price)    )
   };

   struct powerup_config {
      powerup_config_resource  net;             // NET market configuration
      powerup_config_resource  cpu;             // CPU market configuration
      std::optional<uint32_t> powerup_days;     // `powerup` `days` argument must match this. Do not specify to preserve the
                                                //    existing setting or use the default.
      std::optional<asset>    min_powerup_fee;  // Fees below this amount are rejected. Do not specify to preserve the
                                                //    existing setting (no default exists).

      EOSLIB_SERIALIZE( powerup_config, (net)(cpu)(powerup_days)(min_powerup_fee) )
   };

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
                                                                   //    satoshi of SYS staked.
      int64_t        weight_ratio            = 0;                  // resource market weight ratio:
                                                                   //    assumed_stake_weight / (assumed_stake_weight + weight).
                                                                   //    calculated; varies over time. 1x = 10^15. 0.01x = 10^13.
      int64_t        assumed_stake_weight    = 0;                  // Assumed stake weight for ratio calculations.
      int64_t        initial_weight_ratio    = powerup_frac;        // Initial weight_ratio used for linear shrinkage.
      int64_t        target_weight_ratio     = powerup_frac / 100;  // Linearly shrink the weight_ratio to this amount.
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

   struct [[eosio::table("powup.state"),eosio::contract("eosio.system")]] powerup_state {
      static constexpr uint32_t default_powerup_days = 30; // 30 day resource powerup

      uint8_t                    version           = 0;
      powerup_state_resource     net               = {};                     // NET market state
      powerup_state_resource     cpu               = {};                     // CPU market state
      uint32_t                   powerup_days      = default_powerup_days;   // `powerup` `days` argument must match this.
      asset                      min_powerup_fee   = {};                     // fees below this amount are rejected

      uint64_t primary_key()const { return 0; }
   };

   typedef eosio::singleton<"powup.state"_n, powerup_state> powerup_state_singleton;

   struct [[eosio::table("powup.order"),eosio::contract("eosio.system")]] powerup_order {
      uint8_t              version = 0;
      uint64_t             id;
      name                 owner;
      int64_t              net_weight;
      int64_t              cpu_weight;
      time_point_sec       expires;

      uint64_t primary_key()const { return id; }
      uint64_t by_owner()const    { return owner.value; }
      uint64_t by_expires()const  { return expires.utc_seconds; }
   };

   typedef eosio::multi_index< "powup.order"_n, powerup_order,
                               indexed_by<"byowner"_n, const_mem_fun<powerup_order, uint64_t, &powerup_order::by_owner>>,
                               indexed_by<"byexpires"_n, const_mem_fun<powerup_order, uint64_t, &powerup_order::by_expires>>
                               > powerup_order_table;

   /**
    * The `eosio.system` smart contract is provided by `block.one` as a sample system contract, and it defines the structures and actions needed for blockchain's core functionality.
    *
    * Just like in the `eosio.bios` sample contract implementation, there are a few actions which are not implemented at the contract level (`newaccount`, `updateauth`, `deleteauth`, `linkauth`, `unlinkauth`, `canceldelay`, `onerror`, `setabi`, `setcode`), they are just declared in the contract so they will show in the contract's ABI and users will be able to push those actions to the chain via the account holding the `eosio.system` contract, but the implementation is at the EOSIO core level. They are referred to as EOSIO native actions.
    *
    * - Users can stake tokens for CPU and Network bandwidth, and then vote for producers or
    *    delegate their vote to a proxy.
    * - Producers register in order to be voted for, and can claim per-block and per-vote rewards.
    * - Users can buy and sell RAM at a market-determined price.
    * - Users can bid on premium names.
    * - A resource exchange system (REX) allows token holders to lend their tokens,
    *    and users to rent CPU and Network resources in return for a market-determined fee.
    * - A resource market separate from REX: `power`.
    */
   class [[eosio::contract("eosio.system")]] system_contract : public native {

      private:
         voters_table             _voters;
         producers_table          _producers;
         producers_table2         _producers2;
         finalizer_keys_table     _finalizer_keys;
         finalizers_table         _finalizers;
         last_prop_fins_table     _last_prop_finalizers;
         std::optional<std::vector<finalizer_auth_info>> _last_prop_finalizers_cached;
         fin_key_id_gen_table     _fin_key_id_generator;
         global_state_singleton   _global;
         global_state2_singleton  _global2;
         global_state3_singleton  _global3;
         global_state4_singleton  _global4;
         eosio_global_state       _gstate;
         eosio_global_state2      _gstate2;
         eosio_global_state3      _gstate3;
         eosio_global_state4      _gstate4;
         schedules_table          _schedules;
         rammarket                _rammarket;
         rex_pool_table           _rexpool;
         rex_return_pool_table    _rexretpool;
         rex_return_buckets_table _rexretbuckets;
         rex_fund_table           _rexfunds;
         rex_balance_table        _rexbalance;
         rex_order_table          _rexorders;
         rex_maturity_singleton   _rexmaturity;

      public:
         static constexpr eosio::name active_permission{"active"_n};
         static constexpr eosio::name token_account{"eosio.token"_n};
         static constexpr eosio::name ram_account{"eosio.ram"_n};
         static constexpr eosio::name ramfee_account{"eosio.ramfee"_n};
         static constexpr eosio::name stake_account{"eosio.stake"_n};
         static constexpr eosio::name bpay_account{"eosio.bpay"_n};
         static constexpr eosio::name vpay_account{"eosio.vpay"_n};
         static constexpr eosio::name names_account{"eosio.names"_n};
         static constexpr eosio::name saving_account{"eosio.saving"_n};
         static constexpr eosio::name rex_account{"eosio.rex"_n};
         static constexpr eosio::name fees_account{"eosio.fees"_n};
         static constexpr eosio::name powerup_account{"eosio.powup"_n};
         static constexpr eosio::name reserve_account{"eosio.reserv"_n}; // cspell:disable-line
         static constexpr eosio::name null_account{"eosio.null"_n};
         static constexpr symbol ramcore_symbol = symbol(symbol_code("RAMCORE"), 4);
         static constexpr symbol ram_symbol     = symbol(symbol_code("RAM"), 0);
         static constexpr symbol rex_symbol     = symbol(symbol_code("REX"), 4);

         system_contract( name s, name code, datastream<const char*> ds );
         ~system_contract();

          // Returns the core symbol by system account name
          // @param system_account - the system account to get the core symbol for.
         static symbol get_core_symbol( name system_account = "eosio"_n ) {
            rammarket rm(system_account, system_account.value);
            auto itr = rm.find(ramcore_symbol.raw());
            check(itr != rm.end(), "system contract must first be initialized");
            return itr->quote.balance.symbol;
         }

         // Returns true/false if the rex system is initialized
         static bool rex_system_initialized( name system_account = "eosio"_n ) {
            eosiosystem::rex_pool_table _rexpool( system_account, system_account.value );
            return _rexpool.begin() != _rexpool.end();
         }

         // Returns true/false if the rex system is available
         static bool rex_available( name system_account = "eosio"_n ) {
            eosiosystem::rex_pool_table _rexpool( system_account, system_account.value );
            return rex_system_initialized() && _rexpool.begin()->total_rex.amount > 0;
         }

         // Actions:
         /**
          * The Init action initializes the system contract for a version and a symbol.
          * Only succeeds when:
          * - version is 0 and
          * - symbol is found and
          * - system token supply is greater than 0,
          * - and system contract wasn’t already been initialized.
          *
          * @param version - the version, has to be 0,
          * @param core - the system symbol.
          */
         [[eosio::action]]
         void init( unsigned_int version, const symbol& core );

         /**
          * On block action. This special action is triggered when a block is applied by the given producer
          * and cannot be generated from any other source. It is used to pay producers and calculate
          * missed blocks of other producers. Producer pay is deposited into the producer's stake
          * balance and can be withdrawn over time. Once a minute, it may update the active producer config from the
          * producer votes. The action also populates the blockinfo table.
          *
          * @param header - the block header produced.
          */
         [[eosio::action]]
         void onblock( ignore<block_header> header );

         /**
          * Set account limits action sets the resource limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param ram_bytes - ram limit in absolute bytes,
          * @param net_weight - fractionally proportionate net limit of available resources based on (weight / total_weight_of_all_accounts),
          * @param cpu_weight - fractionally proportionate cpu limit of available resources based on (weight / total_weight_of_all_accounts).
          */
         [[eosio::action]]
         void setalimits( const name& account, int64_t ram_bytes, int64_t net_weight, int64_t cpu_weight );

         /**
          * Set account RAM limits action, which sets the RAM limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param ram_bytes - ram limit in absolute bytes.
          */
         [[eosio::action]]
         void setacctram( const name& account, const std::optional<int64_t>& ram_bytes );

         /**
          * Set account NET limits action, which sets the NET limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param net_weight - fractionally proportionate net limit of available resources based on (weight / total_weight_of_all_accounts).
          */
         [[eosio::action]]
         void setacctnet( const name& account, const std::optional<int64_t>& net_weight );

         /**
          * Set account CPU limits action, which sets the CPU limits of an account
          *
          * @param account - name of the account whose resource limit to be set,
          * @param cpu_weight - fractionally proportionate cpu limit of available resources based on (weight / total_weight_of_all_accounts).
          */
         [[eosio::action]]
         void setacctcpu( const name& account, const std::optional<int64_t>& cpu_weight );


         /**
          * The activate action, activates a protocol feature
          *
          * @param feature_digest - hash of the protocol feature to activate.
          */
         [[eosio::action]]
         void activate( const eosio::checksum256& feature_digest );

         /**
          * Logging for actions resulting in system fees.
          *
          * @param protocol - name of protocol fees were earned from.
          * @param fee - the amount of fees collected by system.
          * @param memo - (optional) the memo associated with the action.
          */
         [[eosio::action]]
         void logsystemfee( const name& protocol, const asset& fee, const std::string& memo );

         // functions defined in delegate_bandwidth.cpp

         /**
          * Delegate bandwidth and/or cpu action. Stakes SYS from the balance of `from` for the benefit of `receiver`.
          *
          * @param from - the account to delegate bandwidth from, that is, the account holding
          *    tokens to be staked,
          * @param receiver - the account to delegate bandwidth to, that is, the account to
          *    whose resources staked tokens are added
          * @param stake_net_quantity - tokens staked for NET bandwidth,
          * @param stake_cpu_quantity - tokens staked for CPU bandwidth,
          * @param transfer - if true, ownership of staked tokens is transferred to `receiver`.
          *
          * @post All producers `from` account has voted for will have their votes updated immediately.
          */
         [[eosio::action]]
         void delegatebw( const name& from, const name& receiver,
                          const asset& stake_net_quantity, const asset& stake_cpu_quantity, bool transfer );

         /**
          * Setrex action, sets total_rent balance of REX pool to the passed value.
          * @param balance - amount to set the REX pool balance.
          */
         [[eosio::action]]
         void setrex( const asset& balance );

         /**
          * Deposit to REX fund action. Deposits core tokens to user REX fund.
          * All proceeds and expenses related to REX are added to or taken out of this fund.
          * An inline transfer from 'owner' liquid balance is executed.
          * All REX-related costs and proceeds are deducted from and added to 'owner' REX fund,
          *    with one exception being buying REX using staked tokens.
          * Storage change is billed to 'owner'.
          *
          * @param owner - REX fund owner account,
          * @param amount - amount of tokens to be deposited.
          */
         [[eosio::action]]
         void deposit( const name& owner, const asset& amount );

         /**
          * Withdraw from REX fund action, withdraws core tokens from user REX fund.
          * An inline token transfer to user balance is executed.
          *
          * @param owner - REX fund owner account,
          * @param amount - amount of tokens to be withdrawn.
          */
         [[eosio::action]]
         void withdraw( const name& owner, const asset& amount );

         /**
          * Buyrex action, buys REX in exchange for tokens taken out of user's REX fund by transferring
          * core tokens from user REX fund and converts them to REX stake. By buying REX, user is
          * lending tokens in order to be rented as CPU or NET resources.
          * Storage change is billed to 'from' account.
          *
          * @param from - owner account name,
          * @param amount - amount of tokens taken out of 'from' REX fund.
          *
          * @post User votes are updated following this action.
          * @post Tokens used in purchase are added to user's voting power.
          * @post Bought REX cannot be sold before {num_of_maturity_buckets} days counting from end of day of purchase.
          */
         [[eosio::action]]
         void buyrex( const name& from, const asset& amount );

         /**
          * Unstaketorex action, uses staked core tokens to buy REX.
          * Storage change is billed to 'owner' account.
          *
          * @param owner - owner of staked tokens,
          * @param receiver - account name that tokens have previously been staked to,
          * @param from_net - amount of tokens to be unstaked from NET bandwidth and used for REX purchase,
          * @param from_cpu - amount of tokens to be unstaked from CPU bandwidth and used for REX purchase.
          *
          * @post User votes are updated following this action.
          * @post Tokens used in purchase are added to user's voting power.
          * @post Bought REX cannot be sold before {num_of_maturity_buckets} days counting from end of day of purchase.
          */
         [[eosio::action]]
         void unstaketorex( const name& owner, const name& receiver, const asset& from_net, const asset& from_cpu );

         /**
          * Sellrex action, sells REX in exchange for core tokens by converting REX stake back into core tokens
          * at current exchange rate. If order cannot be processed, it gets queued until there is enough
          * in REX pool to fill order, and will be processed within 30 days at most. If successful, user
          * votes are updated, that is, proceeds are deducted from user's voting power. In case sell order
          * is queued, storage change is billed to 'from' account.
          *
          * @param from - owner account of REX,
          * @param rex - amount of REX to be sold.
          */
         [[eosio::action]]
         void sellrex( const name& from, const asset& rex );

         /**
          * Cnclrexorder action, cancels unfilled REX sell order by owner if one exists.
          *
          * @param owner - owner account name.
          *
          * @pre Order cannot be cancelled once it's been filled.
          */
         [[eosio::action]]
         void cnclrexorder( const name& owner );

         /**
          * Rentcpu action, uses payment to rent as many SYS tokens as possible as determined by market price and
          * stake them for CPU for the benefit of receiver, after 30 days the rented core delegation of CPU
          * will expire. At expiration, if balance is greater than or equal to `loan_payment`, `loan_payment`
          * is taken out of loan balance and used to renew the loan. Otherwise, the loan is closed and user
          * is refunded any remaining balance.
          * Owner can fund or refund a loan at any time before its expiration.
          * All loan expenses and refunds come out of or are added to owner's REX fund.
          *
          * @param from - account creating and paying for CPU loan, 'from' account can add tokens to loan
          *    balance using action `fundcpuloan` and withdraw from loan balance using `defcpuloan`
          * @param receiver - account receiving rented CPU resources,
          * @param loan_payment - tokens paid for the loan, it has to be greater than zero,
          *    amount of rented resources is calculated from `loan_payment`,
          * @param loan_fund - additional tokens can be zero, and is added to loan balance.
          *    Loan balance represents a reserve that is used at expiration for automatic loan renewal.
          */
         [[eosio::action]]
         void rentcpu( const name& from, const name& receiver, const asset& loan_payment, const asset& loan_fund );

         /**
          * Rentnet action, uses payment to rent as many SYS tokens as possible as determined by market price and
          * stake them for NET for the benefit of receiver, after 30 days the rented core delegation of NET
          * will expire. At expiration, if balance is greater than or equal to `loan_payment`, `loan_payment`
          * is taken out of loan balance and used to renew the loan. Otherwise, the loan is closed and user
          * is refunded any remaining balance.
          * Owner can fund or refund a loan at any time before its expiration.
          * All loan expenses and refunds come out of or are added to owner's REX fund.
          *
          * @param from - account creating and paying for NET loan, 'from' account can add tokens to loan
          *    balance using action `fundnetloan` and withdraw from loan balance using `defnetloan`,
          * @param receiver - account receiving rented NET resources,
          * @param loan_payment - tokens paid for the loan, it has to be greater than zero,
          *    amount of rented resources is calculated from `loan_payment`,
          * @param loan_fund - additional tokens can be zero, and is added to loan balance.
          *    Loan balance represents a reserve that is used at expiration for automatic loan renewal.
          */
         [[eosio::action]]
         void rentnet( const name& from, const name& receiver, const asset& loan_payment, const asset& loan_fund );

         /**
          * Fundcpuloan action, transfers tokens from REX fund to the fund of a specific CPU loan in order to
          * be used for loan renewal at expiry.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param payment - tokens transferred from REX fund to loan fund.
          */
         [[eosio::action]]
         void fundcpuloan( const name& from, uint64_t loan_num, const asset& payment );

         /**
          * Fundnetloan action, transfers tokens from REX fund to the fund of a specific NET loan in order to
          * be used for loan renewal at expiry.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param payment - tokens transferred from REX fund to loan fund.
          */
         [[eosio::action]]
         void fundnetloan( const name& from, uint64_t loan_num, const asset& payment );

         /**
          * Defcpuloan action, withdraws tokens from the fund of a specific CPU loan and adds them to REX fund.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param amount - tokens transferred from CPU loan fund to REX fund.
          */
         [[eosio::action]]
         void defcpuloan( const name& from, uint64_t loan_num, const asset& amount );

         /**
          * Defnetloan action, withdraws tokens from the fund of a specific NET loan and adds them to REX fund.
          *
          * @param from - loan creator account,
          * @param loan_num - loan id,
          * @param amount - tokens transferred from NET loan fund to REX fund.
          */
         [[eosio::action]]
         void defnetloan( const name& from, uint64_t loan_num, const asset& amount );

         /**
          * Updaterex action, updates REX owner vote weight to current value of held REX tokens.
          *
          * @param owner - REX owner account.
          */
         [[eosio::action]]
         void updaterex( const name& owner );

         /**
          * Rexexec action, processes max CPU loans, max NET loans, and max queued sellrex orders.
          * Action does not execute anything related to a specific user.
          *
          * @param user - any account can execute this action,
          * @param max - number of each of CPU loans, NET loans, and sell orders to be processed.
          */
         [[eosio::action]]
         void rexexec( const name& user, uint16_t max );

         /**
          * Consolidate action, consolidates REX maturity buckets into one bucket that can be sold after {num_of_maturity_buckets} days
          * starting from the end of the day.
          *
          * @param owner - REX owner account name.
          */
         [[eosio::action]]
         void consolidate( const name& owner );

         /**
          * Mvtosavings action, moves a specified amount of REX into savings bucket. REX savings bucket
          * never matures. In order for it to be sold, it has to be moved explicitly
          * out of that bucket. Then the moved amount will have the regular maturity
          * period of {num_of_maturity_buckets} days starting from the end of the day.
          *
          * @param owner - REX owner account name.
          * @param rex - amount of REX to be moved.
          */
         [[eosio::action]]
         void mvtosavings( const name& owner, const asset& rex );

         /**
          * Mvfrsavings action, moves a specified amount of REX out of savings bucket. The moved amount
          * will have the regular REX maturity period of {num_of_maturity_buckets} days.
          *
          * @param owner - REX owner account name.
          * @param rex - amount of REX to be moved.
          */
         [[eosio::action]]
         void mvfrsavings( const name& owner, const asset& rex );

         /**
          * Closerex action, deletes owner records from REX tables and frees used RAM. Owner must not have
          * an outstanding REX balance.
          *
          * @param owner - user account name.
          *
          * @pre If owner has a non-zero REX balance, the action fails; otherwise,
          *    owner REX balance entry is deleted.
          * @pre If owner has no outstanding loans and a zero REX fund balance,
          *    REX fund entry is deleted.
          */
         [[eosio::action]]
         void closerex( const name& owner );

         /**
          * Facilitates the modification of REX maturity buckets
          *
          * @param num_of_maturity_buckets - used to calculate maturity time of purchase REX tokens from end of the day UTC.
          * @param sell_matured_rex - if true, matured REX is sold immediately.
          *                           https://github.com/eosnetworkfoundation/eos-system-contracts/issues/134
          * @param buy_rex_to_savings - if true, buying REX is moved immediately to REX savings.
          *                             https://github.com/eosnetworkfoundation/eos-system-contracts/issues/135
          */
         [[eosio::action]]
         void setrexmature(const std::optional<uint32_t> num_of_maturity_buckets, const std::optional<bool> sell_matured_rex, const std::optional<bool> buy_rex_to_savings );
         
         /**
          * Donatetorex action, donates funds to REX, increases REX pool return buckets
          * Executes inline transfer from payer to system contract of tokens will be executed.
          *
          * @param payer - the payer of donated funds.
          * @param quantity - the quantity of tokens to donated to REX with.
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         void donatetorex( const name& payer, const asset& quantity, const std::string& memo );

         /**
          * Undelegate bandwidth action, decreases the total tokens delegated by `from` to `receiver` and/or
          * frees the memory associated with the delegation if there is nothing
          * left to delegate.
          * This will cause an immediate reduction in net/cpu bandwidth of the
          * receiver.
          * A transaction is scheduled to send the tokens back to `from` after
          * the staking period has passed. If existing transaction is scheduled, it
          * will be canceled and a new transaction issued that has the combined
          * undelegated amount.
          * The `from` account loses voting power as a result of this call and
          * all producer tallies are updated.
          *
          * @param from - the account to undelegate bandwidth from, that is,
          *    the account whose tokens will be unstaked,
          * @param receiver - the account to undelegate bandwidth to, that is,
          *    the account to whose benefit tokens have been staked,
          * @param unstake_net_quantity - tokens to be unstaked from NET bandwidth,
          * @param unstake_cpu_quantity - tokens to be unstaked from CPU bandwidth,
          *
          * @post Unstaked tokens are transferred to `from` liquid balance via a
          *    deferred transaction with a delay of 3 days.
          * @post If called during the delay period of a previous `undelegatebw`
          *    action, pending action is canceled and timer is reset.
          * @post All producers `from` account has voted for will have their votes updated immediately.
          * @post Bandwidth and storage for the deferred transaction are billed to `from`.
          */
         [[eosio::action]]
         void undelegatebw( const name& from, const name& receiver,
                            const asset& unstake_net_quantity, const asset& unstake_cpu_quantity );

         /**
          * Buy ram action, increases receiver's ram quota based upon current price and quantity of
          * tokens provided. An inline transfer from receiver to system contract of
          * tokens will be executed.
          *
          * @param payer - the ram buyer,
          * @param receiver - the ram receiver,
          * @param quant - the quantity of tokens to buy ram with.
          */
         [[eosio::action]]
         action_return_buyram buyram( const name& payer, const name& receiver, const asset& quant );

         /**
          * Buy a specific amount of ram bytes action. Increases receiver's ram in quantity of bytes provided.
          * An inline transfer from receiver to system contract of tokens will be executed.
          *
          * @param payer - the ram buyer,
          * @param receiver - the ram receiver,
          * @param bytes - the quantity of ram to buy specified in bytes.
          */
         [[eosio::action]]
         action_return_buyram buyrambytes( const name& payer, const name& receiver, uint32_t bytes );

         /**
          * The buyramself action is designed to enhance the permission security by allowing an account to purchase RAM exclusively for itself.
          * This action prevents the potential risk associated with standard actions like buyram and buyrambytes,
          * which can transfer EOS tokens out of the account, acting as a proxy for eosio.token::transfer.
          *
          * @param account - the ram buyer and receiver,
          * @param quant - the quantity of tokens to buy ram with.
          */
         [[eosio::action]]
         action_return_buyram buyramself( const name& account, const asset& quant );

         /**
          * Logging for buyram & buyrambytes action
          *
          * @param payer - the ram buyer,
          * @param receiver - the ram receiver,
          * @param quantity - the quantity of tokens to buy ram with.
          * @param bytes - the quantity of ram to buy specified in bytes.
          * @param ram_bytes - the ram bytes held by receiver after the action.
          * @param fee - the fee to be paid for the ram sold.
          */
         [[eosio::action]]
         void logbuyram( const name& payer, const name& receiver, const asset& quantity, int64_t bytes, int64_t ram_bytes, const asset& fee );

         /**
          * Sell ram action, reduces quota by bytes and then performs an inline transfer of tokens
          * to receiver based upon the average purchase price of the original quota.
          *
          * @param account - the ram seller account,
          * @param bytes - the amount of ram to sell in bytes.
          */
         [[eosio::action]]
         action_return_sellram sellram( const name& account, int64_t bytes );

         /**
          * Gift ram action, which transfers `bytes` of ram from  `gifter` to `giftee`, with the characteristic
          * that the transfered ram is encumbered, meaning it can only be returned to the gifter via the
          * `ungiftram` action. It cannot be traded, sold, re-gifted, or transfered otherwise.
          * Its only use is for storing data.
          *
          * In addition:
          *  - requires that giftee does not hold gifted ram by someone else than gifter,
          *    as one account can only be hold gifted ram from one gifter at any time.
          *  - current gifter can gift additional ram to a giftee at any time (no restriction)
          *  - the fact of receiving gifted ram does not add any restriction to an
          *    account, besides the usage restictions on the gifted ram itself.
          *    For example, the account can purchase additional ram, and transfer,
          *    trade or sell this additional ram freely.
          *  - this will update both:
          *      a. the `gifted_ram` table to record the gift
          *      b. the `user_resources_table` to increase the usable ram
          *  - the ram cost of adding a row in the gifted_ram table will be incurred
          *    by the gifter.
          *
          * @param gifter - the account ram is transfered from,
          * @param giftee - the account ram is transfered to,
          * @param bytes  - the amount of ram to be transfered in bytes,
          * @param memo   - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         action_return_ramtransfer giftram( const name gifter, const name giftee, int64_t bytes, const std::string& memo );

         /**
          *  Return gifted ram (the full amount) to the gifter.
          *
          *  - because an account can only hold gifted ram from one gifter at any time, the `gifter` 
          *    parameter is not necessary.
          *  - there is currently no built-in incentive for a giftee to return gifted ram.
          *  - if giftee account is found to hold gifted ram, and giftee has enough 
          *    ram available to return the gift, this action will:
          *      a. transfer gifted ram back to gifter (full gifted amount)
          *      b. remove row from the gifted_ram table.
          *      c. unlock ram in the giftee's account equal to the overhead of
          *         removed row.
          *      d. decrease `ram_bytes` by the returned amount in `user_resources_table`
          *  - returned ram is unencumbered, which means there are no restrictions
          *    on its use.
          *
          * @param giftee - the account ram is transfered from,
          * @param gifter - the account ram is transfered to,
          * @param memo   - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         action_return_ramtransfer ungiftram( const name giftee, const name gifter, const std::string& memo );

         /**
          * Logging for sellram action
          *
          * @param account - the ram seller,
          * @param quantity - the quantity of tokens to sell ram with.
          * @param bytes - the quantity of ram to sell specified in bytes.
          * @param ram_bytes - the ram bytes held by account after the action.
          * @param fee - the fee to be paid for the ram sold.
          */
         [[eosio::action]]
         void logsellram( const name& account, const asset& quantity, int64_t bytes, int64_t ram_bytes, const asset& fee );

         /**
          * Transfer ram action, reduces sender's quota by bytes and increase receiver's quota by bytes.
          *
          * @param from - the ram sender account,
          * @param to - the ram receiver account,
          * @param bytes - the amount of ram to transfer in bytes,
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         action_return_ramtransfer ramtransfer( const name& from, const name& to, int64_t bytes, const std::string& memo );

         /**
          * Burn ram action, reduces owner's quota by bytes.
          *
          * @param owner - the ram owner account,
          * @param bytes - the amount of ram to be burned in bytes,
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         action_return_ramtransfer ramburn( const name& owner, int64_t bytes, const std::string& memo );

         /**
          * Buy RAM and immediately burn RAM.
          * An inline transfer from payer to system contract of tokens will be executed.
          *
          * @param payer - the payer of buy RAM & burn.
          * @param quantity - the quantity of tokens to buy RAM & burn with.
          * @param memo - the memo string to accompany the transaction.
          */
         [[eosio::action]]
         action_return_buyram buyramburn( const name& payer, const asset& quantity, const std::string& memo );

         /**
          * Logging for ram changes
          *
          * @param owner - the ram owner account,
          * @param bytes - the bytes balance change,
          * @param ram_bytes - the ram bytes held by owner after the action.
          */
         [[eosio::action]]
         void logramchange( const name& owner, int64_t bytes, int64_t ram_bytes );

         /**
          * Refund action, this action is called after the delegation-period to claim all pending
          * unstaked tokens belonging to owner.
          *
          * @param owner - the owner of the tokens claimed.
          */
         [[eosio::action]]
         void refund( const name& owner );

         // functions defined in voting.cpp

         /**
          * Register producer action, indicates that a particular account wishes to become a producer,
          * this action will create a `producer_config` and a `producer_info` object for `producer` scope
          * in producers tables.
          *
          * @param producer - account registering to be a producer candidate,
          * @param producer_key - the public key of the block producer, this is the key used by block producer to sign blocks,
          * @param url - the url of the block producer, normally the url of the block producer presentation website,
          * @param location - is the country code as defined in the ISO 3166, https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
          *
          * @pre Producer to register is an account
          * @pre Authority of producer to register
          */
         [[eosio::action]]
         void regproducer( const name& producer, const public_key& producer_key, const std::string& url, uint16_t location );

         /**
          * Register producer action, indicates that a particular account wishes to become a producer,
          * this action will create a `producer_config` and a `producer_info` object for `producer` scope
          * in producers tables.
          *
          * @param producer - account registering to be a producer candidate,
          * @param producer_authority - the weighted threshold multisig block signing authority of the block producer used to sign blocks,
          * @param url - the url of the block producer, normally the url of the block producer presentation website,
          * @param location - is the country code as defined in the ISO 3166, https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
          *
          * @pre Producer to register is an account
          * @pre Authority of producer to register
          */
         [[eosio::action]]
         void regproducer2( const name& producer, const eosio::block_signing_authority& producer_authority, const std::string& url, uint16_t location );

         /**
          * Unregister producer action, deactivates the block producer with account name `producer`.
          *
          * Deactivate the block producer with account name `producer`.
          * @param producer - the block producer account to unregister.
          */
         [[eosio::action]]
         void unregprod( const name& producer );

         /**
          * Action to permanently transition to Savanna consensus.
          * Create the first generation of finalizer policy and activate
          * the policy by using `set_finalizers` host function
          *
          * @pre Require the authority of the contract itself
          * @pre A sufficient numner of the top 21 block producers have registered a finalizer key
          */
         [[eosio::action]]
         void switchtosvnn();

         /**
          * Action to register a finalizer key by a registered producer.
          * If this was registered before (and still exists) even
          * by other block producers, the registration will fail.
          * If this is the first registered finalizer key of the producer,
          * it will also implicitly be marked active.
          * A registered producer can have multiple registered finalizer keys.
          *
          * @param finalizer_name - account registering `finalizer_key`,
          * @param finalizer_key - key to be registered. The key is in base64url format.
          * @param proof_of_possession - a valid Proof of Possession signature to show the producer owns the private key of the finalizer_key. The signature is in base64url format.
          *
          * @pre `finalizer_name` must be a registered producer
          * @pre `finalizer_key` must be in base64url format
          * @pre `proof_of_possession` must be a valid of proof of possession signature
          * @pre Authority of `finalizer_name` to register. `linkauth` may be used to allow a lower authrity to exectute this action.
          */
         [[eosio::action]]
         void regfinkey( const name& finalizer_name, const std::string& finalizer_key, const std::string& proof_of_possession);

         /**
          * Action to activate a finalizer key. If the finalizer is currently an
          * active block producer (in top 21), then immediately change Finalizer Policy.
          * A finalizer may only have one active finalizer key. Activating a
          * finalizer key implicitly deactivates the previously active finalizer
          * key of that finalizer.
          *
          * @param finalizer_name - account activating `finalizer_key`,
          * @param finalizer_key - key to be activated.
          *
          * @pre `finalizer_key` must be a registered finalizer key in base64url format
          * @pre Authority of `finalizer_name`
          */
         [[eosio::action]]
         void actfinkey( const name& finalizer_name, const std::string& finalizer_key );

         /**
          * Action to delete a finalizer key. An active finalizer key may not be deleted
          * unless it is the last registered finalizer key. If it is the last one,
          * it will be deleted.
          *
          * @param finalizer_name - account deleting `finalizer_key`,
          * @param finalizer_key - key to be deleted.
          *
          * @pre `finalizer_key` must be a registered finalizer key in base64url format
          * @pre `finalizer_key` must not be active, unless it is the last registered finalizer key
          * @pre Authority of `finalizer_name`
          */
         [[eosio::action]]
         void delfinkey( const name& finalizer_name, const std::string& finalizer_key );

         /**
          * Set ram action sets the ram supply.
          * @param max_ram_size - the amount of ram supply to set.
          */
         [[eosio::action]]
         void setram( uint64_t max_ram_size );

         /**
          * Set ram rate action, sets the rate of increase of RAM in bytes per block. It is capped by the uint16_t to
          * a maximum rate of 3 TB per year. If update_ram_supply hasn't been called for the most recent block,
          * then new ram will be allocated at the old rate up to the present block before switching the rate.
          *
          * @param bytes_per_block - the amount of bytes per block increase to set.
          */
         [[eosio::action]]
         void setramrate( uint16_t bytes_per_block );

         /**
          * Vote producer action, votes for a set of producers. This action updates the list of `producers` voted for,
          * for `voter` account. If voting for a `proxy`, the producer votes will not change until the
          * proxy updates their own vote. Voter can vote for a proxy __or__ a list of at most 30 producers.
          * Storage change is billed to `voter`.
          *
          * @param voter - the account to change the voted producers for,
          * @param proxy - the proxy to change the voted producers for,
          * @param producers - the list of producers to vote for, a maximum of 30 producers is allowed.
          *
          * @pre Producers must be sorted from lowest to highest and must be registered and active
          * @pre If proxy is set then no producers can be voted for
          * @pre If proxy is set then proxy account must exist and be registered as a proxy
          * @pre Every listed producer or proxy must have been previously registered
          * @pre Voter must authorize this action
          * @pre Voter must have previously staked some EOS for voting
          * @pre Voter->staked must be up to date
          *
          * @post Every producer previously voted for will have vote reduced by previous vote weight
          * @post Every producer newly voted for will have vote increased by new vote amount
          * @post Prior proxy will proxied_vote_weight decremented by previous vote weight
          * @post New proxy will proxied_vote_weight incremented by new vote weight
          */
         [[eosio::action]]
         void voteproducer( const name& voter, const name& proxy, const std::vector<name>& producers );

         /**
          * Update the vote weight for the producers or proxy `voter_name` currently votes for. This will also
          * update the `staked` value for the `voter_name` by checking `rexbal` and all delegated NET and CPU.
          *
          * @param voter_name - the account to update the votes for,
          *
          * @post the voter.staked will be updated
          * @post previously voted for producers vote weight will be updated with new weight
          * @post previously voted for proxy vote weight will be updated with new weight
          */
         [[eosio::action]]
         void voteupdate( const name& voter_name );

         /**
          * Register proxy action, sets `proxy` account as proxy.
          * An account marked as a proxy can vote with the weight of other accounts which
          * have selected it as a proxy. Other accounts must refresh their voteproducer to
          * update the proxy's weight.
          * Storage change is billed to `proxy`.
          *
          * @param proxy - the account registering as voter proxy (or unregistering),
          * @param isproxy - if true, proxy is registered; if false, proxy is unregistered.
          *
          * @pre Proxy must have something staked (existing row in voters table)
          * @pre New state must be different than current state
          */
         [[eosio::action]]
         void regproxy( const name& proxy, bool isproxy );

         /**
          * Set the blockchain parameters. By tunning these parameters a degree of
          * customization can be achieved.
          * @param params - New blockchain parameters to set.
          */
         [[eosio::action]]
         void setparams( const blockchain_parameters_t& params );

#ifdef SYSTEM_CONFIGURABLE_WASM_LIMITS
         /**
          * Sets the WebAssembly limits.  Valid parameters are "low",
          * "default" (equivalent to low), and "high".  A value of "high"
          * allows larger contracts to be deployed.
          */
         [[eosio::action]]
         void wasmcfg( const name& settings );
#endif

         /**
          * Claim rewards action, claims block producing and vote rewards.
          * @param owner - producer account claiming per-block and per-vote rewards.
          */
         [[eosio::action]]
         void claimrewards( const name& owner );

         /**
          * Set privilege status for an account. Allows to set privilege status for an account (turn it on/off).
          * @param account - the account to set the privileged status for.
          * @param is_priv - 0 for false, > 0 for true.
          */
         [[eosio::action]]
         void setpriv( const name& account, uint8_t is_priv );

         /**
          * Remove producer action, deactivates a producer by name, if not found asserts.
          * @param producer - the producer account to deactivate.
          */
         [[eosio::action]]
         void rmvproducer( const name& producer );

         /**
          * Update revision action, updates the current revision.
          * @param revision - it has to be incremented by 1 compared with current revision.
          *
          * @pre Current revision can not be higher than 254, and has to be smaller
          * than or equal 1 (“set upper bound to greatest revision supported in the code”).
          */
         [[eosio::action]]
         void updtrevision( uint8_t revision );

         /**
          * Bid name action, allows an account `bidder` to place a bid for a name `newname`.
          * @param bidder - the account placing the bid,
          * @param newname - the name the bid is placed for,
          * @param bid - the amount of system tokens payed for the bid.
          *
          * @pre Bids can be placed only on top-level suffix,
          * @pre Non empty name,
          * @pre Names longer than 12 chars are not allowed,
          * @pre Names equal with 12 chars can be created without placing a bid,
          * @pre Bid has to be bigger than zero,
          * @pre Bid's symbol must be system token,
          * @pre Bidder account has to be different than current highest bidder,
          * @pre Bid must increase current bid by 10%,
          * @pre Auction must still be opened.
          */
         [[eosio::action]]
         void bidname( const name& bidder, const name& newname, const asset& bid );

         /**
          * Bid refund action, allows the account `bidder` to get back the amount it bid so far on a `newname` name.
          *
          * @param bidder - the account that gets refunded,
          * @param newname - the name for which the bid was placed and now it gets refunded for.
          */
         [[eosio::action]]
         void bidrefund( const name& bidder, const name& newname );

         /**
          * Change the annual inflation rate of the core token supply and specify how
          * the new issued tokens will be distributed based on the following structure.
          *
          * @param annual_rate - Annual inflation rate of the core token supply.
          *     (eg. For 5% Annual inflation => annual_rate=500
          *          For 1.5% Annual inflation => annual_rate=150
          * @param inflation_pay_factor - Inverse of the fraction of the inflation used to reward block producers.
          *     The remaining inflation will be sent to the `eosio.saving` account.
          *     (eg. For 20% of inflation going to block producer rewards   => inflation_pay_factor = 50000
          *          For 100% of inflation going to block producer rewards  => inflation_pay_factor = 10000).
          * @param votepay_factor - Inverse of the fraction of the block producer rewards to be distributed proportional to blocks produced.
          *     The remaining rewards will be distributed proportional to votes received.
          *     (eg. For 25% of block producer rewards going towards block pay => votepay_factor = 40000
          *          For 75% of block producer rewards going towards block pay => votepay_factor = 13333).
          */
         [[eosio::action]]
         void setinflation( int64_t annual_rate, int64_t inflation_pay_factor, int64_t votepay_factor );

         /**
          * Change how inflated or vested tokens will be distributed based on the following structure.
          *
          * @param inflation_pay_factor - Inverse of the fraction of the inflation used to reward block producers.
          *     The remaining inflation will be sent to the `eosio.saving` account.
          *     (eg. For 20% of inflation going to block producer rewards   => inflation_pay_factor = 50000
          *          For 100% of inflation going to block producer rewards  => inflation_pay_factor = 10000).
          * @param votepay_factor - Inverse of the fraction of the block producer rewards to be distributed proportional to blocks produced.
          *     The remaining rewards will be distributed proportional to votes received.
          *     (eg. For 25% of block producer rewards going towards block pay => votepay_factor = 40000
          *          For 75% of block producer rewards going towards block pay => votepay_factor = 13333).
          */
         [[eosio::action]]
         void setpayfactor( int64_t inflation_pay_factor, int64_t votepay_factor );

         /**
          * Set the schedule for pre-determined annual rate changes.
          *
          * @param start_time - the time to start the schedule.
          * @param continuous_rate - the inflation or distribution rate of the core token supply.
          *     (eg. For 5% => 0.05
          *          For 1.5% => 0.015)
          */
         [[eosio::action]]
         void setschedule( const time_point_sec start_time, double continuous_rate );

         /**
          * Delete the schedule for pre-determined annual rate changes.
          *
          * @param start_time - the time to start the schedule.
          */
         [[eosio::action]]
         void delschedule( const time_point_sec start_time );

         /**
          * Executes the next schedule for pre-determined annual rate changes.
          *
          * Start time of the schedule must be in the past.
          *
          * Can be executed by any account.
          */
         [[eosio::action]]
         void execschedule();

         /**
          * Facilitates the removal of vested staked tokens from an account, ensuring that these tokens are reallocated to the system's pool.
          *
          * @param account - the target account from which tokens are to be unvested.
          * @param unvest_net_quantity - the amount of NET tokens to unvest.
          * @param unvest_cpu_quantity - the amount of CPU tokens to unvest.
          */
         [[eosio::action]]
         void unvest(const name account, const asset unvest_net_quantity, const asset unvest_cpu_quantity);

         /**
          * Configure the `power` market. The market becomes available the first time this
          * action is invoked.
          */
         [[eosio::action]]
         void cfgpowerup( powerup_config& args );

         /**
          * Process power queue and update state. Action does not execute anything related to a specific user.
          *
          * @param user - any account can execute this action
          * @param max - number of queue items to process
          */
         [[eosio::action]]
         void powerupexec( const name& user, uint16_t max );

         /**
          * Powerup NET and CPU resources by percentage
          *
          * @param payer - the resource buyer
          * @param receiver - the resource receiver
          * @param days - number of days of resource availability. Must match market configuration.
          * @param net_frac - fraction of net (100% = 10^15) managed by this market
          * @param cpu_frac - fraction of cpu (100% = 10^15) managed by this market
          * @param max_payment - the maximum amount `payer` is willing to pay. Tokens are withdrawn from
          *    `payer`'s token balance.
          */
         [[eosio::action]]
         void powerup( const name& payer, const name& receiver, uint32_t days, int64_t net_frac, int64_t cpu_frac, const asset& max_payment );

         /**
          * limitauthchg opts into or out of restrictions on updateauth, deleteauth, linkauth, and unlinkauth.
          *
          * If either allow_perms or disallow_perms is non-empty, then opts into restrictions. If
          * allow_perms is non-empty, then the authorized_by argument of the restricted actions must be in
          * the vector, or the actions will abort. If disallow_perms is non-empty, then the authorized_by
          * argument of the restricted actions must not be in the vector, or the actions will abort.
          *
          * If both allow_perms and disallow_perms are empty, then opts out of the restrictions. limitauthchg
          * aborts if both allow_perms and disallow_perms are non-empty.
          *
          * @param account - account to change
          * @param allow_perms - permissions which may use the restricted actions
          * @param disallow_perms - permissions which may not use the restricted actions
          */
         [[eosio::action]]
         void limitauthchg( const name& account, const std::vector<name>& allow_perms, const std::vector<name>& disallow_perms );

         using init_action = eosio::action_wrapper<"init"_n, &system_contract::init>;
         using setacctram_action = eosio::action_wrapper<"setacctram"_n, &system_contract::setacctram>;
         using setacctnet_action = eosio::action_wrapper<"setacctnet"_n, &system_contract::setacctnet>;
         using setacctcpu_action = eosio::action_wrapper<"setacctcpu"_n, &system_contract::setacctcpu>;
         using activate_action = eosio::action_wrapper<"activate"_n, &system_contract::activate>;
         using logsystemfee_action = eosio::action_wrapper<"logsystemfee"_n, &system_contract::logsystemfee>;
         using delegatebw_action = eosio::action_wrapper<"delegatebw"_n, &system_contract::delegatebw>;
         using deposit_action = eosio::action_wrapper<"deposit"_n, &system_contract::deposit>;
         using withdraw_action = eosio::action_wrapper<"withdraw"_n, &system_contract::withdraw>;
         using buyrex_action = eosio::action_wrapper<"buyrex"_n, &system_contract::buyrex>;
         using unstaketorex_action = eosio::action_wrapper<"unstaketorex"_n, &system_contract::unstaketorex>;
         using sellrex_action = eosio::action_wrapper<"sellrex"_n, &system_contract::sellrex>;
         using cnclrexorder_action = eosio::action_wrapper<"cnclrexorder"_n, &system_contract::cnclrexorder>;
         using rentcpu_action = eosio::action_wrapper<"rentcpu"_n, &system_contract::rentcpu>;
         using rentnet_action = eosio::action_wrapper<"rentnet"_n, &system_contract::rentnet>;
         using fundcpuloan_action = eosio::action_wrapper<"fundcpuloan"_n, &system_contract::fundcpuloan>;
         using fundnetloan_action = eosio::action_wrapper<"fundnetloan"_n, &system_contract::fundnetloan>;
         using defcpuloan_action = eosio::action_wrapper<"defcpuloan"_n, &system_contract::defcpuloan>;
         using defnetloan_action = eosio::action_wrapper<"defnetloan"_n, &system_contract::defnetloan>;
         using updaterex_action = eosio::action_wrapper<"updaterex"_n, &system_contract::updaterex>;
         using rexexec_action = eosio::action_wrapper<"rexexec"_n, &system_contract::rexexec>;
         using setrex_action = eosio::action_wrapper<"setrex"_n, &system_contract::setrex>;
         using mvtosavings_action = eosio::action_wrapper<"mvtosavings"_n, &system_contract::mvtosavings>;
         using mvfrsavings_action = eosio::action_wrapper<"mvfrsavings"_n, &system_contract::mvfrsavings>;
         using consolidate_action = eosio::action_wrapper<"consolidate"_n, &system_contract::consolidate>;
         using closerex_action = eosio::action_wrapper<"closerex"_n, &system_contract::closerex>;
         using donatetorex_action = eosio::action_wrapper<"donatetorex"_n, &system_contract::donatetorex>;
         using undelegatebw_action = eosio::action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;
         using buyram_action = eosio::action_wrapper<"buyram"_n, &system_contract::buyram>;
         using buyrambytes_action = eosio::action_wrapper<"buyrambytes"_n, &system_contract::buyrambytes>;
         using logbuyram_action = eosio::action_wrapper<"logbuyram"_n, &system_contract::logbuyram>;
         using sellram_action = eosio::action_wrapper<"sellram"_n, &system_contract::sellram>;
         using logsellram_action = eosio::action_wrapper<"logsellram"_n, &system_contract::logsellram>;
         using ramtransfer_action = eosio::action_wrapper<"ramtransfer"_n, &system_contract::ramtransfer>;
         using ramburn_action = eosio::action_wrapper<"ramburn"_n, &system_contract::ramburn>;
         using buyramburn_action = eosio::action_wrapper<"buyramburn"_n, &system_contract::buyramburn>;
         using logramchange_action = eosio::action_wrapper<"logramchange"_n, &system_contract::logramchange>;
         using refund_action = eosio::action_wrapper<"refund"_n, &system_contract::refund>;
         using regproducer_action = eosio::action_wrapper<"regproducer"_n, &system_contract::regproducer>;
         using regproducer2_action = eosio::action_wrapper<"regproducer2"_n, &system_contract::regproducer2>;
         using unregprod_action = eosio::action_wrapper<"unregprod"_n, &system_contract::unregprod>;
         using setram_action = eosio::action_wrapper<"setram"_n, &system_contract::setram>;
         using setramrate_action = eosio::action_wrapper<"setramrate"_n, &system_contract::setramrate>;
         using voteproducer_action = eosio::action_wrapper<"voteproducer"_n, &system_contract::voteproducer>;
         using voteupdate_action = eosio::action_wrapper<"voteupdate"_n, &system_contract::voteupdate>;
         using regproxy_action = eosio::action_wrapper<"regproxy"_n, &system_contract::regproxy>;
         using claimrewards_action = eosio::action_wrapper<"claimrewards"_n, &system_contract::claimrewards>;
         using rmvproducer_action = eosio::action_wrapper<"rmvproducer"_n, &system_contract::rmvproducer>;
         using updtrevision_action = eosio::action_wrapper<"updtrevision"_n, &system_contract::updtrevision>;
         using bidname_action = eosio::action_wrapper<"bidname"_n, &system_contract::bidname>;
         using bidrefund_action = eosio::action_wrapper<"bidrefund"_n, &system_contract::bidrefund>;
         using setpriv_action = eosio::action_wrapper<"setpriv"_n, &system_contract::setpriv>;
         using setalimits_action = eosio::action_wrapper<"setalimits"_n, &system_contract::setalimits>;
         using setparams_action = eosio::action_wrapper<"setparams"_n, &system_contract::setparams>;
         using setinflation_action = eosio::action_wrapper<"setinflation"_n, &system_contract::setinflation>;
         using setpayfactor_action = eosio::action_wrapper<"setpayfactor"_n, &system_contract::setpayfactor>;
         using cfgpowerup_action = eosio::action_wrapper<"cfgpowerup"_n, &system_contract::cfgpowerup>;
         using powerupexec_action = eosio::action_wrapper<"powerupexec"_n, &system_contract::powerupexec>;
         using powerup_action = eosio::action_wrapper<"powerup"_n, &system_contract::powerup>;
         using execschedule_action = eosio::action_wrapper<"execschedule"_n, &system_contract::execschedule>;
         using setschedule_action = eosio::action_wrapper<"setschedule"_n, &system_contract::setschedule>;
         using delschedule_action = eosio::action_wrapper<"delschedule"_n, &system_contract::delschedule>;
         using unvest_action = eosio::action_wrapper<"unvest"_n, &system_contract::unvest>;

      private:
         //defined in eosio.system.cpp
         static eosio_global_state get_default_parameters();
         static eosio_global_state4 get_default_inflation_parameters();
         symbol core_symbol()const;
         void update_ram_supply();
         void channel_to_system_fees( const name& from, const asset& amount );
         bool execute_next_schedule();

         // defined in rex.cpp
         void runrex( uint16_t max );
         void update_rex_pool();
         void update_resource_limits( const name& from, const name& receiver, int64_t delta_net, int64_t delta_cpu );
         rex_order_outcome fill_rex_order( const rex_balance_table::const_iterator& bitr, const asset& rex );
         asset update_rex_account( const name& owner, const asset& proceeds, const asset& unstake_quant, bool force_vote_update = false );
         template <typename T>
         int64_t rent_rex( T& table, const name& from, const name& receiver, const asset& loan_payment, const asset& loan_fund );
         template <typename T>
         void fund_rex_loan( T& table, const name& from, uint64_t loan_num, const asset& payment );
         template <typename T>
         void defund_rex_loan( T& table, const name& from, uint64_t loan_num, const asset& amount );
         void transfer_from_fund( const name& owner, const asset& amount );
         void transfer_to_fund( const name& owner, const asset& amount );
         bool rex_loans_available()const;
         static time_point_sec get_rex_maturity(const name& system_account_name = "eosio"_n );
         asset add_to_rex_balance( const name& owner, const asset& payment, const asset& rex_received );
         asset add_to_rex_pool( const asset& payment );
         void add_to_rex_return_pool( const asset& fee );
         void process_rex_maturities( const rex_balance_table::const_iterator& bitr );
         void process_sell_matured_rex( const name owner );
         void process_buy_rex_to_savings( const name owner, const asset rex );
         void consolidate_rex_balance( const rex_balance_table::const_iterator& bitr,
                                       const asset& rex_in_sell_order );
         int64_t read_rex_savings( const rex_balance_table::const_iterator& bitr );
         void put_rex_savings( const rex_balance_table::const_iterator& bitr, int64_t rex );
         void update_rex_stake( const name& voter );
         void sell_rex( const name& from, const asset& rex );

         void add_loan_to_rex_pool( const asset& payment, int64_t rented_tokens, bool new_loan );
         void remove_loan_from_rex_pool( const rex_loan& loan );
         template <typename Index, typename Iterator>
         int64_t update_renewed_loan( Index& idx, const Iterator& itr, int64_t rented_tokens );

         // defined in delegate_bandwidth.cpp
         void changebw( name from, const name& receiver,
                        const asset& stake_net_quantity, const asset& stake_cpu_quantity, bool transfer );
         int64_t update_voting_power( const name& voter, const asset& total_update );
         void set_resource_ram_bytes_limits( const name& owner, int64_t bytes );
         int64_t reduce_ram( const name& owner, int64_t bytes );
         int64_t add_ram( const name& owner, int64_t bytes );
         void update_stake_delegated( const name from, const name receiver, const asset stake_net_delta, const asset stake_cpu_delta );
         void update_user_resources( const name from, const name receiver, const asset stake_net_delta, const asset stake_cpu_delta );

         // defined in voting.cpp
         void register_producer( const name& producer, const eosio::block_signing_authority& producer_authority, const std::string& url, uint16_t location );
         void update_elected_producers( const block_timestamp& timestamp );
         void update_votes( const name& voter, const name& proxy, const std::vector<name>& producers, bool voting );
         void propagate_weight_change( const voter_info& voter );
         double update_producer_votepay_share( const producers_table2::const_iterator& prod_itr,
                                               const time_point& ct,
                                               double shares_rate, bool reset_to_zero = false );
         double update_total_votepay_share( const time_point& ct,
                                            double additional_shares_delta = 0.0, double shares_rate_delta = 0.0 );

         // defined in finalizer_key.cpp
         bool is_savanna_consensus();
         void set_proposed_finalizers( std::vector<finalizer_auth_info> finalizers );
         const std::vector<finalizer_auth_info>& get_last_proposed_finalizers();
         uint64_t get_next_finalizer_key_id();
         finalizers_table::const_iterator get_finalizer_itr( const name& finalizer_name ) const;

         template <auto system_contract::*...Ptrs>
         class registration {
            public:
               template <auto system_contract::*P, auto system_contract::*...Ps>
               struct for_each {
                  template <typename... Args>
                  static constexpr void call( system_contract* this_contract, Args&&... args )
                  {
                     std::invoke( P, this_contract, args... );
                     for_each<Ps...>::call( this_contract, std::forward<Args>(args)... );
                  }
               };
               template <auto system_contract::*P>
               struct for_each<P> {
                  template <typename... Args>
                  static constexpr void call( system_contract* this_contract, Args&&... args )
                  {
                     std::invoke( P, this_contract, std::forward<Args>(args)... );
                  }
               };

               template <typename... Args>
               constexpr void operator() ( Args&&... args )
               {
                  for_each<Ptrs...>::call( this_contract, std::forward<Args>(args)... );
               }

               system_contract* this_contract;
         };

         registration<&system_contract::update_rex_stake> vote_stake_updater{ this };

         // defined in power.cpp
         void adjust_resources(name payer, name account, symbol core_symbol, int64_t net_delta, int64_t cpu_delta, bool must_not_be_managed = false);
         void process_powerup_queue(
            time_point_sec now, symbol core_symbol, powerup_state& state,
            powerup_order_table& orders, uint32_t max_items, int64_t& net_delta_available,
            int64_t& cpu_delta_available);

         // defined in block_info.cpp
         void add_to_blockinfo_table(const eosio::checksum256& previous_block_id, const eosio::block_timestamp timestamp) const;
   };

}
