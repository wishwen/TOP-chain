// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbasic/xmemory.hpp"
#include "xdata/xcons_transaction.h"
#include "xtxpool_v2/xtx_table_filter.h"
#include "xtxpool_v2/xtxmgr_table.h"
#include "xtxpool_v2/xtxpool_info.h"
#include "xtxpool_v2/xtxpool_resources_face.h"

#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace top {
namespace xtxpool_v2 {

#define table_unconfirm_txs_num_max (1024)

class xtxpool_table_t {
public:
    xtxpool_table_t(xtxpool_resources_face * para, std::string table_addr, xtxpool_shard_info_t * shard)
      : m_para(para)
      , m_xtable_info(table_addr, shard)
      , m_txmgr_table(&m_xtable_info)
      , m_table_filter(para->get_vblockstore())
      , m_table_indexstore(m_para->get_indexstorehub()->get_index_store(m_xtable_info.get_table_addr()))
      , m_locked_txs(&m_xtable_info) {
    }
    int32_t push_send_tx(const std::shared_ptr<xtx_entry> & tx);
    int32_t push_receipt(const std::shared_ptr<xtx_entry> & tx);
    std::shared_ptr<xtx_entry> pop_tx(const tx_info_t & txinfo, bool clear_follower);
    ready_accounts_t pop_ready_accounts(uint32_t count);
    ready_accounts_t get_ready_accounts(uint32_t count);
    const std::shared_ptr<xtx_entry> query_tx(const std::string & account, const uint256_t & hash);
    void updata_latest_nonce(const std::string & account_addr, uint64_t latest_nonce, const uint256_t & latest_hash);
    enum_xtxpool_error_type reject(const std::string & account, const xcons_transaction_ptr_t & tx, uint64_t pre_unitblock_height, bool & deny);
    xcons_transaction_ptr_t get_unconfirm_tx(const std::string & account, const uint256_t & hash);
    const std::vector<xcons_transaction_ptr_t> get_resend_txs(uint64_t now);
    void on_block_confirmed(xblock_t * block);
    int32_t verify_txs(const std::string & account, const std::vector<xcons_transaction_ptr_t> & txs, uint64_t latest_commit_unit_height);
    void update_unconfirm_accounts();
    void update_locked_txs(const std::vector<tx_info_t> & locked_tx_vec);

private:
    enum_xtxpool_error_type update_reject_rule(const std::string & account, const data::xblock_t * unit_block);
    bool is_account_need_update(const std::string & account_addr) const;
    bool is_unconfirm_txs_reached_upper_limmit() const;
    int32_t verify_tx_common(const xcons_transaction_ptr_t & tx) const;
    int32_t verify_send_tx(const xcons_transaction_ptr_t & tx) const;
    int32_t verify_receipt_tx(const xcons_transaction_ptr_t & tx) const;
    int32_t verify_cons_tx(const xcons_transaction_ptr_t & tx) const;
    xtxpool_resources_face * m_para;
    xtxpool_table_info_t m_xtable_info;
    xtxmgr_table_t m_txmgr_table;
    xtx_table_filter m_table_filter;
    store::xindexstore_face_ptr_t m_table_indexstore;
    xlocked_txs_t m_locked_txs;
    mutable std::mutex m_mgr_mutex;
    mutable std::mutex m_filter_mutex;
};

}  // namespace xtxpool_v2
}  // namespace top
