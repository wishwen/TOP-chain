// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xtxpool_v2/xtxpool_table.h"

#include "xbasic/xmodule_type.h"
#include "xtxpool_v2/xnon_ready_account.h"
#include "xtxpool_v2/xtxpool_error.h"
#include "xtxpool_v2/xtxpool_log.h"
#include "xverifier/xtx_verifier.h"
#include "xverifier/xverifier_utl.h"
#include "xverifier/xwhitelist_verifier.h"

namespace top {
namespace xtxpool_v2 {
int32_t xtxpool_table_t::push_send_tx(const std::shared_ptr<xtx_entry> & tx) {
    int32_t ret = verify_send_tx(tx->get_tx());
    if (ret != xsuccess) {
        return ret;
    }

    if (is_unconfirm_txs_reached_upper_limmit()) {
        xtxpool_warn("xtxpool_table_t::push_send_tx unconfirm txs reached upper limmit tx:%s", tx->get_tx()->dump().c_str());
        XMETRICS_COUNTER_INCREMENT("txpool_push_tx_send_fail_unconfirm_reached_limmit", 1);
        XMETRICS_COUNTER_INCREMENT("txpool_push_tx_send_fail", 1);
        return xtxpool_error_account_unconfirm_txs_reached_upper_limit;
    }

    auto account_addr = tx->get_tx()->get_source_addr();

    store::xaccount_basic_info_t account_basic_info;
    bool result = m_table_indexstore->get_account_basic_info(account_addr, account_basic_info);
    if (!result) {
        // todo : push to non_ready_accounts
        xtxpool_warn("xtxpool_table_t::push_send_tx account state fall behind tx:%s", tx->get_tx()->dump().c_str());
        return xtxpool_error_account_state_fall_behind;
    }

    if (data::is_sys_contract_address(common::xaccount_address_t{tx->get_tx()->get_source_addr()})) {
        tx->get_para().set_tx_type_score(enum_xtx_type_socre_system);
    } else {
        tx->get_para().set_tx_type_score(enum_xtx_type_socre_normal);
    }

    uint64_t latest_nonce = account_basic_info.get_latest_state()->account_send_trans_number();
    uint256_t latest_hash = account_basic_info.get_latest_state()->account_send_trans_hash();
    {
        std::lock_guard<std::mutex> lck(m_mgr_mutex);
        if (m_locked_txs.try_push_tx(tx)) {
            return xsuccess;
        }
        ret = m_txmgr_table.push_send_tx(tx, latest_nonce, latest_hash);
    }
    if (ret != xsuccess) {
        XMETRICS_COUNTER_INCREMENT("txpool_push_tx_send_fail", 1);
    }
    return ret;
}

int32_t xtxpool_table_t::push_receipt(const std::shared_ptr<xtx_entry> & tx) {
    int32_t ret = verify_receipt_tx(tx->get_tx());
    if (ret != xsuccess) {
        return ret;
    }

    auto & account_addr = tx->get_tx()->get_account_addr();

    store::xaccount_basic_info_t account_basic_info;
    bool result = m_table_indexstore->get_account_basic_info(account_addr, account_basic_info);
    if (!result) {
        // todo : push to non_ready_accounts
        return xtxpool_error_account_state_fall_behind;
    }
    // auto latest_unit_block = m_para->get_vblockstore()->get_latest_committed_block(account_addr);

    bool deny = false;
    enum_xtxpool_error_type ret_r = reject(account_addr, tx->get_tx(), account_basic_info.get_latest_block()->get_height(), deny);
    if (deny) {
        XMETRICS_COUNTER_INCREMENT("txpool_push_tx_receipt_fail", 1);
        xtxpool_warn("xtxpool_table_t::push_receipt reject tx:%s,ret:%u", tx->get_tx()->dump(true).c_str(), ret_r);
        return xtxpool_error_tx_duplicate;
    }

    if (data::is_sys_contract_address(common::xaccount_address_t{tx->get_tx()->get_account_addr()})) {
        tx->get_para().set_tx_type_score(enum_xtx_type_socre_system);
    } else {
        tx->get_para().set_tx_type_score(enum_xtx_type_socre_normal);
    }

    {
        std::lock_guard<std::mutex> lck(m_mgr_mutex);
        ret = m_txmgr_table.push_receipt(tx);
    }
    if (ret != xsuccess) {
        XMETRICS_COUNTER_INCREMENT("txpool_push_tx_receipt_fail", 1);
    }
    return ret;
}

std::shared_ptr<xtx_entry> xtxpool_table_t::pop_tx(const tx_info_t & txinfo, bool clear_follower) {
    std::lock_guard<std::mutex> lck(m_mgr_mutex);
    bool exist = false;
    auto tx_ent = m_locked_txs.pop_tx(txinfo, exist);
    if (exist) {
        return tx_ent;
    }
    return m_txmgr_table.pop_tx(txinfo, clear_follower);
}

ready_accounts_t xtxpool_table_t::pop_ready_accounts(uint32_t count) {
    std::lock_guard<std::mutex> lck(m_mgr_mutex);
    return m_txmgr_table.pop_ready_accounts(count);
}

ready_accounts_t xtxpool_table_t::get_ready_accounts(uint32_t count) {
    std::lock_guard<std::mutex> lck(m_mgr_mutex);
    return m_txmgr_table.get_ready_accounts(count);
}

const std::shared_ptr<xtx_entry> xtxpool_table_t::query_tx(const std::string & account, const uint256_t & hash) {
    std::lock_guard<std::mutex> lck(m_mgr_mutex);
    return m_txmgr_table.query_tx(account, hash);
}

void xtxpool_table_t::updata_latest_nonce(const std::string & account_addr, uint64_t latest_nonce, const uint256_t & latest_hash) {
    std::lock_guard<std::mutex> lck(m_mgr_mutex);
    return m_txmgr_table.updata_latest_nonce(account_addr, latest_nonce, latest_hash);
}

bool xtxpool_table_t::is_account_need_update(const std::string & account_addr) const {
    std::lock_guard<std::mutex> lck(m_mgr_mutex);
    return m_txmgr_table.is_account_need_update(account_addr);
}

enum_xtxpool_error_type xtxpool_table_t::reject(const std::string & account_addr, const xcons_transaction_ptr_t & tx, uint64_t pre_unitblock_height, bool & deny) {
    enum_xtxpool_error_type reject_ret = xtxpool_success;
    {
        std::lock_guard<std::mutex> lck(m_filter_mutex);
        reject_ret = m_table_filter.reject(account_addr, tx, pre_unitblock_height, deny);
    }

    if (reject_ret != xtxpool_success || deny) {
        xtxpool_info("xtxpool_table_t::reject reject tx:%s,ret:%u,deny:%d", tx->dump(true).c_str(), reject_ret, deny);
    }
    return reject_ret;
}

xcons_transaction_ptr_t xtxpool_table_t::get_unconfirm_tx(const std::string & account_addr, const uint256_t & hash) {
    xcons_transaction_ptr_t unconfirm_tx = nullptr;
    std::string hash_str = std::string(reinterpret_cast<char *>(hash.data()), hash.size());
    {
        std::lock_guard<std::mutex> lck(m_filter_mutex);
        unconfirm_tx = m_table_filter.get_tx(account_addr, hash_str);
    }

    if (unconfirm_tx == nullptr) {
        auto latest_unit_block = m_para->get_vblockstore()->get_latest_committed_block(account_addr);
        if (latest_unit_block != nullptr) {
            std::lock_guard<std::mutex> lck(m_filter_mutex);
            m_table_filter.update_reject_rule(account_addr, dynamic_cast<xblock_t *>(latest_unit_block.get()));
            unconfirm_tx = m_table_filter.get_tx(account_addr, hash_str);
        }
    }
    return unconfirm_tx;
}

const std::vector<xcons_transaction_ptr_t> xtxpool_table_t::get_resend_txs(uint64_t now) {
    std::lock_guard<std::mutex> lck(m_filter_mutex);
    return m_table_filter.get_resend_txs(now);
}

bool xtxpool_table_t::is_unconfirm_txs_reached_upper_limmit() const {
    std::lock_guard<std::mutex> lck(m_filter_mutex);
    uint32_t num = m_table_filter.get_unconfirm_txs_num();
    XMETRICS_COUNTER_SET("table_unconfirm_tx" + m_xtable_info.get_table_addr(), num);
    xtxpool_warn("xtxpool_table_t::is_unconfirm_txs_reached_upper_limmit table:%s,num:%u,max:%u", m_xtable_info.get_table_addr().c_str(), num, table_unconfirm_txs_num_max);
    return num >= table_unconfirm_txs_num_max;
}

void xtxpool_table_t::on_block_confirmed(xblock_t * block) {
    update_reject_rule(block->get_account(), block);

    if (block->is_lightunit() && !block->is_genesis_block()) {
        data::xlightunit_block_t * lightunit = dynamic_cast<data::xlightunit_block_t *>(block);
        const std::vector<xlightunit_tx_info_ptr_t> & txs = lightunit->get_txs();
        for (auto & tx : txs) {
            tx_info_t txinfo(block->get_account(), tx->get_tx_hash_256(), tx->get_tx_subtype());
            pop_tx(txinfo, false);
        }
    }

    if (is_account_need_update(block->get_account())) {
        base::xauto_ptr<xblockchain2_t> blockchain(m_para->get_store()->clone_account(block->get_account()));
        xassert(blockchain != nullptr);
        updata_latest_nonce(block->get_account(), blockchain->account_send_trans_number(), blockchain->account_send_trans_hash());
    }
}

int32_t xtxpool_table_t::verify_txs(const std::string & account, const std::vector<xcons_transaction_ptr_t> & txs, uint64_t latest_commit_unit_height) {
    bool deny = false;
    for (auto it : txs) {
        int32_t ret = verify_cons_tx(it);
        if (ret != xsuccess) {
            xtxpool_warn("xtxpool_table_t::verify_txs verify fail,tx:%s,err:%u", it->dump(true).c_str(), ret);
            return ret;
        }
        enum_xtxpool_error_type reject_ret = reject(account, it, latest_commit_unit_height, deny);
        reject_ret = deny ? xtxpool_error_tx_duplicate : reject_ret;
        if (reject_ret != xsuccess) {
            xtxpool_warn("xtxpool_table_t::verify_txs reject tx:%s,ret:%u,deny:%d", it->dump(true).c_str(), reject_ret, deny);
            return reject_ret;
        }
    }
    return xsuccess;
}

void xtxpool_table_t::update_unconfirm_accounts() {
    base::xauto_ptr<xblockchain2_t> blockchain{m_para->get_store()->clone_account(m_xtable_info.get_table_addr())};
    if (blockchain != nullptr) {
        std::set<std::string> accounts = blockchain->get_unconfirmed_accounts();
        if (!accounts.empty()) {
            xtxpool_dbg("xtxpool_table_t::update_unconfirm_accounts unconfirmed accounts not empty size:%u", accounts.size());

            for (auto & account : accounts) {
                base::xauto_ptr<base::xvblock_t> unitblock = m_para->get_vblockstore()->get_latest_committed_block(account);
                if (unitblock != nullptr) {
                    xblock_t * block = dynamic_cast<xblock_t *>(unitblock.get());
                    update_reject_rule(account, block);
                }
            }
        }
    }
}

void xtxpool_table_t::update_locked_txs(const std::vector<tx_info_t> & locked_tx_vec) {
    std::vector<std::shared_ptr<xtx_entry>> unlocked_txs;
    int32_t ret = xsuccess;
    locked_tx_map_t locked_tx_map;
    {
        std::lock_guard<std::mutex> lck(m_mgr_mutex);
        for (auto & txinfo : locked_tx_vec) {
            auto tx_ent = m_txmgr_table.pop_tx(txinfo, false);
            std::shared_ptr<locked_tx_t> locked_tx = std::make_shared<locked_tx_t>(txinfo, tx_ent);
            locked_tx_map[txinfo.get_hash_str()] = locked_tx;
        }
        m_locked_txs.update(locked_tx_map, unlocked_txs);
    }

    // roll back txs push to txpool again.
    for (auto & unlocked_tx : unlocked_txs) {
        if (unlocked_tx->get_tx()->is_self_tx() || unlocked_tx->get_tx()->is_send_tx()) {
            store::xaccount_basic_info_t account_basic_info;
            bool result = m_table_indexstore->get_account_basic_info(unlocked_tx->get_tx()->get_account_addr(), account_basic_info);
            if (!result) {
                // todo : push to non_ready_accounts
                xtxpool_warn("xtxpool_table_t::update_locked_txs account state fall behind tx:%s", unlocked_tx->get_tx()->dump().c_str());
                continue;
            }
            uint64_t latest_nonce = account_basic_info.get_latest_state()->account_send_trans_number();
            uint256_t latest_hash = account_basic_info.get_latest_state()->account_send_trans_hash();
            std::lock_guard<std::mutex> lck(m_mgr_mutex);
            ret = m_txmgr_table.push_send_tx(unlocked_tx, latest_nonce, latest_hash);
        } else {
            std::lock_guard<std::mutex> lck(m_mgr_mutex);
            ret = m_txmgr_table.push_receipt(unlocked_tx);
        }
        xtxpool_info("xtxpool_table_t::update_locked_txs roll back to txmgr table tx:%s,ret:%d", unlocked_tx->get_tx()->dump().c_str(), ret);
    }
}

enum_xtxpool_error_type xtxpool_table_t::update_reject_rule(const std::string & account, const data::xblock_t * unit_block) {
    std::lock_guard<std::mutex> lck(m_filter_mutex);
    return m_table_filter.update_reject_rule(account, unit_block);
}

int32_t xtxpool_table_t::verify_cons_tx(const xcons_transaction_ptr_t & tx) const {
    int32_t ret;
    if (tx->is_send_tx() || tx->is_self_tx()) {
        ret = verify_send_tx(tx);
    } else if (tx->is_recv_tx()) {
        ret = verify_receipt_tx(tx);
    } else if (tx->is_confirm_tx()) {
        ret = verify_receipt_tx(tx);
    } else {
        ret = xtxpool_error_tx_invalid_type;
    }
    return ret;
}

int32_t xtxpool_table_t::verify_send_tx(const xcons_transaction_ptr_t & tx) const {
    // 1. validation check
    int32_t ret = xverifier::xtx_verifier::verify_send_tx_validation(tx->get_transaction());
    if (ret) {
        return ret;
    }
    // 2. legal check, include hash/signature check and white/black check
    ret = xverifier::xtx_verifier::verify_send_tx_legitimacy(tx->get_transaction(), make_observer(m_para->get_store()));
    if (ret) {
        return ret;
    }
    // 3. tx duration expire check
    uint64_t now = xverifier::xtx_utl::get_gmttime_s();
    ret = xverifier::xtx_verifier::verify_tx_duration_expiration(tx->get_transaction(), now);
    if (ret) {
        return ret;
    }
    return xsuccess;
}

int32_t xtxpool_table_t::verify_receipt_tx(const xcons_transaction_ptr_t & tx) const {
    // only check digest here for process too long zec_workload contract transaction receipt
    // should recover length check at later version
    if (!tx->get_transaction()->digest_check()) {
        xtxpool_warn("xtxpool_table_t::verify_receipt_tx digest check fail, tx:%s", tx->dump(true).c_str());
        return xtxpool_error_receipt_invalid;
    }

    if (!tx->verify_cons_transaction()) {
        return xtxpool_error_receipt_invalid;
    }

    base::xvqcert_t * prove_cert;
    std::string prove_account;
    if (!tx->get_commit_prove_cert_and_account(prove_cert, prove_account)) {
        return xtxpool_error_tx_multi_sign_error;
    }

    base::enum_vcert_auth_result auth_result = m_para->get_certauth()->verify_muti_sign(prove_cert, prove_account);
    if (auth_result != base::enum_vcert_auth_result::enum_successful) {
        int32_t ret = xtxpool_error_tx_multi_sign_error;
        xtxpool_warn("xtxpool_table_t::verify_receipt_tx fail. account=%s,tx=%s,auth_result:%d,fail-%u", prove_account.c_str(), tx->dump(true).c_str(), auth_result, ret);
        return ret;
    }
    return xsuccess;
}

}  // namespace xtxpool_v2
}  // namespace top
