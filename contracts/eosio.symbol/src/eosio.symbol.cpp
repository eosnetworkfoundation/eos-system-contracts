#include "eosio.symbol.hpp"

namespace eosio {

void symb::setsalefee( uint32_t fee )
{
    require_auth(get_self());
    check(fee < 1000000, "fee too high");

    auto entry_stored = global_config.get_or_create(get_self(), configrow);
    check(fee != entry_stored.fee_ppm, "fee already set");
    entry_stored.fee_ppm = fee;
    global_config.set(entry_stored, get_self());

}

void symb::create( const name&   issuer,
                   const asset&  maximum_supply )
{
    const symbol_code symbol = maximum_supply.symbol.code();

    symbols_table symbols( get_self(), get_self().value );
    auto st = symbols.find( symbol.raw() );
    check(st != symbols.end(), "symbol not found");

    require_auth(st->owner);
    action(permission_level{_self, "active"_n}, "eosio.token"_n, "create"_n, std::make_tuple(issuer, maximum_supply))
      .send();
    
    symbols.erase(st);
}

void symb::buysymbol( const name buyer, const symbol_code& symbol )
{
    require_auth(buyer);

    const asset zero_eos = asset(0, eosio::symbol("EOS", 4));

    symbols_table symbols( get_self(), get_self().value );

    auto st = symbols.find( symbol.raw() );
    check(st != symbols.end(), "symbol not found");
    const asset sale_price = st->sale_price; 
    check(sale_price != zero_eos, "symbol not on sale");

    accounts_table accounts( get_self(), get_self().value );
    const auto& itr = accounts.find( buyer.value );
    check(itr != accounts.end(), "user not found");
    check(itr->balance >= sale_price, "cannot afford");

    const name seller = st->owner;

    auto entry_stored = global_config.get_or_create(get_self(), configrow);
    const uint32_t fee_percent = entry_stored.fee_ppm;

    const asset fee = asset((sale_price.amount * fee_percent) / 1000000, zero_eos.symbol); 
   
    symbols.modify(st, same_payer, [&](auto &col) {
        col.owner = buyer;
        col.sale_price = zero_eos;
    });
    
    add_balance(get_self(), fee, get_self());
    add_balance(seller, sale_price - fee, get_self());
    sub_balance(buyer, sale_price);
}

void symb::sellsymbol( const symbol_code& symbol, 
                       const asset& price )
{
    check(price.symbol == eosio::symbol("EOS", 4), "invalid symbol price");
    symbols_table symbols( get_self(), get_self().value );
    const auto& st = symbols.find( symbol.raw() );
    check(st != symbols.end(), "symbol not found");

    check(st->sale_price != price, "same price already set");
    require_auth(st->owner);

    symbols.modify(st, same_payer, [&](auto &col) {
        col.sale_price = price;
    });
}

void symb::withdraw( const name& owner, const extended_asset& amount )
{
    require_auth(owner);

    accounts_table accounts( get_self(), get_self().value );
    const auto& itr = accounts.find( owner.value ); 
    check(itr != accounts.end(), "account not found");

    check(amount.contract == "eosio.token"_n, "invalid contract");
    check(amount.quantity.amount > 0, "withdrawal must be above 0");
    check(amount.quantity.symbol == EOS_SYMBOL, "unsupported symbol");
    check(itr->balance >= amount.quantity, "insufficient balance");

    action(permission_level{_self, "active"_n}, "eosio.token"_n, "transfer"_n, std::make_tuple(_self, owner, amount.quantity, string("refund")))
        .send();

    sub_balance(owner, amount.quantity);
}

void symb::setsym( const uint32_t& symlen, 
                   const asset&    price, 
                   const asset&    floor,
                   const uint32_t& increase_threshold, 
                   const uint32_t& decrease_threshold, 
                   const uint32_t& window )
{
    require_auth(get_self());

    check(price.symbol == EOS_SYMBOL && floor.symbol == EOS_SYMBOL, "invalid token symbol");
    check(window != 0, "window must be above 0 seconds");
    check(increase_threshold > decrease_threshold, "increase threshold must be greater");
    check(floor.amount >= 0 && price.amount >= 0, "floor and price must be equal or above 0");

    symconfig_table symconfigs( get_self(), get_self().value );
    const auto& itr = symconfigs.find( symlen ); 

    if (itr == symconfigs.end()) {
      symconfigs.emplace( get_self(), [&](auto& s) {
          s.symbol_length = symlen;
          s.price = price;
          s.floor = floor;
          s.window_start = current_time_point();
          s.minted_in_window = 0;
          s.increase_threshold = increase_threshold;
          s.decrease_threshold = decrease_threshold;
          s.window_duration = window;
      });
    } else {
      symconfigs.modify( itr, same_payer, [&](auto& s) {
          s.price = price;
          s.floor = floor;
          s.window_start = current_time_point();
          s.minted_in_window = 0;
          s.increase_threshold = increase_threshold;
          s.decrease_threshold = decrease_threshold;
          s.window_duration = window;
      });
    }
}

void symb::setowner( const name&          to,
                     const symbol_code&   sym,
                     const string&        memo )
{
    check( is_account( to ), "to account does not exist");
    check( sym.is_valid(), "invalid sym");

    symbols_table symbols( get_self(), get_self().value );
    const auto& st = symbols.find( sym.raw() );

    require_auth(st->owner);
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : st->owner;

    symbols.modify(st, payer, [&](auto &col) {
        col.owner = to;
        col.sale_price = asset(0, EOS_SYMBOL);
    });
}


void symb::purchase( const name&         buyer,
                     const symbol_code&  newsymbol,
                     const asset&        amount )
{
    check(newsymbol.is_valid(), "invalid symbol");
    check(amount.symbol == EOS_SYMBOL, "invalid amount sym");
    require_auth(buyer);

    symbols_table symbols( get_self(), get_self().value );
    const auto& st = symbols.find( newsymbol.raw() );
    check(st == symbols.end(), "symbol already exists");

    auto sym_code_raw = newsymbol.raw();
    stats statstable( "eosio.token"_n, sym_code_raw );
    const auto& stt = statstable.find( sym_code_raw );
    check(stt == statstable.end(), "token symbol already exists");

    symconfig_table symconfigs( get_self(), get_self().value );
    auto itr = symconfigs.find( newsymbol.length() );
    check(itr != symconfigs.end(), "sym len not for sale");

    symbols.emplace( get_self(), [&](auto& s) {
        s.symbol_name = newsymbol;
        s.owner = buyer;
        s.sale_price = asset(0, EOS_SYMBOL);
    });

    // Skip further billing / accounting logic
    // in case contract owner
    // wishes to reserve certain symbols for the network
    if (buyer == get_self()) return;

    asset adjusted_price = itr->price;

    const uint64_t time_elasped = current_time_point().sec_since_epoch() - itr->window_start.sec_since_epoch();

    uint32_t decrease_threshold = itr->decrease_threshold;
    uint32_t increase_threshold = itr->increase_threshold;
    uint32_t window = itr->window_duration;
    asset floor = itr->floor;

    const bool within_window = time_elasped < window;
    const double price_adjust_pct = 0.1;

    if (within_window) {
      if (itr->minted_in_window >= increase_threshold) {
        const asset raised_price = asset(adjusted_price.amount * (1.0 + price_adjust_pct), EOS_SYMBOL);
        const asset starting_price = asset(1000, EOS_SYMBOL);

        symconfigs.modify(itr, same_payer, [&](auto& a) {
          a.minted_in_window = 0;
          a.window_start = current_time_point();
          a.price = raised_price == asset(0, EOS_SYMBOL) ? starting_price : raised_price;
        });
      } else {
        symconfigs.modify(itr, same_payer, [&](auto& a) {
          a.minted_in_window += 1;
        });
      }
    } else {
      const uint64_t windows_elasped = time_elasped / window;
      const bool is_next_window = windows_elasped == 1;
      const bool last_window_satisfied = itr->minted_in_window >= decrease_threshold;

      if (is_next_window && last_window_satisfied) {
        symconfigs.modify(itr, same_payer, [&](auto& a) {
          a.minted_in_window = 1;
          a.window_start = current_time_point();
        });
      } else {
        const int64_t lowered_amount = itr->price.amount * pow(1.0 - price_adjust_pct, last_window_satisfied ? windows_elasped - 1 : windows_elasped);
        const asset lowered_price = asset(std::max(lowered_amount, floor.amount), EOS_SYMBOL);
        adjusted_price = lowered_price;
        symconfigs.modify(itr, same_payer, [&](auto& a) {
          a.minted_in_window = 1;
          a.window_start = current_time_point();
          a.price = lowered_price;
        });
      }
    }

    check(amount >= adjusted_price, "insufficient purchase amount");
    
    sub_balance(buyer, adjusted_price);
    add_balance(get_self(), adjusted_price, get_self());
}


void symb::add_balance( const name& owner, const asset& value, const name& ram_payer )
{
   if (value == asset(0, EOS_SYMBOL)) {
     return;
   }

   accounts_table accounts( get_self(), get_self().value );
   auto itr = accounts.find( owner.value );

   if( itr == accounts.end() ) {
      accounts.emplace( ram_payer, [&](auto& a){
        a.balance = value;
        a.account = owner;
      });
   } else {
      accounts.modify( itr, same_payer, [&](auto& a) {
        a.balance += value;
      });
   }
}

void symb::sub_balance( const name& owner, const asset& value ) {

    if (value == asset(0, EOS_SYMBOL)) {
      return;
    }

    accounts_table accounts( get_self(), get_self().value );
    const auto& itr = accounts.find( owner.value ); 
    check(itr != accounts.end(), "account not found");


    check(itr->balance >= value, "insufficient balance");
    const asset new_balance = itr->balance - value;

    if (new_balance == asset(0, EOS_SYMBOL)) {
        accounts.erase(itr);
    } else {
      accounts.modify( itr, same_payer, [&](auto& s) {
        s.balance = new_balance;
      });
    }

}





} /// namespace eosio
