#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <math.h>


#include <string>

#define EOS_SYMBOL symbol("EOS", 4)

namespace eosiosystem {
   class system_contract;
}

namespace eosio {

   using std::string;

   /**
    * The `eosio.symbol` contract defines the structures and actions that allow users to bid and transfer symbols and create tokens    * 
    */
   class [[eosio::contract("eosio.symbol")]] symb : public contract {
      public:
         using contract::contract;

         symb( name receiver, name code, datastream<const char*> ds ) :
           contract(receiver, code, ds),
           global_config(receiver, receiver.value)
           {}

         /**
           * Create action, allows symbol owner to redeem their symbol as a new token on `eosio.token`
           *
           * @param owner - owner of the symbol, also becomes the issuer of the token
           * @param max_supply - max supply of token passed to eosio.token
           *
           *
           * @pre must not already exist on eosio.token
           * @pre must not be for sale by current owner
         */
         [[eosio::action]]
         void create( const name&  owner, 
                      const asset& max_supply );


         /**
          * Purchase action, allows an account `buying` to purchase a symbol `newsymbol`.
          * @param buyer - the account placing the bid,
          * @param newsymbol - the symbol the bid is placed for,
          * @param amount - the amount of tokens to pay for purchase.
          *
          * @pre symbol must not already be purchased
          * @pre amount must be bigger than zero,
          */
         [[eosio::action]]
         void purchase(  const name&         buyer,
                         const symbol_code&  newsymbol, 
                         const asset&        amount );
      

         /**
          * Buy a symbol listed for sale by another user
          * 
          * @param buyer - account purchasing symbol
          * @param symbol - symbol to purchase
          * 
         */
         [[eosio::action]]
         void buysymbol( const name         buyer, 
                         const symbol_code& symbol );

         /**
          * Place an owned symbol on sale for a fixed price
          * 
          * @param symbol - symbol to place for sale
          * @param price - price of symbol
          *
          * @pre only 1 active sale per symbol
         */
         [[eosio::action]]
         void sellsymbol( const symbol_code& symbol, 
                          const asset&       price );

         /**
          * Withdraw an accounts EOS balance
          * 
          * @param owner - owed account
          * @param amount - amount to withdraw
          * 
          * @pre must have an existing balance
          */
         [[eosio::action]]
         void withdraw( const name&           owner, 
                        const extended_asset& amount );


        /**
          * Set secondary sale fee for symbols
          * 
          * @param fee - fee percent in ppm, e.g. 100,000 == 10%
          * 
          * @pre must be contract account
          */
         [[eosio::action]]
         void setsalefee( const uint32_t fee );

        /**
          * Begin / adjust selling symbols of a certain length for an initial price
          * Upsert behavior 
          * 
          * @param symlen - length of symbol
          * @param price - initial price of symbol
          * @param floor - floor price of a symbol
          * @param increase_threshold - amount of sales within window to raise price
          * @param decreased_threshold - minimum amount of sales required before a price drop
          * @param window - window of sale time
          * 
          * @pre must be contract account
          */
         [[eosio::action]]
         void setsym( const uint32_t&  symlen,
                      const asset&     price,
                      const asset&     floor,
                      const uint32_t&  increase_threshold,
                      const uint32_t&  decrease_threshold,
                      const uint32_t&  window );


        /**
          * Transfer a purchased symbol to another account
          * 
          * @param account - destination account 
          * @param symbol - symbol to transfer
          * @param memo - memo
          *
          * @pre must be owner to transfer
          * @pre to account must exist
          * 
          */
         [[eosio::action]]
         void setowner( const name&         to, 
                        const symbol_code&  sym, 
                        const string&       memo );

         using create_action = eosio::action_wrapper<"create"_n, &symb::create>;
         using purchase_action = eosio::action_wrapper<"purchase"_n, &symb::purchase>;
         using buysymbol_action = eosio::action_wrapper<"buysymbol"_n, &symb::buysymbol>;
         using sellsymbol_action = eosio::action_wrapper<"sellsymbol"_n, &symb::sellsymbol>;
         using setsym_action = eosio::action_wrapper<"setsym"_n, &symb::setsym>;
         using setowner_action = eosio::action_wrapper<"setowner"_n, &symb::setowner>;
      


    [[eosio::on_notify("eosio.token::transfer")]]
    void deposit(name hodler, name to, asset quantity, string memo) {
        if (hodler == get_self() || to != get_self() || memo == "ignore")
        {
          return;
        }

        check(quantity.amount > 0, "must be over 0");
        check(quantity.symbol == EOS_SYMBOL, "invalid symbol");

        accounts_table accounts(get_self(), get_self().value);
        auto itr = accounts.find(hodler.value);

        if (itr == accounts.end()) {
          accounts.emplace( get_self(), [&]( auto& col ){
            col.balance = extended_asset(quantity, "eosio.token"_n);
            col.account = hodler;
          });
        } else {
          accounts.modify(itr, same_payer, [&](auto &col) {
            col.balance.quantity += quantity;
          });
        }

    }

      private:
         struct [[eosio::table]] user {
            name              account;
            extended_asset    balance;

            uint64_t primary_key()const { return account.value; }
         };

         struct [[eosio::table]] sym {
            symbol_code    symbol_name;
            name           owner;
            asset          sale_price;

            uint64_t primary_key()const { return symbol_name.raw(); }
         };

         struct [[eosio::table]] symconfig {
           uint64_t   symbol_length;
           asset      price;
           asset      floor;
           time_point window_start;
           uint32_t   window_duration;
           uint32_t   minted_in_window;
           uint32_t   increase_threshold;
           uint32_t   decrease_threshold;

           uint64_t primary_key()const { return symbol_length; }
         };

         struct [[eosio::table]] config {
           name     owner;
           uint32_t fee_ppm;
           
           uint64_t primary_key() const { return owner.value; }
         } configrow;

        struct [[eosio::table]] currency_stats {
            asset    supply;
            asset    max_supply;
            name     issuer;

            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         using singleton_type = eosio::singleton<"cfg"_n, config>;
         singleton_type global_config;
         typedef eosio::multi_index<"accounts"_n, user > accounts_table;
         typedef eosio::multi_index<"symbols"_n, sym > symbols_table;
         typedef eosio::multi_index<"symconfigs"_n, symconfig > symconfig_table;
         typedef eosio::multi_index< "stat"_n, currency_stats > stats;

         void sub_balance( const name& owner, const asset& value );
         void add_balance( const name& owner, const asset& value, const name& ram_payer );

   };

}
