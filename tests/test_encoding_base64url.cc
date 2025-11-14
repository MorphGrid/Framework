// Copyright (C) 2025 Ian Torres <iantorres@outlook.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include <gtest/gtest.h>

#include <framework/encoding.hpp>

using namespace framework;

TEST(test_encoding_base64url, empty_string) {
  const std::string _empty = "";
  EXPECT_EQ(base64url_encode(_empty), "");
  EXPECT_EQ(base64url_decode(""), "");
}

TEST(test_encoding_base64url, simple_string) {
  const std::string _simple = "Man";
  const std::string _encoded = base64url_encode(_simple);
  const std::string _decoded = base64url_decode(_encoded);

  EXPECT_EQ(_encoded, "TWFu");
  EXPECT_EQ(_decoded, _simple);
}

TEST(test_encoding_base64url, padding_test) {
  const std::string _pad1 = "Ma";
  const std::string _pad2 = "M";

  EXPECT_EQ(base64url_encode(_pad1), "TWE=");
  EXPECT_EQ(base64url_decode("TWE="), _pad1);

  EXPECT_EQ(base64url_encode(_pad2), "TQ==");
  EXPECT_EQ(base64url_decode("TQ=="), _pad2);
}

TEST(test_encoding_base64url, multiple_of_three) {
  EXPECT_EQ(base64url_encode("AAA"), "QUFB");
  EXPECT_EQ(base64url_encode("AA"), "QUE=");
  EXPECT_EQ(base64url_encode("A"), "QQ==");
}

TEST(test_encoding_base64url, reversibility) {
  std::vector<std::string> _samples = {
      "", "Hello", "Man", "Base64URL test string", "1234567890", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"};

  for (const auto& _s : _samples) {
    std::string _encoded = base64url_encode(_s);
    std::string _decoded = base64url_decode(_encoded);
    EXPECT_EQ(_decoded, _s);
  }
}

TEST(test_encoding_base64url, binary_data) {
  const std::string _binary = "\x{00}\x{FF}\x{10}\x{80}\x{7F}";
  const std::string _encoded = base64url_encode(_binary);
  const std::string _decoded = base64url_decode(_encoded);
  EXPECT_EQ(_decoded, _binary);
}

TEST(test_encoding_base64url, full_ascii) {
  for (int _iterations = 0; _iterations < 128; ++_iterations) {
    std::string _payload(1, static_cast<char>(_iterations));
    std::string _encoded = base64url_encode(_payload);
    std::string _decoded = base64url_decode(_encoded);
    EXPECT_EQ(_decoded, _payload);
  }
}

TEST(test_encoding_base64url, no_padding) {
  const std::string _payload = "HelloWorld";
  const std::string _encode_no_padding = base64url_encode(_payload, false);
  const std::string _decode_no_padding = base64url_decode(_encode_no_padding);
  EXPECT_EQ(_decode_no_padding, _payload);
}

TEST(test_encoding_base64url, invalid_chars) {
  const std::string _invalid = "!@#$%^&*()";
  const std::string _decoded = base64url_decode(_invalid);
  EXPECT_EQ(_decoded, "");
}

TEST(test_encoding_base64url, incorrect_padding) {
  const std::string _encoded = "TWE===";
  const std::string _decoded = base64url_decode(_encoded);
  EXPECT_EQ(_decoded, "Ma");
}

TEST(test_encoding_base64url, large_string) {
  const std::string _large(1024 * 1024, 'A');
  const std::string _encoded = base64url_encode(_large);
  const std::string _decoded = base64url_decode(_encoded);
  EXPECT_EQ(_decoded, _large);
}