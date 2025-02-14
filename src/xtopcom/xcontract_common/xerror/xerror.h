// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbase/xns_macro.h"

#include <stdexcept>
#include <string>
#include <system_error>

NS_BEG3(top, contract_common, error)

enum class xenum_errc {
    ok = 0,

    receipt_data_not_found,
    receipt_data_already_exist,

    property_internal_error,
    property_permission_not_allowed,
    property_not_exist,
    property_already_exist,
    property_map_key_not_exist,
    account_not_matched,
    token_not_enough,
    deploy_code_failed,
    unknown_error,
};
using xerrc_t = xenum_errc;

std::error_code make_error_code(xerrc_t const ec) noexcept;
std::error_condition make_error_condition(xerrc_t const ec) noexcept;

class xtop_contract_common_error final : public std::runtime_error {
    std::error_code ec_{make_error_code(xerrc_t::ok)};

public:
    xtop_contract_common_error(xtop_contract_common_error const &) = default;
    xtop_contract_common_error & operator=(xtop_contract_common_error const &) = default;
    xtop_contract_common_error(xtop_contract_common_error &&) = default;
    xtop_contract_common_error & operator=(xtop_contract_common_error &&) = default;
    ~xtop_contract_common_error() override = default;

    xtop_contract_common_error();
    xtop_contract_common_error(xerrc_t const error_code);
    xtop_contract_common_error(xerrc_t const error_code, std::string const & message);

    std::error_code const & code() const noexcept;

private:
    xtop_contract_common_error(std::error_code const & ec);
    xtop_contract_common_error(std::error_code const & ec, const std::string & message);
};
using xcontract_common_error_t = xtop_contract_common_error;

std::error_category const & contract_common_category();

NS_END3

NS_BEG1(std)

template <>
struct is_error_code_enum<top::contract_common::error::xerrc_t> : std::true_type {};

template <>
struct is_error_condition_enum<top::contract_common::error::xerrc_t> : std::true_type {};

NS_END1
