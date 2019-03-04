cmake_minimum_required(VERSION 3.5)
project(hello_world VERSION 1.0.0)

find_package(eosio.cdt)

### Generate the wasm and abi
add_contract(cron cron cron.cpp )

### add the path to where the ricardian contracts/clauses are found
target_compile_options( cron.wasm PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR} )
