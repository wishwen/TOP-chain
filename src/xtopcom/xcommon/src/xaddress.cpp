// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xbasic/xutility.h"
#include "xcodec/xmsgpack_codec.hpp"
#include "xcommon/xaddress.h"
#include "xcommon/xaddress_error.h"
#include "xcommon/xcodec/xmsgpack/xaccount_election_address_codec.hpp"
#include "xcommon/xcodec/xmsgpack/xcluster_address_codec.hpp"
#include "xcommon/xcodec/xmsgpack/xnode_address_codec.hpp"
#include "xcommon/xnode_type.h"
#include "xutility/xhash.h"

#include <type_traits>

NS_BEG2(top, common)

xtop_cluster_address::xtop_cluster_address(xip_t const & xip)
    : m_xip{ xip.network_id(), xip.zone_id(), xip.cluster_id(), xip.group_id() } {
    if (m_xip.group_id() != xbroadcast_id_t::group) {
        m_type = xnode_type_t::group | node_type_from(m_xip.zone_id(), m_xip.cluster_id(), m_xip.group_id());
    } else if (m_xip.cluster_id() != xbroadcast_id_t::cluster) {
        m_type = xnode_type_t::cluster | node_type_from(m_xip.zone_id(), m_xip.cluster_id());
    } else if (m_xip.zone_id() != xbroadcast_id_t::zone) {
        m_type = xnode_type_t::zone | node_type_from(m_xip.zone_id());
    } else if (m_xip.network_id() != xbroadcast_id_t::network) {
        m_type = xnode_type_t::network;
    } else {
        m_type = xnode_type_t::platform;
    }
}

xtop_cluster_address::xtop_cluster_address(xnetwork_id_t const & network_id)
    : m_xip{ network_id }, m_type{ xnode_type_t::network } {
    assert(!broadcast(network_id));
    assert(broadcast(m_xip.zone_id()));
    assert(broadcast(m_xip.cluster_id()));
    assert(broadcast(m_xip.group_id()));
    assert(broadcast(m_xip.slot_id()));
    assert(m_xip.network_version() == xdefault_network_version);
}

xtop_cluster_address::xtop_cluster_address(xnetwork_id_t const & network_id,
                                           xzone_id_t const & zone_id)
    : m_xip{ network_id, zone_id }
    , m_type{ xnode_type_t::zone | node_type_from(zone_id) } {
    assert(!broadcast(network_id) && !broadcast(zone_id));
    assert(broadcast(m_xip.cluster_id()));
    assert(broadcast(m_xip.group_id()));
    assert(broadcast(m_xip.slot_id()));
    assert(m_xip.network_version() == xdefault_network_version);
}

xtop_cluster_address::xtop_cluster_address(xnetwork_id_t const & network_id,
                                           xzone_id_t const & zone_id,
                                           xcluster_id_t const & cluster_id)
    : m_xip{ network_id, zone_id, cluster_id }
    , m_type{ xnode_type_t::cluster | node_type_from(zone_id, cluster_id) } {
    assert(!broadcast(network_id));
    assert(!broadcast(zone_id));
    assert(!broadcast(cluster_id));
    assert(broadcast(m_xip.group_id()));
    assert(broadcast(m_xip.slot_id()));
    assert(m_xip.network_version() == xdefault_network_version);
}

xtop_cluster_address::xtop_cluster_address(xnetwork_id_t const & network_id,
                                           xzone_id_t const & zone_id,
                                           xcluster_id_t const & cluster_id,
                                           xgroup_id_t const & group_id)
    : m_xip{ network_id, zone_id, cluster_id, group_id } {
    if (group_id.has_value() && group_id != xbroadcast_id_t::group) {
        m_type = xnode_type_t::group | node_type_from(zone_id, cluster_id, group_id);
    } else if (cluster_id.has_value() && cluster_id != xbroadcast_id_t::cluster) {
        m_type = xnode_type_t::cluster | node_type_from(zone_id, cluster_id);
    } else if (zone_id.has_value() && zone_id != xbroadcast_id_t::zone) {
        m_type = xnode_type_t::zone | node_type_from(zone_id);
    } else if (network_id.has_value() && network_id != xbroadcast_id_t::network) {
        m_type = xnode_type_t::network;
    } else {
        m_type = xnode_type_t::platform;
    }

    assert(broadcast(m_xip.slot_id()));
    assert(m_xip.network_version() == xdefault_network_version);
}

bool
xtop_cluster_address::operator==(xtop_cluster_address const & other) const noexcept {
    return m_xip == other.m_xip;
}

bool
xtop_cluster_address::operator!=(xtop_cluster_address const & other) const noexcept {
    return !(*this == other);
}

bool
xtop_cluster_address::operator<(xtop_cluster_address const & other) const noexcept {
    return m_xip < other.m_xip;
}

bool
xtop_cluster_address::operator>(xtop_cluster_address const & other) const noexcept {
    return other < *this;
}

bool
xtop_cluster_address::operator<=(xtop_cluster_address const & other) const noexcept {
    return !(other < *this);
}

bool
xtop_cluster_address::operator>=(xtop_cluster_address const & other) const noexcept {
    return !(*this < other);
}

xnetwork_id_t
xtop_cluster_address::network_id() const noexcept {
    return m_xip.network_id();
}

xzone_id_t
xtop_cluster_address::zone_id() const noexcept {
    return m_xip.zone_id();
}

xcluster_id_t
xtop_cluster_address::cluster_id() const noexcept {
    return m_xip.cluster_id();
}

xgroup_id_t
xtop_cluster_address::group_id() const noexcept {
    return m_xip.group_id();
}

xsharding_info_t
xtop_cluster_address::sharding_info() const noexcept {
    return xsharding_info_t{ m_xip };
}

xip_t const &
xtop_cluster_address::xip() const noexcept {
    return m_xip;
}

xnode_type_t
xtop_cluster_address::type() const noexcept {
    return m_type;
}

bool
xtop_cluster_address::contains(xtop_cluster_address const & address) const noexcept {
    if (broadcast(network_id())) {
        return true;
    }

    if (network_id() != address.network_id()) {
        return false;
    }

    if (broadcast(zone_id())) {
        return true;
    }

    if (zone_id() != address.zone_id()) {
        return false;
    }

    if (broadcast(cluster_id())) {
        return true;
    }

    if (cluster_id() != address.cluster_id()) {
        return false;
    }

    if (broadcast(group_id())) {
        return true;
    }

    if (group_id() != address.group_id()) {
        return false;
    }

    return true;
}

void
xtop_cluster_address::swap(xtop_cluster_address & other) noexcept {
    m_xip.swap(other.m_xip);
    std::swap(m_type, other.m_type);
}

bool
xtop_cluster_address::empty() const noexcept {
#if defined DEBUG
    if (!m_xip.empty()) {
        assert(m_type != common::xnode_type_t::invalid);
    }
#endif
    return m_xip.empty();
}

xtop_cluster_address::hash_result_type
xtop_cluster_address::hash() const {
    return m_xip.hash();
}

std::string
xtop_cluster_address::to_string() const {
    return top::common::to_string(m_type) + "/" + m_xip.to_string();
}

std::int32_t
xtop_cluster_address::do_write(base::xstream_t & stream) const {
    try {
        return stream << codec::msgpack_encode(*this);
    } catch (...) {
        xerror("[cluster address] do_write stream failed %s", to_string().c_str());
        return -1;
    }
}

std::int32_t
xtop_cluster_address::do_read(base::xstream_t & stream) {
    try {
        xbyte_buffer_t buffer;
        auto const size = stream >> buffer;
        *this = codec::msgpack_decode<xtop_cluster_address>(buffer);
        return size;
    } catch (...) {
        xerror("[cluster address] do_read stream failed");
        return -1;
    }
}

std::int32_t xtop_cluster_address::do_write(base::xbuffer_t & buffer) const {
    try {
        return buffer << codec::msgpack_encode(*this);
    } catch (...) {
        xerror("[cluster address] do_write buffer failed %s", to_string().c_str());
        return -1;
    }
}

std::int32_t xtop_cluster_address::do_read(base::xbuffer_t & buffer) {
    try {
        xbyte_buffer_t bytes;
        auto const size = buffer >> bytes;
        *this = codec::msgpack_decode<xtop_cluster_address>(bytes);
        return size;
    } catch (...) {
        xerror("[cluster address] do_read buffer failed");
        return -1;
    }
}

std::int32_t
operator<<(base::xstream_t & stream, xtop_cluster_address const & o) {
    return o.do_write(stream);
}

std::int32_t
operator>>(base::xstream_t & stream, xtop_cluster_address & o) {
    return o.do_read(stream);
}

std::int32_t
operator<<(base::xbuffer_t & buffer, xtop_cluster_address const & o) {
    return o.do_write(buffer);
}

std::int32_t
operator>>(base::xbuffer_t & buffer, xtop_cluster_address & o) {
    return o.do_read(buffer);
}

xtop_account_election_address::xtop_account_election_address(xaccount_address_t const & account, xslot_id_t const & slot_id)
    : m_account_address{ account }, m_slot_id{ slot_id } {
}

xtop_account_election_address::xtop_account_election_address(xaccount_address_t && account, xslot_id_t && slot_id) noexcept
    : m_account_address{ std::move(account) }, m_slot_id{ std::move(slot_id) } {
}

xaccount_address_t const &
xtop_account_election_address::account_address() const noexcept {
    return m_account_address;
}

xnode_id_t const &
xtop_account_election_address::node_id() const noexcept {
    return m_account_address;
}

xslot_id_t const &
xtop_account_election_address::slot_id() const noexcept {
    return m_slot_id;
}

bool
xtop_account_election_address::empty() const noexcept {
    return m_account_address.empty();
}

void
xtop_account_election_address::swap(xtop_account_election_address & other) noexcept {
    m_account_address.swap(other.m_account_address);
    m_slot_id.swap(other.m_slot_id);
}

bool
xtop_account_election_address::operator==(xtop_account_election_address const & other) const noexcept {
    return m_account_address == other.m_account_address && m_slot_id == other.m_slot_id;
}

bool
xtop_account_election_address::operator!=(xtop_account_election_address const & other) const noexcept {
    return !(*this == other);
}

bool
xtop_account_election_address::operator<(xtop_account_election_address const & other) const noexcept {
    if (m_account_address != other.m_account_address) {
        return m_account_address < other.m_account_address;
    }

    return m_slot_id < other.m_slot_id;
}

bool
xtop_account_election_address::operator>(xtop_account_election_address const & other) const noexcept {
    return other < *this;
}

bool
xtop_account_election_address::operator<=(xtop_account_election_address const & other) const noexcept {
    return !(other < *this);
}

bool
xtop_account_election_address::operator>=(xtop_account_election_address const & other) const noexcept {
    return !(*this < other);
}

xtop_account_election_address::hash_result_type
xtop_account_election_address::hash() const {
    utl::xxh64_t hasher;

    auto const account_address_hash = m_account_address.hash();
    hasher.update(&account_address_hash, sizeof(account_address_hash));

    auto const slot_id_hash = m_slot_id.hash();
    hasher.update(&slot_id_hash, sizeof(slot_id_hash));

    return hasher.get_hash();
}

std::string
xtop_account_election_address::to_string() const {
    return m_account_address.to_string() + u8"/" + m_slot_id.to_string();
}

xtop_logical_version::xtop_logical_version(xversion_t const & version, std::uint16_t const sharding_size, std::uint64_t const associated_blk_height)
  : m_version{version}, m_sharding_size{sharding_size}, m_associated_blk_height{associated_blk_height} {}

xversion_t const & xtop_logical_version::version() const noexcept {
    return m_version;
}

std::uint16_t xtop_logical_version::sharding_size() const noexcept {
    return m_sharding_size;
}

std::uint64_t xtop_logical_version::associated_blk_height() const noexcept {
    return m_associated_blk_height;
}

void xtop_logical_version::swap(xtop_logical_version & other) noexcept {
    m_version.swap(other.m_version);
    std::swap(m_sharding_size, other.m_sharding_size);
    std::swap(m_associated_blk_height, other.m_associated_blk_height);
}

bool xtop_logical_version::empty() const noexcept {
    return m_version.empty() && broadcast(associated_blk_height());
}

bool xtop_logical_version::operator==(xtop_logical_version const & other) const noexcept {
    return m_version == other.m_version && m_sharding_size == other.m_sharding_size && m_associated_blk_height == other.m_associated_blk_height;
}

bool xtop_logical_version::operator!=(xtop_logical_version const & other) const noexcept {
    return !(*this == other);
}

bool xtop_logical_version::operator<(xtop_logical_version const & other) const noexcept {
    if (m_version != other.m_version) {
        return m_version < other.m_version;
    }
    if (m_associated_blk_height != other.m_associated_blk_height) {
        return m_associated_blk_height < other.m_associated_blk_height;
    }
    return false;
}

bool xtop_logical_version::operator>(xtop_logical_version const & other) const noexcept {
    return other < *this;
}

bool xtop_logical_version::operator<=(xtop_logical_version const & other) const noexcept {
    return !(other < *this);
}

bool xtop_logical_version::operator>=(xtop_logical_version const & other) const noexcept {
    return !(*this < other);
}

bool xtop_logical_version::contains(xtop_logical_version const & logical_version) const noexcept {
    if (!version().empty() && version() != logical_version.version()) {
        return false;
    }
    if (!broadcast(associated_blk_height()) && associated_blk_height() != logical_version.associated_blk_height()) {
        return false;
    }
    return true;
}

xtop_logical_version::hash_result_type xtop_logical_version::hash() const {
    utl::xxh64_t hasher;

    auto const version_hash = version().hash();
    hasher.update(&version_hash, sizeof(version_hash));

    auto const associated_blk_height_hash = associated_blk_height();
    hasher.update(&associated_blk_height_hash, sizeof(associated_blk_height_hash));

    return hasher.get_hash();
}

std::string xtop_logical_version::to_string() const {
    return m_version.to_string() + u8"/" + std::to_string(m_sharding_size) + u8"/" + std::to_string(m_associated_blk_height);
}

xtop_node_address::xtop_node_address(xsharding_address_t const & sharding_address)
    : m_cluster_address{ sharding_address } {
}

xtop_node_address::xtop_node_address(xsharding_address_t const & sharding_address,
                                     xversion_t const & version,
                                     std::uint16_t const sharding_size,
                                     std::uint64_t const associated_blk_height)
  : m_cluster_address{ sharding_address }, m_logic_version{ version, sharding_size, associated_blk_height } {}

xtop_node_address::xtop_node_address(xsharding_address_t const & sharding_address,
                                     xaccount_election_address_t const & account_election_address)
    : m_cluster_address{ sharding_address }, m_account_election_address{ account_election_address }
{
    if (m_cluster_address.empty()) {
        XTHROW(xaddress_error_t,
               xaddress_errc_t::cluster_address_empty,
               std::string{});
    }

    if (m_account_election_address.empty()) {
        XTHROW(xaddress_error_t,
               xaddress_errc_t::account_address_empty,
               m_cluster_address.to_string());
    }
}

xtop_node_address::xtop_node_address(xsharding_address_t const & sharding_address,
                                     xaccount_election_address_t const & account_election_address,
                                     xversion_t const & version,
                                     std::uint16_t const sharding_size,
                                     std::uint64_t const associated_blk_size)
    : m_cluster_address{ sharding_address }, m_account_election_address{ account_election_address }
    , m_logic_version{ version, sharding_size, associated_blk_size }
{
    if (m_cluster_address.empty()) {
        XTHROW(xaddress_error_t,
               xaddress_errc_t::cluster_address_empty,
               std::string{});
    }

    if (m_account_election_address.empty()) {
        XTHROW(xaddress_error_t,
               xaddress_errc_t::account_address_empty,
               m_cluster_address.to_string());
    }

    if (m_logic_version.empty()) {
        XTHROW(xaddress_error_t,
               xaddress_errc_t::version_empty,
               m_cluster_address.to_string() + u8" " + m_account_election_address.to_string());
    }
}

bool
xtop_node_address::operator==(xtop_node_address const & other) const noexcept {
    return m_cluster_address          == other.m_cluster_address &&
           m_account_election_address == other.m_account_election_address &&
           m_logic_version            == other.m_logic_version;
}

bool
xtop_node_address::operator!=(xtop_node_address const & other) const noexcept {
    return !(*this == other);
}

bool
xtop_node_address::operator<(xtop_node_address const & other) const noexcept {
    if (m_cluster_address != other.m_cluster_address) {
        return m_cluster_address < other.m_cluster_address;
    }

    if (m_account_election_address != other.m_account_election_address) {
        return m_account_election_address < other.m_account_election_address;
    }

    if (m_logic_version != other.m_logic_version) {
        return m_logic_version < other.m_logic_version;
    }

    return false;
}

bool
xtop_node_address::operator>(xtop_node_address const & other) const noexcept {
    return other < *this;
}

bool
xtop_node_address::operator<=(xtop_node_address const & other) const noexcept {
    return !(other < *this);
}

bool
xtop_node_address::operator>=(xtop_node_address const & other) const noexcept {
    return !(*this < other);
}

bool
xtop_node_address::empty() const noexcept {
    return m_cluster_address.empty();
}

xaccount_address_t const &
xtop_node_address::account_address() const noexcept {
    return m_account_election_address.account_address();
}

xaccount_election_address_t const &
xtop_node_address::account_election_address() const noexcept {
    return m_account_election_address;
}

xsharding_address_t const &
xtop_node_address::cluster_address() const noexcept {
    return m_cluster_address;
}

xsharding_address_t const &
xtop_node_address::sharding_address() const noexcept {
    return m_cluster_address;
}

xnetwork_id_t
xtop_node_address::network_id() const noexcept {
    return m_cluster_address.network_id();
}

xzone_id_t
xtop_node_address::zone_id() const noexcept {
    return m_cluster_address.zone_id();
}

xcluster_id_t
xtop_node_address::cluster_id() const noexcept {
    return m_cluster_address.cluster_id();
}

xgroup_id_t
xtop_node_address::group_id() const noexcept {
    return m_cluster_address.group_id();
}

xslot_id_t
xtop_node_address::slot_id() const noexcept {
    return m_account_election_address.slot_id();
}

xlogical_version_t const &
xtop_node_address::logical_version() const noexcept {
    return m_logic_version;
}

std::uint16_t
xtop_node_address::sharding_size() const noexcept {
    return m_logic_version.sharding_size();
}

std::uint64_t
xtop_node_address::associated_blk_height() const noexcept {
    return m_logic_version.associated_blk_height();
}

xip2_t
xtop_node_address::xip2() const noexcept {
    auto const & sharding_xip = m_cluster_address.xip();
    return {
        sharding_xip.network_id(),
        sharding_xip.zone_id(),
        sharding_xip.cluster_id(),
        sharding_xip.group_id(),
        xslot_id_t{ m_account_election_address.slot_id().value_or(xbroadcast_slot_id_value) },
        xnetwork_version_t{ static_cast<xnetwork_version_t::value_type>(version().value_or(xdefault_network_version_value)) },
        sharding_size(),
        associated_blk_height()
    };
}

xnode_id_t const &
xtop_node_address::node_id() const noexcept {
    return m_account_election_address.node_id();
}

xversion_t const &
xtop_node_address::version() const noexcept {
    return m_logic_version.version();
}

xnode_type_t
xtop_node_address::type() const noexcept {
    auto const cluster_address_type = m_cluster_address.type();
    // assert(common::has<xnode_type_t::cluster>(cluster_address_type) || common::has<xnode_type_t::zone>(cluster_address_type));

    // if (m_account_election_address.has_value()) {
    //     auto node_type = real_part_type(cluster_address_type);
    //     assert(node_type != xnode_type_t::invalid);
    //     return node_type;
    // }

    return cluster_address_type;
}

bool xtop_node_address::contains(xtop_node_address const & address) const noexcept {
    if (!sharding_address().contains(address.sharding_address())) {
        return false;
    }

    if (!logical_version().contains(address.logical_version())) {
        return false;
    }

    return account_address().empty() || account_address() == address.account_address();
}

xtop_node_address::hash_result_type
xtop_node_address::hash() const {
    utl::xxh64_t hasher;

    auto const sharding_address_hash = m_cluster_address.hash();
    hasher.update(&sharding_address_hash, sizeof(sharding_address_hash));

    auto const account_election_address_hash = m_account_election_address.hash();
    hasher.update(&account_election_address_hash, sizeof(account_election_address_hash));

    auto const logical_version_hash = logical_version().hash();
    hasher.update(&logical_version_hash, sizeof(logical_version_hash));

    return hasher.get_hash();
}

void
xtop_node_address::swap(xtop_node_address & other) noexcept {
    m_account_election_address.swap(other.m_account_election_address);
    m_cluster_address.swap(other.m_cluster_address);
    m_logic_version.swap(other.m_logic_version);
}


std::string
xtop_node_address::to_string() const {
    return m_cluster_address.to_string() + u8"/" + m_account_election_address.to_string() + u8"/" + m_logic_version.to_string();
}

std::int32_t
xtop_node_address::do_write(base::xstream_t & stream) const {
    try {
        return stream << codec::msgpack_encode(*this);
    } catch (...) {
        xerror("[node address] do_write failed %s", to_string().c_str());
        return -1;
    }
}

std::int32_t
xtop_node_address::do_read(base::xstream_t & stream) {
    try {
        xbyte_buffer_t buffer;
        auto const size = stream >> buffer;
        *this = codec::msgpack_decode<xtop_node_address>(buffer);
        return size;
    } catch (...) {
        xerror("[cluster address] do_read failed");
        return -1;
    }
}

std::int32_t
operator<<(base::xstream_t & stream, xtop_node_address const & o) {
    return o.do_write(stream);
}

std::int32_t
operator>>(base::xstream_t & stream, xtop_node_address & o) {
    return o.do_read(stream);
}

xsharding_address_t
build_committee_sharding_address(xnetwork_id_t const & network_id) {
    return xsharding_address_t{
        network_id,
        xcommittee_zone_id,
        xcommittee_cluster_id,
        xcommittee_group_id
    };
}

xsharding_address_t
build_zec_sharding_address(xnetwork_id_t const & network_id) {
    return xsharding_address_t{
        network_id,
        xzec_zone_id,
        xcommittee_cluster_id,
        xcommittee_group_id
    };
}

xsharding_address_t
build_edge_sharding_address(xnetwork_id_t const & network_id) {
    return xsharding_address_t{
        network_id,
        xedge_zone_id,
        xdefault_cluster_id,
        xdefault_group_id
    };
}

xsharding_address_t
build_archive_sharding_address(xnetwork_id_t const & network_id) {
    return xsharding_address_t{
        network_id,
        xarchive_zone_id,
        xdefault_cluster_id,
        xdefault_group_id
    };
}

xsharding_address_t
build_consensus_sharding_address(xgroup_id_t const & group_id,
                                 xnetwork_id_t const & network_id) {
    return xsharding_address_t{
        network_id,
        xconsensus_zone_id,
        xdefault_cluster_id,
        group_id
    };
}

xsharding_address_t
build_network_broadcast_sharding_address(xnetwork_id_t const & network_id) {
    return xsharding_address_t{
        network_id
    };
}

xsharding_address_t
build_platform_broadcast_sharding_address() {
    return xsharding_address_t{
    };
}

xsharding_address_t
build_frozen_sharding_address(xnetwork_id_t const & network_id, xcluster_id_t const & cluster_id, xgroup_id_t const & group_id) {
    return xsharding_address_t{
        network_id,
        xfrozen_zone_id,
        cluster_id,
        group_id
    };
}

NS_END2

std::ostream &
operator<<(std::ostream & o, top::common::xnode_address_t const & addr) {
    return o << addr.to_string();
}

NS_BEG1(std)

std::size_t
hash<top::common::xnode_address_t>::operator()(top::common::xnode_address_t const & vnode_address) const {
    return static_cast<std::size_t>(vnode_address.hash());
}

std::size_t
hash<top::common::xsharding_address_t>::operator()(top::common::xsharding_address_t const & cluster_address) const {
    return static_cast<std::size_t>(cluster_address.hash());
}

NS_END1
