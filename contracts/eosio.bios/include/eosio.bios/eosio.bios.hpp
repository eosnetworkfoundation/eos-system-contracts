#pragma once

#include <eosio/action.hpp>
#include <eosio/crypto.hpp>
#include <eosio/eosio.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/privileged.hpp>
#include <eosio/producer_schedule.hpp>
#include "eosio.bios.ricardian.hpp"

namespace eosio_bios {

   using eosio::action_wrapper;
   using eosio::authority;
   using eosio::check;
   using eosio::checksum256;
   using eosio::ignore;
   using eosio::name;
   using eosio::permission_level;
   using eosio::public_key;

   struct abi_hash {
      name              owner;
      checksum256       hash;
      uint64_t primary_key() const { return owner.value; }
   };
   EOSIO_REFLECT(abi_hash, owner, hash)

   typedef eosio::multi_index<"abihash"_n, abi_hash> abi_hash_table;

   /**
    * The `eosio.bios` is the first sample of system contract provided by `block.one` through the EOSIO platform. It is a minimalist system contract because it only supplies the actions that are absolutely critical to bootstrap a chain and nothing more. This allows for a chain agnostic approach to bootstrapping a chain.
    * 
    * Just like in the `eosio.system` sample contract implementation, there are a few actions which are not implemented at the contract level (`newaccount`, `updateauth`, `deleteauth`, `linkauth`, `unlinkauth`, `canceldelay`, `onerror`, `setabi`, `setcode`), they are just declared in the contract so they will show in the contract's ABI and users will be able to push those actions to the chain via the account holding the `eosio.system` contract, but the implementation is at the EOSIO core level. They are referred to as EOSIO native actions.
    */
   class contract : public eosio::contract {
      public:
         using eosio::contract::contract;
         /**
          * New account action, called after a new account is created. This code enforces resource-limits rules
          * for new accounts as well as new account naming conventions.
          *
          * 1. accounts cannot contain '.' symbols which forces all acccounts to be 12
          * characters long without '.' until a future account auction process is implemented
          * which prevents name squatting.
          *
          * 2. new accounts must stake a minimal number of tokens (as set in system parameters)
          * therefore, this method will execute an inline buyram from receiver for newacnt in
          * an amount equal to the current new account creation fee.
          */
         void newaccount( name             creator,
                          name             name,
                          ignore<authority> owner,
                          ignore<authority> active){}
         /**
          * Update authorization action updates pemission for an account.
          *
          * @param account - the account for which the permission is updated,
          * @param pemission - the permission name which is updated,
          * @param parem - the parent of the permission which is updated,
          * @param aut - the json describing the permission authorization.
          */
         void updateauth(  ignore<name>  account,
                           ignore<name>  permission,
                           ignore<name>  parent,
                           ignore<authority> auth ) {}

         /**
          * Delete authorization action deletes the authorization for an account's permission.
          *
          * @param account - the account for which the permission authorization is deleted,
          * @param permission - the permission name been deleted.
          */
         void deleteauth( ignore<name>  account,
                          ignore<name>  permission ) {}

         /**
          * Link authorization action assigns a specific action from a contract to a permission you have created. Five system
          * actions can not be linked `updateauth`, `deleteauth`, `linkauth`, `unlinkauth`, and `canceldelay`.
          * This is useful because when doing authorization checks, the EOSIO based blockchain starts with the
          * action needed to be authorized (and the contract belonging to), and looks up which permission
          * is needed to pass authorization validation. If a link is set, that permission is used for authoraization
          * validation otherwise then active is the default, with the exception of `eosio.any`.
          * `eosio.any` is an implicit permission which exists on every account; you can link actions to `eosio.any`
          * and that will make it so linked actions are accessible to any permissions defined for the account.
          *
          * @param account - the permission's owner to be linked and the payer of the RAM needed to store this link,
          * @param code - the owner of the action to be linked,
          * @param type - the action to be linked,
          * @param requirement - the permission to be linked.
          */
         void linkauth(  ignore<name>    account,
                         ignore<name>    code,
                         ignore<name>    type,
                         ignore<name>    requirement  ) {}

         /**
          * Unlink authorization action it's doing the reverse of linkauth action, by unlinking the given action.
          *
          * @param account - the owner of the permission to be unlinked and the receiver of the freed RAM,
          * @param code - the owner of the action to be unlinked,
          * @param type - the action to be unlinked.
          */
         void unlinkauth( ignore<name>  account,
                          ignore<name>  code,
                          ignore<name>  type ) {}

         /**
          * Cancel delay action cancels a deferred transaction.
          *
          * @param canceling_auth - the permission that authorizes this action,
          * @param trx_id - the deferred transaction id to be cancelled.
          */
         void canceldelay( ignore<permission_level> canceling_auth, ignore<checksum256> trx_id ) {}

         /**
          * Set code action sets the contract code for an account.
          *
          * @param account - the account for which to set the contract code.
          * @param vmtype - reserved, set it to zero.
          * @param vmversion - reserved, set it to zero.
          * @param code - the code content to be set, in the form of a blob binary..
          */
         void setcode( name account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code ) {}

         /**
          * Set abi action sets the abi for contract identified by `account` name. Creates an entry in the abi_hash_table
          * index, with `account` name as key, if it is not already present and sets its value with the abi hash.
          * Otherwise it is updating the current abi hash value for the existing `account` key.
          *
          * @param account - the name of the account to set the abi for
          * @param abi     - the abi hash represented as a vector of characters
          */
         void setabi( name account, const std::vector<char>& abi );

         /**
          * On error action, notification of this action is delivered to the sender of a deferred transaction
          * when an objective error occurs while executing the deferred transaction.
          * This action is not meant to be called directly.
          *
          * @param sender_id - the id for the deferred transaction chosen by the sender,
          * @param sent_trx - the deferred transaction that failed.
          */
         void onerror( ignore<uint128_t> sender_id, ignore<std::vector<char>> sent_trx );

         /**
          * Set privilege action allows to set privilege status for an account (turn it on/off).
          * @param account - the account to set the privileged status for.
          * @param is_priv - 0 for false, > 0 for true.
          */
         void setpriv( name account, uint8_t is_priv );

         /**
          * Sets the resource limits of an account
          *
          * @param account - name of the account whose resource limit to be set
          * @param ram_bytes - ram limit in absolute bytes
          * @param net_weight - fractionally proportionate net limit of available resources based on (weight / total_weight_of_all_accounts)
          * @param cpu_weight - fractionally proportionate cpu limit of available resources based on (weight / total_weight_of_all_accounts)
          */
         void setalimits( name account, int64_t ram_bytes, int64_t net_weight, int64_t cpu_weight );

         /**
          * Set producers action, sets a new list of active producers, by proposing a schedule change, once the block that
          * contains the proposal becomes irreversible, the schedule is promoted to "pending"
          * automatically. Once the block that promotes the schedule is irreversible, the schedule will
          * become "active".
          *
          * @param schedule - New list of active producers to set
          */
         void setprods( const std::vector<eosio::producer_authority>& schedule );

         /**
          * Set params action, sets the blockchain parameters. By tuning these parameters, various degrees of customization can be achieved.
          *
          * @param params - New blockchain parameters to set
          */
         void setparams( const eosio::blockchain_parameters& params );

         /**
          * Require authorization action, checks if the account name `from` passed in as param has authorization to access
          * current action, that is, if it is listed in the action’s allowed permissions vector.
          *
          * @param from - the account name to authorize
          */
         void reqauth( name from );

         /**
          * Activate action, activates a protocol feature
          *
          * @param feature_digest - hash of the protocol feature to activate.
          */
         void activate( const eosio::checksum256& feature_digest );

         /**
          * Require activated action, asserts that a protocol feature has been activated
          *
          * @param feature_digest - hash of the protocol feature to check for activation.
          */
         void reqactivated( const eosio::checksum256& feature_digest );
   }; // class contract

   EOSIO_ACTIONS(contract,
                 "eosio"_n,
                 action(newaccount, creator, name, owner, active, ricardian_contract(newaccount_ricardian)),
                 action(updateauth, account, permission, parent, auth, ricardian_contract(updateauth_ricardian)),
                 action(deleteauth, account, permission, ricardian_contract(deleteauth_ricardian)),
                 action(linkauth, account, code, type, requirement, ricardian_contract(linkauth_ricardian)),
                 action(unlinkauth, account, code, type, ricardian_contract(unlinkauth_ricardian)),
                 action(canceldelay, canceling_auth, trx_id, ricardian_contract(canceldelay_ricardian)),
                 action(setcode, account, vmtype, vmversion, code, ricardian_contract(setcode_ricardian)),
                 action(setabi, account, abi, ricardian_contract(setabi_ricardian)),
                 action(onerror, sender_id, sent_trx),
                 action(setpriv, account, is_priv, ricardian_contract(setpriv_ricardian)),
                 action(setalimits, account, ram_bytes, net_weight, cpu_weight, ricardian_contract(setalimits_ricardian)),
                 action(setprods, schedule, ricardian_contract(setprods_ricardian)),
                 action(setparams, params, ricardian_contract(setparams_ricardian)),
                 action(reqauth, from, ricardian_contract(reqauth_ricardian)),
                 action(activate, feature_digest, ricardian_contract(activate_ricardian)),
                 action(reqactivated, feature_digest, ricardian_contract(reqactivated_ricardian)))
} // namespace eosio_bios
