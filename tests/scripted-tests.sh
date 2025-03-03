# Jungle `hokieshokies` owns safe and esafe 
# Mainnet `proposer.enf` owns safe and esafe 
TEST_USER=alicetest123
EXT_OWER="hokieshokies"


### Multi element hash ###
# create the hash
DNY_HASH=$(cleos push action eosio denyhashcalc '{"patterns":["safe","esafe"]}' -p $TEST_USER |  grep 'return value: ' | cut -d: -f2 | sed 's/[ "]//g')
# add the hash
cleos push action eosio denyhashadd "{\"hash\":\"${DNY_HASH}\"}" -p eosio
# fails partial pattern
cleos push action eosio  denynames '{"patterns":["safe"]}' -p $TEST_USER
# succeeds 
cleos push action eosio  denynames '{"patterns":["safe","esafe"]}' -p $TEST_USER


# This should fail 
USER_PUBLIC_KEY=$(grep Public ~/eosio-wallet/user.keys | head -1 | cut -d: -f2 | sed 's/ //g')
cleos system newaccount $EXT_OWER safe $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $EXT_OWER
cleos system newaccount $EXT_OWER esafe $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $EXT_OWER

# This should work 
cleos system newaccount $EXT_OWER rrr $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $EXT_OWER
cleos system newaccount $EXT_OWER hunkyokk.rrr $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $EXT_OWER


### Single Element Hash ###
# create the hash
DNY_HASH=$(cleos push action eosio denyhashcalc '{"patterns":["rrr"]}' -p $TEST_USER |  grep 'return value: ' | cut -d: -f2 | sed 's/[ "]//g')
# add the hash
cleos push action eosio denyhashadd "{\"hash\":\"${DNY_HASH}\"}" -p eosio
# fails partial pattern
cleos push action eosio  denynames '{"patterns":["rrr"]}' -p $TEST_USER

### Real World Test ###
# create account with banned name ok 
# This works 
cleos system newaccount $TEST_USER hellohello11 $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $TEST_USER
# This fails 
cleos system newaccount $TEST_USER rrrlohello11 $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $TEST_USER
# this should fail
cleos system newaccount $TEST_USER hunkyokk.rrr $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $TEST_USER
# this should work
cleos system newaccount $EXT_OWER hunkyokk.rrr $USER_PUBLIC_KEY  --stake-net "50 EOS" --stake-cpu "50 EOS" --buy-ram "100 EOS" -p $EXT_OWER
  
# This works
cleos system bidname $TEST_USER ugy "1 EOS"
# This Works
cleos system bidname $TEST_USER rrr "1 EOS"
