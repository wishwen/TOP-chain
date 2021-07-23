// Copyright (c) 2017-2020 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include "xtxpool_service_v2/xcons_utl.h"
#include "xtxpool_service_v2/xtxpool_service_face.h"
#include "xdata/xgenesis_data.h"
#include "xtxpool_v2/xtxpool_error.h"

#include <signal.h>
#include <unistd.h>
#include <unordered_set>

#include <vector>

NS_BEG2(top, xtxpool_service_v2)

#define auto_tx_timer_interval_ms (1000)
#define auto_tx_create_account_number_once (50)
const uint32_t tx_send_interval = 60; // seconds

bool is_forever_leader(const xvip2_t & xip, const std::shared_ptr<vnetwork::xvnetwork_driver_face_t> & vnet_driver) {
    common::xnode_address_t node_addr = xcons_utl::to_address(xip, vnet_driver->address().version());
    auto type = node_addr.type();
    if (common::has<common::xnode_type_t::auditor>(type)) {
        uint16_t node_id = static_cast<std::uint16_t>(get_node_id_from_xip2(xip));
        if (node_id == 0) {
            return true;
        }
    }
    return false;
}

// default block service entry
class xauto_tx_mock : public top::base::xxtimer_t {
public:
    xauto_tx_mock(base::xcontext_t & _context, int32_t timer_thread_id, const observer_ptr<xtxpool_v2::xtxpool_face_t> & txpool, const observer_ptr<base::xvblockstore_t> & blockstore)
      : base::xxtimer_t(_context, timer_thread_id), m_txpool(txpool), m_blockstore(blockstore) {
        m_account_num = m_tps * tx_send_interval; // 
        // signal(SIGUSER1, sig_catcher);
    }

protected:
    ~xauto_tx_mock() override {
    }

    bool on_timer_fire(const int32_t thread_id, const int64_t timer_id, const int64_t current_time_ms, const int32_t start_timeout_ms, int32_t & in_out_cur_interval_ms) override {
        xdbg("xauto_tx_mock");
        if (m_account_num == 0) {
            reset_auto_tx_config();
        }

        if (m_account_key_vec.size() < m_account_num) {
            uint32_t need_create_num = m_account_num - m_account_key_vec.size();
            uint32_t create_account_num = (auto_tx_create_account_number_once < need_create_num) ? auto_tx_create_account_number_once : need_create_num;
            create_accounts(create_account_num);
        }

        if (m_cnt % tx_send_interval == 0) {
            if (m_account_key_ready_vec.size() < m_account_num) {
                for (auto it = m_account_key_vec.begin(); it != m_account_key_vec.end() && m_account_key_ready_vec.size() < m_account_num; it++) {
                    auto account_addr = it->first;
                    if (m_succ_accounts.find(account_addr) == m_succ_accounts.end() && check_account_create_succ(account_addr)) {
                        m_succ_accounts.insert(account_addr);
                        m_account_key_ready_vec.push_back(*it);
                    }
                }
            } else {
                uint32_t tx_num_to_send = m_tps * tx_send_interval;
                if (tx_num_to_send == 0) {
                    xdbg("xauto_tx_mock tx_num_to_send 0 to 1, m_tps:%u, auto_tx_timer_interval_ms:%u", m_tps, auto_tx_timer_interval_ms);
                    tx_num_to_send = 1;
                }
                make_and_push_origin_txs(tx_num_to_send);
            }
        }
        
        m_cnt++;

        return true;
    }

private:
    void sig_catcher(int sig) {
        xauto_tx_mock::reset_auto_tx_config();
        signal(sig, SIG_IGN);
    }

    void reset_auto_tx_config() {
        // std::string auto_tx_config_file = "/root/code/runtest_nathan/auto_tx.conf";
        // char config_str[1024];
        // std::ifstream is(auto_tx_config_file);
        // is.read(config_str, sizeof(config_str));
        // printf("config,%d:%s\n", is.gcount(), config_str);
        // is.close();

        // cJSON * pRoot = cJSON_CreateObject();
        // pRoot = cJSON_Parse(config_str);
        // m_account_num = cJSON_GetObjectItem(pRoot, "account_num")->valueint;
        // m_tps = cJSON_GetObjectItem(pRoot, "tps")->valueint;
        // cJSON_Delete(pRoot);
    }

    void create_accounts(uint32_t num) {
        xdbg("xauto_tx_mock create_accounts %u", num);
        uint32_t count = 0;
        while (count < num) {
            utl::xecprikey_t test_pri_key_obj;
            uint8_t addr_type = base::enum_vaccount_addr_type_secp256k1_eth_user_account;
            uint16_t ledger_id = base::xvaccount_t::make_ledger_id(base::enum_main_chain_id, base::enum_chain_zone_consensus_index);
            std::string account_address = test_pri_key_obj.to_account_address(addr_type, ledger_id);
            std::string from_table = data::account_address_to_block_address(common::xaccount_address_t{account_address});
            std::string target_table = std::string(sys_contract_sharding_table_block_addr) + "@0";
            xdbg("xauto_tx_mock create_accounts account_address: %s, from_table: %s, target_table: %s", account_address.c_str(), from_table.c_str(), target_table.c_str());
            if (from_table == target_table) {
                m_account_key_vec.push_back(std::make_pair(account_address, test_pri_key_obj));
                auto tx_ent = make_create_user_account_tx(account_address, test_pri_key_obj);
                xdbg("xauto_tx_mock create_accounts tx hash: %s,account:%s", base::xstring_utl::to_hex(tx_ent->get_tx()->get_tx_hash()).c_str(), account_address.c_str());
                m_txpool->push_send_tx(tx_ent);
                count++;
            }
        }
    }

    std::shared_ptr<xtxpool_v2::xtx_entry> make_create_user_account_tx(const std::string & account_addr, const utl::xecprikey_t & pri_key) {
        xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
        tx->make_tx_create_user_account(account_addr);
        tx->set_same_source_target_address(account_addr);
        tx->set_fire_and_expire_time(600);
        tx->set_deposit(100000);
        tx->set_digest();
        utl::xecdsasig_t signature_obj = pri_key.sign(tx->digest());
        auto signature = std::string(reinterpret_cast<char *>(signature_obj.get_compact_signature()), signature_obj.get_compact_signature_size());
        tx->set_signature(signature);
        tx->set_len();

        xcons_transaction_ptr_t cons_tx = make_object_ptr<xcons_transaction_t>(tx.get());
        xtxpool_v2::xtx_para_t para;
        return std::make_shared<xtxpool_v2::xtx_entry>(cons_tx, para);
    }

    bool check_account_create_succ(const std::string & account_addr) {
        base::xvaccount_t _account_vaddress(account_addr);
        base::xauto_ptr<base::xvblock_t> _block_ptr = m_blockstore->get_latest_cert_block(_account_vaddress);
        if (_block_ptr != nullptr && _block_ptr->get_height() >= 1) {
            xdbg("xauto_tx_mock check_account_create_succ %s", account_addr.c_str());
            return true;
        }
        xdbg("xauto_tx_mock check_account_create_fail %s", account_addr.c_str());
        return false;
    }

    std::shared_ptr<xtxpool_v2::xtx_entry> make_transfer_tx(uint32_t account_idx) {
        auto & account_addr = m_account_key_ready_vec[account_idx].first;
        auto & pri_key = m_account_key_ready_vec[account_idx].second;
        uint32_t target_account_idx = m_account_key_ready_vec.size() - 1 - account_idx;
        auto & target_account_addr = m_account_key_ready_vec[target_account_idx].first;

        xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
        data::xproperty_asset asset(1);
        tx->make_tx_transfer(asset);
        tx->set_different_source_target_address(account_addr, target_account_addr);
        uint64_t nonce = 1;
        // m_txpool->get_txpool_table_by_addr(account_addr)->get_account_latest_nonce(account_addr, nonce);
        tx->set_last_trans_hash_and_nonce({}, nonce);
        tx->set_fire_and_expire_time(600);
        tx->set_deposit(100000);
        tx->set_digest();
        utl::xecdsasig_t signature_obj = pri_key.sign(tx->digest());
        auto signature = std::string(reinterpret_cast<char *>(signature_obj.get_compact_signature()), signature_obj.get_compact_signature_size());
        tx->set_signature(signature);
        tx->set_len();

        xdbg("xauto_tx_mock transfer tx hash: %s,source:%s,target:%s,account_idx:%u,target_idx:%u", tx->get_digest_hex_str().c_str(), account_addr.c_str(), target_account_addr.c_str(), account_idx, target_account_idx);

        xcons_transaction_ptr_t cons_tx = make_object_ptr<xcons_transaction_t>(tx.get());
        xtxpool_v2::xtx_para_t para;
        return std::make_shared<xtxpool_v2::xtx_entry>(cons_tx, para);
    }

    void make_and_push_origin_txs(uint32_t tx_num) {
        xdbg("xauto_tx_mock make_and_push_origin_txs num: %u, m_next_tx_account_idx:%u, m_account_key_ready_vec:%zu", tx_num, m_next_tx_account_idx, m_account_key_ready_vec.size());
        for (uint32_t i = 0; i < tx_num; i++) {

            auto tx_ent = make_transfer_tx(m_next_tx_account_idx);
            auto ret = m_txpool->push_send_tx(tx_ent);
            if (ret == xtxpool_v2::xtxpool_error_tx_nonce_expired) {
                m_map_nonce[m_next_tx_account_idx]++;
                xdbg("xauto_tx_mock nonce:%u, idx:%u", m_map_nonce[m_next_tx_account_idx], m_next_tx_account_idx);
            }

            m_next_tx_account_idx = (m_next_tx_account_idx + 1) % m_account_key_ready_vec.size();
        }
    }

    observer_ptr<xtxpool_v2::xtxpool_face_t> m_txpool;
    observer_ptr<base::xvblockstore_t> m_blockstore;
    std::vector<std::pair<std::string, utl::xecprikey_t>> m_account_key_vec;
    std::vector<std::pair<std::string, utl::xecprikey_t>> m_account_key_ready_vec;
    std::unordered_set<std::string> m_succ_accounts;
    uint32_t m_account_num{120};
    uint32_t m_tps{2};
    uint32_t m_next_tx_account_idx{0};
    uint32_t m_nonce{0};
    std::unordered_map<uint32_t, uint32_t> m_map_nonce;
    uint32_t m_cnt{0};
};

NS_END2
