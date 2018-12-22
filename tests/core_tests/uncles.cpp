// Copyright (c) 2018, The Masari Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "uncles.h"

using namespace std;

using namespace epee;
using namespace cryptonote;

// TODO-TK: hacky reuse - look for something better
#define CHAIN_BASE(DIFFICULTY) \
  GENERATE_ACCOUNT(first_miner_account); \
  GENERATE_ACCOUNT(uncle_miner_account); \
  MAKE_GENESIS_BLOCK(events, blk_a, first_miner_account, 0); \
  events.push_back(first_miner_account); \
  events.push_back(uncle_miner_account); \
  REWIND_BLOCKS_VDN(events, blk_b, blk_a, first_miner_account, 1, 9, DIFFICULTY); \
  REWIND_BLOCKS_VDN(events, blk_c, blk_b, first_miner_account, 2, 10, DIFFICULTY); \
  REWIND_BLOCKS_VDN(events, blk_d, blk_c, first_miner_account, 3, 10, DIFFICULTY); \
  REWIND_BLOCKS_VDN(events, blk_e, blk_d, first_miner_account, 4, 10, DIFFICULTY); \
  REWIND_BLOCKS_VDN(events, blk_f, blk_e, first_miner_account, 5, 10, DIFFICULTY); \
  REWIND_BLOCKS_VDN(events, blk_g, blk_f, first_miner_account, 6, 10, DIFFICULTY);

//-----------------------------------------------------------------------------------------------------
bool gen_uncles_base::generate_with(std::vector<test_event_entry> &events, const std::function<bool(std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)> &add_blocks, const uint64_t &difficulty /*= 1*/) const
{
  CHAIN_BASE(difficulty);
  REWIND_BLOCKS_VDN(events, blk_h, blk_g, first_miner_account, 7, 10, difficulty);
  REWIND_BLOCKS_VDN(events, blk_i, blk_h, first_miner_account, 8, 1, difficulty);

  MAKE_NEXT_BLOCKVD(events, blk_0a, blk_i, first_miner_account, 8, difficulty);

  MAKE_NEXT_BLOCKVD(events, blk_0b, blk_i, uncle_miner_account, 8, difficulty);

  bool r = add_blocks(events, blk_0a, blk_0b, first_miner_account, uncle_miner_account, generator);
  if (!r) {
    return false;
  }

  return true;
}

// valid
//--------------------------------------------------------------------------

bool gen_uncle::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, top_bl, original_miner, 8, alt_bl);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_reorg::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, alt_bl, original_miner, 8, top_bl);
    DO_CALLBACK(events, "check_uncle_reorg");
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncles_base::check_uncle_reorg(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  std::list<cryptonote::block> block_list;
  bool r = c.get_blocks(0, 73, block_list);
  if (!r) return false;

  map_hash2tx_t mtx;
  std::vector<cryptonote::block> chain;
  std::vector<cryptonote::block> blocks(block_list.begin(), block_list.end());
  r = find_block_chain(events, chain, mtx, get_block_hash(blocks.back()));

  cryptonote::account_base miner_acc = boost::get<cryptonote::account_base>(events[1]);
  cryptonote::account_base uncle_miner_acc = boost::get<cryptonote::account_base>(events[2]);

  uint64_t balance = get_balance(miner_acc, blocks, mtx);
  uint64_t expected_balance = 2498803116737595;

  uint64_t uncle_balance = get_balance(uncle_miner_acc, blocks, mtx);
  uint64_t expected_uncle_balance = 17589820613753;

  CHECK_AND_ASSERT_MES(balance == expected_balance, false, "Balance " << balance << " doesn't match expected amount " << expected_balance);
  CHECK_AND_ASSERT_MES(uncle_balance == expected_uncle_balance, false, "Balance " << uncle_balance << " doesn't match expected amount " << expected_uncle_balance);
  return true;
}

bool gen_uncle_alt_nephews::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, alt_bl, original_miner, 8, top_bl);
    // no reorg + nephew mining an uncle
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew0, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew1, top_bl, original_miner, 8, alt_bl);

    // no reorg between two different nephews
    MAKE_NEXT_BLOCKV(events, new_top, nephew0, original_miner, 8);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_reorg_alt_nephews::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    // no reorg + nephew mining an uncle
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew0, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew1, alt_bl, uncle_miner, 8, top_bl);

    // reorg between two different nephews
    MAKE_NEXT_BLOCKV(events, new_top, nephew1, original_miner, 8);
    DO_CALLBACK(events, "check_uncle_reorg_alt_nephews");
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncles_base::check_uncle_reorg_alt_nephews(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  std::list<cryptonote::block> block_list;
  bool r = c.get_blocks(0, 74, block_list);
  if (!r) return false;

  map_hash2tx_t mtx;
  std::vector<cryptonote::block> chain;
  std::vector<cryptonote::block> blocks(block_list.begin(), block_list.end());
  r = find_block_chain(events, chain, mtx, get_block_hash(blocks.back()));

  cryptonote::account_base miner_acc = boost::get<cryptonote::account_base>(events[1]);
  cryptonote::account_base uncle_miner_acc = boost::get<cryptonote::account_base>(events[2]);

  uint64_t balance = get_balance(miner_acc, chain, mtx);
  uint64_t expected_balance = 2497923609770711;
  uint64_t uncle_balance = get_balance(uncle_miner_acc, chain, mtx);
  uint64_t expected_uncle_balance = 36059114644485; // one block + uncle (??)

  CHECK_AND_ASSERT_MES(balance == expected_balance, false, "Balance " << balance << " doesn't match expected amount " << expected_balance);
  CHECK_AND_ASSERT_MES(uncle_balance == expected_uncle_balance, false, "Balance " << uncle_balance << " doesn't match expected amount " << expected_uncle_balance);
  return true;
}

bool gen_uncle_alt_nephews_as_uncle::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, alt_bl, original_miner, 8, top_bl);
    // no reorg + nephew mining an uncle
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew0, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew1, top_bl, original_miner, 8, alt_bl);

    // no reorg between two different nephews and mine alt as an uncle
    MAKE_NEXT_BLOCKV_UNCLE(events, new_top, nephew0, original_miner, 8, nephew1);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_reorg_alt_nephews_as_uncle::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    // no reorg + nephew mining an uncle
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew0, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew1, alt_bl, uncle_miner, 8, top_bl);

    // reorg between two different nephews and mine the other as an uncle
    MAKE_NEXT_BLOCKV_UNCLE(events, new_top, nephew1, original_miner, 8, nephew0);
    DO_CALLBACK(events, "check_uncle_reorg_alt_nephews_as_uncle");
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncles_base::check_uncle_reorg_alt_nephews_as_uncle(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  std::list<cryptonote::block> block_list;
  bool r = c.get_blocks(0, 74, block_list);
  if (!r) return false;

  map_hash2tx_t mtx;
  std::vector<cryptonote::block> chain;
  std::vector<cryptonote::block> blocks(block_list.begin(), block_list.end());
  r = find_block_chain(events, chain, mtx, get_block_hash(blocks.back()));

  cryptonote::account_base miner_acc = boost::get<cryptonote::account_base>(events[1]);
  cryptonote::account_base uncle_miner_acc = boost::get<cryptonote::account_base>(events[2]);

  uint64_t balance = get_balance(miner_acc, blocks, mtx);
  uint64_t expected_balance = 2498803099123903;
  uint64_t uncle_balance = get_balance(uncle_miner_acc, blocks, mtx);
  uint64_t expected_uncle_balance = 36059114644485; // one block + uncle

  CHECK_AND_ASSERT_MES(balance == expected_balance, false, "Balance " << balance << " doesn't match expected amount " << expected_balance);
  CHECK_AND_ASSERT_MES(uncle_balance == expected_uncle_balance, false, "Balance " << uncle_balance << " doesn't match expected amount " << expected_uncle_balance);
  return true;
}
// invalid
//--------------------------------------------------------------------------

bool gen_uncle_is_parent::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    DO_CALLBACK(events, "mark_invalid_block");
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, top_bl, original_miner, 8, top_bl);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_wrong_uncle::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, top_bl, original_miner, 8, alt_bl);

    crypto::hash h_original = get_block_hash(nephew);

    nephew.uncle = crypto::null_hash;
    nephew.invalidate_hashes();
    crypto::hash h_modified = get_block_hash(nephew);

    return h_original != h_modified;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_wrong_height::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV(events, new_bl, top_bl, original_miner, 8);
    DO_CALLBACK(events, "mark_invalid_block");
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew, new_bl, original_miner, 8, alt_bl);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_wrong_version::generate(std::vector<test_event_entry>& events) const
{
  CHAIN_BASE(1);
  REWIND_BLOCKS_VN(events, blk_h, blk_g, first_miner_account, 7, 1);
  MAKE_NEXT_BLOCKV(events, blk_0a, blk_h, first_miner_account, 7);
  MAKE_NEXT_BLOCKV(events, blk_0b, blk_h, first_miner_account, 7);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCKV_UNCLE(events, blk_1, blk_0a, first_miner_account, 7, blk_0b);
  return true;
}

bool gen_uncle_bad_ancestry::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV(events, bl_i0, top_bl, original_miner, 8);
    MAKE_NEXT_BLOCKV(events, bl_j0, alt_bl, original_miner, 8);
    DO_CALLBACK(events, "mark_invalid_block");
    MAKE_NEXT_BLOCKV_UNCLE(events, bl_i1, bl_i0, original_miner, 8, bl_j0);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_bad_timestamp::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, bl_new, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV(events, bl_i0, bl_new, original_miner, 8);

    cryptonote::block bl_timewarped;
    generator.construct_block_manually(bl_timewarped, bl_new, original_miner, test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_hf_version | test_generator::bf_timestamp, 8, 8, 8840, crypto::hash(), 1, transaction(), std::vector<crypto::hash>(), 0, 0, 8);
    events.push_back(bl_timewarped);

    DO_CALLBACK(events, "mark_invalid_block");
    MAKE_NEXT_BLOCKV_UNCLE(events, bl_i1, bl_i0, original_miner, 8, bl_timewarped);
    return true;
  };
  return generate_with(events, modifier, 1);
}

bool gen_uncle_too_far_extended_ancestry::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew_i0, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew_j0, alt_bl, original_miner, 8, top_bl);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew_i1, nephew_i0, original_miner, 8, nephew_j0);
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew_j1, nephew_j0, original_miner, 8, nephew_i0);

    DO_CALLBACK(events, "mark_invalid_block");
    MAKE_NEXT_BLOCKV_UNCLE(events, nephew_i2, nephew_i1, original_miner, 8, nephew_j1);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_wrong_out::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    MAKE_NEXT_BLOCKV_UNCLE(events, blk_i0, top_bl, original_miner, 8, alt_bl);
    MAKE_NEXT_BLOCKV(events, blk_j0, top_bl, original_miner, 8);
    MAKE_NEXT_BLOCKV(events, blk_k0, top_bl, original_miner, 8);
    DO_CALLBACK(events, "mark_invalid_block");
    cryptonote::block blk_i1;
    blk_i1.uncle = cryptonote::get_block_hash(blk_k0);
    PUSH_NEXT_BLOCKV_UNCLE(events, blk_i1, blk_i0, original_miner, 8, blk_j0);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_wrong_amount::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    cryptonote::block nephew;
    nephew.uncle = cryptonote::get_block_hash(alt_bl);

    generator.construct_block_manually(nephew, top_bl, original_miner, test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_hf_version | test_generator::bf_timestamp | test_generator::bf_uncle, 8, 8, 8840, crypto::hash(), 1, transaction(), std::vector<crypto::hash>(), 0, 0, 8, 0, alt_bl, 0, 0, 1);
    DO_CALLBACK(events, "mark_invalid_block");
    events.push_back(nephew);
    return true;
  };
  return generate_with(events, modifier);
}

bool gen_uncle_overflow_amount::generate(std::vector<test_event_entry>& events) const
{
  auto modifier = [](std::vector<test_event_entry> &events, const cryptonote::block &top_bl, const cryptonote::block &alt_bl, const cryptonote::account_base &original_miner, const cryptonote::account_base &uncle_miner, test_generator &generator)
  {
    cryptonote::block nephew;
    nephew.uncle = cryptonote::get_block_hash(alt_bl);

    uint64_t max_uint = -1; //18446744073709551615UL
    generator.construct_block_manually(nephew, top_bl, original_miner, test_generator::bf_major_ver | test_generator::bf_minor_ver | test_generator::bf_hf_version | test_generator::bf_timestamp | test_generator::bf_uncle | test_generator::bf_ul_reward, 8, 8, 8840, crypto::hash(), 1, transaction(), std::vector<crypto::hash>(), 0, 0, 8, 0, alt_bl, 0, max_uint, 1);
    DO_CALLBACK(events, "mark_invalid_block");
    events.push_back(nephew);
    return true;
  };
  return generate_with(events, modifier);
}
