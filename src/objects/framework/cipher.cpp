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

#include <framework/cipher.hpp>
#include <framework/encoding.hpp>
#include <framework/errors/cipher_error.hpp>

namespace framework {
__attribute__((noreturn)) void on_openssl_error() {
  const unsigned long _error_code = ERR_get_error();
  std::string _error_message(256, '\0');
  ERR_error_string_n(_error_code, _error_message.data(), _error_message.size());
  std::string _error_output = "OpenSSL error: ";
  _error_output.append(_error_message);
  throw errors::cipher_error(_error_output);
}

constexpr int CIPHER_KEY_LENGTH = 32;
constexpr int CIPHER_IV_LENGTH = 16;
constexpr int CIPHER_DIGEST_LENGTH = 64;

std::string generate_sha_256() {
  std::string _bytes(CIPHER_KEY_LENGTH, '\0');

  if (RAND_bytes(reinterpret_cast<unsigned char *>(&_bytes[0]), CIPHER_KEY_LENGTH) != 1) {
    on_openssl_error();
  }

  return base64_encode(_bytes);
}

std::string hmac(const std::string &input, const std::string_view &app_key) {
  std::string _output;

  EVP_PKEY *_public_key =
      EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, reinterpret_cast<const unsigned char *>(app_key.data()), CIPHER_KEY_LENGTH);

  if (!_public_key) {
    on_openssl_error();
  }

  EVP_MD_CTX *_openssl_context = EVP_MD_CTX_new();

  if (!_openssl_context) {
    EVP_PKEY_free(_public_key);
    on_openssl_error();
  }

  std::size_t _length;
  std::string _digest(CIPHER_DIGEST_LENGTH, '\0');

  if (EVP_DigestSignInit(_openssl_context, nullptr, EVP_sha256(), nullptr, _public_key) != 1) {
    EVP_MD_CTX_free(_openssl_context);
    EVP_PKEY_free(_public_key);
    on_openssl_error();
  }

  if (EVP_DigestSignUpdate(_openssl_context, input.c_str(), input.size()) != 1) {
    EVP_MD_CTX_free(_openssl_context);
    EVP_PKEY_free(_public_key);
    on_openssl_error();
  }

  if (EVP_DigestSignFinal(_openssl_context, nullptr, &_length) != 1) {
    EVP_MD_CTX_free(_openssl_context);
    EVP_PKEY_free(_public_key);
    on_openssl_error();
  }

  if (EVP_DigestSignFinal(_openssl_context, reinterpret_cast<unsigned char *>(&_digest[0]), &_length) != 1) {
    EVP_MD_CTX_free(_openssl_context);
    EVP_PKEY_free(_public_key);
    on_openssl_error();
  }

  EVP_MD_CTX_free(_openssl_context);
  EVP_PKEY_free(_public_key);

  _digest.resize(_length);

  return _digest;
}

pair_of<std::string, std::string> generate_aes_key_iv() {
  pair_of<std::string, std::string> _output;

  vector_of<unsigned char> _key(CIPHER_KEY_LENGTH);

  if (RAND_bytes(_key.data(), static_cast<int>(_key.size())) != 1) {
    on_openssl_error();
  }

  _output.first.assign(_key.begin(), _key.end());

  vector_of<unsigned char> _iv(CIPHER_IV_LENGTH);
  if (RAND_bytes(_iv.data(), static_cast<int>(_iv.size())) != 1) {
    on_openssl_error();
  }

  _output.second.assign(_iv.begin(), _iv.end());

  return _output;
}

std::string encrypt(const std::string &input, const std::string &key, const std::string &iv) {
  EVP_CIPHER_CTX *_openssl_context = EVP_CIPHER_CTX_new();
  if (!_openssl_context) {
    on_openssl_error();
  }

  if (EVP_EncryptInit_ex(_openssl_context, EVP_aes_256_gcm(), nullptr, reinterpret_cast<const unsigned char *>(key.c_str()),
                         reinterpret_cast<const unsigned char *>(iv.c_str())) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  std::string _output(input.size() + EVP_MAX_BLOCK_LENGTH, '\0');
  int _out_length = 0;

  if (EVP_EncryptUpdate(_openssl_context, reinterpret_cast<unsigned char *>(&_output[0]), &_out_length,
                        reinterpret_cast<const unsigned char *>(input.c_str()), static_cast<int>(input.size())) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  int _final_length = 0;
  if (EVP_EncryptFinal_ex(_openssl_context, reinterpret_cast<unsigned char *>(&_output[0]) + _out_length, &_final_length) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  _output.resize(_out_length + _final_length);

  std::string _tag(CIPHER_IV_LENGTH, '\0');

  if (RAND_bytes(reinterpret_cast<unsigned char *>(&_tag[0]), CIPHER_IV_LENGTH) != 1) {
    on_openssl_error();
  }

  if (EVP_CIPHER_CTX_ctrl(_openssl_context, EVP_CTRL_GCM_GET_TAG, CIPHER_IV_LENGTH, &_tag[0]) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  EVP_CIPHER_CTX_free(_openssl_context);

  return _output + _tag;
}

std::string decrypt(const std::string_view input, const std::string &key, const std::string &iv) {
  const std::string_view _ciphertext = input.substr(0, input.size() - CIPHER_IV_LENGTH);
  const std::string_view _tag = input.substr(input.size() - CIPHER_IV_LENGTH);

  EVP_CIPHER_CTX *_openssl_context = EVP_CIPHER_CTX_new();

  if (!_openssl_context) {
    on_openssl_error();
  }

  if (EVP_DecryptInit_ex(_openssl_context, EVP_aes_256_gcm(), nullptr, reinterpret_cast<const unsigned char *>(key.c_str()),
                         reinterpret_cast<const unsigned char *>(iv.c_str())) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  std::string _output(_ciphertext.size(), '\0');
  int _out_length = 0;

  if (EVP_DecryptUpdate(_openssl_context, reinterpret_cast<unsigned char *>(&_output[0]), &_out_length,
                        reinterpret_cast<const unsigned char *>(_ciphertext.data()), static_cast<int>(_ciphertext.size())) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  if (std::string _tag_buffer(_tag);
      EVP_CIPHER_CTX_ctrl(_openssl_context, EVP_CTRL_GCM_SET_TAG, CIPHER_IV_LENGTH, _tag_buffer.data()) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  int _final_length = 0;
  if (EVP_DecryptFinal_ex(_openssl_context, reinterpret_cast<unsigned char *>(&_output[0]) + _out_length, &_final_length) != 1) {
    EVP_CIPHER_CTX_free(_openssl_context);
    on_openssl_error();
  }

  _output.resize(_out_length + _final_length);

  EVP_CIPHER_CTX_free(_openssl_context);

  return _output;
}
}  // namespace framework
