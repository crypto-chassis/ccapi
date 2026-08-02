#pragma once

#ifdef CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT
#ifdef CCAPI_ENABLE_EXCHANGE_HYPERLIQUID
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "ccapi_cpp/service/ccapi_execution_management_service.h"
#include "msgpack.hpp"
#include "secp256k1.h"
#include "secp256k1_recovery.h"

namespace ccapi {

class ExecutionManagementServiceHyperliquid : public ExecutionManagementService {
 public:
  ExecutionManagementServiceHyperliquid(std::function<void(Event&, Queue<Event>*)> eventHandler, SessionOptions sessionOptions, SessionConfigs sessionConfigs,
                                        ServiceContextPtr serviceContextPtr)
      : ExecutionManagementService(eventHandler, sessionOptions, sessionConfigs, serviceContextPtr) {
    this->exchangeName = CCAPI_EXCHANGE_NAME_HYPERLIQUID;
    this->baseUrlWs = sessionConfigs.getUrlWebsocketBase().at(this->exchangeName) + "/ws";
    this->baseUrlRest = sessionConfigs.getUrlRestBase().at(this->exchangeName);
    this->setHostRestFromUrlRest(this->baseUrlRest);
    this->walletKeyName = CCAPI_HYPERLIQUID_WALLET_KEY;
    this->accountAddressName = CCAPI_HYPERLIQUID_ACCOUNT_ADDRESS;
    this->setupCredential({this->walletKeyName, this->accountAddressName});
    this->infoTarget = "/info";
    this->exchangeTarget = "/exchange";
  }

  virtual ~ExecutionManagementServiceHyperliquid() {}
#ifndef CCAPI_EXPOSE_INTERNAL

 private:
#endif

  static uint64_t rol64(uint64_t a, int offset) { return (a << offset) | (a >> (64 - offset)); }

  static constexpr uint64_t keccakf_rndc[24] = {
      0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
      0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
      0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
      0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

  static constexpr int keccakf_rotc[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};

  static constexpr int keccakf_piln[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};

  void keccakf(uint64_t st[25]) {
    uint64_t t, bc[5];
    for (int round = 0; round < 24; round++) {
      for (int i = 0; i < 5; i++) bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
      for (int i = 0; i < 5; i++) {
        t = bc[(i + 4) % 5] ^ this->rol64(bc[(i + 1) % 5], 1);
        for (int j = 0; j < 25; j += 5) st[j + i] ^= t;
      }
      t = st[1];
      for (int i = 0; i < 24; i++) {
        int j = keccakf_piln[i];
        bc[0] = st[j];
        st[j] = this->rol64(t, keccakf_rotc[i]);
        t = bc[0];
      }
      for (int j = 0; j < 25; j += 5) {
        for (int i = 0; i < 5; i++) bc[i] = st[j + i];
        for (int i = 0; i < 5; i++) st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
      }
      st[0] ^= keccakf_rndc[round];
    }
  }

  std::vector<uint8_t> keccak256_raw(const std::vector<uint8_t>& data) {
    uint64_t st[25] = {0};
    uint8_t temp[144];
    size_t rsiz = 136;
    size_t pt = 0;
    for (size_t i = 0; i < data.size(); i++) {
      temp[pt++] = data[i];
      if (pt >= rsiz) {
        for (size_t j = 0; j < rsiz / 8; j++) {
          uint64_t val = 0;
          for (int k = 0; k < 8; k++) {
            val |= ((uint64_t)temp[j * 8 + k]) << (8 * k);
          }
          st[j] ^= val;
        }
        this->keccakf(st);
        pt = 0;
      }
    }
    temp[pt++] = 0x01;
    while (pt < rsiz) {
      temp[pt++] = 0x00;
    }
    temp[rsiz - 1] |= 0x80;
    for (size_t j = 0; j < rsiz / 8; j++) {
      uint64_t val = 0;
      for (int k = 0; k < 8; k++) {
        val |= ((uint64_t)temp[j * 8 + k]) << (8 * k);
      }
      st[j] ^= val;
    }
    this->keccakf(st);
    std::vector<uint8_t> hash(32);
    for (size_t i = 0; i < 32; i++) {
      hash[i] = (st[i / 8] >> (8 * (i % 8))) & 0xFF;
    }
    return hash;
  }

  std::string toHex(const std::vector<uint8_t>& v) {
    std::ostringstream oss;
    oss << "0x";
    for (auto b : v) {
      oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
  }

  std::string deriveEthAddress(const std::array<uint8_t, 32>& privkey) {
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

    secp256k1_pubkey pubkey;

    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, privkey.data())) {
      secp256k1_context_destroy(ctx);
      throw std::runtime_error("pubkey derivation failed");
    }

    uint8_t serialized[65];
    size_t len = 65;

    secp256k1_ec_pubkey_serialize(ctx, serialized, &len, &pubkey, SECP256K1_EC_UNCOMPRESSED);

    secp256k1_context_destroy(ctx);

    std::vector<uint8_t> pub(serialized + 1, serialized + 65);

    auto hash = this->keccak256_raw(pub);

    std::vector<uint8_t> addr(hash.end() - 20, hash.end());

    return this->toHex(addr);
  }

  bool doesHttpBodyContainError(boost::beast::string_view bodyView) override { return bodyView.find("\"status\":\"err\"") != boost::beast::string_view::npos; }

  void pingOnApplicationLevel(std::shared_ptr<WsConnection> wsConnectionPtr, ErrorCode& ec) override {
    this->send(wsConnectionPtr, R"({"method":"ping"})", ec);
  }

  void convertRequestForRest(http::request<http::string_body>& req, const Request& request, const TimePoint& now, const std::string& symbolId,
                             const std::map<std::string, std::string>& credential) override {
    Request::Operation operation = request.getOperation();
    switch (operation) {
      case Request::Operation::GENERIC_PRIVATE_REQUEST: {
        ExecutionManagementService::convertRequestForRestGenericPrivateRequest(req, request, now, symbolId, credential);
      } break;
      case Request::Operation::CREATE_ORDER: {
        req.method(http::verb::post);
        req.target(this->exchangeTarget);
        req.set(http::field::content_type, "application/json");
        const auto& param = request.getFirstParamWithDefault();

        rapidjson::Document actionJson;
        actionJson.SetObject();
        auto& allocator = actionJson.GetAllocator();
        actionJson.AddMember("type", "order", allocator);
        rapidjson::Value orders(rapidjson::kArrayType);
        orders.PushBack(this->buildOrderWire(param, allocator, symbolId), allocator);
        actionJson.AddMember("orders", orders, allocator);
        actionJson.AddMember("grouping", "na", allocator);

        std::vector<uint8_t> actionBytes = this->buildMsgpackActionForCreateOrder(param, symbolId);

        uint64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        auto privkey = hexToBytes32(mapGetWithDefault(credential, this->walletKeyName));

        auto sig = this->signL1Action(actionBytes, std::nullopt, nonce, std::nullopt, true, privkey);

        rapidjson::Document body;
        body.SetObject();
        auto& a = body.GetAllocator();
        body.AddMember("action", actionJson, a);
        body.AddMember("nonce", nonce, a);
        rapidjson::Value sigObj(rapidjson::kObjectType);
        sigObj.AddMember("r", rapidjson::Value(bytesToHex_2(sig.r).c_str(), a), a);
        sigObj.AddMember("s", rapidjson::Value(bytesToHex_2(sig.s).c_str(), a), a);
        sigObj.AddMember("v", sig.v, a);
        body.AddMember("signature", sigObj, a);
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        body.Accept(writer);
        req.body() = buffer.GetString();
        req.prepare_payload();
      } break;
      case Request::Operation::CANCEL_ORDER: {
        req.method(http::verb::post);
        req.target(this->exchangeTarget);
        req.set(http::field::content_type, "application/json");

        const auto& param = request.getFirstParamWithDefault();

        rapidjson::Document actionJson;
        actionJson.SetObject();
        auto& allocator = actionJson.GetAllocator();

        actionJson.AddMember("type", "cancel", allocator);

        rapidjson::Value cancels(rapidjson::kArrayType);

        rapidjson::Value cancelObj(rapidjson::kObjectType);
        cancelObj.AddMember("a", std::stoi(symbolId), allocator);
        cancelObj.AddMember("o", rapidjson::Value().SetUint64(std::stoull(param.at(CCAPI_EM_ORDER_ID))), allocator);

        cancels.PushBack(cancelObj, allocator);

        actionJson.AddMember("cancels", cancels, allocator);

        std::vector<uint8_t> actionBytes = this->buildMsgpackActionForCancelOrder(param, symbolId);

        uint64_t nonce = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        auto privkey = hexToBytes32(mapGetWithDefault(credential, this->walletKeyName));

        auto sig = this->signL1Action(actionBytes, std::nullopt, nonce, std::nullopt, true, privkey);

        rapidjson::Document body;
        body.SetObject();
        auto& a = body.GetAllocator();

        body.AddMember("action", actionJson, a);
        body.AddMember("nonce", nonce, a);

        rapidjson::Value sigObj(rapidjson::kObjectType);
        sigObj.AddMember("r", rapidjson::Value(bytesToHex_2(sig.r).c_str(), a), a);
        sigObj.AddMember("s", rapidjson::Value(bytesToHex_2(sig.s).c_str(), a), a);
        sigObj.AddMember("v", sig.v, a);

        body.AddMember("signature", sigObj, a);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        body.Accept(writer);

        req.body() = buffer.GetString();
        req.prepare_payload();
      } break;
      case Request::Operation::GET_OPEN_ORDERS: {
        req.method(http::verb::post);
        req.target(this->infoTarget);

        rapidjson::Document body;
        body.SetObject();
        auto& a = body.GetAllocator();
        body.AddMember("type", "frontendOpenOrders", a);
        body.AddMember("user", rapidjson::Value(this->resolveUserAddress(credential).c_str(), a), a);
        req.set(http::field::content_type, "application/json");
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        body.Accept(writer);

        req.body() = buffer.GetString();
        req.prepare_payload();

      } break;
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        req.method(http::verb::post);
        req.target(this->infoTarget);

        rapidjson::Document body;
        body.SetObject();
        auto& a = body.GetAllocator();
        body.AddMember("type", "spotClearinghouseState", a);
        body.AddMember("user", rapidjson::Value(this->resolveUserAddress(credential).c_str(), a), a);
        req.set(http::field::content_type, "application/json");
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        body.Accept(writer);

        req.body() = buffer.GetString();
        req.prepare_payload();
      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        req.method(http::verb::post);
        req.target(this->infoTarget);

        rapidjson::Document body;
        body.SetObject();
        auto& a = body.GetAllocator();
        body.AddMember("type", "clearinghouseState", a);
        body.AddMember("user", rapidjson::Value(this->resolveUserAddress(credential).c_str(), a), a);
        req.set(http::field::content_type, "application/json");
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        body.Accept(writer);

        req.body() = buffer.GetString();
        req.prepare_payload();
      } break;

      default:
        this->convertRequestForRestCustom(req, request, now, symbolId, credential);
    }
  }

  std::string resolveUserAddress(const std::map<std::string, std::string>& credential) {
    const std::string& providedAddress = mapGetWithDefault(credential, this->accountAddressName);

    if (!providedAddress.empty()) return providedAddress;

    const auto privateKey = hexToBytes32(mapGetWithDefault(credential, walletKeyName));

    return deriveEthAddress(privateKey);
  }

  rapidjson::Value buildOrderWire(const std::map<std::string, std::string>& param, rapidjson::Document::AllocatorType& allocator, const std::string& symbolId) {
    rapidjson::Value order(rapidjson::kObjectType);
    order.AddMember("a", std::stoi(symbolId), allocator);
    order.AddMember("b", param.at(CCAPI_EM_ORDER_SIDE) == CCAPI_EM_ORDER_SIDE_BUY, allocator);
    order.AddMember("p", rapidjson::Value(UtilString::trimTrailingZeros(param.at(CCAPI_EM_ORDER_LIMIT_PRICE)).c_str(), allocator), allocator);
    order.AddMember("s", rapidjson::Value(UtilString::trimTrailingZeros(param.at(CCAPI_EM_ORDER_QUANTITY)).c_str(), allocator), allocator);
    order.AddMember("r", false, allocator);
    if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
      order.AddMember("c", rapidjson::Value(param.at(CCAPI_EM_CLIENT_ORDER_ID).c_str(), allocator), allocator);
    }
    rapidjson::Value t(rapidjson::kObjectType);
    rapidjson::Value limit(rapidjson::kObjectType);
    limit.AddMember("tif", "Gtc", allocator);
    t.AddMember("limit", limit, allocator);
    order.AddMember("t", t, allocator);
    return order;
  }

  std::vector<uint8_t> buildMsgpackActionForCreateOrder(const std::map<std::string, std::string>& param, const std::string& symbolId) {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(&sbuf);

    pk.pack_map(3);

    pk.pack(std::string("type"));
    pk.pack(std::string("order"));

    pk.pack(std::string("orders"));
    pk.pack_array(1);

    int fieldCount = 6;
    if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
      fieldCount += 1;
    }

    pk.pack_map(fieldCount);

    pk.pack(std::string("a"));
    pk.pack(std::stoi(symbolId));

    pk.pack(std::string("b"));
    pk.pack(param.at(CCAPI_EM_ORDER_SIDE) == CCAPI_EM_ORDER_SIDE_BUY);

    pk.pack(std::string("p"));
    pk.pack(UtilString::trimTrailingZeros(param.at(CCAPI_EM_ORDER_LIMIT_PRICE)));

    pk.pack(std::string("s"));
    pk.pack(UtilString::trimTrailingZeros(param.at(CCAPI_EM_ORDER_QUANTITY)));

    pk.pack(std::string("r"));
    pk.pack(false);

    pk.pack(std::string("t"));
    pk.pack_map(1);

    pk.pack(std::string("limit"));
    pk.pack_map(1);

    pk.pack(std::string("tif"));
    pk.pack(std::string("Gtc"));

    if (param.find(CCAPI_EM_CLIENT_ORDER_ID) != param.end()) {
      pk.pack(std::string("c"));
      pk.pack(param.at(CCAPI_EM_CLIENT_ORDER_ID));
    }

    pk.pack(std::string("grouping"));
    pk.pack(std::string("na"));

    return std::vector<uint8_t>(sbuf.data(), sbuf.data() + sbuf.size());
  }

  std::vector<uint8_t> buildMsgpackActionForCancelOrder(const std::map<std::string, std::string>& param, const std::string& symbolId) {
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> pk(&sbuf);

    pk.pack_map(2);

    pk.pack(std::string("type"));
    pk.pack(std::string("cancel"));

    pk.pack(std::string("cancels"));
    pk.pack_array(1);

    pk.pack_map(2);

    pk.pack(std::string("a"));
    pk.pack(std::stoi(symbolId));

    pk.pack(std::string("o"));
    pk.pack(std::stoull(param.at(CCAPI_EM_ORDER_ID)));

    return std::vector<uint8_t>(sbuf.data(), sbuf.data() + sbuf.size());
  }

  struct Signature {
    std::array<uint8_t, 32> r;
    std::array<uint8_t, 32> s;
    int v;
  };

  struct Agent {
    std::string source;
    std::array<uint8_t, 32> connectionId;
  };

  Signature signL1Action(const std::vector<uint8_t>& actionBytes, const std::optional<std::string>& vault, uint64_t nonce, std::optional<uint64_t> expiresAfter,
                         bool isMainnet, const std::array<uint8_t, 32>& privkey) {
    auto hash = this->actionHash(actionBytes, vault, nonce, expiresAfter);

    Agent agent;
    agent.source = isMainnet ? "a" : "b";
    agent.connectionId = hash;

    auto digest = this->eip712Hash(agent);

    return this->signHash(digest, privkey);
  }

  std::array<uint8_t, 32> actionHash(const std::vector<uint8_t>& actionBytes, const std::optional<std::string>& vault, uint64_t nonce,
                                     const std::optional<uint64_t>& expiresAfter) {
    auto hash = this->keccak256(actionBytes);

    std::vector<uint8_t> data = actionBytes;

    this->appendU64(data, nonce);

    data.push_back(vault ? 0x01 : 0x00);

    if (vault) {
      auto addr = this->hexToBytes(vault.value());
      data.insert(data.end(), addr.begin(), addr.end());
    }

    if (expiresAfter) {
      data.push_back(0x00);
      this->appendU64(data, *expiresAfter);
    }

    return this->keccak256(data);
  }

  std::vector<uint8_t> packMsgpack(const msgpack::type::variant& v) {
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, v);
    return {sbuf.data(), sbuf.data() + sbuf.size()};
  }

  void appendU64(std::vector<uint8_t>& v, uint64_t x) {
    for (int i = 7; i >= 0; i--) v.push_back((x >> (8 * i)) & 0xff);
  }

  std::array<uint8_t, 32> hexToBytes32(const std::string& hex) {
    std::array<uint8_t, 32> out{};
    std::string h = hex.substr(0, 2) == "0x" ? hex.substr(2) : hex;
    for (int i = 0; i < 32; i++) out[i] = std::stoi(h.substr(i * 2, 2), nullptr, 16);
    return out;
  }

  std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::string h = hex.substr(0, 2) == "0x" ? hex.substr(2) : hex;
    std::vector<uint8_t> out(h.size() / 2);
    for (size_t i = 0; i < out.size(); i++) out[i] = std::stoi(h.substr(2 * i, 2), nullptr, 16);
    return out;
  }

  std::string bytesToHex_2(const std::array<uint8_t, 32>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (uint8_t b : data) {
      oss << std::setw(2) << static_cast<int>(b);
    }

    return oss.str();
  }

  std::string bytesToHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (uint8_t b : data) {
      oss << std::setw(2) << static_cast<int>(b);
    }

    return oss.str();
  }

  std::array<uint8_t, 32> keccak256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash = keccak256_raw(data);

    std::array<uint8_t, 32> out{};
    std::copy(hash.begin(), hash.end(), out.begin());

    return out;
  }

  std::array<uint8_t, 32> keccak256Str(const std::string& s) {
    std::vector<uint8_t> v(s.begin(), s.end());
    return this->keccak256(v);
  }

  std::array<uint8_t, 32> eip712Hash(const Agent& agent) {
    auto keccak_str = [&](const std::string& s) { return this->keccak256Str(s); };

    auto pad32 = [](const std::vector<uint8_t>& v) {
      std::vector<uint8_t> out(32, 0);
      std::copy(v.begin(), v.end(), out.begin() + (32 - v.size()));
      return out;
    };

    auto domainTypeHash = keccak_str("EIP712Domain(string name,string version,uint256 chainId,address verifyingContract)");

    auto nameHash = keccak_str("Exchange");
    auto versionHash = keccak_str("1");

    std::vector<uint8_t> chainId(32, 0);
    uint64_t chainIdValue = 1337;
    for (int i = 0; i < 8; i++) {
      chainId[31 - i] = (chainIdValue >> (8 * i)) & 0xff;
    }

    std::vector<uint8_t> verifyingContract(32, 0);

    std::vector<uint8_t> domainEncoded;
    domainEncoded.insert(domainEncoded.end(), domainTypeHash.begin(), domainTypeHash.end());
    domainEncoded.insert(domainEncoded.end(), nameHash.begin(), nameHash.end());
    domainEncoded.insert(domainEncoded.end(), versionHash.begin(), versionHash.end());
    domainEncoded.insert(domainEncoded.end(), chainId.begin(), chainId.end());
    domainEncoded.insert(domainEncoded.end(), verifyingContract.begin(), verifyingContract.end());

    auto domainSeparator = this->keccak256(domainEncoded);

    auto agentTypeHash = keccak_str("Agent(string source,bytes32 connectionId)");
    auto sourceHash = keccak_str(agent.source);

    std::vector<uint8_t> agentEncoded;
    agentEncoded.insert(agentEncoded.end(), agentTypeHash.begin(), agentTypeHash.end());
    agentEncoded.insert(agentEncoded.end(), sourceHash.begin(), sourceHash.end());
    agentEncoded.insert(agentEncoded.end(), agent.connectionId.begin(), agent.connectionId.end());

    auto structHash = this->keccak256(agentEncoded);

    std::vector<uint8_t> finalData = {0x19, 0x01};
    finalData.insert(finalData.end(), domainSeparator.begin(), domainSeparator.end());
    finalData.insert(finalData.end(), structHash.begin(), structHash.end());

    return this->keccak256(finalData);
  }

  Signature signHash(const std::array<uint8_t, 32>& hash, const std::array<uint8_t, 32>& privkey) {
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

    secp256k1_ecdsa_recoverable_signature sig;

    int ok = secp256k1_ecdsa_sign_recoverable(ctx, &sig, hash.data(), privkey.data(), nullptr, nullptr);

    if (!ok) {
      throw std::runtime_error("secp256k1 signing failed");
    }

    uint8_t compact[64];
    int recid = 0;

    secp256k1_ecdsa_recoverable_signature_serialize_compact(ctx, compact, &recid, &sig);

    Signature out;
    memcpy(out.r.data(), compact, 32);
    memcpy(out.s.data(), compact + 32, 32);

    out.v = recid + 27;

    secp256k1_context_destroy(ctx);

    return out;
  }

  void extractOrderInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                   const rapidjson::Document& document) override {
    if (operation == Request::Operation::CREATE_ORDER) {
      const auto& response = document["response"];

      const auto& data = response["data"];

      const auto& statuses = data["statuses"];

      for (const auto& s : statuses.GetArray()) {
        Element element;

        if (s.HasMember("resting")) {
          const auto& r = s["resting"];

          if (r.HasMember("oid")) {
            element.insert(CCAPI_EM_ORDER_ID, r["oid"].GetString());
          }

          element.insert(CCAPI_EM_ORDER_STATUS, "open");
        }

        else if (s.HasMember("filled")) {
          const auto& f = s["filled"];

          if (f.HasMember("oid")) {
            element.insert(CCAPI_EM_ORDER_ID, f["oid"].GetString());
          }

          if (f.HasMember("totalSz")) {
            element.insert(CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, f["totalSz"].GetString());
          }

          if (f.HasMember("avgPx")) {
            element.insert(CCAPI_EM_ORDER_AVERAGE_FILLED_PRICE, f["avgPx"].GetString());
          }

          element.insert(CCAPI_EM_ORDER_STATUS, "filled");
        }

        elementList.emplace_back(std::move(element));
      }
    } else if (operation == Request::Operation::GET_OPEN_ORDERS) {
      if (document.IsArray()) {
        for (const auto& x : document.GetArray()) {
          Element element;

          if (x.HasMember("oid")) {
            element.insert(CCAPI_EM_ORDER_ID, x["oid"].GetString());
          }

          if (x.HasMember("cloid") && !x["cloid"].IsNull()) {
            element.insert(CCAPI_EM_CLIENT_ORDER_ID, x["cloid"].GetString());
          }

          if (x.HasMember("coin")) {
            element.insert(CCAPI_EM_ORDER_INSTRUMENT, x["coin"].GetString());
          }

          if (x.HasMember("side")) {
            std::string side = x["side"].GetString();
            element.insert(CCAPI_EM_ORDER_SIDE, side == "B" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          }

          if (x.HasMember("origSz")) {
            element.insert(CCAPI_EM_ORDER_QUANTITY, x["origSz"].GetString());
          }

          if (x.HasMember("limitPx")) {
            element.insert(CCAPI_EM_ORDER_LIMIT_PRICE, x["limitPx"].GetString());
          }

          element.insert(CCAPI_EM_ORDER_STATUS, "open");

          elementList.emplace_back(std::move(element));
        }
      }
    }
  }

  void extractAccountInfoFromRequest(std::vector<Element>& elementList, const Request& request, const Request::Operation operation,
                                     const rapidjson::Document& document) override {
    switch (request.getOperation()) {
      case Request::Operation::GET_ACCOUNT_BALANCES: {
        const auto& balances = document["balances"];
        for (const auto& x : balances.GetArray()) {
          Element element;
          element.insert(CCAPI_EM_ASSET, x["coin"].GetString());
          element.insert(CCAPI_EM_QUANTITY_TOTAL, x["total"].GetString());
          elementList.emplace_back(std::move(element));
        }

      } break;
      case Request::Operation::GET_ACCOUNT_POSITIONS: {
        const auto& positions = document["assetPositions"];
        for (const auto& x : positions.GetArray()) {
          Element element;

          const auto& pos = x["position"];
          element.insert(CCAPI_INSTRUMENT, pos["coin"].GetString());
          std::string sizeStr = pos["szi"].GetString();
          double size = std::stod(sizeStr);
          element.insert(CCAPI_EM_POSITION_SIDE, size > 0 ? CCAPI_EM_POSITION_SIDE_LONG : CCAPI_EM_POSITION_SIDE_SHORT);
          element.insert(CCAPI_EM_POSITION_QUANTITY, sizeStr);
          std::string marginType = pos["leverage"]["type"].GetString();
          element.insert(CCAPI_EM_POSITION_MARGIN_TYPE, marginType == "cross" ? CCAPI_EM_MARGIN_TYPE_CROSS_MARGIN : CCAPI_EM_MARGIN_TYPE_ISOLATED_MARGIN);
          element.insert(CCAPI_EM_POSITION_ENTRY_PRICE, pos["entryPx"].GetString());
          element.insert(CCAPI_EM_POSITION_LEVERAGE, pos["leverage"]["value"].GetString());
          elementList.emplace_back(std::move(element));
        }
      } break;
      default:
        CCAPI_LOGGER_FATAL(CCAPI_UNSUPPORTED_VALUE);
    }
  }

  void extractOrderInfo(Element& element, const rapidjson::Value& x,
                        const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap,
                        const std::map<std::string_view, std::function<std::string(const std::string&)>> conversionMap = {}) override {
    ExecutionManagementService::extractOrderInfo(element, x, extractionFieldNameMap);
  }

  std::vector<std::string> createSendStringListFromSubscription(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription,
                                                                const TimePoint& now, const std::map<std::string, std::string>& credential) override {
    std::vector<std::string> sendStringList;
    const auto& fieldSet = subscription.getFieldSet();
    const auto& user = this->resolveUserAddress(credential);
    if (fieldSet.find(CCAPI_EM_ORDER_UPDATE) != fieldSet.end()) {
      rj::Document document;
      document.SetObject();
      auto& allocator = document.GetAllocator();
      document.AddMember("method", rj::Value("subscribe").Move(), allocator);
      rj::Value subscriptionObject;
      subscriptionObject.SetObject();
      subscriptionObject.AddMember("type", "orderUpdates", allocator);
      subscriptionObject.AddMember("user", rj::Value(user.c_str(), allocator).Move(), allocator);

      document.AddMember("subscription", subscriptionObject, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      sendStringList.push_back(stringBuffer.GetString());
    }

    if (fieldSet.find(CCAPI_EM_PRIVATE_TRADE) != fieldSet.end()) {
      rj::Document document;
      document.SetObject();
      auto& allocator = document.GetAllocator();
      document.AddMember("method", rj::Value("subscribe").Move(), allocator);
      rj::Value subscriptionObject;
      subscriptionObject.SetObject();
      subscriptionObject.AddMember("type", "userFills", allocator);
      subscriptionObject.AddMember("user", rj::Value(user.c_str(), allocator).Move(), allocator);

      document.AddMember("subscription", subscriptionObject, allocator);
      rj::StringBuffer stringBuffer;
      rj::Writer<rj::StringBuffer> writer(stringBuffer);
      document.Accept(writer);
      sendStringList.push_back(stringBuffer.GetString());
    }

    return sendStringList;
  }

  void onTextMessage(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, boost::beast::string_view textMessageView,
                     const TimePoint& timeReceived) override {
    this->jsonDocumentAllocator.Clear();
    rapidjson::Document document(&this->jsonDocumentAllocator);
    document.Parse<rapidjson::kParseNumbersAsStringsFlag>(textMessageView.data(), textMessageView.size());
    std::string channel = document["channel"].GetString();
    Event event = this->createEvent(wsConnectionPtr, subscription, textMessageView, document, channel, timeReceived);
    if (!event.getMessageList().empty()) {
      this->eventHandler(event, nullptr);
    }
  }

  Event createEvent(std::shared_ptr<WsConnection> wsConnectionPtr, const Subscription& subscription, const std::string& textMessageView,
                    const rapidjson::Document& document, const std::string& channel, const TimePoint& timeReceived) {
    Event event;

    if (channel == "pong") {
      return event;
    }

    std::vector<Message> messageList;
    Message message;
    message.setTimeReceived(timeReceived);
    const auto& correlationId = subscription.getCorrelationId();
    const auto& fieldSet = subscription.getFieldSet();

    message.setCorrelationIdList({correlationId});
    const rapidjson::Value& data = document["data"];

    if (channel == "subscriptionResponse") {
      event.setType(Event::Type::SUBSCRIPTION_STATUS);
      message.setType(Message::Type::SUBSCRIPTION_STARTED);
      message.setCorrelationIdList({correlationId});
      Element element;
      element.insert(CCAPI_INFO_MESSAGE, textMessageView);
      message.setElementList({element});
      messageList.emplace_back(std::move(message));
    } else {
      event.setType(Event::Type::SUBSCRIPTION_DATA);
      if (channel == "orderUpdates") {
        for (const auto& x : data.GetArray()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["statusTimestamp"].GetString()))));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE);
          const std::map<std::string_view, std::pair<std::string_view, JsonDataType>>& extractionFieldNameMap = {
              {CCAPI_EM_ORDER_ID, std::make_pair("oid", JsonDataType::STRING)},
              {CCAPI_EM_CLIENT_ORDER_ID, std::make_pair("cloid", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_LIMIT_PRICE, std::make_pair("limitPx", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_QUANTITY, std::make_pair("origSz", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_CUMULATIVE_FILLED_QUANTITY, std::make_pair("accFillSz", JsonDataType::STRING)},
              {CCAPI_EM_ORDER_INSTRUMENT, std::make_pair("coin", JsonDataType::STRING)},
          };
          Element info;
          this->extractOrderInfo(info, x["order"], extractionFieldNameMap);
          info.insert(CCAPI_EM_ORDER_SIDE, std::string_view(x["order"]["side"].GetString()) == "B" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          info.insert(CCAPI_EM_ORDER_STATUS, x["status"].GetString());
          std::vector<Element> elementList;
          elementList.emplace_back(std::move(info));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }

      } else if (channel == "userFills") {
        for (const auto& x : data["fills"].GetArray()) {
          Message message;
          message.setTimeReceived(timeReceived);
          message.setCorrelationIdList({subscription.getCorrelationId()});
          message.setTime(UtilTime::makeTimePointFromMilliseconds(std::stoll(std::string(x["time"].GetString()))));
          message.setType(Message::Type::EXECUTION_MANAGEMENT_EVENTS_PRIVATE_TRADE);
          std::vector<Element> elementList;
          Element element;
          element.insert(CCAPI_TRADE_ID, x["tid"].GetString());
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_PRICE, x["px"].GetString());
          element.insert(CCAPI_EM_ORDER_LAST_EXECUTED_SIZE, x["sz"].GetString());
          element.insert(CCAPI_EM_ORDER_SIDE, std::string_view(x["side"].GetString()) == "B" ? CCAPI_EM_ORDER_SIDE_BUY : CCAPI_EM_ORDER_SIDE_SELL);
          element.insert(CCAPI_IS_MAKER, x["crossed"].GetBool() ? "0" : "1");
          element.insert(CCAPI_EM_ORDER_ID, x["oid"].GetString());
          element.insert(CCAPI_EM_ORDER_INSTRUMENT, x["coin"].GetString());
          element.insert(CCAPI_EM_ORDER_FEE_QUANTITY, x["fee"].GetString());
          element.insert(CCAPI_EM_ORDER_FEE_ASSET, x["feeToken"].GetString());
          elementList.emplace_back(std::move(element));
          message.setElementList(elementList);
          messageList.emplace_back(std::move(message));
        }
      }
    }

    event.setMessageList(messageList);
    return event;
  }

  std::string infoTarget;
  std::string exchangeTarget;
  std::string walletKeyName;
  std::string walletAddressName;
  std::string accountAddressName;
};

}  // namespace ccapi
#endif
#endif
