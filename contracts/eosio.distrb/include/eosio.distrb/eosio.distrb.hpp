#pragma once

#include <eosio/asset.hpp>
#include <eosio/contract.hpp>
#include <eosio/eosio.hpp>
#include <eosio/name.hpp>
#include <eosio/singleton.hpp>
#include <eosio.system/eosio.system.hpp>

namespace eosiodistrb {
    using eosiosystem::system_contract;
    using eosio::asset;
    using eosio::check;
    using eosio::contract;
    using eosio::name;
    using eosio::datastream;
    
    static constexpr int64_t max_distrib_pct = 100 * eosiosystem::inflation_precision; // 100%
    static constexpr name system_account = "eosio"_n;
    static constexpr name rex_account = system_contract::rex_account;

    struct distribute_account {
        name        account;
        uint16_t    percent;
    };

    struct [[eosio::table("distribstate") , eosio::contract("eosio.distrb")]] distrib_state {
        std::vector<distribute_account>  accounts;

        EOSLIB_SERIALIZE (distrib_state, (accounts))
    };

    using distrib_state_singleton = eosio::singleton< "distribstate"_n, distrib_state >;
    
    struct [[eosio::table("distribclaim") , eosio::contract("eosio.distrb")]] distrbute_claimer {
        name        account;
        asset       balance;

        uint64_t    primary_key() const { return account.value; }
    };
    using claimer_table = eosio::multi_index<"claimers"_n, distrbute_claimer>;
    /**
     * 
     **/
    class [[eosio::contract("eosio.distrb")]] distrib_contract : public contract {
        using contract::contract;

        public:
            distrib_contract( name s, name code, datastream<const char*> ds );
            ~distrib_contract(); 

            /**
             * 
             * */
            [[eosio::action]]
            void setdistrib(const std::vector<distribute_account>& accounts);

            /**
             * 
             * */
            [[eosio::action]]
            void claimdistrib(const name& claimer);

            /**
             * 
             **/
            [[eosio::on_notify("eosio.token::transfer")]]
            void distribute(name from, name to, asset quantity, eosio::ignore<std::string> memo);
            
        private:
            distrib_state_singleton _distrib_singleton;
            distrib_state           _distrib_state;
            claimer_table           _claimers;
            void donate_to_rex(const asset& amount, const std::string& memo);

    }; // distrb_contract

} // ns eosiodistrb