#include <eosio.token/eosio.token.hpp>

namespace eosio {

void token::create( const name&   issuer,
                    const asset&  maximum_supply )
{   
    require_auth( get_self() );

    auto sym = maximum_supply.symbol;
    check( maximum_supply.is_valid(), "invalid supply");
    check( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( get_self(), [&]( auto& s ) {
       s.avg_daily_inflation     = 0;
       s.avg_yearly_inflation    = 0;
       s.supply.symbol           = maximum_supply.symbol;
       s.max_supply              = maximum_supply;
       s.issuer                  = issuer;
       s.recall                  = true;
       s.authorize               = true;
       s.authorizer              = ""_n;
       s.last_update             = current_time_point();
       s.daily_inf_per_limit     = 10000000000000000000;
       s.yearly_inf_per_limit    = 10000000000000000000;
       s.allowed_daily_inflation = maximum_supply.amount;
    });
}

int64_t token::calculate_avg(uint64_t delta, uint64_t window_span_secs, int64_t current_avg)
{
    const uint64_t window_travelled_ppm = std::min(delta * MILLI / window_span_secs, MILLI);
    const uint64_t reverse_travelled_ppm = MILLI - window_travelled_ppm;
    return int64_t(current_avg * int128_t(reverse_travelled_ppm) / MILLI);
}

void token::issue( const name& to, const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;
    check( to == st.issuer, "tokens can only be issued to issuer account" );

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must issue positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");
   
    const uint64_t delta = current_time_point().sec_since_epoch() - st.last_update.sec_since_epoch();

    const int64_t new_day_avg = calculate_avg(delta, DAY, st.avg_daily_inflation) + quantity.amount;
    const int64_t new_year_avg = calculate_avg(delta, YEAR, st.avg_yearly_inflation) + quantity.amount;

    const asset old_supply = st.supply;
    const asset new_supply = old_supply + quantity; 

    if (new_day_avg > st.allowed_daily_inflation) {
       const uint64_t avg_daily_ppm_inf  = (new_day_avg  * int128_t(MILLI)) / old_supply.amount;
       const uint64_t avg_yearly_ppm_inf = (new_year_avg * int128_t(MILLI)) / old_supply.amount;
       
       check(avg_daily_ppm_inf < st.daily_inf_per_limit , "daily inflation reached");
       check(avg_yearly_ppm_inf < st.yearly_inf_per_limit, "yearly inflation reached");
    }
 
    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply = new_supply;
       s.last_update = current_time_point();
       s.avg_daily_inflation = new_day_avg;
       s.avg_yearly_inflation = new_year_avg;
    });

    add_balance( st.issuer, quantity, st.issuer );
}


void token::update( const symbol_code&    sym, 
                    const bool&      recall, 
                    const bool&      authorize, 
                    const name&      authorizer, 
                    const uint64_t&  daily_inf_per_limit,
                    const uint64_t&  yearly_inf_per_limit,
                    const asset&     allowed_daily_inflation )
{
    stats statstable( get_self(), sym.raw() );
    auto existing = statstable.find( sym.raw() );
    check( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;
    
    require_auth( st.issuer );
    check(sym == allowed_daily_inflation.symbol.code(), "symbol precision mismatch");

    if (recall) {
       check(st.recall, "cannot enable recall once disabled");
    }

    if (authorize) {
       check(st.authorize, "cannot enable recall once disabled");
       if (authorizer != ""_n) {
         check( is_account( authorizer ), "authorizer account does not exist");
       }
    } else {
       check(authorizer == ""_n, "authorizer must be empty");
    }

    check(daily_inf_per_limit <= st.daily_inf_per_limit, "cannot raise daily inflation percent limit");
    check(yearly_inf_per_limit <= st.yearly_inf_per_limit, "cannot raise yearly inflation percent limit");
    check(allowed_daily_inflation.amount <= st.allowed_daily_inflation, "cannot raise absolute daily inflation limit");

    statstable.modify( st, same_payer, [&]( auto& s ) {
      s.recall = recall;
      s.authorize = authorize;
      s.authorizer = authorizer;
      s.daily_inf_per_limit = daily_inf_per_limit;
      s.yearly_inf_per_limit = yearly_inf_per_limit;
      s.allowed_daily_inflation = allowed_daily_inflation.amount;
    });

}


void token::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "invalid symbol name" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must retire positive quantity" );

    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    const uint64_t delta = current_time_point().sec_since_epoch() - st.last_update.sec_since_epoch();

    const int64_t new_day_avg =  calculate_avg(delta, DAY, st.avg_daily_inflation) - quantity.amount;
    const int64_t new_year_avg = calculate_avg(delta, YEAR, st.avg_yearly_inflation) - quantity.amount;

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
       s.last_update = current_time_point();
       s.avg_daily_inflation = new_day_avg;
       s.avg_yearly_inflation = new_year_avg;
    });

    sub_balance( st.issuer, quantity, st.issuer );
}

void token::transfer( const name&    from,
                      const name&    to,
                      const asset&   quantity,
                      const string&  memo )
{
    check( from != to, "cannot transfer to self" );
    check( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    const bool from_is_auth = has_auth(from);
    check(from_is_auth || (has_auth(st.issuer) && st.recall), "invalid authority");

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "invalid quantity" );
    check( quantity.amount > 0, "must transfer positive quantity" );
    check( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    if (st.authorize && st.authorizer != ""_n) {
      action(permission_level{_self, "active"_n}, st.authorizer, "authtrans"_n, std::make_tuple(from, to, quantity, memo))
        .send();
    }
    
    auto modify_payer = from_is_auth ? from : st.issuer;
    auto payer = has_auth( to ) ? to : modify_payer;

    sub_balance( from, quantity, modify_payer );
    add_balance( to, quantity, payer );
}

void token::sub_balance( const name& owner, const asset& value, const name& fallback_payer ) {
   accounts from_acnts( get_self(), owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   check( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, same_payer == owner ? same_payer : fallback_payer, [&]( auto& a ) {
         a.balance -= value;
   });
}

void token::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
   require_auth( ram_payer );

   check( is_account( owner ), "owner account does not exist" );

   auto sym_code_raw = symbol.code().raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   check( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

} /// namespace eosio
