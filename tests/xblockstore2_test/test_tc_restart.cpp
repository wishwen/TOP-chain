#include "gtest/gtest.h"

#include "xbase/xcontext.h"
#include "xbase/xdata.h"
#include "xbase/xobject.h"
#include "xbase/xmem.h"
#include "xbase/xcontext.h"
// TODO(jimmy) #include "xbase/xvledger.h"

#include "xdata/xdatautil.h"
#include "xdata/xemptyblock.h"
#include "xdata/xblocktool.h"
#include "xdata/xlightunit.h"
#include "xdata/xnative_contract_address.h"
#include "xmbus/xevent_store.h"
#include "xmbus/xmessage_bus.h"

#include "xstore/xstore.h"
#include "xblockstore/xblockstore_face.h"
#include "tests/mock/xvchain_creator.hpp"
#include "tests/mock/xdatamock_table.hpp"

#include <chrono>

using namespace top;
using namespace top::mock;
using namespace top::base;
using namespace top::mbus;
using namespace top::store;
using namespace top::data;

class test_tc_restart : public testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
    static void TearDownTestCase() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::string cmd = "rm -rf " + tc_restart_db_path;
        // system(cmd.c_str());

    }
    static void SetUpTestCase() {
    }
    static std::string tc_restart_db_path;
};
std::string test_tc_restart::tc_restart_db_path = "test_tc_restart_db";

#if 0
TEST_F(test_tc_restart, block_connect_restart_0_BENCH) {
    mock::xvchain_creator creator;
    base::xvblockstore_t* blockstore = creator.get_blockstore();

    std::string address = sys_contract_beacon_timer_addr;
    const std::vector<std::string>  unit_addrs;
    mock::xdatamock_table mocktable(address, unit_addrs);
    uint64_t max_block_height = 100;
    mocktable.genrate_table_chain(max_block_height);
    const std::vector<xblock_ptr_t> & blocks = mocktable.get_history_tables();
    xassert(blocks.size() == max_block_height + 1);

    for (uint64_t i = 1; i <= max_block_height; i++) {
        if (i % 3 == 0) {
            continue;
        }
        blockstore->store_block(mocktable, blocks[i].get());
    }

    base::xauto_ptr<xvblock_t> connected_block = blockstore->get_latest_connected_block(address);
    ASSERT_TRUE(connected_block != nullptr);
    ASSERT_EQ(connected_block->get_height(), max_block_height);

    base::xauto_ptr<xvblock_t> executed_block = blockstore->get_latest_executed_block(address);
    ASSERT_TRUE(executed_block != nullptr);
    ASSERT_EQ(executed_block->get_height(), max_block_height);

    // outdated block
    ASSERT_TRUE(blockstore->store_block(mocktable, blocks[3].get()));
    std::this_thread::sleep_for(std::chrono::seconds(2));

    sleep(100);  // account will close after idle time

    base::xauto_ptr<xvblock_t> connected_block_2 = blockstore->get_latest_connected_block(address);
    ASSERT_TRUE(connected_block_2 != nullptr);
    ASSERT_EQ(connected_block_2->get_height(), max_block_height);
}
#endif
