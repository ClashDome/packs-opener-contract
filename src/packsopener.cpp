#include <packsopener.hpp>

/**
* Funcion from atomicpacks contract
*
* Create a new pack
* The possible outcomes packed in rolls must be provided afterwards with the addpackroll action
* 
* @required_auth authorized_account, who must be authorized within the specfied collection
*/
ACTION packsopener::createpack(
    name authorized_account,
    name collection_name,
    uint32_t unlock_time,
    int32_t pack_template_id,
    string display_data
) {
    require_auth(authorized_account);

    check_has_collection_auth(authorized_account, collection_name);
    check_has_collection_auth(get_self(), collection_name);

    uint64_t pack_id = packs.available_primary_key();
    if (pack_id == 0) {
        pack_id = 1;
    }
    
    packs.emplace(authorized_account, [&](auto &_pack) {
        _pack.pack_id = pack_id;
        _pack.collection_name = collection_name;
        _pack.unlock_time = unlock_time;
        _pack.pack_template_id = pack_template_id;
        _pack.display_data = display_data;
    });

    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("lognewpack"),
        std::make_tuple(
            pack_id,
            collection_name,
            unlock_time
        )
    ).send();
}

/**
* Funcion from atomicpacks contract
*
* This action is called by the rng oracle and provides the randomness for unboxing a pack
* The assoc id is equal to the asset id of the pack that is being unboxed
* 
* The unboxed assets are not immediately minted but instead placed in the unboxassets table with
* the scope <asset id of the pack that is being unboxed> and need to be claimed using the claimunboxed action
* This functionality is split in an effort to prevent transaction timeouts
* 
* @required_auth rng oracle account
*/
ACTION packsopener::receiverand(
    uint64_t assoc_id,
    checksum256 random_value
) {

    require_auth(name("orng.wax"));
    // require_auth(get_self());

    // get all available packs 
    auto unboxpack_itr = unboxpacks.find(assoc_id);

    auto idx = availpacks.get_index<"packid"_n>();

    auto itr = idx.lower_bound(unboxpack_itr->pack_id);

    vector<uint64_t> availables;

    while (itr != idx.end() && itr->pack_id == unboxpack_itr->pack_id) {
        availables.push_back(itr->id);
        itr++;
    } 

    //cast the random_value to a smaller number
    uint64_t max_value = availables.size() - 1;
    uint64_t final_random_value = 0;

    if (max_value > 0) {

        auto byte_array = random_value.extract_as_byte_array();

        uint64_t random_int = 0;
        for (int i = 0; i < 8; i++) {
            random_int <<= 8;
            random_int |= (uint64_t)byte_array[i];
        }

        final_random_value = random_int % max_value;   
    }

    uint64_t selected_pack = availables[final_random_value];

    auto available_itr = availpacks.find(selected_pack);

    unboxpacks.modify(unboxpack_itr, get_self(), [&](auto &_pack) {
        _pack.assets_ids = available_itr->assets_ids;
    });

    availpacks.erase(available_itr);

    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("loggetrand"),
        std::make_tuple(
            assoc_id,
            max_value,
            final_random_value,
            available_itr->assets_ids
        )
    ).send();

    // burn the pack
    action(
        permission_level{get_self(), name("active")},
        atomicassets::ATOMICASSETS_ACCOUNT,
        name("burnasset"),
        std::make_tuple(
            get_self(),
            assoc_id
        )
    ).send();
}

ACTION packsopener::addpack(
    uint64_t pack_id,
    vector<uint64_t> assets_ids
) {
    require_auth(get_self());

    uint64_t id = availpacks.available_primary_key();
    if (id == 0) {
        id = 1;
    }

    availpacks.emplace(get_self(), [&](auto &_availpack) {
        _availpack.id = id;
        _availpack.pack_id = pack_id;
        _availpack.assets_ids = assets_ids;
    });
}

ACTION packsopener::genpacks(
    name authorized_account,
    uint64_t pack_template_id
) {

    require_auth(authorized_account);

    auto idx = packs.get_index<"templateid"_n>();

    auto itr = idx.require_find(pack_template_id, 
        "No pack with this template exists");
    
    check_has_collection_auth(authorized_account, itr->collection_name);
    check_has_collection_auth(get_self(), itr->collection_name);

    atomicassets::assets_t own_assets = atomicassets::get_assets(get_self());

    auto assets_itr = own_assets.begin();

    while(assets_itr != own_assets.end()) {

        if (assets_itr->collection_name == itr->collection_name) {

            uint64_t id = availpacks.available_primary_key();
            if (id == 0) {
                id = 1;
            }

            vector<uint64_t> vec;
            vec.push_back(assets_itr->asset_id);

            availpacks.emplace(get_self(), [&](auto &_availpack) {
                _availpack.id = id;
                _availpack.pack_id = itr->pack_id;
                _availpack.assets_ids = vec;
            });
        }
        
        assets_itr ++;
    }
}

ACTION packsopener::claimunboxed(
    uint64_t pack_asset_id
) {

    auto unboxpack_itr = unboxpacks.require_find(pack_asset_id,
        "No unboxpack with this pack asset id exists");

    check(has_auth(unboxpack_itr->unboxer) || has_auth(get_self()),
        "The transaction needs to be authorized either by the unboxer or by the contract itself");

    action(
        permission_level{get_self(), name("active")},
        atomicassets::ATOMICASSETS_ACCOUNT,
        name("transfer"),
        std::make_tuple(
            get_self(),
            unboxpack_itr->unboxer,
            unboxpack_itr->assets_ids,
            "claim unbox pack " + to_string(pack_asset_id)
        )
    ).send();

    unboxpacks.erase(unboxpack_itr);
}

ACTION packsopener::removeall(
    string table
) {

    require_auth(get_self());

    if (table == "packs") {
        auto it = packs.begin();
        while (it != packs.end()) {
            it = packs.erase(it);
        }
    } else if (table == "unboxpacks") {
        auto it = unboxpacks.begin();
        while (it != unboxpacks.end()) {
            it = unboxpacks.erase(it);
        }
    } else if (table == "availpacks") {
        auto it = availpacks.begin();
        while (it != availpacks.end()) {
            it = availpacks.erase(it);
        }
    }
}

ACTION packsopener::lognewpack(
    uint64_t pack_id,
    name collection_name,
    uint32_t unlock_time
) {
    require_auth(get_self());
}

ACTION packsopener::loggetrand(
    uint64_t assoc_id,
    uint64_t max_value,
    uint64_t rand_value,
    vector<uint64_t> vec
) {
    require_auth(get_self());
}

ACTION packsopener::loggenpacks(
    name authorized_account,
    uint64_t pack_template_id,
    vector<uint64_t> vec
) {
    require_auth(get_self());
}

/**
* Funcion from atomicpacks contract
*
* This function is called when AtomicAssets assets are transferred to the pack contract

* This is used to unbox packs, by transferring exactly one pack to the pack contract
* The pack asset is then burned and the rng oracle is called to request a random value
*/
void packsopener::receive_asset_transfer(
    name from,
    name to,
    vector <uint64_t> asset_ids,
    string memo
) {
    if (to != get_self()) {
        return;
    }

    if (memo == "unbox") {

        check(asset_ids.size() == 1, "Only one pack can be opened at a time");

        atomicassets::assets_t own_assets = atomicassets::get_assets(get_self());
        auto asset_itr = own_assets.find(asset_ids[0]);

        check(asset_itr->template_id != -1, "The transferred asset does not belong to a template");
        
        auto packs_by_template_id = packs.get_index<name("templateid")>();
        auto pack_itr = packs_by_template_id.require_find(asset_itr->template_id,
            "The transferred asset's template does not belong to any pack");
        
        check(pack_itr->unlock_time <= current_time_point().sec_since_epoch(), "The pack has not unlocked yet");

        //Get signing value from transaction id
        //As this is only used as the signing value for the randomness oracle, it does not matter that this
        //signing value is not truly random

        auto size = transaction_size();
        char buf[size];

        auto read = read_transaction(buf, size);
        check(size == read, "read_transaction() has failed.");

        checksum256 tx_id = eosio::sha256(buf, read);

        uint64_t signing_value;

        memcpy(&signing_value, tx_id.data(), sizeof(signing_value));

        //It is not necessary to check the necessary RAM because the content of the packs is pre-mined

        unboxpacks.emplace(get_self(), [&](auto &_unboxpack) {
            _unboxpack.pack_asset_id = asset_ids[0];
            _unboxpack.pack_id = pack_itr->pack_id;
            _unboxpack.unboxer = from;
        });

        action(
            permission_level{get_self(), name("active")},
            name("orng.wax"),
            name("requestrand"),
            std::make_tuple(
                asset_ids[0], //used as assoc id
                signing_value,
                get_self()
            )
        ).send();
    } else {
        check(memo == "transfer", "Invalid memo");
    }
}

/**
* Checks if the account_to_check is in the authorized_accounts vector of the specified collection
*/
void packsopener::check_has_collection_auth(
    name account_to_check,
    name collection_name
) {
    auto collection_itr = atomicassets::collections.require_find(collection_name.value,
        "No collection with this name exists");

    check(std::find(
        collection_itr->authorized_accounts.begin(),
        collection_itr->authorized_accounts.end(),
        account_to_check
        ) != collection_itr->authorized_accounts.end(),
        "The account " + account_to_check.to_string() + " is not authorized within the collection");
}