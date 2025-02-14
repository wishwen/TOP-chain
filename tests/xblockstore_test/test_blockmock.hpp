#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "gtest/gtest.h"
#include "xbase/xmem.h"
#include "xbase/xutl.h"
#include "xbase/xcontext.h"
#include "xdata/xdata_defines.h"
#include "xdata/xblock.h"
#include "xdata/xblockchain.h"
#include "xdata/xlightunit.h"
#include "xdata/xfullunit.h"
#include "xdata/xtransaction.h"
#include "xdata/xdatautil.h"
#include "xdata/xtableblock.h"
#include "xdata/xblocktool.h"
#include "xdata/tests/test_blockutl.hpp"
#include "xdata/xchain_param.h"

#include "xdata/xgenesis_data.h"

#include "xconfig/xconfig_register.h"
#include "xcommon/xip.h"


#include "xcrypto/xckey.h"
#include "xcrypto/xcrypto_util.h"
#include "xcodec/xmsgpack_codec.hpp"

#include "xstore/xaccount_cmd.h"

#include "xbasic/xutility.h"
#include "xbase/xobject_ptr.h"
#include "xbasic/xcrypto_key.h"

#include "test_certauth.hpp"

using namespace top;
using namespace test;
using namespace top::data;
using namespace top::store;

class test_blockmock_t {
private:
    xaccount_cmd_ptr_t modify_list_property(xaccount_ptr_t account, const std::string& name, const std::string& value) {
        assert(account != nullptr);
        auto cmd = std::make_shared<xaccount_cmd>(account.get(), m_store);

        if (account->get_chain_height() == 0) {
            auto ret = cmd->list_create(name, true);
            assert(ret == 0);
        } else {
            if (value.empty()) {
                return cmd;
            } else {
                int32_t error_code;
                auto prop_ptr = cmd->get_property(name, error_code);
                // property must initiate at first block
                if (prop_ptr == nullptr) {
                    assert(0);
                }
                auto ret = cmd->list_push_back(name, value, true);
                assert(ret == 0);
            }
        }
        return cmd;
    }
 public:
    explicit test_blockmock_t(store::xstore_face_t* store)
    : m_store(store) {}

    base::xvblock_t* create_sample_block(xaccount_cmd* cmd, const std::string& address) {
        assert(m_store != nullptr);
        assert(cmd != nullptr);
        assert(m_store == cmd->get_store());
        auto account = m_store->clone_account(address);
        if (account == nullptr) {
            account = new xblockchain2_t(address);
        }
        uint64_t amount = 100;
        std::string to_account = "T-to-xxxxxxxxxxxxxxxxxxxxx";
        xtransaction_ptr_t tx = account->make_transfer_tx(to_account, -amount, 0, 0, 0);

        xlightunit_block_para_t para;
        para.set_one_input_tx(tx);
        para.set_balance_change(amount);
        para.set_property_log(cmd->get_property_log());
        para.set_propertys_change(cmd->get_property_hash());
        auto vblock = test_blocktuil::create_next_lightunit(para, account);
        assert(vblock);

        return vblock;
    }

    base::xvblock_t* create_sample_block(base::xvblock_t* prev_block, xaccount_cmd* cmd, const std::string& address) {
        assert(m_store != nullptr);
        uint64_t amount = 100;
        std::string to_account = "T-to-xxxxxxxxxxxxxxxxxxxxx";
        if (prev_block == nullptr) {

            xtransaction_ptr_t tx = create_tx_create_user_account(address);

            data::xtransaction_result_t result;
            result.m_balance_change = amount;
            if (cmd) {
                result.m_props = cmd->get_property_hash();
                result.m_prop_log = cmd->get_property_log();
            }

            base::xvblock_t* genesis_block =  data::xblocktool_t::create_genesis_lightunit(address, tx, result);
            return genesis_block;
        }

        auto account = m_store->clone_account(address);
        if (account == nullptr) {
            account = new xblockchain2_t(address);
        }
        assert(account != nullptr);
        auto& config_register = top::config::xconfig_register_t::get_instance();

        uint32_t fullunit_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(fullunit_contain_of_unit_num);
        if ((account->get_chain_height() + 1) % fullunit_num == 0) {
            std::map<std::string, std::string> properties;
            const auto & property_map = account->get_property_hash_map();
            for (auto & v : property_map) {
                xdataobj_ptr_t db_prop = m_store->clone_property(address, v.first);
                if (db_prop == nullptr) {
                    xerror("test::create_sample_block account:%s property(%s) not exist.",
                        address.c_str(), v.first.c_str());
                    return nullptr;
                }

                std::string db_prop_hash = xhash_base_t::calc_dataunit_hash(db_prop.get());
                if (db_prop_hash != v.second) {
                    xerror("test::create_sample_block account:%s property(%s) hash not match fullunit.",
                        address.c_str(), v.first.c_str());
                    return nullptr;
                }
                base::xstream_t _stream(base::xcontext_t::instance());
                db_prop->serialize_to(_stream);
                std::string prop_str((const char *)_stream.data(), _stream.size());
                properties[v.first] = prop_str;
            }

            xfullunit_block_para_t block_para;
            block_para.m_account_state = account->get_account_mstate();
            block_para.m_first_unit_hash = account->get_last_full_unit_hash();
            block_para.m_first_unit_height = account->get_last_full_unit_height();
            block_para.m_account_propertys = properties;

            base::xvblock_t* proposal_block = test_blocktuil::create_next_fullunit_with_consensus(block_para, prev_block);
            account->release_ref();
            return proposal_block;
        }
        xtransaction_ptr_t tx = account->make_transfer_tx(to_account, -amount, 0, 0, 0);

        xlightunit_block_para_t para;
        para.set_one_input_tx(tx);
        para.set_balance_change(amount);
        if (cmd != nullptr) {
            para.set_property_log(cmd->get_property_log());
            para.set_propertys_change(cmd->get_property_hash());
        }
        base::xvblock_t* proposal_block = nullptr;
        proposal_block = test_blocktuil::create_next_lightunit_with_consensus(para, prev_block);

        assert(proposal_block != nullptr);
        account->release_ref();
        return proposal_block;
    }

    base::xvblock_t* create_property_block(base::xvblock_t* prev_block, const std::string& address, const std::string& name, const std::string& value = "") {
        assert(m_store != nullptr);
        uint64_t amount = 100;
        std::string to_address = "T-to-xxxxxxxxxxxxxxxxxxxxx";
        xaccount_cmd_ptr_t cmd = nullptr;
        auto account = m_store->clone_account(address);
        if (account == nullptr) {
            account = new xblockchain2_t(address);
            cmd = std::make_shared<xaccount_cmd>(account, m_store);
            auto ret = cmd->list_create(name, true);
            assert(ret == 0);
            xtransaction_ptr_t tx = create_tx_create_user_account(address);

            data::xtransaction_result_t result;
            result.m_balance_change = amount;

            result.m_props = cmd->get_property_hash();
            result.m_prop_log = cmd->get_property_log();
            base::xvblock_t* genesis_block =  data::xblocktool_t::create_genesis_lightunit(address, tx, result);
            ret = m_store->set_vblock(genesis_block);
            assert(ret);
            m_store->execute_block(genesis_block);
            assert(ret);
            return genesis_block;
        }

        base::auto_reference<xblockchain2_t> autohold(account);

        auto& config_register = top::config::xconfig_register_t::get_instance();
        uint32_t fullunit_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(fullunit_contain_of_unit_num);
        if ((account->get_chain_height() + 1) % fullunit_num == 0) {
            std::map<std::string, std::string> properties;
            const auto & property_map = account->get_property_hash_map();
            for (auto & v : property_map) {
                xdataobj_ptr_t db_prop = m_store->clone_property(address, v.first);
                if (db_prop == nullptr) {
                    xerror("test::create_sample_block account:%s property(%s) not exist.",
                        address.c_str(), v.first.c_str());
                    return nullptr;
                }

                std::string db_prop_hash = xhash_base_t::calc_dataunit_hash(db_prop.get());
                if (db_prop_hash != v.second) {
                    xerror("test::create_sample_block account:%s property(%s) hash not match fullunit.",
                        address.c_str(), v.first.c_str());
                    return nullptr;
                }
                base::xstream_t _stream(base::xcontext_t::instance());
                db_prop->serialize_to(_stream);
                std::string prop_str((const char *)_stream.data(), _stream.size());
                properties[v.first] = prop_str;
            }

            xfullunit_block_para_t block_para;
            block_para.m_account_state = account->get_account_mstate();
            block_para.m_first_unit_hash = account->get_last_full_unit_hash();
            block_para.m_first_unit_height = account->get_last_full_unit_height();
            block_para.m_account_propertys = properties;

            base::xvblock_t* proposal_block = xblocktool_t::create_next_fullunit(block_para, prev_block);
            return proposal_block;
        }

        cmd = std::make_shared<xaccount_cmd>(account, m_store);
        if (!value.empty()) {
            int32_t error_code;
            auto prop_ptr = cmd->get_property(name, error_code);
            // property must initiate at first block
            if (prop_ptr == nullptr) {
                assert(0);
            }
            auto ret = cmd->list_push_back(name, value, true);
            assert(ret == 0);
        }

        xtransaction_ptr_t tx = account->make_transfer_tx(to_address, -amount, 0, 0, 0);
        // xtransaction_ptr_t tx = xtransaction_maker::make_transfer_tx(account, to_addr, 1, 10000, 100, 1000);

        xlightunit_block_para_t para;
        para.set_one_input_tx(tx);
        para.set_balance_change(amount);
        if (cmd != nullptr) {
            para.set_property_log(cmd->get_property_log());
            para.set_propertys_change(cmd->get_property_hash());
        }
        base::xvblock_t* proposal_block = nullptr;
        proposal_block = xblocktool_t::create_next_lightunit(para, prev_block);

        assert(proposal_block != nullptr);
        account->release_ref();
        return proposal_block;
    }

    base::xvblock_t* create_next_empty_tableblock(base::xvblock_t* prev_block) {
        assert(prev_block != nullptr);
        base::xvblock_t* next_block = xblocktool_t::create_next_emptyblock(prev_block);
        assert(next_block != nullptr);
        test_certauth::instance().do_multi_sign(next_block);

        return next_block;
    }

    base::xvblock_t* create_unit(const std::string & address, const std::map<std::string, std::string> &prop_list, uint64_t timer_height=1) {
        auto account = m_store->clone_account(address);
        base::xvblock_t* genesis_block = nullptr;
        if (account == nullptr) {
            genesis_block = test_blocktuil::create_genesis_empty_unit(address);
            account = new xblockchain2_t(address);

        } else {
            if (account->is_state_behind()) {
                assert(0);
                return nullptr;
            }
        }

        auto& config_register = top::config::xconfig_register_t::get_instance();

        uint32_t fullunit_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(fullunit_contain_of_unit_num);
        if ((account->get_chain_height() + 1) % fullunit_num == 0) {
            auto vblock = test_blocktuil::create_next_fullunit(account);
            assert(vblock);

            return vblock;
        } else {
            uint64_t amount = 100;
            std::string to_account = "T-to-xxxxxxxxxxxxxxxxxxxxx";
            xtransaction_ptr_t tx = account->make_transfer_tx(to_account, -amount, 0, 0, 0);

            // create property
            xaccount_cmd accountcmd(account, m_store);
            int prop_ret = create_and_modify_property(accountcmd, address, prop_list);
            xproperty_log_ptr_t bin_log = nullptr;
            if (prop_ret == 0) {
                auto prop_hashs = accountcmd.get_property_hash();
                assert(prop_hashs.size() == 1);
            }

            xlightunit_block_para_t para;
            para.set_one_input_tx(tx);
            para.set_balance_change(amount);
            para.set_property_log(accountcmd.get_property_log());
            para.set_propertys_change(accountcmd.get_property_hash());

            base::xvblock_t* vblock = nullptr;
            if (genesis_block) {
                vblock = test_blocktuil::create_next_lightunit(para, genesis_block);
            } else {
                vblock = test_blocktuil::create_next_lightunit(para, account);
            }

            assert(vblock);
            auto lightunit = dynamic_cast<xblock_t*>(vblock);
            assert(lightunit);

            return vblock;
        }
    }

    int create_and_modify_property(xaccount_cmd &accountcmd, const std::string &address, const std::map<std::string, std::string> &prop_list) {

        if (prop_list.size() == 0)
            return -1;

        uint64_t height = m_store->get_blockchain_height(address);
        if (height == 0) {
            for (auto &it: prop_list) {
                const std::string & prop_name = it.first;
                const std::string & value = it.second;
                auto ret = accountcmd.list_create(prop_name, true);
                assert(ret == 0);
            }
        } else {
            for (auto &it: prop_list) {
                const std::string & prop_name = it.first;
                const std::string & value = it.second;

                int32_t error_code;
                auto prop_ptr = accountcmd.get_property(prop_name, error_code);

                // property must initiate at first block
                if (prop_ptr == nullptr) {
                    assert(0);
                }
                auto ret = accountcmd.list_push_back(prop_name, value, true);
                assert(ret == 0);
            }
        }

        return 0;
    }

    xtransaction_ptr_t create_tx_create_user_account(const std::string &from, uint16_t expire_time = 100) {
        uint64_t last_nonce = 0;
        uint256_t last_hash;

        xtransaction_ptr_t tx = make_object_ptr<xtransaction_t>();
        tx->set_deposit(100000);
        tx->make_tx_create_user_account(from);
        tx->set_last_trans_hash_and_nonce(last_hash, last_nonce);
        tx->set_same_source_target_address(from);
        tx->set_fire_and_expire_time(expire_time);
        tx->set_digest();
        return tx;
    }

#if 0
    base::xvblock_t* create_raw_unitblock(const std::string & address, const std::map<std::string, std::string> &prop_list) {
        auto blockchain = m_store->clone_account(address);
        xobject_ptr_t<xblockchain2_t> account;
        base::xvblock_t* genesis_block = nullptr;
        if (account == nullptr) {
            genesis_block = xblocktool_t::create_genesis_empty_unit(address);
            blockchain = new xblockchain2_t(address);
            account.attach(blockchain);
        } else {
            account.attach(blockchain);
            if (account->is_state_behind()) {
                assert(0);
                return nullptr;
            }
        }

        auto& config_register = top::config::xconfig_register_t::get_instance();

        uint32_t fullunit_num = XGET_ONCHAIN_GOVERNANCE_PARAMETER(fullunit_contain_of_unit_num);
        if ((account->get_chain_height() + 1) % fullunit_num == 0) {
            auto vblock = xblocktool_t::create_next_fullunit(account);
            assert(vblock);
            test_certauth::instance().do_multi_sign(vblock);
            return vblock;
        } else {
            uint64_t amount = 100;
            std::string to_account = "T-to-xxxxxxxxxxxxxxxxxxxxx";
            xtransaction_ptr_t tx = account->make_transfer_tx(to_account, -amount, 0, 0, 0);

            // create property
            xaccount_cmd accountcmd(account, m_store);
            int prop_ret = create_and_modify_property(accountcmd, address, prop_list);
            xproperty_log_ptr_t bin_log = nullptr;
            if (prop_ret == 0) {
                auto prop_hashs = accountcmd.get_property_hash();
                assert(prop_hashs.size() == 1);
            }

            xlightunit_block_para_t para;
            para.set_one_input_tx(tx);
            para.set_balance_change(amount);
            para.set_property_log(accountcmd.get_property_log());
            para.set_propertys_change(accountcmd.get_property_hash());

            base::xvblock_t* vblock = nullptr;
            if (genesis_block) {
                vblock = xblocktool_t::create_next_lightunit(para, genesis_block);
            } else {
                vblock = xblocktool_t::create_next_lightunit(para, account);
            }

            assert(vblock);
            test_certauth::instance().do_multi_sign(vblock);

            auto lightunit = dynamic_cast<xblock_t*>(vblock);
            assert(lightunit);

            return vblock;
        }
    }
#endif

    store::xstore_face_t* m_store;
};
