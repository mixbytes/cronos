# Cronos

The ultimate cron on EOS blockchain

## Compile

```
cd cron 
eosio-cpp -o cron.wasm cron.cpp --abigen
```

## Create account
```bash 
cleos wallet unlock
cleos create account eosio hello <password>
cleos set account permission hello active --add-code
```

## Deploy 
```cleos set contract hello /<workdir>/cron```

## Test
```bash
cleos push action hello run '["hello", 1]' -p hello@active
cleos push action hello schedule '["hello", "hello", "dumb", 3]' -p hello@active

$ docker logs -f nodeos

[(hello,run)->hello]: CONSOLE OUTPUT BEGIN =====================
Scanning timetable...
Scanning table
Processing 0
Creating transaction: payer=hello target=[hello:run] delay=1
Scheduled with a delay of 1

[(hello,run)->hello]: CONSOLE OUTPUT END   =====================
info  2019-02-25T15:25:33.001 thread-0  producer_plugin.cpp:1522      produce_block        ] Produced block 0000007342d6caa4... #115 @ 2019-02-25T15:25:33.000 signed by eosio [trxs: 1, lib: 114, confirmed: 0]
info  2019-02-25T15:25:33.502 thread-0  producer_plugin.cpp:1522      produce_block        ] Produced block 0000007499bf737f... #116 @ 2019-02-25T15:25:33.500 signed by eosio [trxs: 0, lib: 115, confirmed: 0]
debug 2019-02-25T15:25:33.504 thread-0  apply_context.cpp:28          print_debug          ] 
[(hello,dumb)->hello]: CONSOLE OUTPUT BEGIN =====================
THE DUMB ACTION CALLED by hello
[(hello,dumb)->hello]: CONSOLE OUTPUT END   =====================
debug 2019-02-25T15:25:33.516 thread-0  apply_context.cpp:28          print_debug          ] 
```

