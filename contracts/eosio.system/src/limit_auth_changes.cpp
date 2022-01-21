#include <eosio.system/limit_auth_changes.hpp>
#include <eosio.system/eosio.system.hpp>

namespace eosiosystem {

   void system_contract::limitauthchg(const name& account, const std::vector<name>& allow_perms,
                                      const std::vector<name>& disallow_perms) {
      limit_auth_change_table table(get_self(), get_self().value);
      require_auth(account);
      eosio::check(allow_perms.empty() || disallow_perms.empty(), "either allow_perms or disallow_perms must be empty");
      eosio::check(allow_perms.empty() ||
                   std::find(allow_perms.begin(), allow_perms.end(), "owner"_n) != allow_perms.end(),
                   "allow_perms does not contain owner");
      eosio::check(disallow_perms.empty() ||
                   std::find(disallow_perms.begin(), disallow_perms.end(), "owner"_n) == disallow_perms.end(),
                   "disallow_perms contains owner");
      auto it = table.find(account.value);
      if(!allow_perms.empty() || !disallow_perms.empty()) {
         if(it == table.end()) {
            table.emplace(account, [&](auto& row){
               row.account = account;
               row.allow_perms = allow_perms;
               row.disallow_perms = disallow_perms;
            });
         } else {
            table.modify(it, account, [&](auto& row){
               row.allow_perms = allow_perms;
               row.disallow_perms = disallow_perms;
            });
         }
      } else {
         if(it != table.end())
            table.erase(it);
      }
   }

   void check_auth_change(name contract, name account, const binary_extension<name>& authorized_by) {
      name by(authorized_by.has_value() ? authorized_by.value().value : 0);
      if(by.value)
         eosio::require_auth({account, by});
      limit_auth_change_table table(contract, contract.value);
      auto it = table.find(account.value);
      if(it == table.end())
         return;
      eosio::check(by.value, "authorized_by is required for this account");
      if(!it->allow_perms.empty())
         eosio::check(
            std::find(it->allow_perms.begin(), it->allow_perms.end(), by) != it->allow_perms.end(),
            "authorized_by does not appear in allow_perms");
      else
         eosio::check(
            std::find(it->disallow_perms.begin(), it->disallow_perms.end(), by) == it->disallow_perms.end(),
            "authorized_by appears in disallow_perms");
   }

} // namespace eosiosystem
