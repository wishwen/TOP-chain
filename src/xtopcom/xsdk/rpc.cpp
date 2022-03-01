#include <iostream>
#include <chrono>

#include "rpc.h"
#include "xcertauth/xcertauth_face.h"
#include "xdata/xblock.h"
#include "xdata/xnative_contract_address.h"
#include "xvledger/xvcertauth.h"

#include "xpbase/base/top_utils.h"
#include "xdata/xtransaction.h"
#include "xdata/xtx_factory.h"
#include "xcrypto/xckey.h"
#include "simplewebserver/client_http.hpp"

using namespace top;
using namespace top::data;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

int verify_raw_elect_block(char* buf, size_t buf_len) {
    std::cout << std::string(buf) << " " << buf_len << std::endl;
    top::base::xstream_t stream{top::base::xcontext_t::instance(), (uint8_t *)buf, static_cast<uint32_t>(buf_len)};
    top::data::xblock_t* block = dynamic_cast<top::data::xblock_t*>(top::data::xblock_t::full_block_read_from(stream));  // for block sync
    if (block == nullptr) {
        return -1;
    }
    std::cout << block->get_account() << std::endl;
    std::cout << block->get_height() << std::endl;

    // election infos, lack election::xvnode_house_t
    base::xvnodesrv_t* nodesrv = nullptr; 
    base::xvcertauth_t* ca = &auth::xauthcontext_t::instance(*nodesrv);
    ca->verify_muti_sign(block);

    // assume rec elect zec
    if (block->get_account() == sys_contract_rec_elect_zec_addr) {
        // xaccount_ptr_t xstore::query_account(const std::string &address)
        // state_store + block
    }

    // txs packed by block
    if (block->is_tableblock()) {
        auto tx_actions = block->get_tx_actions();
        for (auto action : tx_actions) {
            std::cout << "tx hash: " << data::to_hex_str(action.get_org_tx_hash()) << std::endl;
            std::string orgtx_bin = block->get_input()->query_resource(action.get_org_tx_hash());
            if (orgtx_bin.empty()) {
                return -1;
            }
            base::xauto_ptr<base::xdataunit_t> raw_tx = base::xdataunit_t::read_from(orgtx_bin);
            auto tx = dynamic_cast<xtransaction_t*>(raw_tx.get());

        }
    }

    return 0;
}

int send_top_tx(char *ip, unsigned len, unsigned long long nonce) {
    try {
        std::string sign_key = DecodePrivateString("KA7yth6xI2Nf58N38xtOEm9DucqSfzCemY7a3PXjWhY=");
        std::string sender = "T00000LNi53Ub726HcPXZfC4z6zLgTo5ks6GzTUp";
        std::string receiver = "T8000037d4fbc08bf4513a68a287ed218b0adbd497ef30";
        data::xtransaction_ptr_t tx = xtx_factory::create_tx(data::xtransaction_version_2);
        data::xproperty_asset asset(1000000);
        tx->make_tx_transfer(asset);
        tx->set_different_source_target_address(sender, receiver);
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        auto tmp=std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        std::time_t timestamp = tmp.count();
        tx->set_fire_timestamp(timestamp / 1000);
        tx->set_expire_duration(300);
        tx->set_deposit(100000);
        tx->set_last_nonce(nonce);
        tx->set_digest();

        utl::xecprikey_t pri_key_obj((uint8_t*)sign_key.data());
        utl::xecdsasig_t signature_obj = pri_key_obj.sign(tx->digest());
        auto signature = std::string(reinterpret_cast<char *>(signature_obj.get_compact_signature()), signature_obj.get_compact_signature_size());
        tx->set_authorization(signature);
        tx->set_len();

        std::string send_tx_request = "version=1.0&target_account_addr=" + sender + "&method=sendTransaction&sequence_id=3&token=";
        xJson::FastWriter writer;
        xJson::Value tx_json;
        tx->parse_to_json(tx_json["params"], data::RPC_VERSION_V2);
        tx_json["params"]["authorization"] = data::uint_to_str(tx->get_authorization().data(), tx->get_authorization().size());
        send_tx_request += "&body=" + SimpleWeb::Percent::encode(writer.write(tx_json));
        std::string host_ip(ip);
        HttpClient client(host_ip + ":19081");
        auto send_tx_response = client.request("POST", "/", send_tx_request);
        std::cout << host_ip << " " << send_tx_response << std::endl;
    } catch(const SimpleWeb::system_error &e) {
        std::cout << "Client request error: " << e.what() << std::endl;
    } catch (const std::exception &e) {
        // xerror("Client exception error: %s", e.what());
        std::cout << "Client exception error: " << e.what() << std::endl;
    }

    return 0;
}
