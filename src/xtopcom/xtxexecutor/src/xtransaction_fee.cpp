// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xtxexecutor/xtransaction_fee.h"
#include "xdata/xgenesis_data.h"
#include "xtxexecutor/xunit_service_error.h"
#include <algorithm>

using namespace top::data;

NS_BEG2(top, txexecutor)

int32_t xtransaction_fee_t::update_tgas_disk_sender(const uint64_t amount, bool is_contract) {
    xdbg("tgas_disk deposit: %d, balance: %d, amount: %d, is_contract: %d",
          m_trans->get_transaction()->get_deposit(), m_account_ctx->get_blockchain()->balance(), amount, is_contract);

    if (m_account_ctx->get_blockchain()->balance() < amount) {
        return xconsensus_service_error_balance_not_enough;
    }

    // deposit more than balance
    if (m_trans->get_transaction()->get_deposit() > (m_account_ctx->get_blockchain()->balance() - amount)) {
        return xtransaction_too_much_deposit;
    }

    if (!is_sys_contract_address(common::xaccount_address_t{ m_trans->get_source_addr() })) {
        if (m_trans->get_transaction()->get_deposit() < XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_tx_deposit)) {
            xdbg("not_enough_deposit, source addr: %s, target addr: %s, deposit: %d",
                m_trans->get_source_addr().c_str(), m_trans->get_target_addr().c_str(), m_trans->get_transaction()->get_deposit());
            return xtransaction_not_enough_deposit;
        }
    }
    uint64_t tgas_used_deposit{0};
    auto ret = update_tgas_sender(tgas_used_deposit, is_contract);
    if (ret != 0) {
        return ret;
    }
    uint64_t frozen_tgas = std::min((m_trans->get_transaction()->get_deposit() - tgas_used_deposit) / XGET_ONCHAIN_GOVERNANCE_PARAMETER(tx_deposit_gas_exchange_ratio),
                                     m_account_ctx->get_available_tgas() - m_account_ctx->get_blockchain()->lock_tgas());
    xdbg("tgas_disk sender frozen_tgas: %llu, available_tgas: %llu, lock_tgas: %llu",
          frozen_tgas, m_account_ctx->get_available_tgas(), m_account_ctx->get_blockchain()->lock_tgas());

    ret = update_disk(is_contract);
    m_trans->set_current_used_disk(get_disk_usage(is_contract));
    xdbg("tgas_disk m_used_disk %d, %s, %s", m_trans->get_current_used_disk(), m_trans->get_target_addr().c_str(), m_trans->get_digest_hex_str().c_str());
    if(ret != 0){
        return ret;
    }
    m_used_deposit = tgas_used_deposit;
    xdbg("tgas_disk sender, ret: %d, used_deposit: %d, amount: %d", ret, m_used_deposit, amount);

    if(is_contract){
        m_trans->set_current_send_tx_lock_tgas(frozen_tgas);
        m_account_ctx->set_lock_tgas_change(frozen_tgas);
        xdbg("tgas_disk tx hash: %s, frozen_tgas: %u", m_trans->get_digest_hex_str().c_str(), frozen_tgas);
    }
    m_account_ctx->set_lock_balance_change(m_trans->get_transaction()->get_deposit());
    m_account_ctx->set_balance_change(-static_cast<int64_t>(m_trans->get_transaction()->get_deposit()));
    xdbg("tgas_disk tx hash: %s, deposit: %u", m_trans->get_digest_hex_str().c_str(), m_trans->get_transaction()->get_deposit());

    return ret;
}

int32_t xtransaction_fee_t::update_tgas_sender(uint64_t& used_deposit, bool is_contract){
    uint64_t tgas_usage = get_tgas_usage(is_contract);
    auto ret = m_account_ctx->update_tgas_sender(tgas_usage, m_trans->get_transaction()->get_deposit(), used_deposit, is_contract);
    m_trans->set_current_used_tgas(tgas_usage - used_deposit / XGET_ONCHAIN_GOVERNANCE_PARAMETER(tx_deposit_gas_exchange_ratio));
    m_trans->set_current_used_deposit(used_deposit);
    xdbg("tgas_disk m_used_tgas %d, %d, %s, %s", m_trans->get_current_used_tgas(), used_deposit, m_trans->get_target_addr().c_str(), m_trans->get_digest_hex_str().c_str());
    return ret;
}

bool xtransaction_fee_t::need_use_tgas_disk(const std::string &source_addr, const std::string &target_addr, const std::string &func_name){
    xdbg("need_use_tgas_disk:%s, %s", target_addr.c_str(), func_name.c_str());
#ifdef XENABLE_MOCK_ZEC_STAKE
    if (is_sys_contract_address(common::xaccount_address_t{ target_addr }) && (func_name == "nodeJoinNetwork")){
        return false;
    }
#endif
    return !is_sys_contract_address(common::xaccount_address_t{ source_addr });
}

int32_t xtransaction_fee_t::update_disk(bool is_contract){
    uint64_t disk_usage = get_disk_usage(is_contract);
    auto ret = m_account_ctx->update_disk(disk_usage);
    return ret;
}

int32_t xtransaction_fee_t::update_tgas_disk_after_sc_exec(xvm::xtransaction_trace_ptr trace){
    uint32_t instruction_usage = trace->m_instruction_usage + trace->m_tgas_usage;
    if(trace->m_errno == xvm::enum_xvm_error_code::ok && 0 == instruction_usage){
        // system contract consumes no resource, return 0 directly
        return 0;
    }
    uint64_t used_deposit{0};
    uint64_t frozen_tgas = m_trans->get_last_action_send_tx_lock_tgas();

    // set contract exec duration to be instruction_usage * 40 temporarily
    uint32_t time_instruction_ratio = 40;
    auto exec_duration = instruction_usage * time_instruction_ratio;
    auto cpu_gas_exchange_ratio = XGET_ONCHAIN_GOVERNANCE_PARAMETER(cpu_gas_exchange_ratio);
    auto deal_used_tgas = exec_duration / cpu_gas_exchange_ratio;
    xdbg("tgas_disk contract exec before update frozen_tgas: %d, used_deposit: %d, instruction: %d, cpu_gas_exchange_ratio: %d, deal_used_tgas: %d",
          frozen_tgas, used_deposit, instruction_usage, cpu_gas_exchange_ratio, deal_used_tgas);
    // frozen_tgas after consuming
    auto ret = update_tgas_disk_contract_recv(used_deposit, frozen_tgas, deal_used_tgas);
    xdbg("tgas_disk contract exec after update ret: %d, unused frozen_tgas: %d, used_deposit: %d", ret, frozen_tgas, used_deposit);
    if(ret != 0){
        return ret;
    }

    uint32_t send_tx_lock_tgas = m_trans->get_last_action_send_tx_lock_tgas();
    m_trans->set_current_send_tx_lock_tgas(send_tx_lock_tgas);
    m_trans->set_current_recv_tx_use_send_tx_tgas(send_tx_lock_tgas - frozen_tgas);
    xdbg("tgas_disk tx hash: %s, lock_tgas: %u, addr: %s, ret: %d, frozen_tgas: %d, used_deposit: %d",
          m_trans->get_digest_hex_str().c_str(), send_tx_lock_tgas, m_trans->get_source_addr().c_str(), ret, frozen_tgas, used_deposit);
    return ret;
}

int32_t xtransaction_fee_t::update_tgas_disk_contract_recv(uint64_t& used_deposit, uint64_t& frozen_tgas, uint64_t deal_used_tgas){
    // minimum tx deposit
    if(m_trans->get_transaction()->get_deposit() < XGET_ONCHAIN_GOVERNANCE_PARAMETER(min_tx_deposit)){
        return xtransaction_not_enough_deposit;
    }
    auto ret = update_tgas_contract_recv(used_deposit, frozen_tgas, deal_used_tgas);
    if(ret != 0){
        xdbg("tgas_disk update_tgas_contract_recv(used_deposit: %d, frozen_tgas: %d), ret: %d", used_deposit, frozen_tgas, ret);
        return ret;
    }

    ret = update_disk(true);
    uint32_t used_disk = get_disk_usage(true);
    m_trans->set_current_used_disk(used_disk);
    xdbg("tgas_disk m_used_disk %d, %s, %s", used_disk, m_trans->get_target_addr().c_str(), m_trans->get_digest_hex_str().c_str());
    if(ret != 0){
        xdbg("tgas_disk update_disk(used_deposit: %d, frozen_tgas: %d), ret: %d", used_deposit, frozen_tgas, ret);
        return ret;
    }

    return ret;
}

int32_t xtransaction_fee_t::update_tgas_contract_recv(uint64_t& used_deposit, uint64_t& frozen_tgas, uint64_t deal_used_tgas){
    // for contract as receiver, do not count tx size as
    uint64_t tgas_usage = 0;
    uint64_t sender_used_deposit = m_trans->get_last_action_used_deposit();
    auto ret = m_account_ctx->update_tgas_contract_recv(sender_used_deposit, m_trans->get_transaction()->get_deposit(), used_deposit, frozen_tgas, deal_used_tgas);
    m_trans->set_current_used_tgas(m_account_ctx->m_cur_used_tgas);
    m_trans->set_current_used_deposit(sender_used_deposit + used_deposit);
    xdbg("tgas_disk m_used_tgas %d, used_deposit: %d, %s, %s",
          m_trans->get_current_used_tgas(), m_trans->get_current_used_deposit(), m_trans->get_target_addr().c_str(), m_trans->get_digest_hex_str().c_str());
    return ret;
}

uint64_t xtransaction_fee_t::get_service_fee(){
    return m_service_fee;
}

void xtransaction_fee_t::update_service_fee(){
    m_service_fee = cal_service_fee(m_trans->get_source_addr(), m_trans->get_target_addr());
}

uint64_t xtransaction_fee_t::cal_service_fee(const std::string& source, const std::string& target) {
    uint64_t beacon_tx_fee{0};
#ifndef XENABLE_MOCK_ZEC_STAKE
    if (!is_sys_contract_address(common::xaccount_address_t{ source })
     && is_beacon_contract_address(common::xaccount_address_t{ target })){
        beacon_tx_fee = XGET_ONCHAIN_GOVERNANCE_PARAMETER(beacon_tx_fee);
    }
#endif
    return beacon_tx_fee;
}

void xtransaction_fee_t::update_fee_recv() {
    m_trans->set_current_used_deposit(m_trans->get_last_action_used_deposit());
}

int32_t xtransaction_fee_t::update_fee_recv_self() {
    m_account_ctx->set_lock_balance_change(-static_cast<int64_t>(m_trans->get_transaction()->get_deposit()));
    m_account_ctx->set_balance_change(m_trans->get_transaction()->get_deposit());
    int32_t ret = m_account_ctx->top_token_transfer_out(0, get_deposit_usage());
    xassert(ret == 0);
    return 0;
}

int32_t xtransaction_fee_t::update_fee_confirm() {
    m_account_ctx->set_lock_balance_change(-static_cast<int64_t>(m_trans->get_transaction()->get_deposit()));
    m_account_ctx->set_balance_change(m_trans->get_transaction()->get_deposit());

    uint32_t last_action_used_deposit = m_trans->get_last_action_used_deposit();
    m_trans->set_current_used_deposit(last_action_used_deposit);
    int32_t ret = m_account_ctx->top_token_transfer_out(0, last_action_used_deposit);
    xassert(ret == 0);
    return 0;
}

int32_t xtransaction_fee_t::update_contract_fee_confirm(uint64_t amount) {
    uint32_t last_action_used_deposit = m_trans->get_last_action_used_deposit();
    auto status = m_trans->get_last_action_exec_status();

    m_account_ctx->set_lock_tgas_change(-static_cast<int64_t>(m_trans->get_last_action_send_tx_lock_tgas()));
    m_account_ctx->set_lock_balance_change(-static_cast<int64_t>(m_trans->get_transaction()->get_deposit()));
    m_account_ctx->set_balance_change(m_trans->get_transaction()->get_deposit());
    xdbg("tgas_disk tx hash: %s, deposit: %u, frozen_tgas: %u, status: %d",
          m_trans->get_digest_hex_str().c_str(), m_trans->get_transaction()->get_deposit(), m_trans->get_last_action_send_tx_lock_tgas(), status);

    uint64_t target_used_tgas = m_trans->get_last_action_recv_tx_use_send_tx_tgas();
    // auto total_tgas = m_fee.get_tgas_usage(true) + target_used_tgas;
    xdbg("tgas_disk tx hash: %s, recv_tx_use_send_tx_tgas: %llu, used_deposit: %u, lock_tgas: %u",
          m_trans->get_digest_hex_str().c_str(), target_used_tgas, last_action_used_deposit, m_trans->get_last_action_send_tx_lock_tgas());
    auto ret = m_account_ctx->calc_resource(target_used_tgas, m_trans->get_transaction()->get_deposit(), last_action_used_deposit);
    m_trans->set_current_used_tgas(target_used_tgas);
    m_trans->set_current_used_deposit(last_action_used_deposit);

    int64_t lock_balance = m_account_ctx->get_blockchain()->lock_balance();
    if(status == enum_xunit_tx_exec_status_fail){
        if (lock_balance >= static_cast<int64_t>(amount)) {
            xdbg("amount balance change for tx failed: %lld:%llu", lock_balance, amount);
            m_account_ctx->top_token_transfer_in(amount);
            m_account_ctx->set_lock_balance_change(-static_cast<int64_t>(amount));
        } else {
            xerror("amount balance change is less than amount: %lld:%llu", lock_balance, amount);
        }
    } else {
        xdbg("amount balance change : %lld:%llu", lock_balance, amount);
        m_account_ctx->set_lock_balance_change(-static_cast<int64_t>(amount));
    }
    return ret;
}

NS_END2
