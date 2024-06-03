#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/singleton.hpp>
#include <eosio/binary_extension.hpp>
#include <eosio/producer_schedule.hpp>
#include <eosio/privileged.hpp>

#include <string>

using namespace eosio;

namespace eosiosystem {

class [[eosio::contract("eosio")]] system_contract : public contract
{
public:
    using contract::contract;

    static eosio::block_signing_authority convert_to_block_signing_authority( const eosio::public_key& producer_key ) {
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

    typedef eosio::multi_index< "producers"_n, producer_info,
        indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes>>
    > producers_table;



    // struct [[eosio::table, eosio::contract("eosio.system")]]
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

   typedef eosio::singleton< "global"_n, eosio_global_state >   global_state_singleton;


   struct [[eosio::table, eosio::contract("eosio.system")]] exchange_state {
      asset    supply;

      struct connector {
         asset balance;
         double weight = .5;

         EOSLIB_SERIALIZE( connector, (balance)(weight) )
      };

      connector base;
      connector quote;

      uint64_t primary_key()const { return supply.symbol.raw(); }

      EOSLIB_SERIALIZE( exchange_state, (supply)(base)(quote) )
   };

   typedef eosio::multi_index< "rammarket"_n, exchange_state > rammarket;

   static constexpr symbol ramcore_symbol = symbol(symbol_code("RAMCORE"), 4);
   static symbol get_core_symbol( name system_account = "eosio"_n ) {
        rammarket rm(system_account, system_account.value);
        auto itr = rm.find(ramcore_symbol.raw());
        check(itr != rm.end(), "system contract must first be initialized");
        return itr->quote.balance.symbol;
    }
};
}