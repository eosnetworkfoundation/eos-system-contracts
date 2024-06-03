#include <eosio.bpay/eosio.bpay.hpp>

namespace eosio {

void bpay::claimrewards( const name owner ) {
    require_auth( owner );

    rewards_table _rewards( get_self(), get_self().value );

    const auto& row = _rewards.get( owner.value, "no rewards to claim" );

    // transfer rewards to owner
    eosio::token::transfer_action transfer( "eosio.token"_n, { get_self(), "active"_n });
    transfer.send( get_self(), owner, row.quantity, "producer block pay" );

    _rewards.erase(row);
}

void bpay::on_transfer( const name from, const name to, const asset quantity, const string memo ) {
    if (from == get_self() || to != get_self()) {
        return;
    }
    // ignore eosio system incoming transfers (caused by bpay income transfers eosio => eosio.bpay => producer)
    if ( from == "eosio"_n) return;

    check( get_first_receiver() == "eosio.token"_n, "only eosio.token allowed") ;

    symbol system_symbol = eosiosystem::system_contract::get_core_symbol();

    check( quantity.symbol == system_symbol, "only core token allowed" );

    rewards_table _rewards( get_self(), get_self().value );
    eosiosystem::system_contract::producers_table _producers( "eosio"_n, "eosio"_n.value );

    eosiosystem::system_contract::global_state_singleton _global("eosio"_n, "eosio"_n.value);
    check( _global.exists(), "global state does not exist");
    uint16_t producer_count = _global.get().last_producer_schedule_size;

    // calculate rewards equal share for top n producers
    asset reward = quantity / producer_count;

    // get producer with the most votes
    // using `by_votes` secondary index
    auto idx = _producers.get_index<"prototalvote"_n>();
    auto prod = idx.begin();

    // get top n producers by vote
    std::vector<name> top_producers;
    while (true) {
        if ( prod == idx.end() ) {
            break;
        }
        if ( prod->is_active == false ) {
            continue;
        }
        top_producers.push_back(prod->owner);
        
        // TODO: Remove
        print("rank=", top_producers.size(), " producer=", prod->owner, " reward=", reward.to_string(), "\n");

        if ( top_producers.size() == producer_count ) {
            break;
        }
        prod++;
    }

    // update rewards table
    for (auto producer : top_producers) {
        auto row = _rewards.find( producer.value );
        if (row == _rewards.end()) {
            _rewards.emplace( get_self(), [&](auto& row) {
                row.owner = producer;
                row.quantity = reward;
            });
        } else {
            _rewards.modify(row, get_self(), [&](auto& row) {
                row.quantity += reward;
            });
        }
    }
}

} /// namespace eosio
