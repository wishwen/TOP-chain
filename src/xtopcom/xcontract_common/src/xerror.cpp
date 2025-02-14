// Copyright (c) 2017-2018 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xcontract_common/xerror/xerror.h"

NS_BEG3(top, contract_common, error)

static char const * const errc_to_string(int code) {
    auto const ec = static_cast<xerrc_t>(code);

    switch (ec) {
    case xerrc_t::ok:
        return "ok";

    case xerrc_t::property_internal_error:
        return "property internal_error";

    case xerrc_t::property_permission_not_allowed:
        return "property permission not allowed";

    case xerrc_t::property_already_exist:
        return "property already exist";

    case xerrc_t::receipt_data_not_found:
        return "receipt data not found";

    case xerrc_t::property_not_exist:
        return "property not exist";

    case xerrc_t::property_map_key_not_exist:
        return "map property: key not exist";

    case xerrc_t::receipt_data_already_exist:
        return "receipt data already exist";

    case xerrc_t::account_not_matched:
        return "account not matched";

    case xerrc_t::token_not_enough:
        return "token not enough";

    case xerrc_t::deploy_code_failed:
        return "deploy code failed";

    default:
        return "unknown contract runtime error";
    }
}

std::error_code make_error_code(xerrc_t const errc) noexcept {
    return std::error_code(static_cast<int>(errc), contract_common_category());
}

std::error_condition make_error_condition(xerrc_t const errc) noexcept {
    return std::error_condition(static_cast<int>(errc), contract_common_category());
}

xtop_contract_common_error::xtop_contract_common_error() : std::runtime_error{make_error_code(xerrc_t::ok).message()} {
}

xtop_contract_common_error::xtop_contract_common_error(xerrc_t const error_code) : xtop_contract_common_error{make_error_code(error_code)} {
}

xtop_contract_common_error::xtop_contract_common_error(xerrc_t const error_code, std::string const & message) : xtop_contract_common_error{make_error_code(error_code), message} {
}

xtop_contract_common_error::xtop_contract_common_error(std::error_code const & ec) : std::runtime_error{ec.message()}, ec_{ec} {
}

xtop_contract_common_error::xtop_contract_common_error(std::error_code const & ec, const std::string& message) : std::runtime_error{message}, ec_{ec} {
}

const std::error_code & xtop_contract_common_error::code() const noexcept {
    return ec_;
}

class xtop_contract_common_category final : public std::error_category {
    char const * name() const noexcept override {
        return "contract_common";
    }

    std::string message(int errc) const override {
        return errc_to_string(errc);
    }
};
using xcontract_common_category_t = xtop_contract_common_category;

std::error_category const & contract_common_category() {
    static xcontract_common_category_t category{};
    return category;
}

NS_END3
