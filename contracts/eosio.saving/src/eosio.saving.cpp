#include <eosio.saving/eosio.saving.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosaving {
    using eosio::token;

    saving_contract::saving_contract( name s, name code, datastream<const char*> ds )
    :contract(s,code,ds),
    _distrib_pay(get_self(), get_self().value)
    {
        _distrib_state = _distrib_pay.exists() ? _distrib_pay.get() : distrib_pay{};
    }

    saving_contract::~saving_contract() { }
    
    void saving_contract::donate_to_rex(const asset& amount) {
        system_contract::donatetorex_action donate_action{ system_account, { get_self(), system_contract::active_permission } };
        donate_action.send( get_self(), amount );
    }

    void saving_contract::setdistrib(const std::vector<distribute_account>& accounts) {
        require_auth(get_self());
        check(accounts.size() > 0, "accounts cannot be empty");

        int64_t remaining_pct = max_distrib_pct;
        std::vector<distribute_account> new_accounts{};
        for( distribute_account acct : accounts ) {
            check(acct.account != get_self(), "Cannot set account to " + get_self().to_string() );
            check(0 < acct.percent, "Only positive percentages are allowed");
            check(acct.percent <= remaining_pct, "Total percentage exceeds 100%");
            remaining_pct -= acct.percent;
            if(acct.account == rex_account){
                donate_to_rex(asset(0, system_contract::get_core_symbol()) );
            }
            distribute_account new_acct{};
            new_acct.account = acct.account;
            new_acct.percent = acct.percent;
            new_accounts.push_back(new_acct);
        }
        check(remaining_pct == 0, "Total percentage does not equal 100%");
        // set accounts
        auto& accts = _distrib_state.accounts;
        accts = new_accounts;

        _distrib_pay.set( _distrib_state, get_self() );
   }

    void saving_contract::claimdistrib(const name& claimer) {
        require_auth(claimer);

        for( distribute_account& acct : _distrib_state.accounts ) {
            if(acct.account == claimer) {
                auto& balance = acct.balance.value();
                check(balance.amount > 0, "Nothing to claim");
                token::transfer_action transfer_act{ system_contract::token_account, { get_self(), system_contract::active_permission } };
                transfer_act.send( get_self(), claimer, balance, "eosio.saving claim" );
                balance = {0, system_contract::get_core_symbol()};
            }
        }
        _distrib_pay.set( _distrib_state, get_self() );
    }

   void saving_contract::distribute(name from, name to, asset quantity, std::string memo) {
       if (from != system_account || to != get_self())
           return;
        check(quantity.symbol == eosiosystem::system_contract::get_core_symbol(), "Invalid symbol");
        check(_distrib_state.accounts.size() > 0, "distribution accounts were not setup");

        if(quantity.amount > 0) {
            asset remaining = quantity;
            for( auto& acct : _distrib_state.accounts) {
                if( remaining.amount == 0 ) {
                    break;
                }
                asset dist_amount = quantity * acct.percent / max_distrib_pct;
                asset& bal = acct.balance.value();

                if( remaining < dist_amount )
                    dist_amount = remaining;
                
                if( acct.account == rex_account ) {
                    donate_to_rex(dist_amount);
                } else {
                    bal += dist_amount;
                }
                remaining -= dist_amount;
            }
            _distrib_pay.set( _distrib_state, get_self() );
        }
   }

} // ns eosiosaving