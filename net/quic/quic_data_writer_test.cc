// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_writer.h"

#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_data_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicDataWriterTest, WriteUInt8ToOffset) {
  QuicDataWriter writer(4);

  writer.WriteUInt32(0xfefdfcfb);
  EXPECT_TRUE(writer.WriteUInt8ToOffset(1, 0));
  EXPECT_TRUE(writer.WriteUInt8ToOffset(2, 1));
  EXPECT_TRUE(writer.WriteUInt8ToOffset(3, 2));
  EXPECT_TRUE(writer.WriteUInt8ToOffset(4, 3));

  scoped_ptr<char[]> data(writer.take());

  EXPECT_EQ(1, data[0]);
  EXPECT_EQ(2, data[1]);
  EXPECT_EQ(3, data[2]);
  EXPECT_EQ(4, data[3]);
}

TEST(QuicDataWriterDeathTest, WriteUInt8ToOffset) {
  QuicDataWriter writer(4);

#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(writer.WriteUInt8ToOffset(5, 4), "Check failed");
#else
  EXPECT_DEATH(writer.WriteUInt8ToOffset(5, 4), "Check failed");
#endif
#endif
}

TEST(QuicDataWriterTest, SanityCheckUFloat16Consts) {
  // Check the arithmetic on the constants - otherwise the values below make
  // no sense.
  EXPECT_EQ(30, kUFloat16MaxExponent);
  EXPECT_EQ(11, kUFloat16MantissaBits);
  EXPECT_EQ(12, kUFloat16MantissaEffectiveBits);
  EXPECT_EQ(GG_UINT64_C(0x3FFC0000000), kUFloat16MaxValue);
}

TEST(QuicDataWriterTest, WriteUFloat16) {
  struct TestCase {
    uint64 decoded;
    uint16 encoded;
  };
  TestCase test_cases[] = {
    // Small numbers represent themselves.
    { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 6 },
    { 7, 7 }, { 15, 15 }, { 31, 31 }, { 42, 42 }, { 123, 123 }, { 1234, 1234 },
    // Check transition through 2^11.
    { 2046, 2046 }, { 2047, 2047 }, { 2048, 2048 }, { 2049, 2049 },
    // Running out of mantissa at 2^12.
    { 4094, 4094 }, { 4095, 4095 }, { 4096, 4096 }, { 4097, 4096 },
    { 4098, 4097 }, { 4099, 4097 }, { 4100, 4098 }, { 4101, 4098 },
    // Check transition through 2^13.
    { 8190, 6143 }, { 8191, 6143 }, { 8192, 6144 }, { 8193, 6144 },
    { 8194, 6144 }, { 8195, 6144 }, { 8196, 6145 }, { 8197, 6145 },
    // Half-way through the exponents.
    { 0x7FF8000, 0x87FF }, { 0x7FFFFFF, 0x87FF }, { 0x8000000, 0x8800 },
    { 0xFFF0000, 0x8FFF }, { 0xFFFFFFF, 0x8FFF }, { 0x10000000, 0x9000 },
    // Transition into the largest exponent.
    { 0x1FFFFFFFFFE, 0xF7FF}, { 0x1FFFFFFFFFF, 0xF7FF},
    { 0x20000000000, 0xF800}, { 0x20000000001, 0xF800},
    { 0x2003FFFFFFE, 0xF800}, { 0x2003FFFFFFF, 0xF800},
    { 0x20040000000, 0xF801}, { 0x20040000001, 0xF801},
    // Transition into the max value and clamping.
    { 0x3FF80000000, 0xFFFE}, { 0x3FFBFFFFFFF, 0xFFFE},
    { 0x3FFC0000000, 0xFFFF}, { 0x3FFC0000001, 0xFFFF},
    { 0x3FFFFFFFFFF, 0xFFFF}, { 0x40000000000, 0xFFFF},
    { 0xFFFFFFFFFFFFFFFF, 0xFFFF},
  };
  int num_test_cases = sizeof(test_cases) / sizeof(test_cases[0]);

  for (int i = 0; i < num_test_cases; ++i) {
    QuicDataWriter writer(2);
    EXPECT_TRUE(writer.WriteUFloat16(test_cases[i].decoded));
    scoped_ptr<char[]> data(writer.take());
    EXPECT_EQ(test_cases[i].encoded, *reinterpret_cast<uint16*>(data.get()));
  }
}

TEST(QuicDataWriterTest, ReadUFloat16) {
  struct TestCase {
    uint64 decoded;
    uint16 encoded;
  };
  TestCase test_cases[] = {
    // There are fewer decoding test cases because encoding truncates, and
    // decoding returns the smallest expansion.
    // Small numbers represent themselves.
    { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 6 },
    { 7, 7 }, { 15, 15 }, { 31, 31 }, { 42, 42 }, { 123, 123 }, { 1234, 1234 },
    // Check transition through 2^11.
    { 2046, 2046 }, { 2047, 2047 }, { 2048, 2048 }, { 2049, 2049 },
    // Running out of mantissa at 2^12.
    { 4094, 4094 }, { 4095, 4095 }, { 4096, 4096 },
    { 4098, 4097 }, { 4100, 4098 },
    // Check transition through 2^13.
    { 8190, 6143 }, { 8192, 6144 }, { 8196, 6145 },
    // Half-way through the exponents.
    { 0x7FF8000, 0x87FF }, { 0x8000000, 0x8800 },
    { 0xFFF0000, 0x8FFF }, { 0x10000000, 0x9000 },
    // Transition into the largest exponent.
    { 0x1FFE0000000, 0xF7FF}, { 0x20000000000, 0xF800},
    { 0x20040000000, 0xF801},
    // Transition into the max value.
    { 0x3FF80000000, 0xFFFE}, { 0x3FFC0000000, 0xFFFF},
  };
  int num_test_cases = sizeof(test_cases) / sizeof(test_cases[0]);

  for (int i = 0; i < num_test_cases; ++i) {
    QuicDataReader reader(reinterpret_cast<char*>(&test_cases[i].encoded), 2);
    uint64 value;
    EXPECT_TRUE(reader.ReadUFloat16(&value));
    EXPECT_EQ(test_cases[i].decoded, value);
  }
}

TEST(QuicDataWriterTest, RoundTripUFloat16) {
  // Just test all 16-bit encoded values. 0 and max already tested above.
  uint64 previous_value = 0;
  for (uint16 i = 1; i < 0xFFFF; ++i) {
    // Read the two bytes.
    QuicDataReader reader(reinterpret_cast<char*>(&i), 2);
    uint64 value;
    // All values must be decodable.
    EXPECT_TRUE(reader.ReadUFloat16(&value));
    // Check that small numbers represent themselves
    if (i < 4097)
      EXPECT_EQ(i, value);
    // Check there's monotonic growth.
    EXPECT_LT(previous_value, value);
    // Check that precision is within 0.5% away from the denormals.
    if (i > 2000)
      EXPECT_GT(previous_value * 1005, value * 1000);
    // Check we're always within the promised range.
    EXPECT_LT(value, GG_UINT64_C(0x3FFC0000000));
    previous_value = value;
    QuicDataWriter writer(6);
    EXPECT_TRUE(writer.WriteUFloat16(value - 1));
    EXPECT_TRUE(writer.WriteUFloat16(value));
    EXPECT_TRUE(writer.WriteUFloat16(value + 1));
    scoped_ptr<char[]> data(writer.take());
    // Check minimal decoding (previous decoding has previous encoding).
    EXPECT_EQ(i-1, *reinterpret_cast<uint16*>(data.get()));
    // Check roundtrip.
    EXPECT_EQ(i, *reinterpret_cast<uint16*>(data.get() + 2));
    // Check next decoding.
    EXPECT_EQ(i < 4096? i+1 : i, *reinterpret_cast<uint16*>(data.get() + 4));
  }
}

}  // namespace
}  // namespace test
}  // namespace net
