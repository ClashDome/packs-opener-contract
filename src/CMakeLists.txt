project(packsopener)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( packsopener packsopener packsopener.cpp )
target_include_directories( packsopener PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( packsopener ${CMAKE_SOURCE_DIR}/../ricardian )