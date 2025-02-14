// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xtxpool_v2/xtxmgr_table.h"

#include "xtxpool_v2/xtxpool_error.h"
#include "xtxpool_v2/xtxpool_log.h"
#include "xverifier/xtx_verifier.h"
#include "xverifier/xverifier_utl.h"

namespace top {
namespace xtxpool_v2 {

using data::xcons_transaction_ptr_t;

#define send_txs_num_pop_from_queue_max (100)
#define receipts_num_pop_from_queue_max (200)
#define send_txs_num_pop_from_queue_batch_num (100)
#define receipts_num_pop_from_queue_batch_num (200)

int32_t xtxmgr_table_t::push_send_tx(const std::shared_ptr<xtx_entry> & tx, uint64_t latest_nonce, const uint256_t & latest_hash) {
    auto & account_addr = tx->get_tx()->get_transaction()->get_source_addr();
    updata_latest_nonce(account_addr, latest_nonce, latest_hash);

    if (nullptr != query_tx(account_addr, tx->get_tx()->get_transaction()->digest())) {
        xtxpool_warn("xtxmgr_table_t::push_send_tx tx repeat tx:%s", tx->get_tx()->dump().c_str());
        return xtxpool_error_request_tx_repeat;
    }

    int32_t ret = m_send_tx_queue.push_tx(tx, latest_nonce, latest_hash);
    if (ret != xsuccess) {
        xtxpool_warn("xtxmgr_table_t::push_tx fail.table %s,tx:%s,last nonce:%u,ret:%s",
                     m_xtable_info->get_table_addr().c_str(),
                     tx->get_tx()->dump(true).c_str(),
                     latest_nonce,
                     xtxpool_error_to_string(ret).c_str());
    } else {
        xtxpool_info("xtxmgr_table_t::push_tx success.table %s,tx:%s,last nonce:%u", m_xtable_info->get_table_addr().c_str(), tx->get_tx()->dump(true).c_str(), latest_nonce);
    }
    // pop tx from queue and push to pending here would bring about that queue is empty frequently,
    // thus the tx usually directly goes to pending without ordered in queue.
    // therefore, we should not do that without any conditions
    // queue_to_pending();
    return ret;
}

int32_t xtxmgr_table_t::push_receipt(const std::shared_ptr<xtx_entry> & tx) {
    auto & account_addr = tx->get_tx()->get_account_addr();
    if (nullptr != query_tx(account_addr, tx->get_tx()->get_transaction()->digest())) {
        xtxpool_warn("xtxmgr_table_t::push_receipt tx repeat tx:%s", tx->get_tx()->dump().c_str());
        return xtxpool_error_request_tx_repeat;
    }

    int32_t ret = m_receipt_queue.push_tx(tx);
    if (ret != xsuccess) {
        xtxpool_warn("xtxmgr_table_t::push_receipt fail.table %s,tx:%s,ret:%s",
                     m_xtable_info->get_table_addr().c_str(),
                     tx->get_tx()->dump(true).c_str(),
                     xtxpool_error_to_string(ret).c_str());
    } else {
        xtxpool_info("xtxmgr_table_t::push_receipt success.table %s,tx:%s", m_xtable_info->get_table_addr().c_str(), tx->get_tx()->dump(true).c_str());
    }
    return ret;
}

std::shared_ptr<xtx_entry> xtxmgr_table_t::pop_tx(const tx_info_t & txinfo, bool clear_follower) {
    // maybe m_tx_queue m_pending_accounts both contains the tx
    std::shared_ptr<xtx_entry> tx_ent = nullptr;
    if (txinfo.get_subtype() == enum_transaction_subtype_self || txinfo.get_subtype() == enum_transaction_subtype_send) {
        tx_ent = m_send_tx_queue.pop_tx(txinfo, clear_follower);
    } else {
        tx_ent = m_receipt_queue.pop_tx(txinfo);
    }
    if (tx_ent == nullptr) {
        tx_ent = m_pending_accounts.pop_tx(txinfo, clear_follower);
    }

    return tx_ent;
}

ready_accounts_t xtxmgr_table_t::pop_ready_accounts(uint32_t count) {
    queue_to_pending();
    ready_accounts_t accounts = m_pending_accounts.pop_ready_accounts(count);
    return accounts;
}

ready_accounts_t xtxmgr_table_t::get_ready_accounts(uint32_t count) {
    queue_to_pending();
    ready_accounts_t accounts = m_pending_accounts.get_ready_accounts(count);
    xtxpool_dbg("xtxmgr_table_t::get_ready_accounts table:%s,accounts size:%u", m_xtable_info->get_table_addr().c_str(), accounts.size());
    return accounts;
}

const std::shared_ptr<xtx_entry> xtxmgr_table_t::query_tx(const std::string & account_addr, const uint256_t & hash) const {
    auto tx = m_send_tx_queue.find(account_addr, hash);
    if (tx == nullptr) {
        tx = m_receipt_queue.find(account_addr, hash);
    }
    if (tx == nullptr) {
        tx = m_pending_accounts.find(account_addr, hash);
    }
    return tx;
}

void xtxmgr_table_t::updata_latest_nonce(const std::string & account_addr, uint64_t latest_nonce, const uint256_t & latest_hash) {
    xtxpool_info("xtxmgr_table_t::updata_latest_nonce.table %s,account:%s,last nonce:%u", m_xtable_info->get_table_addr().c_str(), account_addr.c_str(), latest_nonce);
    m_send_tx_queue.updata_latest_nonce(account_addr, latest_nonce, latest_hash);
    m_pending_accounts.updata_latest_nonce(account_addr, latest_nonce, latest_hash);
}

bool xtxmgr_table_t::is_account_need_update(const std::string & account_addr) const {
    return m_send_tx_queue.is_account_need_update(account_addr);
}

void xtxmgr_table_t::queue_to_pending() {
    std::vector<std::shared_ptr<xtx_entry>> expired_send_txs;
    std::vector<std::shared_ptr<xtx_entry>> push_succ_send_txs;
    std::vector<std::shared_ptr<xtx_entry>> push_succ_receipts;
    // Three steps:get----push----pop, for reduce the complexity, this process avoid pop in traverse of txs.
    std::vector<std::shared_ptr<xtx_entry>> receipts = m_receipt_queue.get_txs(receipts_num_pop_from_queue_max);
    uint32_t receipts_pos = 0;
    uint32_t receipts_pos_max = 0;
    std::vector<std::shared_ptr<xtx_entry>> send_txs = m_send_tx_queue.get_txs(send_txs_num_pop_from_queue_max);
    uint32_t send_txs_pos = 0;
    uint32_t send_txs_pos_max = 0;
    int32_t ret = 0;
    uint64_t now = xverifier::xtx_utl::get_gmttime_s();

    while ((receipts_pos_max < receipts_num_pop_from_queue_max || send_txs_pos_max < send_txs_num_pop_from_queue_max) &&
           (receipts_pos < receipts.size() || send_txs_pos < send_txs.size())) {
        receipts_pos_max += receipts_num_pop_from_queue_batch_num;
        send_txs_pos_max += send_txs_num_pop_from_queue_batch_num;

        // system send/self txs is first priority
        for (; send_txs_pos < send_txs.size() && send_txs[send_txs_pos]->get_para().get_charge_score() > enum_xtx_type_socre_normal; send_txs_pos++) {
            // check if send tx is expired.
            ret = xverifier::xtx_verifier::verify_tx_duration_expiration(send_txs[send_txs_pos]->get_tx()->get_transaction(), now);
            if (ret) {
                expired_send_txs.push_back(send_txs[send_txs_pos]);
                continue;
            }
            ret = m_pending_accounts.push_tx(send_txs[send_txs_pos]);
            xtxpool_dbg("xtxmgr_table_t::push_to_pending push tx to pending tx:%s, ret=%d", send_txs[send_txs_pos]->get_tx()->dump(true).c_str(), ret);
            if (ret == xsuccess) {
                push_succ_send_txs.push_back(send_txs[send_txs_pos]);
            }
        }

        // receipts are in preference to send txs.
        for (; receipts_pos < receipts_pos_max && receipts_pos < receipts.size(); receipts_pos++) {
            ret = m_pending_accounts.push_tx(receipts[receipts_pos]);
            xtxpool_dbg("xtxmgr_table_t::push_to_pending push tx to pending tx:%s, ret=%d", receipts[receipts_pos]->get_tx()->dump(true).c_str(), ret);
            if (ret == xsuccess) {
                push_succ_receipts.push_back(receipts[receipts_pos]);
            }
        }

        for (; send_txs_pos < send_txs_pos_max && send_txs_pos < send_txs.size(); send_txs_pos++) {
            // check if send tx is expired.
            ret = xverifier::xtx_verifier::verify_tx_duration_expiration(send_txs[send_txs_pos]->get_tx()->get_transaction(), now);
            if (ret) {
                expired_send_txs.push_back(send_txs[send_txs_pos]);
                continue;
            }
            ret = m_pending_accounts.push_tx(send_txs[send_txs_pos]);
            xtxpool_dbg("xtxmgr_table_t::push_to_pending push tx to pending tx:%s, ret=%d", send_txs[send_txs_pos]->get_tx()->dump(true).c_str(), ret);
            if (ret == xsuccess) {
                push_succ_send_txs.push_back(send_txs[send_txs_pos]);
            }
        }
    }

    for (auto tx_ent : push_succ_receipts) {
        tx_info_t txinfo(tx_ent->get_tx());
        m_receipt_queue.pop_tx(txinfo);
    }

    for (auto tx_ent : expired_send_txs) {
        tx_info_t txinfo(tx_ent->get_tx());
        m_send_tx_queue.pop_tx(txinfo, true);
    }

    for (auto tx_ent : push_succ_send_txs) {
        tx_info_t txinfo(tx_ent->get_tx());
        m_send_tx_queue.pop_tx(txinfo, false);
    }
}

}  // namespace xtxpool_v2
}  // namespace top
