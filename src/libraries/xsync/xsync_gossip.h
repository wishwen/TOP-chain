// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xstore/xstore_face.h"
#include "xmbus/xmessage_bus.h"
#include "xvnetwork/xaddress.h"
#include "xmbus/xevent_common.h"
#include "xmbus/xevent_account.h"
#include "xmbus/xevent_store.h"
#include "xmetrics/xmetrics.h"
#include "xmbus/xevent_behind.h"
#include "xsync/xrole_chains_mgr.h"
#include "xsync/xrole_xips_manager.h"
#include "xsync/xsync_sender.h"
#include "xsync/xsync_message.h"
#include "xsync/xgossip_message.h"

NS_BEG2(top, sync)

enum enum_walk_type {
    enum_walk_type_add_role,
    enum_walk_type_timer,
};

class xgossip_behind_info_t {
public:
    xgossip_behind_info_t(uint64_t _local_height, uint64_t _local_view_id, uint64_t _peer_height, uint64_t _peer_view_id):
    local_height(_local_height),
    local_view_id(_local_view_id),
    peer_height(_peer_height),
    peer_view_id(_peer_view_id) {
    }

    uint64_t local_height{0};
    uint64_t local_view_id{0};
    uint64_t peer_height{0};
    uint64_t peer_view_id{0};
};

class xsync_gossip_t {
public:

    xsync_gossip_t(std::string vnode_id, const observer_ptr<mbus::xmessage_bus_face_t> &mbus,
            xrole_chains_mgr_t *role_chains_mgr, xrole_xips_manager_t *role_xips_mgr, sync::xsync_sender_t *sync_sender);

    virtual ~xsync_gossip_t();

    // 1s
    void on_timer();
    void on_chain_timer(const mbus::xevent_ptr_t& e);
    void on_behind_event(const mbus::xevent_ptr_t &e);
    void add_role(const vnetwork::xvnode_address_t& addr);
    void remove_role(const vnetwork::xvnode_address_t& addr);
    void handle_message(const std::vector<xgossip_chain_info_ptr_t> &info_list, 
        const vnetwork::xvnode_address_t &from_address, const vnetwork::xvnode_address_t &network_self, std::map<std::string, xgossip_behind_info_t> &behind_chain_set);

protected:
    void walk_role(const vnetwork::xvnode_address_t &self_addr, const std::shared_ptr<xrole_chains_t> &role_chains, enum_walk_type walk_type);
    void process_timer(bool is_frozen);
    bool handle_consensus_role_behind(const xsync_roles_t &roles, common::xnode_type_t type, const std::string &address);
    void process_behind_event(const mbus::xevent_ptr_t& e);
    void send_gossip(const vnetwork::xvnode_address_t &self_addr, std::vector<xgossip_chain_info_ptr_t> &info_list, uint32_t max_peers, enum_gossip_target_type target_type);
    void send_gossip_to_target(const vnetwork::xvnode_address_t &self_addr, std::vector<xgossip_chain_info_ptr_t> &info_list, const vnetwork::xvnode_address_t &target_addr);
    void send_frozen_gossip(const vnetwork::xvnode_address_t &self_addr, std::vector<xgossip_chain_info_ptr_t> &info_list, uint32_t max_peers);
    void send_frozen_gossip_to_target(const vnetwork::xvnode_address_t &self_addr, std::vector<xgossip_chain_info_ptr_t> &info_list, const vnetwork::xvnode_address_t &target_addr);
    void update_behind(const std::string &address, uint64_t local_height, uint64_t local_view_id, uint64_t peer_height, uint64_t peer_view_id);
    void dump_behind();
    void update_behind_role();

private:
    std::string m_vnode_id;
    observer_ptr<mbus::xmessage_bus_face_t> m_mbus;
    xrole_chains_mgr_t *m_role_chains_mgr;
    xrole_xips_manager_t *m_role_xips_mgr;
    sync::xsync_sender_t *m_sync_sender;
    uint32_t m_frozen_count{0};
    uint32_t m_count{0};
    uint64_t m_chain_timer_height{0};

    uint32_t m_behind_dump_count{0};
    std::mutex m_lock;
    std::map<std::string, xgossip_behind_info_t> m_chain_behind_info;
};

using xsync_gossip_ptr_t = std::shared_ptr<xsync_gossip_t>;
NS_END2
