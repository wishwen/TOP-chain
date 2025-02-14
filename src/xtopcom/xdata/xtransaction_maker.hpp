﻿// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

#include "xdata/xtransaction.h"

NS_BEG2(top, data)

class xtransaction_maker {
 public:
    static xtransaction_ptr_t make_transfer_tx(const xaccount_ptr_t & account, const std::string & to,
        uint64_t amount, uint64_t firestamp, uint16_t duration, uint32_t deposit) {
        xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
        data::xproperty_asset asset(amount);
        tx->make_tx_transfer(asset);
        tx->set_last_trans_hash_and_nonce(account->account_send_trans_hash(), account->account_send_trans_number());
        tx->set_different_source_target_address(account->address(), to);
        tx->set_fire_timestamp(firestamp);
        tx->set_expire_duration(duration);
        tx->set_deposit(deposit);
        tx->set_digest();
        tx->set_len();

        // update account send tx hash and number
        account->set_account_send_trans_hash(tx->digest());
        account->set_account_send_trans_number(tx->get_tx_nonce());
        return tx;
    }

    static xtransaction_ptr_t make_run_contract_tx(const xaccount_ptr_t & account, const std::string & to,
        const std::string& func_name, const std::string& func_param, uint64_t amount, uint64_t firestamp,
        uint16_t duration, uint32_t deposit) {
        xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
        data::xproperty_asset asset(amount);
        tx->make_tx_run_contract(asset, func_name, func_param);
        tx->set_last_trans_hash_and_nonce(account->account_send_trans_hash(), account->account_send_trans_number());
        tx->set_different_source_target_address(account->address(), to);
        tx->set_fire_timestamp(firestamp);
        tx->set_expire_duration(duration);
        tx->set_deposit(deposit);
        tx->set_digest();
        tx->set_len();
        // update account send tx hash and number
        account->set_account_send_trans_hash(tx->digest());
        account->set_account_send_trans_number(tx->get_tx_nonce());
        return tx;
    }

    static xtransaction_ptr_t make_create_sub_account_tx(const xaccount_ptr_t & account, const std::string & to,
        uint64_t amount, uint64_t firestamp, uint16_t duration, uint32_t deposit) {
        xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
        data::xproperty_asset asset(amount);
        tx->make_tx_create_sub_account(asset);
        tx->set_last_trans_hash_and_nonce(account->account_send_trans_hash(), account->account_send_trans_number());
        tx->set_different_source_target_address(account->address(), to);
        tx->set_fire_timestamp(firestamp);
        tx->set_expire_duration(duration);
        tx->set_deposit(deposit);
        tx->set_digest();
        tx->set_len();
        // update account send tx hash and number
        account->set_account_send_trans_hash(tx->digest());
        account->set_account_send_trans_number(tx->get_tx_nonce());
        return tx;
    }
};

NS_END2
