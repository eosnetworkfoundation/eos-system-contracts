#pragma once

#include <eosio/action.hpp>
#include <eosio/contract.hpp>
#include <eosio/crypto.hpp>
#include <eosio/fixed_bytes.hpp>
#include <eosio/ignore.hpp>
#include <eosio/might_not_exist.hpp>
#include <eosio/print.hpp>
#include <eosio/privileged.hpp>
#include <eosio/producer_schedule.hpp>

namespace eosio_system {

   using eosio::authority;
   using eosio::checksum256;
   using eosio::ignore;
   using eosio::might_not_exist;
   using eosio::name;
   using eosio::permission_level;
   using eosio::public_key;

   /**
    * Blockchain block header.
    *
    * A block header is defined by:
    * - a timestamp,
    * - the producer that created it,
    * - a confirmed flag default as zero,
    * - a link to previous block,
    * - a link to the transaction merkel root,
    * - a link to action root,
    * - a schedule version,
    * - and a producers' schedule.
    */
   struct block_header {
      uint32_t                                  timestamp;
      name                                      producer;
      uint16_t                                  confirmed = 0;
      checksum256                               previous;
      checksum256                               transaction_mroot;
      checksum256                               action_mroot;
      uint32_t                                  schedule_version = 0;
      std::optional<eosio::producer_schedule>   new_producers;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE(block_header, (timestamp)(producer)(confirmed)(previous)(transaction_mroot)(action_mroot)
                                     (schedule_version)(new_producers))
   };

   /**
    * abi_hash is the structure underlying the abihash table and consists of:
    * - `owner`: the account owner of the contract's abi
    * - `hash`: is the sha256 hash of the abi/binary
    */
   struct abi_hash {
      name              owner;
      checksum256       hash;
      uint64_t primary_key()const { return owner.value; }

      EOSLIB_SERIALIZE( abi_hash, (owner)(hash) )
   };

   void check_auth_change(name contract, name account, const might_not_exist<name>& authorized_by);

   // Method parameters commented out to prevent generation of code that parses input data.
   /**
    * The EOSIO core `native` contract that governs authorization and contracts' abi.
    */
   class native : public eosio::contract {
      public:

         using eosio::contract::contract;

         /**
          * These actions map one-on-one with the ones defined in core layer of EOSIO, that's where their implementation
          * actually is done.
          * They are present here only so they can show up in the abi file and thus user can send them
          * to this contract, but they have no specific implementation at this contract level,
          * they will execute the implementation at the core layer and nothing else.
          */
         /**
          * New account action is called after a new account is created. This code enforces resource-limits rules
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
         void newaccount( const name&       creator,
                          const name&       name,
                          ignore<authority> owner,
                          ignore<authority> active);

         /**
          * Update authorization action updates pemission for an account.
          *
          * This contract enforces additional rules:
          *
          * 1. If authorized_by is present and not "", then the contract does a
          *    require_auth2(account, authorized_by).
          * 2. If the account has opted into limitauthchg, then authorized_by
          *    must be present and not "".
          * 3. If the account has opted into limitauthchg, and allow_perms is
          *    not empty, then authorized_by must be in the array.
          * 4. If the account has opted into limitauthchg, and disallow_perms is
          *    not empty, then authorized_by must not be in the array.
          *
          * @param account - the account for which the permission is updated
          * @param permission - the permission name which is updated
          * @param parent - the parent of the permission which is updated
          * @param auth - the json describing the permission authorization
          * @param authorized_by - the permission which is authorizing this change
          */
         void updateauth( name                   account,
                          name                   permission,
                          name                   parent,
                          authority              auth,
                          might_not_exist<name>  authorized_by ) {
            check_auth_change(get_self(), account, authorized_by);
         }

         /**
          * Delete authorization action deletes the authorization for an account's permission.
          *
          * This contract enforces additional rules:
          *
          * 1. If authorized_by is present and not "", then the contract does a
          *    require_auth2(account, authorized_by).
          * 2. If the account has opted into limitauthchg, then authorized_by
          *    must be present and not "".
          * 3. If the account has opted into limitauthchg, and allow_perms is
          *    not empty, then authorized_by must be in the array.
          * 4. If the account has opted into limitauthchg, and disallow_perms is
          *    not empty, then authorized_by must not be in the array.
          *
          * @param account - the account for which the permission authorization is deleted,
          * @param permission - the permission name been deleted.
          * @param authorized_by - the permission which is authorizing this change
          */
         void deleteauth( name                   account,
                          name                   permission,
                          might_not_exist<name>  authorized_by ) {
            check_auth_change(get_self(), account, authorized_by);
         }

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
          * This contract enforces additional rules:
          *
          * 1. If authorized_by is present and not "", then the contract does a
          *    require_auth2(account, authorized_by).
          * 2. If the account has opted into limitauthchg, then authorized_by
          *    must be present and not "".
          * 3. If the account has opted into limitauthchg, and allow_perms is
          *    not empty, then authorized_by must be in the array.
          * 4. If the account has opted into limitauthchg, and disallow_perms is
          *    not empty, then authorized_by must not be in the array.
          *
          * @param account - the permission's owner to be linked and the payer of the RAM needed to store this link,
          * @param code - the owner of the action to be linked,
          * @param type - the action to be linked,
          * @param requirement - the permission to be linked.
          * @param authorized_by - the permission which is authorizing this change
          */
         void linkauth( name                   account,
                        name                   code,
                        name                   type,
                        name                   requirement,
                        might_not_exist<name>  authorized_by ) {
            check_auth_change(get_self(), account, authorized_by);
         }

         /**
          * Unlink authorization action it's doing the reverse of linkauth action, by unlinking the given action.
          *
          * This contract enforces additional rules:
          *
          * 1. If authorized_by is present and not "", then the contract does a
          *    require_auth2(account, authorized_by).
          * 2. If the account has opted into limitauthchg, then authorized_by
          *    must be present and not "".
          * 3. If the account has opted into limitauthchg, and allow_perms is
          *    not empty, then authorized_by must be in the array.
          * 4. If the account has opted into limitauthchg, and disallow_perms is
          *    not empty, then authorized_by must not be in the array.
          *
          * @param account - the owner of the permission to be unlinked and the receiver of the freed RAM,
          * @param code - the owner of the action to be unlinked,
          * @param type - the action to be unlinked.
          * @param authorized_by - the permission which is authorizing this change
          */
         void unlinkauth( name                   account,
                          name                   code,
                          name                   type,
                          might_not_exist<name>  authorized_by ) {
            check_auth_change(get_self(), account, authorized_by);
         }

         /**
          * Cancel delay action cancels a deferred transaction.
          *
          * @param canceling_auth - the permission that authorizes this action,
          * @param trx_id - the deferred transaction id to be cancelled.
          */
         void canceldelay( ignore<permission_level> canceling_auth, ignore<checksum256> trx_id ) {}

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
          * Set abi action sets the contract abi for an account.
          *
          * @param account - the account for which to set the contract abi.
          * @param abi - the abi content to be set, in the form of a blob binary.
          * @param memo - may be omitted
          */
         void setabi( const name& account, const std::vector<char>& abi, const might_not_exist<std::string>& memo );

         /**
          * Set code action sets the contract code for an account.
          *
          * @param account - the account for which to set the contract code.
          * @param vmtype - reserved, set it to zero.
          * @param vmversion - reserved, set it to zero.
          * @param code - the code content to be set, in the form of a blob binary..
          * @param memo - may be omitted
          */
         void setcode( const name& account, uint8_t vmtype, uint8_t vmversion, const std::vector<char>& code,
                       const might_not_exist<std::string>& memo ) {}
   };
} // namespace eosio_system
