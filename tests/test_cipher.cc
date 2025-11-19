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

#include <framework/cipher.hpp>
#include <framework/errors/cipher_error.hpp>

using namespace framework;

TEST(test_cipher, generate_sha256_returns_base64_string) {
  const auto _token = generate_sha_256();
  EXPECT_EQ(_token.size(), 44);  // 32 bytes → base64 → 44 chars
  EXPECT_TRUE(_token.find('=') == std::string::npos || _token.back() == '=');
}

TEST(test_cipher, hmac_same_input_and_key_produce_same_output) {
  const std::string _key = "12345678901234567890123456789012";  // 32 bytes
  const std::string _input = "some message";

  const auto _digest1 = hmac(_input, _key);
  const auto _digest2 = hmac(_input, _key);

  EXPECT_EQ(_digest1, _digest2);
}

TEST(test_cipher, hmac_different_keys_produce_different_output) {
  const std::string _input = "sensitive data";

  const auto _digest1 = hmac(_input, "key-one-123456789012345678901234");
  const auto _digest2 = hmac(_input, "key-two-123456789012345678901234");

  EXPECT_NE(_digest1, _digest2);
}

TEST(test_cipher, generate_aes_key_iv_produces_correct_lengths) {
  const auto [_key, _iv] = generate_aes_key_iv();

  EXPECT_EQ(_key.size(), 32);  // AES-256
  EXPECT_EQ(_iv.size(), 16);   // GCM IV
}

TEST(test_cipher, aes_encrypt_decrypt_cycle_returns_original) {
  const std::string _plaintext = "secret payload: 42";
  const auto [_key, _iv] = generate_aes_key_iv();

  const auto _encrypted = encrypt(_plaintext, _key, _iv);
  const auto _decrypted = decrypt(_encrypted, _key, _iv);

  EXPECT_EQ(_decrypted, _plaintext);
}

TEST(test_cipher, aes_decrypt_fails_with_wrong_key) {
  const std::string _plaintext = "attack at dawn";
  const auto [_key1, _iv] = generate_aes_key_iv();
  const auto [_key2, _] = generate_aes_key_iv();

  const auto _encrypted = encrypt(_plaintext, _key1, _iv);

  EXPECT_THROW({ decrypt(_encrypted, _key2, _iv); }, errors::cipher_error);
}

TEST(test_cipher, aes_decrypt_fails_with_wrong_tag) {
  const std::string _plaintext = "sensitive info";
  const auto [_key, _iv] = generate_aes_key_iv();

  auto _encrypted = encrypt(_plaintext, _key, _iv);
  _encrypted[_encrypted.size() - 1] ^=
      0x01;  // Corrupt the last byte of the tag

  EXPECT_THROW({ decrypt(_encrypted, _key, _iv); }, errors::cipher_error);
}
