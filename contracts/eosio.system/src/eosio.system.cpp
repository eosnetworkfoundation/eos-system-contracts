#include <eosio.system/eosio.system.hpp>
#include <eosio.system/canon_name.hpp>
#include <eosio.token/eosio.token.hpp>

#include <eosio/crypto.hpp>
#include <eosio/dispatcher.hpp>

#include <cmath>

namespace eosiosystem {

   using eosio::current_time_point;
   using eosio::token;

   double get_continuous_rate(int64_t annual_rate) {
      return std::log1p(double(annual_rate)/double(100*inflation_precision));
   }

   system_contract::system_contract( name s, name code, datastream<const char*> ds )
   :native(s,code,ds),
    _voters(get_self(), get_self().value),
    _producers(get_self(), get_self().value),
    _producers2(get_self(), get_self().value),
    _finalizer_keys(get_self(), get_self().value),
    _finalizers(get_self(), get_self().value),
    _last_prop_finalizers(get_self(), get_self().value),
    _fin_key_id_generator(get_self(), get_self().value),
    _global(get_self(), get_self().value),
    _global2(get_self(), get_self().value),
    _global3(get_self(), get_self().value),
    _global4(get_self(), get_self().value),
    _schedules(get_self(), get_self().value),
    _rammarket(get_self(), get_self().value),
    _rexpool(get_self(), get_self().value),
    _rexretpool(get_self(), get_self().value),
    _rexretbuckets(get_self(), get_self().value),
    _rexfunds(get_self(), get_self().value),
    _rexbalance(get_self(), get_self().value),
    _rexorders(get_self(), get_self().value),
    _rexmaturity(get_self(), get_self().value)
   {
      _gstate  = _global.exists() ? _global.get() : get_default_parameters();
      _gstate2 = _global2.exists() ? _global2.get() : eosio_global_state2{};
      _gstate3 = _global3.exists() ? _global3.get() : eosio_global_state3{};
      _gstate4 = _global4.exists() ? _global4.get() : get_default_inflation_parameters();
   }

   eosio_global_state system_contract::get_default_parameters() {
      eosio_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   eosio_global_state4 system_contract::get_default_inflation_parameters() {
      eosio_global_state4 gs4;
      gs4.continuous_rate      = get_continuous_rate(default_annual_rate);
      gs4.inflation_pay_factor = default_inflation_pay_factor;
      gs4.votepay_factor       = default_votepay_factor;
      return gs4;
   }

   symbol system_contract::core_symbol()const {
      const static auto sym = get_core_symbol();
      return sym;
   }

   system_contract::~system_contract() {
      _global.set( _gstate, get_self() );
      _global2.set( _gstate2, get_self() );
      _global3.set( _gstate3, get_self() );
      _global4.set( _gstate4, get_self() );
   }

   void system_contract::setram( uint64_t max_ram_size ) {
      require_auth( get_self() );

      check( _gstate.max_ram_size < max_ram_size, "ram may only be increased" ); /// decreasing ram might result market maker issues
      check( max_ram_size < 1024ll*1024*1024*1024*1024, "ram size is unrealistic" );
      check( max_ram_size > _gstate.total_ram_bytes_reserved, "attempt to set max below reserved" );

      auto delta = int64_t(max_ram_size) - int64_t(_gstate.max_ram_size);
      auto itr = _rammarket.find(ramcore_symbol.raw());

      /**
       *  Increase the amount of ram for sale based upon the change in max ram size.
       */
      _rammarket.modify( itr, same_payer, [&]( auto& m ) {
         m.base.balance.amount += delta;
      });

      _gstate.max_ram_size = max_ram_size;
   }

   void system_contract::update_ram_supply() {
      auto cbt = eosio::current_block_time();

      if( cbt <= _gstate2.last_ram_increase ) return;

      if (_gstate2.new_ram_per_block != 0) {
         auto itr     = _rammarket.find(ramcore_symbol.raw());
         auto new_ram = (cbt.slot - _gstate2.last_ram_increase.slot) * _gstate2.new_ram_per_block;
         _gstate.max_ram_size += new_ram;

         /**
          *  Increase the amount of ram for sale based upon the change in max ram size.
          */
         _rammarket.modify(itr, same_payer, [&](auto& m) { m.base.balance.amount += new_ram; });
      }

      _gstate2.last_ram_increase = cbt;
   }

   void system_contract::setramrate( uint16_t bytes_per_block ) {
      require_auth( get_self() );

      update_ram_supply(); // make sure all previous blocks are accounted at the old rate before updating the rate
      _gstate2.new_ram_per_block = bytes_per_block;
   }

   void system_contract::channel_to_system_fees( const name& from, const asset& amount ) {
      token::transfer_action transfer_act{ token_account, { from, active_permission } };
      transfer_act.send( from, fees_account, amount, "transfer from " + from.to_string() + " to " + fees_account.to_string() );
   }

#ifdef SYSTEM_BLOCKCHAIN_PARAMETERS
   extern "C" [[eosio::wasm_import]] void set_parameters_packed(const void*, size_t);
#endif

   void system_contract::setparams( const blockchain_parameters_t& params ) {
      require_auth( get_self() );
      (eosio::blockchain_parameters&)(_gstate) = params;
      check( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
#ifndef SYSTEM_BLOCKCHAIN_PARAMETERS
      set_blockchain_parameters( params );
#else
      constexpr size_t param_count = 18;
      // an upper bound on the serialized size
      char buf[1 + sizeof(params) + param_count];
      datastream<char*> stream(buf, sizeof(buf));

      stream << uint8_t(17);
      stream << uint8_t(0) << params.max_block_net_usage
             << uint8_t(1) << params.target_block_net_usage_pct
             << uint8_t(2) << params.max_transaction_net_usage
             << uint8_t(3) << params.base_per_transaction_net_usage
             << uint8_t(4) << params.net_usage_leeway
             << uint8_t(5) << params.context_free_discount_net_usage_num
             << uint8_t(6) << params.context_free_discount_net_usage_den

             << uint8_t(7) << params.max_block_cpu_usage
             << uint8_t(8) << params.target_block_cpu_usage_pct
             << uint8_t(9) << params.max_transaction_cpu_usage
             << uint8_t(10) << params.min_transaction_cpu_usage

             << uint8_t(11) << params.max_transaction_lifetime
             << uint8_t(12) << params.deferred_trx_expiration_window
             << uint8_t(13) << params.max_transaction_delay
             << uint8_t(14) << params.max_inline_action_size
             << uint8_t(15) << params.max_inline_action_depth
             << uint8_t(16) << params.max_authority_depth;
      if(params.max_action_return_value_size)
      {
         stream << uint8_t(17) << params.max_action_return_value_size.value();
         ++buf[0];
      }

      set_parameters_packed(buf, stream.tellp());
#endif
   }

#ifdef SYSTEM_CONFIGURABLE_WASM_LIMITS

   // The limits on contract WebAssembly modules
   struct wasm_parameters
   {
      uint32_t max_mutable_global_bytes;
      uint32_t max_table_elements;
      uint32_t max_section_elements;
      uint32_t max_linear_memory_init;
      uint32_t max_func_local_bytes;
      uint32_t max_nested_structures;
      uint32_t max_symbol_bytes;
      uint32_t max_module_bytes;
      uint32_t max_code_bytes;
      uint32_t max_pages;
      uint32_t max_call_depth;
   };

   static constexpr wasm_parameters default_limits = {
       .max_mutable_global_bytes = 1024,
       .max_table_elements = 1024,
       .max_section_elements = 8192,
       .max_linear_memory_init = 64*1024,
       .max_func_local_bytes = 8192,
       .max_nested_structures = 1024,
       .max_symbol_bytes = 8192,
       .max_module_bytes = 20*1024*1024,
       .max_code_bytes = 20*1024*1024,
       .max_pages = 528,
       .max_call_depth = 251
   };

   static constexpr wasm_parameters high_limits = {
       .max_mutable_global_bytes = 8192,
       .max_table_elements = 8192,
       .max_section_elements = 8192,
       .max_linear_memory_init = 16*64*1024,
       .max_func_local_bytes = 8192,
       .max_nested_structures = 1024,
       .max_symbol_bytes = 8192,
       .max_module_bytes = 20*1024*1024,
       .max_code_bytes = 20*1024*1024,
       .max_pages = 528,
       .max_call_depth = 1024
   };

   extern "C" [[eosio::wasm_import]] void set_wasm_parameters_packed( const void*, size_t );

   void set_wasm_parameters( const wasm_parameters& params )
   {
      char buf[sizeof(uint32_t) + sizeof(params)] = {};
      memcpy(buf + sizeof(uint32_t), &params, sizeof(params));
      set_wasm_parameters_packed( buf, sizeof(buf) );
   }

   void system_contract::wasmcfg( const name& settings )
   {
      require_auth( get_self() );
      if( settings == "default"_n || settings == "low"_n )
      {
         set_wasm_parameters( default_limits );
      }
      else if( settings == "high"_n )
      {
         set_wasm_parameters( high_limits );
      }
      else
      {
         check(false, "Unknown configuration");
      }
   }

#endif

   void system_contract::setpriv( const name& account, uint8_t ispriv ) {
      require_auth( get_self() );
      set_privileged( account, ispriv );
   }

   void system_contract::setalimits( const name& account, int64_t ram, int64_t net, int64_t cpu ) {
      require_auth( get_self() );

      user_resources_table userres( get_self(), account.value );
      auto ritr = userres.find( account.value );
      check( ritr == userres.end(), "only supports unlimited accounts" );

      auto vitr = _voters.find( account.value );
      if( vitr != _voters.end() ) {
         bool ram_managed = has_field( vitr->flags1, voter_info::flags1_fields::ram_managed );
         bool net_managed = has_field( vitr->flags1, voter_info::flags1_fields::net_managed );
         bool cpu_managed = has_field( vitr->flags1, voter_info::flags1_fields::cpu_managed );
         check( !(ram_managed || net_managed || cpu_managed), "cannot use setalimits on an account with managed resources" );
      }

      set_resource_limits( account, ram, net, cpu );
   }

   void system_contract::setacctram( const name& account, const std::optional<int64_t>& ram_bytes ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t ram = 0;

      if( !ram_bytes ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::ram_managed ),
                "RAM of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         ram = ram_gift_bytes;
         if( ritr != userres.end() ) {
            ram += ritr->ram_bytes;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, false );
         });
      } else {
         check( *ram_bytes >= 0, "not allowed to set RAM limit to unlimited" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::ram_managed, true );
            });
         }

         ram = *ram_bytes;
      }

      set_resource_limits( account, ram, current_net, current_cpu );
   }

   void system_contract::setacctnet( const name& account, const std::optional<int64_t>& net_weight ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t net = 0;

      if( !net_weight ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::net_managed ),
                "Network bandwidth of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         if( ritr != userres.end() ) {
            net = ritr->net_weight.amount;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::net_managed, false );
         });
      } else {
         check( *net_weight >= -1, "invalid value for net_weight" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::net_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::net_managed, true );
            });
         }

         net = *net_weight;
      }

      set_resource_limits( account, current_ram, net, current_cpu );
   }

   void system_contract::setacctcpu( const name& account, const std::optional<int64_t>& cpu_weight ) {
      require_auth( get_self() );

      int64_t current_ram, current_net, current_cpu;
      get_resource_limits( account, current_ram, current_net, current_cpu );

      int64_t cpu = 0;

      if( !cpu_weight ) {
         auto vitr = _voters.find( account.value );
         check( vitr != _voters.end() && has_field( vitr->flags1, voter_info::flags1_fields::cpu_managed ),
                "CPU bandwidth of account is already unmanaged" );

         user_resources_table userres( get_self(), account.value );
         auto ritr = userres.find( account.value );

         if( ritr != userres.end() ) {
            cpu = ritr->cpu_weight.amount;
         }

         _voters.modify( vitr, same_payer, [&]( auto& v ) {
            v.flags1 = set_field( v.flags1, voter_info::flags1_fields::cpu_managed, false );
         });
      } else {
         check( *cpu_weight >= -1, "invalid value for cpu_weight" );

         auto vitr = _voters.find( account.value );
         if ( vitr != _voters.end() ) {
            _voters.modify( vitr, same_payer, [&]( auto& v ) {
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::cpu_managed, true );
            });
         } else {
            _voters.emplace( account, [&]( auto& v ) {
               v.owner  = account;
               v.flags1 = set_field( v.flags1, voter_info::flags1_fields::cpu_managed, true );
            });
         }

         cpu = *cpu_weight;
      }

      set_resource_limits( account, current_ram, current_net, cpu );
   }

   checksum256 system_contract::denyhashcalc( const std::vector<name>& patterns ) {
      check(!patterns.empty(), "Cannot compute hash on empty vector");
      for (auto n : patterns) {
         // check that pattern is valid (pattern should not be empty or more than 12 character long)
         canon_name_t pattern(n);
         check(pattern.valid(),
               n.value ? ("Pattern " + n.to_string() + " is not valid") : "Empty patterns are not allowed");
      }
      static_assert(sizeof(name) == sizeof(uint64_t));
      return eosio::sha256(reinterpret_cast<const char*>(patterns.data()), patterns.size() * sizeof(name));
   }

   void system_contract::denyhashadd( const checksum256& hash ) {
      require_auth( get_self() );

      deny_hash_table dh_table(get_self(), get_self().value);
      const auto idx = dh_table.get_index<"byhash"_n>();
      auto itr = idx.find(hash);
      check(itr == idx.end(), "Trying to add a deny hash which is already present");
      dh_table.emplace(get_self(), [&](auto& row) { row.hash = hash; });
   }

   void system_contract::denyhashrm( const checksum256& hash ) {
      require_auth( get_self() );

      deny_hash_table dh_table(get_self(), get_self().value);
      auto idx = dh_table.get_index<"byhash"_n>();
      auto itr = idx.find(hash);
      check(itr != idx.end(), "Trying to remove a deny hash which is not present");
      idx.erase(itr);
   }

   void system_contract::denynames( const std::vector<name>& patterns ) {
      // no auth necessary since the hash verification is enough
      
      if (patterns.empty())
         return; // no-op for empty list, consistent with ignoring duplicate names.

      check(patterns.size() <= 512, "Cannot provide more than 512 patterns in one action call");

      deny_hash_table dh_table(get_self(), get_self().value);
      auto dh_idx = dh_table.get_index<"byhash"_n>();
      auto dh_itr = dh_idx.find(denyhashcalc(patterns));
      check(dh_itr != dh_idx.end(), "Verification hash not found in denyhash table");

      auto add_patterns_to = [&patterns](auto& current) {
         // number of blackcklist name patterns expected to be small, so quadratic check OK
         for (auto n : patterns) {
            // check that pattern is valid (pattern should not be empty or more than 12 character long)
            canon_name_t pattern(n);
            check(pattern.valid(),
                  n.value ? ("Pattern " + n.to_string() + " is not valid") : "Empty patterns are not allowed");

            if (std::find(std::cbegin(current), std::cend(current), n) == std::cend(current))
               current.push_back(n);
         }
      };

      account_name_blacklist_table bl_table(get_self(), get_self().value);
      
      if (auto bl_itr = bl_table.begin(); bl_itr != bl_table.end()) {
         bl_table.modify(bl_itr, same_payer, [&](auto& blacklist) {
            add_patterns_to(blacklist.disallowed);
         });
      } else {
         bl_table.emplace(get_self(), [&](auto& blacklist) {
            add_patterns_to(blacklist.disallowed);
         });
      }

      dh_idx.erase(dh_itr); // names patterns have been added - remove hash
   }

   void system_contract::undenynames( const std::vector<name>& patterns ) {
      require_auth( get_self() );

      if (patterns.empty())
         return; // no-op for empty list, consistent with ignoring duplicate names.
      
      check(patterns.size() <= 512, "Cannot provide more than 512 patterns in one action call");

      account_name_blacklist_table bl_table(get_self(), get_self().value);

      if (auto itr = bl_table.begin(); itr != bl_table.end()) {
         bl_table.modify(itr, same_payer, [&](auto& blacklist) {
            auto& current = blacklist.disallowed;

            // number of blackcklist name patterns expected to be small, so quadratic check OK
            for (auto n : patterns) {
               if (auto itr = std::find(std::begin(current), std::end(current), n); itr != std::end(current))
                  current.erase(itr);
            }
         });
      }
      // no-op for empty blacklist table, consistent with ignoring names not in the list
   }

   void system_contract::activate( const eosio::checksum256& feature_digest ) {
      require_auth( get_self() );
      preactivate_feature( feature_digest );
   }

   void system_contract::logsystemfee( const name& protocol, const asset& fee, const std::string& memo ) {
      require_auth( get_self() );
   }

   void system_contract::rmvproducer( const name& producer ) {
      require_auth( get_self() );
      auto prod = _producers.find( producer.value );
      check( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, same_payer, [&](auto& p) {
            p.deactivate();
         });
   }

   void system_contract::updtrevision( uint8_t revision ) {
      require_auth( get_self() );
      check( _gstate2.revision < 255, "can not increment revision" ); // prevent wrap around
      check( revision == _gstate2.revision + 1, "can only increment revision by one" );
      check( revision <= 1, // set upper bound to greatest revision supported in the code
             "specified revision is not yet supported by the code" );
      _gstate2.revision = revision;
   }

   void system_contract::setinflation( int64_t annual_rate, int64_t inflation_pay_factor, int64_t votepay_factor ) {
      require_auth(get_self());
      check(annual_rate >= 0, "annual_rate can't be negative");
      if ( inflation_pay_factor < pay_factor_precision ) {
         check( false, "inflation_pay_factor must not be less than " + std::to_string(pay_factor_precision) );
      }
      if ( votepay_factor < pay_factor_precision ) {
         check( false, "votepay_factor must not be less than " + std::to_string(pay_factor_precision) );
      }
      _gstate4.continuous_rate      = get_continuous_rate(annual_rate);
      _gstate4.inflation_pay_factor = inflation_pay_factor;
      _gstate4.votepay_factor       = votepay_factor;
      _global4.set( _gstate4, get_self() );
   }

   void system_contract::setpayfactor( int64_t inflation_pay_factor, int64_t votepay_factor ) {
      require_auth(get_self());
      if ( inflation_pay_factor < pay_factor_precision ) {
         check( false, "inflation_pay_factor must not be less than " + std::to_string(pay_factor_precision) );
      }
      if ( votepay_factor < pay_factor_precision ) {
         check( false, "votepay_factor must not be less than " + std::to_string(pay_factor_precision) );
      }
      _gstate4.inflation_pay_factor = inflation_pay_factor;
      _gstate4.votepay_factor       = votepay_factor;
      _global4.set( _gstate4, get_self() );
   }

   void system_contract::setschedule( const time_point_sec start_time, double continuous_rate )
   {
      require_auth( get_self() );

      check(continuous_rate >= 0, "continuous_rate can't be negative");
      check(continuous_rate <= 1, "continuous_rate can't be over 100%");

      auto itr = _schedules.find( start_time.sec_since_epoch() );

      if( itr == _schedules.end() ) {
         _schedules.emplace( get_self(), [&]( auto& s ) {
            s.start_time = start_time;
            s.continuous_rate = continuous_rate;
         });
      } else {
         _schedules.modify( itr, same_payer, [&]( auto& s ) {
            s.continuous_rate = continuous_rate;
         });
      }
   }

   void system_contract::delschedule( const time_point_sec start_time )
   {
      require_auth( get_self() );

      auto itr = _schedules.require_find( start_time.sec_since_epoch(), "schedule not found" );
      _schedules.erase( itr );
   }

   void system_contract::execschedule()
   {
      check(execute_next_schedule(), "no schedule to execute");
   }

   bool system_contract::execute_next_schedule()
   {
      auto itr = _schedules.begin();
      if (itr == _schedules.end()) return false; // no schedules to execute

      if ( current_time_point().sec_since_epoch() >= itr->start_time.sec_since_epoch() ) {
         _gstate4.continuous_rate = itr->continuous_rate;
         _global4.set( _gstate4, get_self() );
         _schedules.erase( itr );
         return true;
      }
      return false;
   }

   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void native::newaccount( const name&       creator,
                            const name&       new_account_name,
                            ignore<authority> owner,
                            ignore<authority> active ) {

      if( creator != get_self() ) {
         uint64_t tmp = new_account_name.value >> 4;
         bool has_dot = false;

         for( uint32_t i = 0; i < 12; ++i ) {
           has_dot |= !(tmp & 0x1f);
           tmp >>= 5;
         }
         if( has_dot ) { // or is less than 12 characters
            auto suffix = new_account_name.suffix();
            if( suffix == new_account_name ) {
               name_bid_table bids(get_self(), get_self().value);
               auto current = bids.find( new_account_name.value );
               check( current != bids.end(), "no active bid for name: " + new_account_name.to_string());
               check( current->high_bidder == creator, "only highest bidder can claim" );
               check( current->high_bid < 0, "auction for name is not closed yet" );
               bids.erase( current );
            } else {
               check( creator == suffix, "only " + suffix.to_string() + " may create " + new_account_name.to_string());
            }
         }
    
         // check that the account does not match a blacklist pattern stored in `account_name_blacklist_table`
         // -------------------------------------------------------------------------------------------------------
         account_name_blacklist_table bl_table(get_self(), get_self().value);
         auto itr = bl_table.begin();
         bool present = (itr != bl_table.end());

         if (present) {
            const std::vector<name>& blacklist{itr->disallowed};
            for (auto pattern : blacklist) {
               check(name_allowed(new_account_name, pattern),
                     "Account name " + new_account_name.to_string() + " creation disallowed by rule: " + pattern.to_string());
            }
         }
      }

      user_resources_table  userres( get_self(), new_account_name.value );

      userres.emplace( new_account_name, [&]( auto& res ) {
        res.owner = new_account_name;
        res.net_weight = asset( 0, system_contract::get_core_symbol() );
        res.cpu_weight = asset( 0, system_contract::get_core_symbol() );
      });

      set_resource_limits( new_account_name, 0, 0, 0 );
   }

   void native::setabi( const name& acnt, const std::vector<char>& abi,
                        const binary_extension<std::string>& memo ) {
      eosio::multi_index< "abihash"_n, abi_hash >  table(get_self(), get_self().value);
      auto itr = table.find( acnt.value );
      if( itr == table.end() ) {
         table.emplace( acnt, [&]( auto& row ) {
            row.owner = acnt;
            row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
         });
      } else {
         table.modify( itr, same_payer, [&]( auto& row ) {
            row.hash = eosio::sha256(const_cast<char*>(abi.data()), abi.size());
         });
      }
   }

   void system_contract::init( unsigned_int version, const symbol& core ) {
      require_auth( get_self() );
      check( version.value == 0, "unsupported version for init action" );

      auto itr = _rammarket.find(ramcore_symbol.raw());
      check( itr == _rammarket.end(), "system contract has already been initialized" );

      auto system_token_supply   = eosio::token::get_supply(token_account, core.code() );
      check( system_token_supply.symbol == core, "specified core symbol does not exist (precision mismatch)" );

      check( system_token_supply.amount > 0, "system token supply must be greater than 0" );
      _rammarket.emplace( get_self(), [&]( auto& m ) {
         m.supply.amount = 100000000000000ll;
         m.supply.symbol = ramcore_symbol;
         m.base.balance.amount = int64_t(_gstate.free_ram());
         m.base.balance.symbol = ram_symbol;
         m.quote.balance.amount = system_token_supply.amount / 1000;
         m.quote.balance.symbol = core;
      });

      token::open_action open_act{ token_account, { {get_self(), active_permission} } };
      open_act.send( rex_account, core, get_self() );
   }

} /// eosio.system
