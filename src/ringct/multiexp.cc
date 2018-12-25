// Copyright (c) 2017, The Monero Project
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
// Adapted from Python code by Sarang Noether

#include "misc_log_ex.h"
#include "common/perf_timer.h"
extern "C"
{
#include "crypto/crypto-ops.h"
}
#include "rctOps.h"
#include "multiexp.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "multiexp"

//#define MULTIEXP_PERF(x) x
#define MULTIEXP_PERF(x)

namespace rct
{

static inline bool operator<(const rct::key &k0, const rct::key&k1)
{
  for (int n = 31; n >= 0; --n)
  {
    if (k0.bytes[n] < k1.bytes[n])
      return true;
    if (k0.bytes[n] > k1.bytes[n])
      return false;
  }
  return false;
}

static inline rct::key div2(const rct::key &k)
{
  rct::key res;
  int carry = 0;
  for (int n = 31; n >= 0; --n)
  {
    int new_carry = (k.bytes[n] & 1) << 7;
    res.bytes[n] = k.bytes[n] / 2 + carry;
    carry = new_carry;
  }
  return res;
}

static inline rct::key pow2(size_t n)
{
  CHECK_AND_ASSERT_THROW_MES(n < 256, "Invalid pow2 argument");
  rct::key res = rct::zero();
  res[n >> 3] |= 1<<(n&7);
  return res;
}

rct::key bos_coster_heap_conv(std::vector<MultiexpData> data)
{
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(bos_coster, 1000000));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(setup, 1000000));
  size_t points = data.size();
  CHECK_AND_ASSERT_THROW_MES(points > 1, "Not enough points");
  std::vector<size_t> heap(points);
  for (size_t n = 0; n < points; ++n)
    heap[n] = n;

  auto Comp = [&](size_t e0, size_t e1) { return data[e0].scalar < data[e1].scalar; };
  std::make_heap(heap.begin(), heap.end(), Comp);
  MULTIEXP_PERF(PERF_TIMER_STOP(setup));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(loop, 1000000));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(pop, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(pop));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(add, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(add));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(sub, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(sub));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(push, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(push));
  while (heap.size() > 1)
  {
    MULTIEXP_PERF(PERF_TIMER_RESUME(pop));
    std::pop_heap(heap.begin(), heap.end(), Comp);
    size_t index1 = heap.back();
    heap.pop_back();
    std::pop_heap(heap.begin(), heap.end(), Comp);
    size_t index2 = heap.back();
    heap.pop_back();
    MULTIEXP_PERF(PERF_TIMER_PAUSE(pop));

    MULTIEXP_PERF(PERF_TIMER_RESUME(add));
    ge_cached cached;
    ge_p3_to_cached(&cached, &data[index1].point);
    ge_p1p1 p1;
    ge_add(&p1, &data[index2].point, &cached);
    ge_p1p1_to_p3(&data[index2].point, &p1);
    MULTIEXP_PERF(PERF_TIMER_PAUSE(add));

    MULTIEXP_PERF(PERF_TIMER_RESUME(sub));
    sc_sub(data[index1].scalar.bytes, data[index1].scalar.bytes, data[index2].scalar.bytes);
    MULTIEXP_PERF(PERF_TIMER_PAUSE(sub));

    MULTIEXP_PERF(PERF_TIMER_RESUME(push));
    if (!(data[index1].scalar == rct::zero()))
    {
      heap.push_back(index1);
      std::push_heap(heap.begin(), heap.end(), Comp);
    }

    heap.push_back(index2);
    std::push_heap(heap.begin(), heap.end(), Comp);
    MULTIEXP_PERF(PERF_TIMER_PAUSE(push));
  }
  MULTIEXP_PERF(PERF_TIMER_STOP(push));
  MULTIEXP_PERF(PERF_TIMER_STOP(sub));
  MULTIEXP_PERF(PERF_TIMER_STOP(add));
  MULTIEXP_PERF(PERF_TIMER_STOP(pop));
  MULTIEXP_PERF(PERF_TIMER_STOP(loop));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(end, 1000000));
  //return rct::scalarmultKey(data[index1].point, data[index1].scalar);
  std::pop_heap(heap.begin(), heap.end(), Comp);
  size_t index1 = heap.back();
  heap.pop_back();
  ge_p2 p2;
  ge_scalarmult(&p2, data[index1].scalar.bytes, &data[index1].point);
  rct::key res;
  ge_tobytes(res.bytes, &p2);
  return res;
}

rct::key bos_coster_heap_conv_robust(std::vector<MultiexpData> data)
{
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(bos_coster, 1000000));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(setup, 1000000));
  size_t points = data.size();
  CHECK_AND_ASSERT_THROW_MES(points > 1, "Not enough points");
  std::vector<size_t> heap;
  heap.reserve(points);
  for (size_t n = 0; n < points; ++n)
  {
    if (!(data[n].scalar == rct::zero()) && memcmp(&data[n].point, &ge_p3_identity, sizeof(ge_p3)))
      heap.push_back(n);
  }
  points = heap.size();

  auto Comp = [&](size_t e0, size_t e1) { return data[e0].scalar < data[e1].scalar; };
  std::make_heap(heap.begin(), heap.end(), Comp);
  MULTIEXP_PERF(PERF_TIMER_STOP(setup));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(loop, 1000000));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(pop, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(pop));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(div, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(div));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(add, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(add));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(sub, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(sub));
  MULTIEXP_PERF(PERF_TIMER_START_UNIT(push, 1000000)); MULTIEXP_PERF(PERF_TIMER_PAUSE(push));
  while (heap.size() > 1)
  {
    MULTIEXP_PERF(PERF_TIMER_RESUME(pop));
    std::pop_heap(heap.begin(), heap.end(), Comp);
    size_t index1 = heap.back();
    heap.pop_back();
    std::pop_heap(heap.begin(), heap.end(), Comp);
    size_t index2 = heap.back();
    heap.pop_back();
    MULTIEXP_PERF(PERF_TIMER_PAUSE(pop));

    ge_cached cached;
    ge_p1p1 p1;

    MULTIEXP_PERF(PERF_TIMER_RESUME(div));
    while (1)
    {
      rct::key s1_2 = div2(data[index1].scalar);
      if (!(data[index2].scalar < s1_2))
       break;
      if (data[index1].scalar.bytes[0] & 1)
      {
        data.resize(data.size()+1);
        data.back().scalar = rct::identity();
        data.back().point = data[index1].point;
        heap.push_back(data.size() - 1);
        std::push_heap(heap.begin(), heap.end(), Comp);
      }
      data[index1].scalar = div2(data[index1].scalar);
      ge_p3_to_cached(&cached, &data[index1].point);
      ge_add(&p1, &data[index1].point, &cached);
      ge_p1p1_to_p3(&data[index1].point, &p1);
    }
    MULTIEXP_PERF(PERF_TIMER_PAUSE(div));

    MULTIEXP_PERF(PERF_TIMER_RESUME(add));
    ge_p3_to_cached(&cached, &data[index1].point);
    ge_add(&p1, &data[index2].point, &cached);
    ge_p1p1_to_p3(&data[index2].point, &p1);
    MULTIEXP_PERF(PERF_TIMER_PAUSE(add));

    MULTIEXP_PERF(PERF_TIMER_RESUME(sub));
    sc_sub(data[index1].scalar.bytes, data[index1].scalar.bytes, data[index2].scalar.bytes);
    MULTIEXP_PERF(PERF_TIMER_PAUSE(sub));

    MULTIEXP_PERF(PERF_TIMER_RESUME(push));
    if (!(data[index1].scalar == rct::zero()))
    {
      heap.push_back(index1);
      std::push_heap(heap.begin(), heap.end(), Comp);
    }

    heap.push_back(index2);
    std::push_heap(heap.begin(), heap.end(), Comp);
    MULTIEXP_PERF(PERF_TIMER_PAUSE(push));
  }
  MULTIEXP_PERF(PERF_TIMER_STOP(push));
  MULTIEXP_PERF(PERF_TIMER_STOP(sub));
  MULTIEXP_PERF(PERF_TIMER_STOP(add));
  MULTIEXP_PERF(PERF_TIMER_STOP(pop));
  MULTIEXP_PERF(PERF_TIMER_STOP(loop));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(end, 1000000));
  //return rct::scalarmultKey(data[index1].point, data[index1].scalar);
  std::pop_heap(heap.begin(), heap.end(), Comp);
  size_t index1 = heap.back();
  heap.pop_back();
  ge_p2 p2;
  ge_scalarmult(&p2, data[index1].scalar.bytes, &data[index1].point);
  rct::key res;
  ge_tobytes(res.bytes, &p2);
  return res;
}

rct::key straus(const std::vector<MultiexpData> &data, bool HiGi)
{
  MULTIEXP_PERF(PERF_TIMER_UNIT(straus, 1000000));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(setup, 1000000));
  static constexpr unsigned int c = 4;
  static constexpr unsigned int mask = (1<<c)-1;
  static std::vector<std::vector<ge_cached>> HiGi_multiples;
  std::vector<std::vector<ge_cached>> local_multiples, &multiples = HiGi ? HiGi_multiples : local_multiples;
  ge_cached cached;
  ge_p1p1 p1;
  ge_p3 p3;

  std::vector<uint8_t> skip(data.size());
  for (size_t i = 0; i < data.size(); ++i)
    skip[i] = data[i].scalar == rct::zero() || !memcmp(&data[i].point, &ge_p3_identity, sizeof(ge_p3));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(multiples, 1000000));
  multiples.resize(1<<c);
  size_t offset = multiples[1].size();
  multiples[1].resize(std::max(offset, data.size()));
  for (size_t i = offset; i < data.size(); ++i)
    ge_p3_to_cached(&multiples[1][i], &data[i].point);
  for (size_t i=2;i<1<<c;++i)
    multiples[i].resize(std::max(offset, data.size()));
  for (size_t j=offset;j<data.size();++j)
  {
    for (size_t i=2;i<1<<c;++i)
    {
      ge_add(&p1, &data[j].point, &multiples[i-1][j]);
      ge_p1p1_to_p3(&p3, &p1);
      ge_p3_to_cached(&multiples[i][j], &p3);
    }
  }
  MULTIEXP_PERF(PERF_TIMER_STOP(multiples));

  MULTIEXP_PERF(PERF_TIMER_START_UNIT(digits, 1000000));
  std::vector<std::vector<uint8_t>> digits;
  digits.resize(data.size());
  for (size_t j = 0; j < data.size(); ++j)
  {
    digits[j].resize(256);
    unsigned char bytes33[33];
    memcpy(bytes33,  data[j].scalar.bytes, 32);
    bytes33[32] = 0;
#if 1
    static_assert(c == 4, "optimized version needs c == 4");
    const unsigned char *bytes = bytes33;
    unsigned int i;
    for (i = 0; i < 256; i += 8, bytes++)
    {
      digits[j][i] = bytes[0] & 0xf;
      digits[j][i+1] = (bytes[0] >> 1) & 0xf;
      digits[j][i+2] = (bytes[0] >> 2) & 0xf;
      digits[j][i+3] = (bytes[0] >> 3) & 0xf;
      digits[j][i+4] = ((bytes[0] >> 4) | (bytes[1]<<4)) & 0xf;
      digits[j][i+5] = ((bytes[0] >> 5) | (bytes[1]<<3)) & 0xf;
      digits[j][i+6] = ((bytes[0] >> 6) | (bytes[1]<<2)) & 0xf;
      digits[j][i+7] = ((bytes[0] >> 7) | (bytes[1]<<1)) & 0xf;
    }
#elif 1
    for (size_t i = 0; i < 256; ++i)
      digits[j][i] = ((bytes[i>>3] | (bytes[(i>>3)+1]<<8)) >> (i&7)) & mask;
#else
    rct::key shifted = data[j].scalar;
    for (size_t i = 0; i < 256; ++i)
    {
      digits[j][i] = shifted.bytes[0] & 0xf;
      shifted = div2(shifted, (256-i)>>3);
    }
#endif
  }
  MULTIEXP_PERF(PERF_TIMER_STOP(digits));

  rct::key maxscalar = rct::zero();
  for (size_t i = 0; i < data.size(); ++i)
    if (maxscalar < data[i].scalar)
      maxscalar = data[i].scalar;
  size_t i = 0;
  while (i < 256 && !(maxscalar < pow2(i)))
    i += c;
  MULTIEXP_PERF(PERF_TIMER_STOP(setup));

  ge_p3 res_p3 = ge_p3_identity;
  if (!(i < c))
    goto skipfirst;
  while (!(i < c))
  {
    for (size_t j = 0; j < c; ++j)
    {
      ge_p3_to_cached(&cached, &res_p3);
      ge_add(&p1, &res_p3, &cached);
      ge_p1p1_to_p3(&res_p3, &p1);
    }
skipfirst:
    i -= c;
    for (size_t j = 0; j < data.size(); ++j)
    {
      if (skip[j])
        continue;
      int digit = digits[j][i];
      if (digit)
      {
        ge_add(&p1, &res_p3, &multiples[digit][j]);
        ge_p1p1_to_p3(&res_p3, &p1);
      }
    }
  }

  rct::key res;
  ge_p3_tobytes(res.bytes, &res_p3);
  return res;
}

}
