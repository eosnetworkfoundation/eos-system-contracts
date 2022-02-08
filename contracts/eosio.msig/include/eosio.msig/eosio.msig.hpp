#pragma once

#include <eosio/eosio.hpp>
#include <eosio/ignore.hpp>
#include <eosio/might_not_exist.hpp>
#include <eosio/transaction.hpp>
#include "eosio.msig.ricardian.hpp"

namespace eosio_msig {
   struct proposal {
      eosio::name       proposal_name;
      std::vector<char> packed_transaction;

      uint64_t primary_key() const { return proposal_name.value; }
   };
   EOSIO_REFLECT(proposal, proposal_name, packed_transaction)

   typedef eosio::multi_index<"proposal"_n, proposal> proposals;

   struct old_approvals_info {
      eosio::name                            proposal_name;
      std::vector<eosio::permission_level>   requested_approvals;
      std::vector<eosio::permission_level>   provided_approvals;

      uint64_t primary_key() const { return proposal_name.value; }
   };
   EOSIO_REFLECT(old_approvals_info, proposal_name, requested_approvals, provided_approvals)

   typedef eosio::multi_index<"approvals"_n, old_approvals_info> old_approvals;

   struct approval {
      eosio::permission_level level;
      eosio::time_point       time;
   };
   EOSIO_REFLECT(approval, level, time)

   struct approvals_info {
      uint8_t                 version = 1;
      eosio::name             proposal_name;
      //requested approval doesn't need to cointain time, but we want requested approval
      //to be of exact the same size ad provided approval, in this case approve/unapprove
      //doesn't change serialized data size. So, we use the same type.
      std::vector<approval>   requested_approvals;
      std::vector<approval>   provided_approvals;

      uint64_t primary_key() const { return proposal_name.value; }
   };
   EOSIO_REFLECT(approvals_info, version, proposal_name, requested_approvals, provided_approvals)

   typedef eosio::multi_index<"approvals2"_n, approvals_info> approvals;

   struct invalidation {
      eosio::name       account;
      eosio::time_point last_invalidation_time;

      uint64_t primary_key() const { return account.value; }
   };
   EOSIO_REFLECT(invalidation, account, last_invalidation_time)

   typedef eosio::multi_index<"invals"_n, invalidation> invalidations;

   /**
    * The `eosio.msig` system contract allows for creation of proposed transactions which require authorization from a list of accounts, approval of the proposed transactions by those accounts required to approve it, and finally, it also allows the execution of the approved transactions on the blockchain.
    *
    * In short, the workflow to propose, review, approve and then executed a transaction it can be described by the following:
    * - first you create a transaction json file,
    * - then you submit this proposal to the `eosio.msig` contract, and you also insert the account permissions required to approve this proposal into the command that submits the proposal to the blockchain,
    * - the proposal then gets stored on the blockchain by the `eosio.msig` contract, and is accessible for review and approval to those accounts required to approve it,
    * - after each of the appointed accounts required to approve the proposed transactions reviews and approves it, you can execute the proposed transaction. The `eosio.msig` contract will execute it automatically, but not before validating that the transaction has not expired, it is not cancelled, and it has been signed by all the permissions in the initial proposal's required permission list.
    */
   class contract : public eosio::contract {
      public:
         using eosio::contract::contract;

         /**
          * Propose action, creates a proposal containing one transaction.
          * Allows an account `proposer` to make a proposal `proposal_name` which has `requested`
          * permission levels expected to approve the proposal, and if approved by all expected
          * permission levels then `trx` transaction can we executed by this proposal.
          * The `proposer` account is authorized and the `trx` transaction is verified if it was
          * authorized by the provided keys and permissions, and if the proposal name doesn’t
          * already exist; if all validations pass the `proposal_name` and `trx` trasanction are
          * saved in the proposals table and the `requested` permission levels to the
          * approvals table (for the `proposer` context). Storage changes are billed to `proposer`.
          *
          * @param proposer - The account proposing a transaction
          * @param proposal_name - The name of the proposal (should be unique for proposer)
          * @param requested - Permission levels expected to approve the proposal
          * @param trx - Proposed transaction
          */
         void propose(eosio::ignore<eosio::name> proposer, eosio::ignore<eosio::name> proposal_name,
                      eosio::ignore<std::vector<eosio::permission_level>> requested, eosio::ignore<eosio::transaction> trx);

         /**
          * Approve action approves an existing proposal. Allows an account, the owner of `level` permission, to approve a proposal `proposal_name`
          * proposed by `proposer`. If the proposal's requested approval list contains the `level`
          * permission then the `level` permission is moved from internal `requested_approvals` list to
          * internal `provided_approvals` list of the proposal, thus persisting the approval for
          * the `proposal_name` proposal. Storage changes are billed to `proposer`.
          *
          * @param proposer - The account proposing a transaction
          * @param proposal_name - The name of the proposal (should be unique for proposer)
          * @param level - Permission level approving the transaction
          * @param proposal_hash - Transaction's checksum
          */
         void approve(eosio::name proposer, eosio::name proposal_name, eosio::permission_level level,
                      const eosio::might_not_exist<eosio::checksum256>& proposal_hash);

         /**
          * Unapprove action revokes an existing proposal. This action is the reverse of the `approve` action: if all validations pass
          * the `level` permission is erased from internal `provided_approvals` and added to the internal
          * `requested_approvals` list, and thus un-approve or revoke the proposal.
          *
          * @param proposer - The account proposing a transaction
          * @param proposal_name - The name of the proposal (should be an existing proposal)
          * @param level - Permission level revoking approval for proposal
          */
         void unapprove(eosio::name proposer, eosio::name proposal_name, eosio::permission_level level);

         /**
          * Cancel action cancels an existing proposal.
          *
          * @param proposer - The account proposing a transaction
          * @param proposal_name - The name of the proposal (should be an existing proposal)
          * @param canceler - The account cancelling the proposal (only the proposer can cancel an unexpired transaction, and the canceler has to be different than the proposer)
          *
          * Allows the `canceler` account to cancel the `proposal_name` proposal, created by a `proposer`,
          * only after time has expired on the proposed transaction. It removes corresponding entries from
          * internal proptable and from approval (or old approvals) tables as well.
          */
         void cancel(eosio::name proposer, eosio::name proposal_name, eosio::name canceler);

         /**
          * Exec action allows an `executer` account to execute a proposal.
          *
          * Preconditions:
          * - `executer` has authorization,
          * - `proposal_name` is found in the proposals table,
          * - all requested approvals are received,
          * - proposed transaction is not expired,
          * - and approval accounts are not found in invalidations table.
          *
          * If all preconditions are met the transaction is executed as a deferred transaction,
          * and the proposal is erased from the proposals table.
          *
          * @param proposer - The account proposing a transaction
          * @param proposal_name - The name of the proposal (should be an existing proposal)
          * @param executer - The account executing the transaction
          */
         void exec(eosio::name proposer, eosio::name proposal_name, eosio::name executer);

         /**
          * Invalidate action allows an `account` to invalidate itself, that is, its name is added to
          * the invalidations table and this table will be cross referenced when exec is performed.
          *
          * @param account - The account invalidating the transaction
          */
         void invalidate(eosio::name account);
   }; // contract

   EOSIO_ACTIONS(contract,
                 "eosio.msig"_n,
                 action(propose, proposer, proposal_name, requested, trx, ricardian_contract(propose_ricardian)),
                 action(approve, proposer, proposal_name, level, proposal_hash, ricardian_contract(approve_ricardian)),
                 action(unapprove, proposer, proposal_name, level, ricardian_contract(unapprove_ricardian)),
                 action(cancel, proposer, proposal_name, canceler, ricardian_contract(cancel_ricardian)),
                 action(exec, proposer, proposal_name, executer, ricardian_contract(exec_ricardian)),
                 action(invalidate, account, ricardian_contract(invalidate_ricardian)))
} // namespace eosio_msig
