#include <cstdint>
#include <eosio.system/eosio.system.hpp>
#include <eosio.system/peer_keys.hpp>

#include <eosio/eosio.hpp>

namespace eosiosystem {

void peer_keys::regpeerkey(const name& proposer_finalizer_name, const public_key& key) {
   require_auth(proposer_finalizer_name);
   peer_keys_table peer_keys_table(get_self(), get_self().value);
   check(!std::holds_alternative<eosio::webauthn_public_key>(key), "webauthn keys not allowed in regpeerkey action");

   auto peers_itr = peer_keys_table.find(proposer_finalizer_name.value);
   if (peers_itr == peer_keys_table.end()) {
      peer_keys_table.emplace(proposer_finalizer_name, [&](auto& row) {
         row.init_row(proposer_finalizer_name);
         row.set_public_key(key);
      });
   } else {
      const auto& prev_key = peers_itr->get_public_key();
      check(!prev_key || *prev_key != key, "Provided key is the same as currently stored one");
      peer_keys_table.modify(peers_itr, same_payer, [&](auto& row) {
         row.update_row();
         row.set_public_key(key);
      });
   }
}

void peer_keys::delpeerkey(const name& proposer_finalizer_name, const public_key& key) {
   require_auth(proposer_finalizer_name);
   peer_keys_table peer_keys_table(get_self(), get_self().value);

   auto peers_itr = peer_keys_table.find(proposer_finalizer_name.value);
   check(peers_itr != peer_keys_table.end(), "Key not present for name: " + proposer_finalizer_name.to_string());
   const auto& prev_key = peers_itr->get_public_key();
   check(prev_key && *prev_key == key, "Current key does not match the provided one");
   peer_keys_table.erase(peers_itr);
}

peer_keys::getpeerkeys_res_t peer_keys::getpeerkeys() {
   peer_keys_table  peer_keys_table(get_self(), get_self().value);
   producers_table  producers(get_self(), get_self().value);
   constexpr size_t max_return = 50;

   getpeerkeys_res_t resp;
   resp.reserve(max_return);

   double vote_threshold = 0; // vote_threshold will always be >= 0

   auto add_peer = [&](auto it) {
      auto peers_itr = peer_keys_table.find(it->owner.value);
      if (peers_itr == peer_keys_table.end())
         resp.push_back(peerkeys_t{it->owner, {}});
      else
         resp.push_back(peerkeys_t{it->owner, peers_itr->get_public_key()});

      // once 21 producers have been selected, we will only consider producers that have more
      // than 50% of the votes of the 21st selected producer.
      // ------------------------------------------------------------------------------------
      if (resp.size() == 21)
         vote_threshold = std::abs(it->total_votes) * 0.5;
   };

   auto idx = producers.get_index<"prototalvote"_n>();

   auto it  = idx.cbegin();
   auto rit = idx.cend();
   if (it == rit)
      return resp;
   else
      --rit;

   // 1. Consider both active and non-active producers. as a non-active producer can be
   //    reactivated at any time.
   // 2. Once we have selected 21 producers, the threshold of votes required to be selected
   //    increases from `> 0` to `> 50% of the votes that the 21st selected producer has`.
   // 3. We iterate from both ends as non-active producers have their vote total negated, so
   //    the highest voted non-active producer will be the last entry of our index.
   // --------------------------------------------------------------------------------------
   bool last_one = false;
   do  {
      // invariants:
      //   - `it` and `rit` both point to a valid `producer_info` (possibly the same)
      //   - `it` <= `rit`
      // ----------------------------------------------------------------------------
      last_one = (it == rit);
      if (rit->total_votes < vote_threshold && (-rit->total_votes > std::abs(it->total_votes))) {
         add_peer(rit);
         if (!last_one)
            --rit;
      } else if (it->total_votes > vote_threshold) {
         add_peer(it);
         ++it;
      } else {
         break;
      }
   } while (!last_one && resp.size() < max_return);

   return resp;
}

} // namespace eosiosystem
