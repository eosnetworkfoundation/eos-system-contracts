#include <eosio.bios/eosio.bios.hpp>
#include <eosio/crypto_bls_ext.hpp>

namespace eosiobios {

void bios::setabi( name account, const std::vector<char>& abi ) {
   abi_hash_table table(get_self(), get_self().value);
   auto itr = table.find( account.value );
   if( itr == table.end() ) {
      table.emplace( account, [&]( auto& row ) {
         row.owner = account;
         row.hash  = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
      });
   } else {
      table.modify( itr, eosio::same_payer, [&]( auto& row ) {
         row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
      });
   }
}

void bios::setfinalizer( const finalizer_policy& finalizer_policy ) {
   require_auth( get_self() );
   check(finalizer_policy.finalizers.size() > 0, "require at least one finalizer");

   eosio::abi_finalizer_policy abi_finalizer_policy;
   abi_finalizer_policy.fthreshold = finalizer_policy.threshold;
   abi_finalizer_policy.finalizers.reserve(finalizer_policy.finalizers.size());

   const std::string pk_prefix = "PUB_BLS";
   const std::string sig_prefix = "SIG_BLS";

   for (const auto& f: finalizer_policy.finalizers) {
      // basic key format checks
      check(f.public_key.substr(0, pk_prefix.length()) == pk_prefix, "public key not started with PUB_BLS");
      check(f.pop.substr(0, sig_prefix.length()) == sig_prefix, "proof of possession signature not started with SIG_BLS");

      // proof of possession of private key check
      const auto pk = eosio::decode_bls_public_key_to_g1(f.public_key);
      const auto signature = eosio::decode_bls_signature_to_g2(f.pop);
      check(eosio::bls_pop_verify(pk, signature), "proof of possession failed");

      std::vector<char> pk_vector(pk.begin(), pk.end());
      abi_finalizer_policy.finalizers.emplace_back(eosio::abi_finalizer_authority{f.description, f.weight, std::move(pk_vector)});
   }

   set_finalizers(std::move(abi_finalizer_policy));
}

void bios::onerror( ignore<uint128_t>, ignore<std::vector<char>> ) {
   check( false, "the onerror action cannot be called directly" );
}

void bios::setpriv( name account, uint8_t is_priv ) {
   require_auth( get_self() );
   set_privileged( account, is_priv );
}

void bios::setalimits( name account, int64_t ram_bytes, int64_t net_weight, int64_t cpu_weight ) {
   require_auth( get_self() );
   set_resource_limits( account, ram_bytes, net_weight, cpu_weight );
}

void bios::setprods( const std::vector<eosio::producer_authority>& schedule ) {
   require_auth( get_self() );
   set_proposed_producers( schedule );
}

void bios::setparams( const eosio::blockchain_parameters& params ) {
   require_auth( get_self() );
   set_blockchain_parameters( params );
}

void bios::reqauth( name from ) {
   require_auth( from );
}

void bios::activate( const eosio::checksum256& feature_digest ) {
   require_auth( get_self() );
   preactivate_feature( feature_digest );
}

void bios::reqactivated( const eosio::checksum256& feature_digest ) {
   check( is_feature_activated( feature_digest ), "protocol feature is not activated" );
}

}
