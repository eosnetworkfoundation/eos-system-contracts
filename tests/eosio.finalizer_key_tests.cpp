#include "eosio.system_tester.hpp"

#include <boost/test/unit_test.hpp>

using namespace eosio_system;

struct key_pair_t {
   std::string pub_key;
   std::string pop;
};

// Those are needed to unpack last_prop_finalizers_info
struct finalizer_authority_t {
   std::string           description;
   uint64_t              weight = 0;
   std::vector<char>     public_key;
};
FC_REFLECT(finalizer_authority_t, (description)(weight)(public_key))

struct finalizer_auth_info {
   uint64_t               key_id;
   finalizer_authority_t  fin_authority;
};
FC_REFLECT(finalizer_auth_info, (key_id)(fin_authority))

struct last_prop_finalizers_info {
   std::vector<finalizer_auth_info> last_proposed_finalizers;
};
FC_REFLECT(last_prop_finalizers_info, (last_proposed_finalizers))

struct finalizer_key_tester : eosio_system_tester {
   static const std::vector<key_pair_t> key_pair;

   fc::variant get_finalizer_key_info( uint64_t id ) {
      vector<char> data = get_row_by_id( config::system_account_name, config::system_account_name, "finkeys"_n, id );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "finalizer_key_info", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_finalizer_info( const account_name& act ) {
      vector<char> data = get_row_by_account( config::system_account_name, config::system_account_name, "finalizers"_n, act );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "finalizer_info", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   std::vector<finalizer_auth_info> get_last_prop_finalizers_info() {
      const auto* table_id_itr = control->db().find<eosio::chain::table_id_object, eosio::chain::by_code_scope_table>(
         boost::make_tuple(config::system_account_name, config::system_account_name, "lastpropfins"_n));

      if (!table_id_itr) {
         return {};
      }

      auto t_id = table_id_itr->id;
      const auto& idx = control->db().get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();

      if( idx.begin() == idx.end() ) {
         return {};
      }

      vector<char> data;
      auto itr = idx.lower_bound( boost::make_tuple( t_id, 0 ) );
      data.resize( itr->value.size() );
      memcpy( data.data(), itr->value.data(), data.size() );
      fc::variant fins_info = data.empty() ? fc::variant() : abi_ser.binary_to_variant( "last_prop_finalizers_info", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
      std::vector<finalizer_auth_info> finalizers = fins_info["last_proposed_finalizers"].as<std::vector<finalizer_auth_info>>();

      return finalizers;
   };

   std::unordered_set<uint64_t> get_last_prop_fin_ids() {
      auto finalizers = get_last_prop_finalizers_info();

      std::unordered_set<uint64_t> last_prop_fin_ids;
      for(auto f: finalizers) {
         last_prop_fin_ids.insert(f.key_id);
      }

      return last_prop_fin_ids;
   };

   action_result register_finalizer_key( const account_name& act, const std::string& finalizer_key, const std::string& pop  ) {
      return push_action( act, "regfinkey"_n, mvo()
                          ("finalizer_name", act)
                          ("finalizer_key", finalizer_key)
                          ("proof_of_possession", pop) );
   }

   action_result activate_finalizer_key( const account_name& act, const std::string& finalizer_key ) {
      return push_action( act, "actfinkey"_n, mvo()
                          ("finalizer_name",  act)
                          ("finalizer_key", finalizer_key) );
   }

   action_result delete_finalizer_key( const account_name& act, const std::string& finalizer_key ) {
      return push_action( act, "delfinkey"_n, mvo()
                          ("finalizer_name",  act)
                          ("finalizer_key", finalizer_key) );
   }

   void register_finalizer_keys(const std::vector<name>& producer_names, uint32_t num_keys_to_register) {
      uint32_t i = 0;
      for (const auto& p: producer_names) {
         BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(p, key_pair[i].pub_key, key_pair[i].pop));
         ++i;

         if ( i  == num_keys_to_register ) {
            break;
         }
      }
   }

   // Verify finalizers_table and last_prop_fins_table match
   void verify_last_proposed_finalizers(const std::vector<name>& producer_names) {
      auto last_finalizers = get_last_prop_finalizers_info();
      BOOST_REQUIRE_EQUAL( 21, last_finalizers.size() );
      for( auto& p : producer_names ) {
         auto finalizer_info = get_finalizer_info(p);
         uint64_t active_key_id = finalizer_info["active_key_id"].as_uint64();
         auto itr = std::find_if(last_finalizers.begin(), last_finalizers.end(), [&active_key_id](const finalizer_auth_info& f) { return f.key_id == active_key_id; });
         // finalizer's active key id is in last proposed finalizers table
         BOOST_REQUIRE_EQUAL( true, itr != last_finalizers.end() );
         // finalizer's active key matches one in last proposed finalizers table
         BOOST_REQUIRE_EQUAL( true, itr->fin_authority.public_key == finalizer_info["active_key_binary"].as<std::vector<char>>() );
      }
   }
};


const std::vector<key_pair_t> finalizer_key_tester::key_pair {
   {"PUB_BLS_9Pbv53DbC9EDGGPEZUrlKuMGeXYWHzIPfmn-Ncrxj7sOTiTEOYIkcCgBrvfKYiIG0BsiafG1dRK9MV40aGWKREE4rWHpiDLIkHR91huq9CEdeZHUb2bRXvcxs0RAa7oADXSTmw", "SIG_BLS_IyjIzDWKzlekYSiRbJmBqYmWoiMsmHX_1aB0Hkqk1woeSJLbtd9D83lMDi6LiMEEpP79rP-AKiJAg1rKHODkt2habnn-y4jFMl76egQOv2KHLjobVzWWj_nz667VxeoF2FwF-NMnX7FQfpPgVRINSDKc5maRADRYQGrnlE-boh_23GBuCaxELnAFD-4L_ZEZRr8B9UIA62Vj9JBS_dExquL3wtlhx7vNq0MAGf9J3_Q69BsTuH_OMH6bs5m2AF4ZqiKpdQ" },
   { "PUB_BLS_WIuHboHhjl3zxB3pt1bdSdF9O6GPOmjMgQvxbzpJBPYYIrWIpcHr6uGbuNGHh8ITlMwLhb_7Fe2QTUStMU0sOgLlXl737hu9GBNyF9W7ZIc7U1xSOmBS5-OpCjViTs4DXQJrjg", "SIG_BLS_RL0hu5YQiXvuvDYdi6LuQqPAISBmS0OuUzGhRezqRYHQC1yL2VlV4lg17rDFzekFCYeB4Agtv0P3Zs3mCDRVwnIbZ3ETin-ONvXKo78Vi71BM4g-AWqdbXbvIbp86u0IXzWjQgSNTuKP2Jj4AqKwhNbDhMiqH5VXUuV1Ogp7P-kN6TcJiqQfqs69iAWklDgYMeDT9Y6qvrb7E1ZtkRbFzISKBrKDYCJ1KFYL7XgknPdqydjPiHEJEIX8ORNwPEgG11PbZQ"},
   {"PUB_BLS_OKI5k1cZcEAvkXQqWCMICE5BSXd1lI5VH_n-PagmHOP6MnKPpydAcS9qgHxm0bIWJwyzvMUdpm-TNqlQQGcQggAn_3GUQx7zSlrdzSFZ3h2kIXjpIDNDB2ZMQJ6wqYIXNX3kZg", "SIG_BLS_BGQbDqe4kpCtMEZsl4cfsSYGwquW8jGeq_9zppptRU4XQvAqRT_flXSg84sR9PMY9_m1lMsa6S9nnlilQ7SdMooOzOXVvYzNFmLuCR6b2c4jDZrJQGtgYleUINFQZEgBhzYS1WRbnlDIsRCBPbS_mDbfqQ80g-DKkUCcyeY0Cr7CJ9IoafmcVWJLFPWV0ukRcGAk0XKBWVbI23REXbgBeKD_XNMJe5PHJS36gZYWyaiO_wHIXAbUuYgOptmC0rAIEPVGow"},
   {"PUB_BLS_0kQpMupOH51c0cFLk-IoFALA9ePsCWys46A7HgExjvHzVqFNYBGIO4-SzhcdqToMCrs457K092irPa1JlUGCxdO0rluiyu6bJAYemTqPQZvzNYihZJ8OULOObIcsN8gW1aZNmA", "SIG_BLS_lTVkt0YINpwBIHXbthD_b21srBBJB5C6BMeyhA4RUCP4tkiyw9I4DUlJyR90v7AS-yhOHCFwDNmB1ld0wQYxnhxim1xB0zThhH9PZkkQH-Iksh46EujwEDSMQ7tLaqcXFEB9SmO9Yf8uaNEzY0qn11dgOWnAyxH4wb73QHE0g75gutiZ8XwugjPrpXUSj_ERiJcXE15_jr1TGGVYo1AIJYvhkiIpT4K3mNqtcFnawbhw2hr5Q0RXLGrSHCq21ncTRvtEUw"},
   {"PUB_BLS_pCUtXfVRKcErpxc0nh4wfSyEsCwj2pdQadJiXxArz-OTxhbkl12rezpiSlff01YWYvwGf_N11z1p6KyBUEyJFmOGTi-o89InkPoanMLe-CH9_HoifsbXLgKISdfs7CAZ-_DELA","SIG_BLS_R-vksnp0wpwJ0I8E0d4DIk4AuGsL6q3tzwoqk7wzKbPoQhHdtAcD1D00um8WQoUSdSAT1Z9PtFRFOUdsvS5oBLmOPqCtXW-GWiKH4GonU-NttXGdAnndPNTchvXzkEkSWSPl-2NUx1Ep2TUxvxyLuSkjE5-HFYGFZQuYTClf5-YZzstUUgAARrk2IZtsMpsI4l3_LM6bgfCrVNLxA5INOut1-EheK1M8ZBupmY9Ue6pMOJ4Po6LF8uAeLDxvDOgAToKDIQ"},
   {"PUB_BLS_FUu1bOokTWaf00LpoZl1OaO1Rr01EjxZ46fOs4FIgLVSf_n-WDG7QhMYFj5JGBoTWxUe-Hkk0VsvBzwVM85F4ZBEa8H6dLgQhevdjiYth8TDC2ZOSkcQdEUmtpcn4-8Hjy9aMg", "SIG_BLS_2nCBKKw377gYomeIq3_3kktsry6lZPr4_pCggFBL6sQSvkCuCeAi5igbdY9tNEkVJ9u6NVH5_JhkLvN7-oyw83B9WwajWZf7sERxebilDl5Spy-YLxXvorH8BOFap1kEIf5JktH4tsmwwJgySqZTGsGXzRIu3K1fk3F2rZ0I9aAE5XR2hYPrIIhV0Vv2T68PpAg1RUDuaxhXSM1woMA0IeCIvSISLF52m4XOvAu4tBRf03sNQzc2oXCLb9BZ4HYHJK7rSQ"},
   {"PUB_BLS_kYxGibaaU5EfQrnJpWcjwmY07j6px-uwJ0J_6RE7f9DrqhI3-NVyjw_jayvZ03wUjt96D7mecxSjEw3ZVHlguTykrjkI06otfpvv9Go_yS0gGmNYIRSWpOKtBIY8cIkEFVc8Ng", "SIG_BLS_NdXmsMAdWh8qSvUqDjKCJVjCjnCuDIeXQ_4XzFvCNGxZGwEDNhWgMVNdcLoEl7cUbiiuIdZVPC16E5_iYbSGfkPUBfx3Vh03GTy31a3dsXSciEM2VVGlBg5c_2vZTwEAbyYrVUT-sdt0nxO139I3Mk6Y6bauELYVkMVvv9E9Y_MjbkRnKDNQDqJACg46FEkFk-YQ0vvOA7sDyTP70BFwvF89DsE3db_g1sPjDfj7Kk3bda4KqMFncDObXIPk8QsY6gqzvw"},
   {"PUB_BLS_p819OPn04Fz4EI1QKrAOY0cKeGBPAyM1vFOc7yZn-cMSjIgyuSy3VVqNl8SgS2wRVE_kW9Ix2DIowByUsUrDVhJ-aay-VeP-EGzqpJLXu-baMRiGO5Wq8HQHMv-70zsXicba1Q", "SIG_BLS_38H7fX82apWxSCHS9-r3xW14hoH0zeFjqCq3bKajptvsEREzvY-m8njOocQJpQwJWmG5lxQ0Ij9JAu1qS9NMvnzt3tGWYWPK5cvczO6y4JrGPsa9c_Qx-g4YEFvj0oILtypCHl_c3moWttBZRra-QIESSeRnrsgtjpbb7EDORTeSa7_fp15aLaKbl3pugUIZU7uINgsCetG-mim3v4KiBRcZbAkG_m-dwqY099vTTFMA7Vt3xc3HOMk7urrvxAsQP2GnFA"},
   {"PUB_BLS_tbqdEUxBSTTpN60gIz8CGQL19G9mlOK9hcFuRQc9HQ9aZnF84BJQh3BP2mDG-kYDom_pohAEWry2w1e6AdgDcIcxpP2IbiZm000rDXZu7Bfv4Ebh6X7fVtDUt4piD9gCHXVw1A", "SIG_BLS_GmwLJWgq905FbJaJm0oTi8wiXlGIAm865thKx5MVVpGetMTh_9LtEEi0-iB0mmwC4OrVYbEoZwRSPsiMpA_VEcmrubWMN1noXEdjFG69LJVn9GLS4guCJQe_0W3_i9IDwIv7fixBaJyj8-iWvDR2n_vAmKdMUTDDtfdWeeXZ6yEwdYfsoyjIqUhXQq3Axk4AxiVFZ7yfiOkk3aGKOVwdCwd623WgiPldI-DEPUS0GaqXwgXtEyLtlDtdC13NovwC-9Jmsw"},
   {"PUB_BLS_mtAiKKCgt5QKDDn14QuQhs2AugC8XFQFQr7qUqHLodf264yUawOtzMr__juNTFQUWOr-JfKfUW1GfxLi0xXOYmEpw4wZaDm6VQCgTsOtTp5gaFcp1IJiVHDG1UuDB_oFKmMA1Q", "SIG_BLS_RMIvMoGMmd3tBHrDZSR_lQiR-mx0BJE7ZU_3HrdYfmhO8O9TE8iphSs3rETKFfIRIEU9N46DQBWU0t9wQLD468Pi92Gt6zEYREctw4_tNDtcqBGrYfr7g69qX97uqzsUulqyT_lRLH4-rwhPlRJgI6hg-G4C4bXTjUs_2LGoEhpvD6Y6LDTR9ek_Buuwra0OojtZ7anuFotVOD4uuBf34IQPyWrO9-X5RJ9bv19C09vCLbO8Djf8JCfr9cI0sh8TrOj4Gg"},
    {"PUB_BLS_j5fD6oG8C76CX7HzWxWf3XSU3gvROUybbTD9W1B_w2ilH98zBzXZ5VHmtIZCQ7YHUyQISAbjDUKwR2h1SL_jyHSw7G18Fj8Q8WK_I-UWdATbvH0xjjnmM-FQurPIxRsWsu7VvQ", "SIG_BLS_kzxMg5-VsVPgSKLdLJwRpJE7Zh8GtX8KaC-5zGU0RTm0mHx4wU_IbhqXBgEYrLkPhTtD7GIOzIMQgHTjsFxfDXOw2_s9dFiHwtjzPvOnQ5f4zFgmXipWjFBOSKzFP-sUwk4mMuvFO0dbva1RghLvk1G5T8H_aNGnWyb8ywQwC06axbBw5_poiU6DtonashoZ-ei2YRttxKdUbcg87BghKZNuBUfRXiNe1p4knuKM5o9W96pbI6uz6Dryn3IVuaMKUhtlnw"},
   {"PUB_BLS_M8ZZq5MspUdQmeclKbv1vZnSNy-2Qn_Yc9euP3zzG_bNfRwLPzA0UNQNmW66OZELZM_4lQ2scuQbb2gSKK462_p9we3ea_aTf18GLTinFm6Pp8Xr-ESveFEc48rx4kcQjY_Ddw", "SIG_BLS_ZRps69hLvs23lYlr68NEt6iIDvuUBhbDjIaUX3prARqYGJZB2FnB24tOLV83EE4IBrmGw-frILt1v5NRHUDJHgaoMndsmR2C2itoUDY7af7nOmxB5xaI5SNSiO_I2mMYQBDGaThDcnhCdalhzkDVmhOUeXR3wGSmNbPccTUf0J_DS5FmBzwnroaYJe8C6o0WRn4K0Wdsy5r_QNBsYtmSXxZf6vSdQJyaWQMIyowRHE3ve1L0F9PMVlNzZndemVwWExKQag"},
   {"PUB_BLS_aYnBaZVLoANPkIe3uyD453xc2NNYnIXupXTlAWbjaZ32CiGqfJYjtUJuoDf4i2wCMi_cQrv2tlLGpcYKql8g_B3NbFQrmHHwc80JaJi_YTVn4LDkIi1-jtHpsQ9pU28Zqfqx2Q", "SIG_BLS_kfYCci5ZsW0AJGWSxVxyx3Q3Lud_xB_VO1u0-9cJi1yfGC-rNbo_erFzbeOgLCIHYIE-kE4BWNJuvbO5sZ05walDhp33cCCMMeiEZUlk-sFUQdyhVJ4HtnsTea45jSMCjn650lXufOXlAbo3D_LrL_GH8y24OZVcI--iFuesi97SzEHifWBhLPy_yEBHcY4Gj0Ll37-y6moBx8L2NO4ZzSEiYViIyMQHHBTcb_qwPHzHR1TJeuKWp8hnnf3dirUA4n1XdQ"},
   {"PUB_BLS_xY48yStp-XsqjKikK50yzyGmK_6NacUmsJXUJNbjT-FKbeQ1bvbb4kEhtBP9K8QXfjRNvogpO2LSnK7G7P2xe7PP9NAZY-VmmeWBCUxCU_ZBPB7OiYeMr9JT9gXCLusQl-Amfg", "SIG_BLS_wYQJ7gnJCWhhsIjK1zbJzUXNVde_h0FH_6l6WtkiG0jdVTp-zkQtGzJR-P3aSPoFveLLpf8pTBU8vosk0eP3Ur3oRMl7Xnh-Xy9dWq0JglRSZQm-n1999TFYkWNH8T0HiNYFaXXBt5NA3LnMkoulZ16DsHfDxfw3Ggsq7BTJg68dsDrWscGXoa6C5ixU8lMOa2nf7sY_U6Qm0jnvNqLBCe1sNlgVDPhYXguIanAx92rEvW0KAFBykS96wg9sV2sRXsuJTQ"},
   {"PUB_BLS_v-d2afMcdbQp1EyukkQ_CUw8Q2a1mNC5ir4aJUUGTz96ER57K8TEwZGnYdoTmh0I5hzT3YaXkAuhvdkxuRKGi0saJgpkSP6UA8QKWqF0oJ3dAh7pG6mhZUGWybi51R8EZttgQg", "SIG_BLS_qgw6olECCtBviiEwjk3ve4Unomlkk37jSoG5IPfExsLXh-MfPGfkxZZfLDXzBWsY_wb7LZh2oxPKKYl_wXzIVGLFfSk8623WNegg8Hy7C32Uo2mlm0Eg-3fdjs_OFKIXdVZwheDXOOBycMAUEV1FR-jOC7TfOASqcO6eOAyo8Vjy0VoyIQx2mI73ocQFQeQLUUigmylx4mB2jjnbbZ7JY4y4X1gWIYiSoe8PoxRYmZJdSz8W9Ku0dRN9f8ir4SoQJ8lfKw"},
   {"PUB_BLS_TGdjRLnSlH5lN75APoANTjnuZ3mh8Uqy4GEqzoc6su9t7dOmrsoAtKn2TqHaZFYYt1MuoDF8eYBwS1YHN8w_vbaEzUupBtWdU-QMzOxw4Qei6CLt5xlju1WfRXaPCu8IQXbOTw", "SIG_BLS_v6xg1vlIB4QZzZCx1CUQSTSOnhRJQpx7Qf3VB2NdAXRs09vrmB2fxVfvxdkwngYUQgcwjs_XBvSGX0ECvqYz2wNHmTS-8O0SIeI2Ne6BRcLO_s47ssG7ookuMOWD14sYeM1embZy4BLD68hapxPeyEzY6c9U8knwHXOs_PmgTfxPKbj_Gd53n6YogEHyltIQV_xo83mdu-eG2ny4pQ0PgTBJakhbKfL5z_TtAHCkUbVO84Gnx4rPYW8JjaeNbkUHAa9IRA"},
   {"PUB_BLS_Dow8yQTh_mVETS4ky1y8ae9zcDuQZtHXlYEMnTTx10nczdrwZExP-6bWQcYpfu4Cj5UGJdxjHjHhdk1BQItK22FaZ_bZb1yURrSuqmr7CeuU9HyRPOZ3id03xCbcaFoFqvif5g", "SIG_BLS_C5uvcsKu09x5sBzWjmz7V3amYKnKPOJrw8s0FyaohXvF71b2_WZogus1K692VdgTTckPItM__Xe38TgXb5rJTCpD7nKiKjXTpuKQ2IMM0PEQFXhY_1sVrUXYdIghX9cJm0_wILLcJc39--lVCrPKC0lGdV5d1RUy3cnb-Z4au4BFUSh5vY9oE7R0jf9jqO8FOiB0G7VFurRPeWVKfQbtgohriP1_TmmDH9iTjtgzvKO3YvZAxrnxiYCTocXy_poMekgNnQ"},
   {"PUB_BLS_1ZV6MNrDyg1_VOqkIQ6xaYoYi6diO_obKjSnj9-wspP2PQGZ1mIPqJ-2zMuHuv4F-D0mFVkh4TBZd_ildGCr7foEzxUG25f1DjSuhcAfC-X-q0eYh1NxnZzeiHCKjU0OXnAfXQ", "SIG_BLS_Ht4ZYXmuDXUw3jDgi7dwvUhm9nj8cs3kMgqJYmUvH5Kb75BmjKOgOE8EOi4NsWQNoA6JmvjZ5WZyFFm0Fv8uNmvgrlWu4xU_uEkmThkD5C47dk88tRi-Ve74GjjjOIMEDkUjmMka18dUDu-UTPqy56ju-XGDIwvPrED8ZqK7L_3ZOSPcb21QXb2iVEP-UbQXJHnDaU4zSbKt60EFaWEWgPt501A98Gwti0aBJFPJjW8jaziErlCHUKmJaEISjjUCQBbc4Q"},
   {"PUB_BLS_xCZ8dBxW7GCPvCh2v5AEAMLpPlqb5MY6KQ4AjgZiH63WCrsPF-tNOSb1tj3Tml0WQHnXtuQtAXdysAX6wGX92zvCtl779vtEnXcSkZrcoXQ036IeDg91B7s4HZVDdTIRhkWdIg", "SIG_BLS_P7bbvvAPtz1hX86X2NSEZKn6AljBpq00pL8wwpIrEjlVkWh-tSwPephv9SNyoTkE8_4HB5e97-MPILmEpYp_5lphbcimrCIZsO53dWxxHmp7TgxofGmzk6_fSDCuZI4M3fsnI3-NnjzpRKg8YK7FmAv0clHA9wj6nLa7HPsDsV8hHqAyAJwxxttMpSOpBMkESUuLXJL0AuaRsTU0KUCGeNQQxCkR_tYhOUPLZwh_Lc__35PxpZQk5Ivm1UmDZRcNRQUXEA"},
   {"PUB_BLS_lG6RfU8c0kJVvFNA8ikdU1dOtt_FrsHd8L7OMRHO9D-ct18IztEKqkHBAb6cXJEL2VL7cr2rI_LL20Fgu_9e4PIG4U54o5lp_ydwAweGtay1190QB5l8RDGT-s_sshMBkV6N8A", "SIG_BLS_--zVqQC10legB0F_UiBsQlgdNDi-VfFPo4KJPa8-Npf_bKqqaTjvHrKl56398ecBpo2GQbPeYvEM0aE2Pg-lfQLbRlFCKuOBvHsz01nBmlJU3Ai1wkwiIaLrhURIrhELcnFnA4EF60DSwMTND5FGRYHqpX3nZULiu5z-a6hrvrFUMDuCiI6fMblhX1-3sbcTwQAm7X8m-V70X_1zNHI1uuZEC4AdSouXdyG3vgeg10osfnBQn7XMhJr4SA45EW8QLBwQ4w"},
   {"PUB_BLS_3yY58HBFvJwwIcc-nADx4WqAAfLOhVa3ORc_bqN7bH5zsFImQHv98GRInYU8PWQUBbE-mNM_nsjBYcjPGMXRkePZTlgNfWtFkm5X6JkWLshvwMSzABl86UF1i7eXPbkWaHLzHQ", "SIG_BLS_pWQzUEOXHvgrf7UL7rSvwFgd6tZ7BrYtUFZG2AQgvC76cj5bUf9Cg0h-yTwOMkEDX48ZuPu4BsbifACr1cnXQ8vSYYFMcthVCL04FF6nGzh3Rungs4MyUDO0OwmXEtMW9U39CnquCoGhjFtgq--En4PuGmPhBnpDaDZzUau3Y0pqnji3gWpEM_wYJQUDvtIYUkXmOFXQvizWwYKMT2F7bXr6TLRIcIoMv7qkKReiNSBwnId8qY6rsqEd1MFpb1cJV6meeQ" },
   {"PUB_BLS_DwHoibpeQ6i1VnoF8cjYFI-Z6etehxApERYpKpjP8IvVEXeDUaUO54siI6oSFAYB4ZqKrBykU081FDqpf7ao41McS_N92cnNNjB56_gB1uGmjYlQMJgrj26fX2tveZYT2BXsTQ", "SIG_BLS_zfP89oJUxuryHX0Ok45UbzM3It-pqoBxgcqnF3fZ9Z48e6AYHkPWiwvktfBlsYYVYUtSYvERBbu2YVf9S-DA5i7F-feHoHKEf0MNEam-w-qy09lZn0jaT8OX4JIK078ItlLfMULIkLyQ_RxzjX9G-khbsXN0ody6GD3L4JV69Ol_fRPt9OH20CFkgKfUF18FESPSxIPXgyawxPJY3ND-GAfsftZmggTrcJsLJMab7wkJ4j_8L1I9hBfZ9tO3H3AZ1hQ5Hg"}
};

BOOST_AUTO_TEST_SUITE(eosio_system_finalizer_key_tests)

const name alice = "alice1111111"_n;
const name bob   = "bob111111111"_n;

const std::string finalizer_key_1 = "PUB_BLS_6j4Y3LfsRiBxY-DgvqrZNMCttHftBQPIWwDiN2CMhHWULjN1nGwM1O_nEEJefqwAG4X09n4Kdt4a1mfZ1ES1cLGjQo6uLLSloiVW4i9BUhMHU2nVujP1_U_9ihdI3egZ17N-iA";
const std::string finalizer_key_2 = "PUB_BLS_gtaOjOTa0NzDt8etBDqLoZKlfKTpTalcdfmbTJknLUJB2Fu4Cv-uoa8unF3bJ5kFewaCzf3tjYUyNE6CDSrwvYP5Nw47Y9oE9x4nqJWfJykMOoaI0kJz-GDrGN2nZdUAp5tWEg";
const std::string finalizer_key_3 = "PUB_BLS_CT8khvZYYZdObeIV9aTnd8fZ8bdaCL1UpSRyqNLZZM5sdrOSpOjDTAY2drTYGvQPVS21BhtD8acLJhqGyTfjqrWjyY5FTfqLdcligofSpa2lrG3FqKVNeUULR5QgcIMYga4vkQ";
const std::string finalizer_key_4 = "PUB_BLS_hJYC9REVk4Pzgt3NMycIaCRpRqTX8IIEAB8xhWg6pOYsV7n9gJnTTUOzPGH8VE4FPhxvzJuvrb5TNeR5MHwIjPMMKVPYHI-dDFwl5Oqj0yH9uoKRcMqjEaFZ5VYX3zMJuA1jQQ";

const std::string pop_1 = "SIG_BLS_N5r73_i50OVkydasCVVBOqqAqM4XQo_-DHgNawK77bcf06Bx0_rh5TNn9iZewNMZ6ecyEjs_sEkwjAXplhqyqf7S9FqSt8mfRxO7pE3bUZS0Z-Fxitsh9X0l_-kj3Z8VD8IwsaUwBLacudzShIXA-5E47cEqYoV3bGhANerKuDhZ4Pesm2xotAScK0pcNp0LbTNj0MZpVr0u6kJh169IoeG4ngCvD6uE2EicNrzyvDhu0u925Q1cm5z_bVha-DsANq3zcA";
const std::string pop_2 = "SIG_BLS_9e1SzM60bWLdxwz4lYNQMNzMGeFuzFgJDYy7WykmynRVRQeIx2O2xnyzwv1WXvgYHLyMYZ4wK0Y_kU6jl330WazkBsw-_GzvIOGy8fnBnt5AyMaj9X5bhDbvB5MZc0QQz4-P2Z4SltTY17ZItGeekkjX_fgQ9kegM4qnuGU-2iqFj5i3Qf322L77b2SHjFoLmxdFOsfGpz7LyImSP8GcZH39W30cj5bmxfsp_90tGdAkz-7DG9nhSHYxFq6qTqMGijVPGg";
const std::string pop_3 = "SIG_BLS_cJTQMGv1isqpcHEfhogLhlU56bKpGgo-Svi3Z4NXvWcly5TJo8hDChodIV-aEHgMqr06LuZftR7WFvgGkbOSdmwdO4t58R3RYOMSK-jjif2z-fEwCl7jsxUutASIRwIYtTI7h6NLCjARiKNi5BkES33xY8wYMWf-JkgbpsD2cGsZW4hkMW7T2j_1w89HmNwCn4V_hjlPM_kgz45RoYpKq4w2QEaLdCYTJ6xYOfc9Occc15c76dd1jjty_yT2RMAKO0mfUA";
const std::string pop_4 = "SIG_BLS_GNlwGxjL-LCVDApTHernv8Hj6EqlsxWlZBUzOu6DcJmNNuHsfetXK14RPJ-L63wVnhPRL9aNrQAURy2wYJ1__rNiGk-nUMZ5RDTO7tO2EPTiyySq9cbzgn43vKG8FgsA4gbNlqVFeTCbo5CgGj8m9vXNV4-Cv68WW-ivcwJzDtnNA3O9PPIpRY6_HhbmwTUVrHL2v7X_arNCyAf29nucAYNOsCM-br6F6HwpSjqSxi4-KqcFfQCWbAbn_SgJNVAA4yx5fQ";

const std::string finalizer_key_binary_1 = "ea3e18dcb7ec46207163e0e0beaad934c0adb477ed0503c85b00e237608c8475942e33759c6c0cd4efe710425e7eac001b85f4f67e0a76de1ad667d9d444b570b1a3428eae2cb4a5a22556e22f415213075369d5ba33f5fd4ffd8a1748dde819";
const std::string finalizer_key_binary_2 = "82d68e8ce4dad0dcc3b7c7ad043a8ba192a57ca4e94da95c75f99b4c99272d4241d85bb80affaea1af2e9c5ddb2799057b0682cdfded8d8532344e820d2af0bd83f9370e3b63da04f71e27a8959f27290c3a8688d24273f860eb18dda765d500";
const std::string finalizer_key_binary_3 = "093f2486f65861974e6de215f5a4e777c7d9f1b75a08bd54a52472a8d2d964ce6c76b392a4e8c34c063676b4d81af40f552db5061b43f1a70b261a86c937e3aab5a3c98e454dfa8b75c9628287d2a5ada5ac6dc5a8a54d79450b479420708318";
const std::string finalizer_key_binary_4 = "849602f511159383f382ddcd33270868246946a4d7f08204001f3185683aa4e62c57b9fd8099d34d43b33c61fc544e053e1c6fcc9bafadbe5335e479307c088cf30c2953d81c8f9d0c5c25e4eaa3d321fdba829170caa311a159e55617df3309";

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_failure_tests, finalizer_key_tester) try {
   {  // bob111111111 does not have Alice's authority
      BOOST_REQUIRE_EQUAL( error( "missing authority of bob111111111" ),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "bob111111111")
                              ("finalizer_key", finalizer_key_1 )
                              ("proof_of_possession", pop_1 )
                           ) );
   }

   { // attempt to register finalizer_key for an unregistered producer
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer alice1111111 is not a registered producer" ),
                           register_finalizer_key(alice, finalizer_key_1, pop_1));
   }

   // Now alice1111111 registers as a producer
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );


   {  // finalizer key does not start with PUB_BLS
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key does not start with PUB_BLS: UB_BLS_6j4Y3LfsRiBxY-DgvqrZNMCttHftBQPIWwDiN2CMhHWULjN1nGwM1O_nEEJefqwAG4X09n4Kdt4a1mfZ1ES1cLGjQo6uLLSloiVW4i9BUhMHU2nVujP1_U_9ihdI3egZ17N-iA"),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "alice1111111")
                              ("finalizer_key", "UB_BLS_6j4Y3LfsRiBxY-DgvqrZNMCttHftBQPIWwDiN2CMhHWULjN1nGwM1O_nEEJefqwAG4X09n4Kdt4a1mfZ1ES1cLGjQo6uLLSloiVW4i9BUhMHU2nVujP1_U_9ihdI3egZ17N-iA" )
                              ("proof_of_possession", pop_1 )
                           ) );
   }

   {  // proof_of_possession does not start with SIG_BLS
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "proof of possession signature does not start with SIG_BLS: XIG_BLS_N5r73_i50OVkydasCVVBOqqAqM4XQo_-DHgNawK77bcf06Bx0_rh5TNn9iZewNMZ6ecyEjs_sEkwjAXplhqyqf7S9FqSt8mfRxO7pE3bUZS0Z-Fxitsh9X0l_-kj3Z8VD8IwsaUwBLacudzShIXA-5E47cEqYoV3bGhANerKuDhZ4Pesm2xotAScK0pcNp0LbTNj0MZpVr0u6kJh169IoeG4ngCvD6uE2EicNrzyvDhu0u925Q1cm5z_bVha-DsANq3zcA" ),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "alice1111111")
                              ("finalizer_key", finalizer_key_1)
                              ("proof_of_possession", "XIG_BLS_N5r73_i50OVkydasCVVBOqqAqM4XQo_-DHgNawK77bcf06Bx0_rh5TNn9iZewNMZ6ecyEjs_sEkwjAXplhqyqf7S9FqSt8mfRxO7pE3bUZS0Z-Fxitsh9X0l_-kj3Z8VD8IwsaUwBLacudzShIXA-5E47cEqYoV3bGhANerKuDhZ4Pesm2xotAScK0pcNp0LbTNj0MZpVr0u6kJh169IoeG4ngCvD6uE2EicNrzyvDhu0u925Q1cm5z_bVha-DsANq3zcA")
                           ) );
   }

   {  // proof_of_possession fails
      BOOST_REQUIRE_EQUAL( wasm_assert_msg( "proof of possession check failed" ),
                           push_action(alice, "regfinkey"_n, mvo()
                              ("finalizer_name",  "alice1111111")
                              // use a valid formatted finalizer_key for another signature
                              ("finalizer_key", finalizer_key_1)
                              ("proof_of_possession", pop_2)
                           ) );
   }
} // register_finalizer_key_invalid_key_tests
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_by_same_finalizer_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // Register first finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );

   // Make sure binary format is correct, which is important
   auto alice_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", alice_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, alice_info["finalizer_key_count"].as_uint64() );
   BOOST_REQUIRE_EQUAL( finalizer_key_binary_1, alice_info["active_key_binary"].as_string() );

   // Cross check finalizer keys table
   uint64_t active_key_id = alice_info["active_key_id"].as_uint64();
   auto fin_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_1, fin_key_info["finalizer_key"].as_string() );

   // Register second finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_2, pop_2 ));

   alice_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( 2, alice_info["finalizer_key_count"].as_uint64() ); // count incremented by 1
   BOOST_REQUIRE_EQUAL( active_key_id, alice_info["active_key_id"].as_uint64() ); // active key should not change
}
FC_LOG_AND_RETHROW() // register_finalizer_key_by_same_finalizer_tests

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_duplicate_key_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // The first finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );

   auto alice_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", alice_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, alice_info["finalizer_key_count"].as_uint64() );

   // Tries to register the same finalizer key
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "duplicate finalizer key: " + finalizer_key_1 ),
                        register_finalizer_key(alice, finalizer_key_1, pop_1) );

   // finalizer key count still 1
   alice_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( 1, alice_info["finalizer_key_count"].as_uint64() );
}
FC_LOG_AND_RETHROW() // register_finalizer_key_duplicate_key_tests

BOOST_FIXTURE_TEST_CASE(register_finalizer_key_by_different_finalizers_tests, finalizer_key_tester) try {
   // register 2 producers
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // alice1111111 registers a finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_2, pop_2) );

   auto alice_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", alice_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_binary_1, alice_info["active_key_binary"].as_string() );
   BOOST_REQUIRE_EQUAL( 2, alice_info["finalizer_key_count"].as_uint64() );

   // bob111111111 registers another finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_3, pop_3) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_4, pop_4) );

   auto bob_info = get_finalizer_info(bob);
   BOOST_REQUIRE_EQUAL( 2, bob_info["finalizer_key_count"].as_uint64() );
   BOOST_REQUIRE_EQUAL( finalizer_key_binary_3, bob_info["active_key_binary"].as_string() );
}
FC_LOG_AND_RETHROW() // register_finalizer_key_by_different_finalizers_tests


BOOST_FIXTURE_TEST_CASE(register_duplicate_key_from_different_finalizers_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // The first finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );

   auto fin_info = get_finalizer_info(alice);
   BOOST_REQUIRE_EQUAL( "alice1111111", fin_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( 1, fin_info["finalizer_key_count"].as_uint64() );

   // bob111111111 tries to register the same finalizer key as the first one
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "duplicate finalizer key: " + finalizer_key_1 ),
                        register_finalizer_key(bob, finalizer_key_1, pop_1) );
}
FC_LOG_AND_RETHROW() // register_duplicate_key_from_different_finalizers_tests

BOOST_FIXTURE_TEST_CASE(activate_finalizer_key_failure_tests, finalizer_key_tester) try {
   // bob111111111 does not have Alice's authority
   BOOST_REQUIRE_EQUAL( error( "missing authority of bob111111111" ),
                        push_action(alice, "actfinkey"_n, mvo()
                           ("finalizer_name", "bob111111111")
                           ("finalizer_key",  finalizer_key_1 )
                        ) );

   // Register producers
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // finalizer has not registered any finalizer keys yet.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer alice1111111 has not registered any finalizer keys" ),
                        activate_finalizer_key(alice, finalizer_key_1) );

   // Alice registers a finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_2, pop_2) );

   // Activate a finalizer key not registered by anyone
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key was not registered: " + finalizer_key_3 ),
                        activate_finalizer_key(alice, finalizer_key_3) );

   // Activate a finalizer key not registered by Alice
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key was not registered by the finalizer: " + finalizer_key_2 ),
                        activate_finalizer_key(alice, finalizer_key_2) );

   // Activate a finalizer key that is already active (the first key registered is
   // automatically set to active
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key was already active: " + finalizer_key_1 ),
                        activate_finalizer_key(alice, finalizer_key_1) );
}
FC_LOG_AND_RETHROW() // activate_finalizer_key_failure_tests

BOOST_FIXTURE_TEST_CASE(activate_finalizer_key_success_tests, finalizer_key_tester) try {
   // Alice registers as a producer
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // Alice registers two finalizer keys. The first key is active
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_2, pop_2) );

   // Check finalizer_key_1 is the active key
   auto alice_info = get_finalizer_info(alice);
   uint64_t active_key_id = alice_info["active_key_id"].as_uint64();
   auto finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", finalizer_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_1, finalizer_key_info["finalizer_key"].as_string() );

   // Activate the second key
   BOOST_REQUIRE_EQUAL( success(), activate_finalizer_key(alice, finalizer_key_2) );

   // Check finalizer_key_2 is the active key
   alice_info = get_finalizer_info(alice);
   active_key_id = alice_info["active_key_id"].as_uint64();
   finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", finalizer_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_2, finalizer_key_info["finalizer_key"].as_string() );

   // Make sure active_key_binary is correct. This test is important.
   BOOST_REQUIRE_EQUAL( finalizer_key_binary_2, alice_info["active_key_binary"].as_string() );
}
FC_LOG_AND_RETHROW() // activate_finalizer_key_success_tests

BOOST_FIXTURE_TEST_CASE(delete_finalizer_key_failure_tests, finalizer_key_tester) try {
   // bob111111111 does not have Alice's authority
   BOOST_REQUIRE_EQUAL( error( "missing authority of bob111111111" ),
                        push_action(alice, "delfinkey"_n, mvo()
                           ("finalizer_name", "bob111111111")
                           ("finalizer_key",  finalizer_key_1 )
                        ) );

   // Register producers
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );
   BOOST_REQUIRE_EQUAL( success(), regproducer(bob) );

   // finalizer has not registered any finalizer keys yet.
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer alice1111111 has not registered any finalizer keys" ),
                        delete_finalizer_key(alice, finalizer_key_1) );

   // Alice and Bob register finalizer keys
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_2, pop_2) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(bob, finalizer_key_3, pop_3) );

   // Alice tries to delete  a finalizer key not registered by anyone
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key was not registered: " + finalizer_key_4 ),
                        delete_finalizer_key(alice, finalizer_key_4) );

   // Alice tries to delete a finalizer key registered by Bob
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "finalizer key " + finalizer_key_2 + " was not registered by the finalizer alice1111111" ),
                        delete_finalizer_key(alice, finalizer_key_2) );

   // Make sure finalizer_key_2 is Bob's active finalizer key and Bob has 2 keys
   auto bob_info = get_finalizer_info(bob);
   uint64_t active_key_id = bob_info["active_key_id"].as_uint64();
   auto finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( finalizer_key_2, finalizer_key_info["finalizer_key"].as_string() );
   BOOST_REQUIRE_EQUAL( 2, bob_info["finalizer_key_count"].as_uint64() );

   // Bob tries to delete his active finalizer key but he has 2 keys
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "cannot delete an active key unless it is the last registered finalizer key, has 2 keys" ),
                        delete_finalizer_key(bob, finalizer_key_2) );

}
FC_LOG_AND_RETHROW() // delete_finalizer_key_failure_tests

BOOST_FIXTURE_TEST_CASE(delete_finalizer_key_success_test, finalizer_key_tester) try {
   // Alice registers as a producer
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // Alice registers two keys and the first key is active 
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_2, pop_2) );

   // Check finalizer_key_1 is the active key
   auto alice_info = get_finalizer_info(alice);
   uint64_t active_key_id = alice_info["active_key_id"].as_uint64();
   auto finalizer_key_count_before = alice_info["finalizer_key_count"].as_uint64();
   auto finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", finalizer_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_1, finalizer_key_info["finalizer_key"].as_string() );

   // Delete the non-active key
   BOOST_REQUIRE_EQUAL( success(), delete_finalizer_key(alice, finalizer_key_2) );

   alice_info = get_finalizer_info(alice);
   auto finalizer_key_count_after = alice_info["finalizer_key_count"].as_uint64();
   BOOST_REQUIRE_EQUAL( finalizer_key_count_before - 1, finalizer_key_count_after );
}
FC_LOG_AND_RETHROW() // delete_finalizer_key_success_test

BOOST_FIXTURE_TEST_CASE(delete_last_finalizer_key_test, finalizer_key_tester) try {
   // Alice registers as a producer
   BOOST_REQUIRE_EQUAL( success(), regproducer(alice) );

   // Alice registers one key and it is active
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(alice, finalizer_key_1, pop_1) );

   // Check finalizer_key_1 is the active key
   auto alice_info = get_finalizer_info(alice);
   uint64_t active_key_id = alice_info["active_key_id"].as_uint64();
   auto finalizer_key_info = get_finalizer_key_info(active_key_id);
   BOOST_REQUIRE_EQUAL( "alice1111111", finalizer_key_info["finalizer_name"].as_string() );
   BOOST_REQUIRE_EQUAL( finalizer_key_1, finalizer_key_info["finalizer_key"].as_string() );

   // Delete it
   BOOST_REQUIRE_EQUAL( success(), delete_finalizer_key(alice, finalizer_key_1) );

   // Both finalizer_key_1 and alice should be removed from finalizers and finalizer_keys tables
   BOOST_REQUIRE_EQUAL( true, get_finalizer_key_info(active_key_id).is_null() );
   BOOST_REQUIRE_EQUAL( true, get_finalizer_info(alice).is_null() );
}
FC_LOG_AND_RETHROW() // delete_last_finalizer_key_test

BOOST_FIXTURE_TEST_CASE(switchtosvnn_success_tests, finalizer_key_tester) try {
   // Register and vote 26 producers
   auto producer_names = active_and_vote_producers();
   // Register 21 finalizer keys for the first 21 producers
   register_finalizer_keys(producer_names, 21);
   BOOST_REQUIRE_EQUAL(success(),  push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );

   // Verify finalizers_table and last_prop_fins_table match
   verify_last_proposed_finalizers(producer_names);

   // Produce enough blocks so transition to Savanna finishes
   produce_blocks(504); // 21 Producers * 12 Blocks per producer * 2 rounds to reach Legacy finality

   // If head_finality_data has value, it means we are at least after or on
   // Savanna Genesis Block
   BOOST_REQUIRE_EQUAL( true, control->head_finality_data().has_value() );

   // Cannot switch to Savanna multiple times
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "switchtosvnn can be run only once" ),
                        push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );
}
FC_LOG_AND_RETHROW()

// Activate 3 keys after transition to Savanna
BOOST_FIXTURE_TEST_CASE(multi_activation_tests, finalizer_key_tester) try {
   // Register and vote 26 producers
   auto producer_names = active_and_vote_producers();
   // Register 21 finalizer keys for the first 21 producers
   register_finalizer_keys(producer_names, 21);
   BOOST_REQUIRE_EQUAL(success(),  push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );

   // Verify finalizers_table and last_prop_fins_table match
   verify_last_proposed_finalizers(producer_names);

   // Produce enough blocks so transition to Savanna finishes
   produce_blocks(504); // 21 Producers * 12 Blocks per producer * 2 rounds to reach Legacy finality

   // If head_finality_data has value, it means we are at least after or on
   // Savanna Genesis Block
   BOOST_REQUIRE_EQUAL( true, control->head_finality_data().has_value() );

   // Register two more key for defproducera
   account_name producera = "defproducera"_n;
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(producera, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(producera, finalizer_key_2, pop_2) );

   auto producera_info = get_finalizer_info(producera);
   auto active_key_id_before = producera_info["active_key_id"].as_uint64();
   auto last_prop_fin_ids_before = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_prop_fin_ids_before.size() );

   // Activate finalizer_key_1
   BOOST_REQUIRE_EQUAL( success(), activate_finalizer_key(producera, finalizer_key_1) );
   produce_block();

   // Make sure last proposed finalizers set has changed
   producera_info = get_finalizer_info(producera);
   auto active_key_id_after = producera_info["active_key_id"].as_uint64();
   auto last_prop_fin_ids_after = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_prop_fin_ids_after.size() );

   last_prop_fin_ids_before.erase(active_key_id_before);
   last_prop_fin_ids_before.insert(active_key_id_after);
   BOOST_REQUIRE_EQUAL( true, last_prop_fin_ids_before == last_prop_fin_ids_after );

   // Activate finalizer_key_2
   last_prop_fin_ids_before = last_prop_fin_ids_after;
   active_key_id_before = active_key_id_after;

   BOOST_REQUIRE_EQUAL( success(), activate_finalizer_key(producera, finalizer_key_2) );
   produce_block();

   // Make sure last proposed finalizers set has changed
   producera_info = get_finalizer_info(producera);
   active_key_id_after = producera_info["active_key_id"].as_uint64();
   last_prop_fin_ids_after = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_prop_fin_ids_after.size() );

   last_prop_fin_ids_before.erase(active_key_id_before);
   last_prop_fin_ids_before.insert(active_key_id_after);
   BOOST_REQUIRE_EQUAL( true, last_prop_fin_ids_before == last_prop_fin_ids_after );
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(switchtosvnn_missing_authority_tests, finalizer_key_tester) try {
   BOOST_REQUIRE_EQUAL( error( "missing authority of eosio" ),
                        push_action( alice, "switchtosvnn"_n, mvo()) );
}
FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE(switchtosvnn_not_enough_finalizer_keys_tests, finalizer_key_tester) try {
   auto producer_names = active_and_vote_producers();

   // Register 20 finalizer keys
   register_finalizer_keys(producer_names, 20);

   // Have only 20 finalizer keys, short by 1
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "not enough top producers have registered finalizer keys, has 20, require 21" ),
                        push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );
}
FC_LOG_AND_RETHROW()

// Finalizers are not changed in current schedule rounds
BOOST_FIXTURE_TEST_CASE(update_elected_producers_no_finalizers_changed_test, finalizer_key_tester) try {
   auto producer_names = active_and_vote_producers();
   register_finalizer_keys(producer_names, 21);
   BOOST_REQUIRE_EQUAL(success(),  push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );

   // Produce enough blocks so transition to Savanna finishes
   produce_blocks(504); // 21 Producers * 12 Blocks per producer * 2 rounds to reach Legacy finality

   // head_finality_data is available when nodoes is in Savanna
   BOOST_REQUIRE_EQUAL( true, control->head_finality_data().has_value() );

   // Verify finalizers_table and last_prop_fins_table match
   verify_last_proposed_finalizers(producer_names);

   auto last_finkey_ids = get_last_prop_fin_ids();

   // Produce for one round
   produce_block( fc::minutes(2) );

   // Since finalizer keys have not changed, last_finkey_ids should be the same
   auto last_finkey_ids_2 = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_finkey_ids_2.size() );
   BOOST_REQUIRE_EQUAL( true, last_finkey_ids == last_finkey_ids_2 );
}
FC_LOG_AND_RETHROW()

// An active finalizer activates another key. The change takes effect immediately.
BOOST_FIXTURE_TEST_CASE(update_elected_producers_finalizers_changed_test, finalizer_key_tester) try {
   auto producer_names = active_and_vote_producers();
   register_finalizer_keys(producer_names, 21);
   BOOST_REQUIRE_EQUAL(success(),  push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );

   // Produce enough blocks so transition to Savanna finishes
   produce_blocks(504); // 21 Producers * 12 Blocks per producer * 2 rounds to reach Legacy finality

   // head_finality_data is available when nodoes is in Savanna
   BOOST_REQUIRE_EQUAL( true, control->head_finality_data().has_value() );

   // Verify finalizers_table and last_prop_fins_table match
   verify_last_proposed_finalizers(producer_names);
   // Verify last finalizer key id table contains all finalzer keys
   auto last_finkey_ids = get_last_prop_fin_ids();

   // Pick a producer
   name test_producer = producer_names.back();

   // Take a note of old active_key_id
   auto p_info = get_finalizer_info(test_producer);
   uint64_t old_id = p_info["active_key_id"].as_uint64();

   // Register and activate a new finalizer key
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(test_producer, finalizer_key_1, pop_1) );
   BOOST_REQUIRE_EQUAL( success(), activate_finalizer_key(test_producer, finalizer_key_1));

   // Since producer is an active producer, the finalizer key change takes effective
   // immediately.
   auto last_finkey_ids_2 = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_finkey_ids_2.size() );

   // Take a note of new active_key_id
   auto p_info_2 = get_finalizer_info(test_producer);
   uint64_t new_id = p_info_2["active_key_id"].as_uint64();

   // After replace the old_id with new_id in last_finkey_ids,
   // last_finkey_ids should be the same as last_finkey_ids_2
   last_finkey_ids.erase(old_id);
   last_finkey_ids.insert(new_id);
   BOOST_REQUIRE_EQUAL( true, last_finkey_ids == last_finkey_ids_2 );
}
FC_LOG_AND_RETHROW()

// An active finalizer deletes its only key. It is replaced by another finalizer in next round.
BOOST_FIXTURE_TEST_CASE(update_elected_producers_finalizers_replaced_test, finalizer_key_tester) try {
   // Create and vote 26 producers
   auto producer_names = active_and_vote_producers(26);

   // But only the first 21 producers (defproducera .. defproduceru) register finalizer
   // keys at the beginning
   register_finalizer_keys(producer_names, 21);

   // Transition to Savanna
   BOOST_REQUIRE_EQUAL(success(),  push_action( config::system_account_name, "switchtosvnn"_n, mvo()) );

   auto last_finkey_ids = get_last_prop_fin_ids();

   // Produce enough blocks so transition to Savanna finishes
   produce_blocks(2 * 21 * 12);
   // head_finality_data is available when nodoes is in Savanna
   BOOST_REQUIRE_EQUAL( true, control->head_finality_data().has_value() );

   // Verify finalizers_table and last_prop_fins_table match
   std::vector<name> producer_names_first_21(producer_names.begin(), producer_names.begin() + 21);
   verify_last_proposed_finalizers(producer_names_first_21);

   // Test delete the first finalizer key

   // defproducerv registers its first finalizer key and is marked active
   account_name  producerv_name = "defproducerv"_n;
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(producerv_name, finalizer_key_1, pop_1) );
   auto producerv_info = get_finalizer_info(producerv_name);
   uint64_t producerv_id = producerv_info["active_key_id"].as_uint64();

   // Wait for two rounds of producer schedule so new finalizer policy takes effect
   produce_block( fc::minutes(2) );

   // Delete defproducera's finalizer key
   name producera_name = "defproducera"_n;
   auto p_info = get_finalizer_info(producera_name);
   uint64_t deleted_id = p_info["active_key_id"].as_uint64();
   auto k_info = get_finalizer_key_info(deleted_id);
   auto producera_id = k_info["finalizer_key"].as_string();
   BOOST_REQUIRE_EQUAL( success(), delete_finalizer_key(producera_name, producera_id) );

   // Wait for two rounds of producer schedule so defproducera is replaced by defproducerv
   // because defproducera does not have an active finalizer key
   produce_blocks(504);

   // find new last_finkey_ids
   auto last_finkey_ids_2 = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_finkey_ids_2.size() );
   // Make sure new_id is in the new last_finkey_ids
   BOOST_REQUIRE_EQUAL( true, last_finkey_ids_2.contains(producerv_id) );

   // After replace the deleted_id with new_id in the old last_finkey_ids,
   // last_finkey_ids should be the same as last_finkey_ids_2
   last_finkey_ids.erase(deleted_id);
   last_finkey_ids.insert(producerv_id);
   BOOST_REQUIRE_EQUAL( true, last_finkey_ids == last_finkey_ids_2 );

   // Test delete last finalizer key

   // defproducerw registers its first finalizer key and is marked active
   account_name  producerw_name = "defproducerw"_n;
   BOOST_REQUIRE_EQUAL( success(), register_finalizer_key(producerw_name, finalizer_key_2, pop_2) );
   auto producerw_info = get_finalizer_info(producerw_name);
   uint64_t producerw_id = producerw_info["active_key_id"].as_uint64();

   // Wait for two rounds of producer schedule so new finalizer policy takes effect
   produce_block( fc::minutes(2) );

   // Delete defproducerv's finalizer key
   BOOST_REQUIRE_EQUAL( success(), delete_finalizer_key(producerv_name, finalizer_key_1) );

   // Wait for two rounds of producer schedule so defproducera is replaced by defproducerv
   // because defproducera does not have an active finalizer key
   produce_blocks(504);

   // find new last_finkey_ids
   auto last_finkey_ids_3 = get_last_prop_fin_ids();
   BOOST_REQUIRE_EQUAL( 21, last_finkey_ids_3.size() );
   // Make sure producerw_id is in the new last_finkey_ids
   BOOST_REQUIRE_EQUAL( true, last_finkey_ids_3.contains(producerw_id) );

   // After replace producerv_id wth producerw_id in the old last_finkey_ids_2,
   // last_finkey_ids should be the same as last_finkey_ids_3
   last_finkey_ids_2.erase(producerv_id);
   last_finkey_ids_2.insert(producerw_id);
   BOOST_REQUIRE_EQUAL( true, last_finkey_ids_2 == last_finkey_ids_3 );
}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
