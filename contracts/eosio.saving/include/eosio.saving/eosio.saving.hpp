#pragma once

#include <eosio/asset.hpp>
#include <eosio/contract.hpp>
#include <eosio/eosio.hpp>
#include <eosio/name.hpp>
#include <eosio/singleton.hpp>
#include <eosio.system/eosio.system.hpp>

namespace eosiosaving {
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
        name                        account;
        uint16_t                    percent;
        std::optional<asset>        balance{{0, system_contract::get_core_symbol()}};
    };

    struct [[eosio::table("distribpay") , eosio::contract("eosio.saving")]] distrib_pay {
        distrib_pay() { }
        std::vector<distribute_account>  accounts;

        EOSLIB_SERIALIZE (distrib_pay, (accounts))
    };

    using distrib_pay_singleton = eosio::singleton< "distribpay"_n, distrib_pay >;
    
    /**
     * 
     **/
    class [[eosio::contract("eosio.saving")]] saving_contract : public contract {
        using contract::contract;

        public:
            saving_contract( name s, name code, datastream<const char*> ds );
            ~saving_contract(); 

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
            void distribute(name from, name to, asset quantity, std::string memo);
            
        private:
            distrib_pay_singleton _distrib_pay;
            distrib_pay           _distrib_state;

            void donate_to_rex(const asset& amount);

    }; // saving_contract

} // ns eosiosaving