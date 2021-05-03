#pragma once

#include <eosio/asset.hpp>
#include <eosio/contract.hpp>
#include <eosio/eosio.hpp>
#include <eosio/name.hpp>
#include <eosio/singleton.hpp>
#include <eosio.system/eosio.system.hpp>

namespace eosiodistribute {
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
        std::vector<distribute_account>  accounts; // list of claimant accounts and their percentage of the distribution

        EOSLIB_SERIALIZE (distrib_state, (accounts))
    };

    using distrib_state_singleton = eosio::singleton< "distribstate"_n, distrib_state >;
    
    struct [[eosio::table("distribclaim") , eosio::contract("eosio.distrb")]] distrbute_claimer {
        name        account;    // account that can claim the distribution
        asset       balance;    // current balance of the claimant account

        uint64_t    primary_key() const { return account.value; }
    };
    using claimer_table = eosio::multi_index<"claimers"_n, distrbute_claimer>;
    
    /**
     * A reference contract for distributing received `core tokens` to accounts. Special consideration
     * is given to the `eosio.rex` to distribute all tokens to REX participants. 
     **/
    class [[eosio::contract("eosio.distribute")]] distribute_contract : public contract {
        using contract::contract;

        public:
            distribute_contract( name s, name code, datastream<const char*> ds );
            ~distribute_contract() {} 

            /**
             * Set the accounts and their percentage of the distributed tokens.
             * 
             * @pre accounts exist
             * @pre percentages add up to 100%
             * 
             * @post any tokens sent to the contract account will be distributed based on the percentage
             * @post if `accounts` is an empty array no tokens will be distributed
             * */
            [[eosio::action]]
            void setdistrib(const std::vector<distribute_account>& accounts);

            /**
             * Claim tokens that have been marked for distribution.
             * 
             * @pre `claimer` has tokens to claim
             * 
             * @post row in `claimers` table will be erased
             * */
            [[eosio::action]]
            void claimdistrib(const name& claimer);

            /**
             * Action that will be called when eosio.token::transfer is performed.
             **/
            [[eosio::on_notify("eosio.token::transfer")]]
            void distribute(name from, name to, asset quantity, eosio::ignore<std::string> memo);
            
        private:
            distrib_state_singleton _distrib_singleton;
            distrib_state           _distrib_state;
            claimer_table           _claimers;
            void donate_to_rex(const asset& amount, const std::string& memo);

    }; // distrb_contract

} // ns eosiodistribute