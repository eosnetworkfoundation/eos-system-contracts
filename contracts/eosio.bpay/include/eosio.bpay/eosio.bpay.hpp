#pragma once

#include <eosio/eosio.hpp>
#include <eosio.system/eosio.system.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace std;

namespace eosio {
    /**
     * The `eosio.bpay` contract handles system bpay distribution.
     */
    class [[eosio::contract("eosio.bpay")]] bpay : public contract {
    public:
        using contract::contract;

        /**
         * ## TABLE `rewards`
         *
         * @param owner - block producer owner account
         * @param quantity - reward quantity in EOS
         *
         * ### example
         *
         * ```json
         * [
         *   {
         *     "owner": "alice",
         *     "quantity": "8.800 EOS"
         *   }
         * ]
         * ```
         */
        struct [[eosio::table("rewards")]] rewards_row {
            name                owner;
            asset               quantity;

            uint64_t primary_key() const { return owner.value; }
        };
        typedef eosio::multi_index< "rewards"_n, rewards_row > rewards_table;

        /**
         * Claim rewards for a block producer.
         *
         * @param owner - block producer owner account
         */
        [[eosio::action]]
        void claimrewards( const name owner);

        [[eosio::on_notify("eosio.token::transfer")]]
        void on_transfer( const name from, const name to, const asset quantity, const string memo );

    private:
    };
} /// namespace eosio
