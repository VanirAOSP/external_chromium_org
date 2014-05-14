// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/verified_contents.h"

#include "base/base64.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/extension.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace {

// Note: this structure is an ASN.1 which encodes the algorithm used with its
// parameters.  The signature algorithm is "RSA256" aka "RSASSA-PKCS-v1_5 using
// SHA-256 hash algorithm". This is defined in PKCS #1 (RFC 3447).
// It is encoding: { OID sha256WithRSAEncryption      PARAMETERS NULL }
const uint8 kSignatureAlgorithm[15] = {0x30, 0x0d, 0x06, 0x09, 0x2a,
                                       0x86, 0x48, 0x86, 0xf7, 0x0d,
                                       0x01, 0x01, 0x0b, 0x05, 0x00};

const char kBlockSizeKey[] = "block_size";
const char kContentHashesKey[] = "content_hashes";
const char kFilesKey[] = "files";
const char kFormatKey[] = "format";
const char kHashBlockSizeKey[] = "hash_block_size";
const char kHeaderKidKey[] = "header.kid";
const char kItemIdKey[] = "item_id";
const char kItemVersionKey[] = "item_version";
const char kPathKey[] = "path";
const char kPayloadKey[] = "payload";
const char kProtectedKey[] = "protected";
const char kRootHashKey[] = "root_hash";
const char kSignatureKey[] = "signature";
const char kSignaturesKey[] = "signatures";
const char kTreeHash[] = "treehash";
const char kWebstoreKId[] = "webstore";

// This function fixes up a string in base64url encoding to be in standard
// base64.
//
// The JSON signing spec we're following uses "base64url" encoding (RFC 4648
// section 5 without padding). The slight differences from regular base64
// encoding are:
//   1. uses '_' instead of '/'
//   2. uses '-' instead of '+'
//   3. omits trailing '=' padding
bool FixupBase64Encoding(std::string* input) {
  for (std::string::iterator i = input->begin(); i != input->end(); ++i) {
    if (*i == '-')
      *i = '+';
    else if (*i == '_')
      *i = '/';
  }
  switch (input->size() % 4) {
    case 0:
      break;
    case 2:
      input->append("==");
      break;
    case 3:
      input->append("=");
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

namespace extensions {

VerifiedContents::VerifiedContents(const uint8* public_key, int public_key_size)
    : public_key_(public_key),
      public_key_size_(public_key_size),
      valid_signature_(false),  // Guilty until proven innocent.
      block_size_(0) {
}

VerifiedContents::~VerifiedContents() {
}

// The format of the payload json is:
// {
//   "content_hashes": [
//     {
//       "block_size": 4096,
//       "hash_block_size": 4096,
//       "format": "treehash",
//       "files": [
//         {
//           "path": "foo/bar",
//           "root_hash": "<hex encoded bytes>"
//         },
//         ...
//       ]
//     }
//   ]
// }
bool VerifiedContents::InitFrom(const base::FilePath& path,
                                bool ignore_invalid_signature) {
  std::string payload;
  if (!GetPayload(path, &payload, ignore_invalid_signature))
    return false;

  scoped_ptr<base::Value> value(base::JSONReader::Read(payload));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY))
    return false;
  DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.get());

  std::string item_id;
  if (!dictionary->GetString(kItemIdKey, &item_id) ||
      !Extension::IdIsValid(item_id))
    return false;
  extension_id_ = item_id;

  std::string version_string;
  if (!dictionary->GetString(kItemVersionKey, &version_string))
    return false;
  version_ = base::Version(version_string);
  if (!version_.IsValid())
    return false;

  ListValue* hashes_list = NULL;
  if (!dictionary->GetList(kContentHashesKey, &hashes_list))
    return false;

  for (size_t i = 0; i < hashes_list->GetSize(); i++) {
    DictionaryValue* hashes = NULL;
    if (!hashes_list->GetDictionary(i, &hashes))
      return false;
    std::string format;
    if (!hashes->GetString(kFormatKey, &format) || format != kTreeHash)
      continue;

    int block_size = 0;
    int hash_block_size = 0;
    if (!hashes->GetInteger(kBlockSizeKey, &block_size) ||
        !hashes->GetInteger(kHashBlockSizeKey, &hash_block_size))
      return false;
    block_size_ = block_size;

    // We don't support using a different block_size and hash_block_size at
    // the moment.
    if (block_size_ != hash_block_size)
      return false;

    ListValue* files = NULL;
    if (!hashes->GetList(kFilesKey, &files))
      return false;

    for (size_t j = 0; j < files->GetSize(); j++) {
      DictionaryValue* data = NULL;
      if (!files->GetDictionary(j, &data))
        return false;
      std::string file_path_string;
      std::string encoded_root_hash;
      std::vector<uint8> root_hash;
      if (!data->GetString(kPathKey, &file_path_string) ||
          !base::IsStringUTF8(file_path_string) ||
          !data->GetString(kRootHashKey, &encoded_root_hash) ||
          !base::HexStringToBytes(encoded_root_hash, &root_hash))
        return false;
      base::FilePath file_path =
          base::FilePath::FromUTF8Unsafe(file_path_string);
      root_hashes_[file_path] = std::string();
      root_hashes_[file_path].append(root_hash.begin(), root_hash.end());
    }

    break;
  }
  return true;
}

const std::string* VerifiedContents::GetTreeHashRoot(
    const base::FilePath& relative_path) {
  std::map<base::FilePath, std::string>::const_iterator i =
      root_hashes_.find(relative_path);
  if (i == root_hashes_.end())
    return NULL;
  return &i->second;
}

// We're loosely following the "JSON Web Signature" draft spec for signing
// a JSON payload:
//
//   http://tools.ietf.org/html/draft-ietf-jose-json-web-signature-26
//
// The idea is that you have some JSON that you want to sign, so you
// base64-encode that and put it as the "payload" field in a containing
// dictionary. There might be signatures of it done with multiple
// alrogirhtms/parameters, so the payload is followed by a list of one or more
// signature sections. Each signature section specifies the
// algorithm/parameters in a JSON object which is base64url encoded into one
// string and put into a "protected" field in the signature. Then the encoded
// "payload" and "protected" strings are concatenated with a "." in between
// them and those bytes are signed and the resulting signature is base64url
// encoded and placed in the "signature" field. E.g.
// {
//   "payload": "<base64url encoded JSON to sign>",
//   "signatures": [
//     {
//       "protected": "<base64url encoded JSON with algorithm/parameters>",
//       "header": {
//         <object with metadata about this signature, eg a key identifier>
//       }
//       "signature":
//          "<base64url encoded signature done over payload || . || protected>"
//     },
//     ... <zero or more additional signatures> ...
//   ]
// }
//
// There might be both a signature generated with a webstore private key and a
// signature generated with the extension's private key - for now we only
// verify the webstore one (since the id is in the payload, so we can trust
// that it is for a given extension), but in the future we may validate using
// the extension's key too (eg for non-webstore hosted extensions such as
// enterprise installs).
bool VerifiedContents::GetPayload(const base::FilePath& path,
                                  std::string* payload,
                                  bool ignore_invalid_signature) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;
  scoped_ptr<base::Value> value(base::JSONReader::Read(contents));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY))
    return false;
  DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.get());

  ListValue* signatures = NULL;
  if (!dictionary->GetList(kSignaturesKey, &signatures))
    return false;

  std::string protected_value;
  std::string decoded_signature;
  for (size_t i = 0; i < signatures->GetSize(); i++) {
    DictionaryValue* signature_dict = NULL;
    if (!signatures->GetDictionary(i, &signature_dict))
      return false;
    std::string kid;
    if (!signature_dict->GetString(kHeaderKidKey, &kid))
      continue;
    if (kid == std::string(kWebstoreKId)) {
      std::string encoded_signature;
      if (!signature_dict->GetString(kProtectedKey, &protected_value) ||
          !signature_dict->GetString(kSignatureKey, &encoded_signature) ||
          !FixupBase64Encoding(&encoded_signature) ||
          !base::Base64Decode(encoded_signature, &decoded_signature))
        return false;
      break;
    }
  }
  std::string encoded_payload;
  if (!dictionary->GetString(kPayloadKey, &encoded_payload))
    return false;

  valid_signature_ =
      VerifySignature(protected_value, encoded_payload, decoded_signature);
  if (!valid_signature_ && !ignore_invalid_signature)
    return false;

  if (!FixupBase64Encoding(&encoded_payload) ||
      !base::Base64Decode(encoded_payload, payload))
    return false;

  return true;
}

bool VerifiedContents::VerifySignature(const std::string& protected_value,
                                       const std::string& payload,
                                       const std::string& signature_bytes) {
  crypto::SignatureVerifier signature_verifier;
  if (!signature_verifier.VerifyInit(
          kSignatureAlgorithm,
          sizeof(kSignatureAlgorithm),
          reinterpret_cast<const uint8*>(signature_bytes.data()),
          signature_bytes.size(),
          public_key_,
          public_key_size_)) {
    VLOG(1) << "Could not verify signature - VerifyInit failure";
    return false;
  }

  signature_verifier.VerifyUpdate(
      reinterpret_cast<const uint8*>(protected_value.data()),
      protected_value.size());

  std::string dot(".");
  signature_verifier.VerifyUpdate(reinterpret_cast<const uint8*>(dot.data()),
                                  dot.size());

  signature_verifier.VerifyUpdate(
      reinterpret_cast<const uint8*>(payload.data()), payload.size());

  if (!signature_verifier.VerifyFinal()) {
    VLOG(1) << "Could not verify signature - VerifyFinal failure";
    return false;
  }
  return true;
}

}  // namespace extensions
