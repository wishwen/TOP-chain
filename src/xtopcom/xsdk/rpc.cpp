#include <iostream>
#include "rpc.h"
#include "xdata/xblock.h"

int verify_raw_elect_block(char* buf, size_t buf_len) {
    std::cout << std::string(buf) << " " << buf_len << std::endl;
    top::base::xstream_t stream{top::base::xcontext_t::instance(), (uint8_t *)buf, static_cast<uint32_t>(buf_len)};
    top::data::xblock_t* block = dynamic_cast<top::data::xblock_t*>(top::data::xblock_t::full_block_read_from(stream));  // for block sync
    if (block == nullptr) {
        return -1;
    }
    std::cout << block->get_account() << std::endl;
    std::cout << block->get_height() << std::endl;
    return 0;
}

