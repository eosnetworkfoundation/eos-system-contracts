#include <eosio.distribute/eosio.distribute.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiodistrb {
    using eosio::token;

    distrbute_contract::distrbute_contract( name s, name code, datastream<const char*> ds )
    :contract(s,code,ds),
    _distrib_singleton(get_self(), get_self().value),
    _claimers(get_self(), get_self().value)
    {
        _distrib_state = _distrib_singleton.exists() ? _distrib_singleton.get() : distrib_state{};
    }
    
    void distrbute_contract::donate_to_rex(const asset& amount, const std::string& memo) {
        system_contract::donatetorex_action donate_action{ system_account, { get_self(), system_contract::active_permission } };
        donate_action.send( get_self(), amount, memo );
    }

    void distrbute_contract::setdistrib(const std::vector<distribute_account>& accounts) {
        require_auth(get_self());
        
        int64_t remaining_pct = max_distrib_pct;
        for( distribute_account acct : accounts ) {
            check(acct.account != get_self(), "Cannot set account to " + get_self().to_string() );
            check(0 < acct.percent, "Only positive percentages are allowed");
            check(acct.percent <= remaining_pct, "Total percentage exceeds 100%");
            remaining_pct -= acct.percent;
            if(acct.account == rex_account)
                donate_to_rex(asset(0, system_contract::get_core_symbol()), "rex enabled check" );
        }
        check(remaining_pct == 0, "Total percentage does not equal 100%");
        // set accounts
        auto& accts = _distrib_state.accounts;
        accts = accounts;

        _distrib_singleton.set( _distrib_state, get_self() );
   }

    void distrbute_contract::claimdistrib(const name& claimer) {
        require_auth(claimer);

        auto citr = _claimers.find(claimer.value);
        check(citr != _claimers.end(), "Not a valid distribution account");
        if(citr->balance.amount != 0) {
            token::transfer_action transfer_act{ system_contract::token_account, { get_self(), system_contract::active_permission } };
            transfer_act.send( get_self(), claimer, citr->balance, "distribution claim" );
        }
        _claimers.erase(citr);
    }

   void distrbute_contract::distribute(name from, name to, asset quantity, eosio::ignore<std::string> memo) {
       if (to != get_self())
           return;
        check(quantity.symbol == eosiosystem::system_contract::get_core_symbol(), "Invalid symbol");
        check(_distrib_state.accounts.size() > 0, "distribution accounts were not setup");

        if(quantity.amount > 0) {
            asset remaining = quantity;
            for( const auto& acct : _distrib_state.accounts) {
                if( remaining.amount == 0 ) {
                    break;
                }
                asset dist_amount = quantity * acct.percent / max_distrib_pct;

                if( remaining < dist_amount )
                    dist_amount = remaining;
                
                if( acct.account == rex_account ) {
                    donate_to_rex(dist_amount, std::string("donation from ") + from.to_string() + " to eosio.rex");
                } else {
                    // add to table index
                    auto citr = _claimers.find(acct.account.value);
                    if(citr == _claimers.end()) {
                        // create record
                        _claimers.emplace(get_self(), [&]( auto& row) {
                            row.account = acct.account;
                            row.balance = dist_amount;
                        });
                    } else {
                        // update balance
                        _claimers.modify(citr, get_self(), [&]( auto& row ){
                            row.balance += dist_amount;
                        });
                    }
                }
                remaining -= dist_amount;
            }
        }
   }

} // ns eosiodistrb