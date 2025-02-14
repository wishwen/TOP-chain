// Copyright (c) 2017-2020 Telos Foundation & contributors
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "xbase/xdata.h"
#include "xbasic/xmemory.hpp"
#include "xstore/xstore_face.h"
#include "xtxpool_service_v2/xtxpool_service_face.h"
#include "xtxpool_v2/xtxpool_face.h"

#include <atomic>
#include <string>

NS_BEG2(top, xtxpool_service_v2)

class xtxpool_svc_para_t : public base::xobject_t {
public:
    xtxpool_svc_para_t(const observer_ptr<store::xstore_face_t> & store,
                       const observer_ptr<base::xvblockstore_t> & blockstore,
                       const observer_ptr<xtxpool_v2::xtxpool_face_t> & txpool)
      : m_store(store), m_blockstore(blockstore), m_txpool(txpool) {
    }

public:
    void set_dispatcher(const observer_ptr<xtxpool_service_dispatcher_t> & dispatcher) {
        m_dispatcher = dispatcher;
    };
    store::xstore_face_t * get_store() const {
        return m_store.get();
    }
    base::xvblockstore_t * get_vblockstore() const {
        return m_blockstore.get();
    }
    xtxpool_v2::xtxpool_face_t * get_txpool() const {
        return m_txpool.get();
    }
    xtxpool_service_dispatcher_t * get_dispatcher() const {
        return m_dispatcher.get();
    }

private:
    observer_ptr<store::xstore_face_t> m_store{nullptr};
    observer_ptr<base::xvblockstore_t> m_blockstore{nullptr};
    observer_ptr<xtxpool_v2::xtxpool_face_t> m_txpool{nullptr};
    observer_ptr<xtxpool_service_dispatcher_t> m_dispatcher{nullptr};
};

NS_END2
