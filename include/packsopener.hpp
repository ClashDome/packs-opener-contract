#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/transaction.hpp>
#include <atomicassets.hpp>
#include <atomicdata.hpp>

using namespace eosio;
using namespace std;

#define ATOMIC name("atomicassets")
#define EOSIO name("eosio")

#define CONTRACTN name("packsopenerx")

CONTRACT packsopener : public contract
{
public:
   using contract::contract;

    ACTION createpack(
        name authorized_account,
        name collection_name,
        uint32_t unlock_time,
        int32_t pack_template_id,
        string display_data
    );

    ACTION retryrand(
        uint64_t pack_asset_id
    );

    ACTION receiverand(
        uint64_t assoc_id,
        checksum256 random_value
    );

    ACTION claimunboxed(
        uint64_t pack_asset_id
    );

    // TODO: esto hay que hacerlo de otra forma, una vez se tienen todos los assets se crean todos los sobres disponibles
    ACTION addpack(
        uint64_t pack_id,
        vector<uint64_t> assets_ids
    );

    ACTION genpacks(
        name authorized_account,
        uint64_t pack_template_id
    );

    ACTION removeall(
        string table
    );

    ACTION lognewpack(
        uint64_t pack_id,
        name collection_name,
        uint32_t unlock_time
    );

    ACTION loggetrand(
        uint64_t assoc_id,
        uint64_t max_value,
        uint64_t rand_value,
        vector<uint64_t> vec
    );

    ACTION loggenpacks(
        name authorized_account,
        uint64_t pack_template_id,
        vector<uint64_t> vec
    );

   [[eosio::on_notify("atomicassets::transfer")]] void receive_asset_transfer(
        name from,
        name to,
        vector <uint64_t> asset_ids,
        string memo
    );

private:

    TABLE packs_s {
        uint64_t pack_id;
        name     collection_name;
        uint32_t unlock_time;
        int32_t  pack_template_id;
        string   display_data;

        uint64_t primary_key() const { return pack_id; }
        uint64_t by_template_id() const { return (uint64_t) pack_template_id; };
    };

    typedef multi_index<name("packs"), packs_s,
        indexed_by < name("templateid"), const_mem_fun < packs_s, uint64_t, &packs_s::by_template_id>>>
    packs_t;

    TABLE unboxpacks_s {
        uint64_t            pack_asset_id;
        uint64_t            pack_id;
        name                unboxer;
        vector<uint64_t>    assets_ids;

        uint64_t primary_key() const { return pack_asset_id; }
        uint64_t by_unboxer() const { return unboxer.value; }
    };

    typedef multi_index<name("unboxpacks"), unboxpacks_s,
        indexed_by < name("unboxer"), const_mem_fun < unboxpacks_s, uint64_t, &unboxpacks_s::by_unboxer>>>
    unboxpacks_t;

    TABLE availpacks_s {
        uint64_t            id;
        uint64_t            pack_id;
        vector<uint64_t>    assets_ids;

        uint64_t primary_key() const { return id; }
        uint64_t by_pack_id() const { return (uint64_t) pack_id; };
    };

    typedef multi_index<name("availpacks"), availpacks_s,
        indexed_by < name("packid"), const_mem_fun < availpacks_s, uint64_t, &availpacks_s::by_pack_id>>>
    availpacks_t;

    packs_t             packs           = packs_t(get_self(), get_self().value);
    unboxpacks_t        unboxpacks      = unboxpacks_t(get_self(), get_self().value);
    availpacks_t        availpacks      = availpacks_t(get_self(), get_self().value);

    void check_has_collection_auth(name account_to_check, name collection_name);

};