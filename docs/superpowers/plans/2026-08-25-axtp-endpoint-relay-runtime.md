# AXTP Endpoint Relay Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add backward-compatible Endpoint Relay metadata and canonical Endpoint ID generation to axtp-cpp-runtime.

**Architecture:** The protocol model owns opaque optional Endpoint metadata, the WebSocket/object-RPC codec owns the `m` wire object, and broker/SDK response paths use one address-reversal helper. The runtime remains header-only, adds no platform or Axent dependency, and keeps all legacy calls unaddressed unless the caller explicitly supplies metadata.

**Tech Stack:** C++17, header-only axtp core/runtime/SDK, nlohmann/json, CMake/CTest, AXTP conformance runner, portable SHA-256.

**Spec:** `docs/superpowers/specs/2026-08-25-endpoint-relay-multi-device-design.md`

## Global Constraints

- Work in the standalone `axtp-cpp-runtime` repository on branch `codex/endpoint-relay-runtime`; do not implement runtime changes only inside Axent's detached submodule checkout.
- Runtime dependency direction remains `NearCast -> Axent -> axtp-cpp-runtime`; runtime headers must not include or reference Axent or NearCast.
- Endpoint Relay metadata is only `m.src` and `m.dst`; do not add route, next-hop, hop-count, deadline, topology, or authorization semantics.
- `m` is optional; when `src` and `dst` are both absent, the encoder must omit `m` and preserve the existing `{sid, op, d}` output.
- `m.src` and `m.dst` are optional non-empty strings; `m.dst` is never an array; structurally valid unknown fields do not invalidate the payload or session.
- Response metadata is `src=request.dst`, `dst=request.src`; requests with no Endpoint metadata produce responses with no Endpoint metadata.
- JSON_BINARY keeps its existing 15-byte fixed envelope; JSON_BINARY plus explicit Endpoint metadata fails locally with `ErrorCode::InvalidArgument`.
- Canonical IDs are exactly `ep_` plus the first 16 SHA-256 bytes of UTF-8 `AXTP-ENDPOINT-v1|<endpointKey>` rendered as 32 lowercase hex characters.
- `endpointIdFromKey("")` throws `std::invalid_argument`.
- `CallOptions::endpoint` is appended after existing members with an empty default so existing aggregate initializers remain source-compatible.
- Do not edit `include/core/protocol/generated/**` or `generated/axtp_generated_manifest.json` by hand.
- Do not run conformance against unpinned axtp `main`. Before the conformance task can pass, a released `spec/vMAJOR.MINOR.PATCH` tag must contain authority commit `2d88ff4c369411d8e3afbb551886d423bd63bb82`.

---

### Task 1: Endpoint Metadata Model and Canonical Identity

**Files:**
- Create: `include/core/protocol/model/endpoint_metadata.hpp`
- Create: `include/core/protocol/endpoint/endpoint_identity.hpp`
- Modify: `include/core/protocol/model/payload.hpp`
- Modify: `include/axtp_core.hpp`
- Test: `tests/core/phase1_model_io_test.cpp`

**Interfaces:**
- Consumes: C++17 standard library only.
- Produces: `axtp::EndpointMetadata`, `axtp::hasEndpointMetadata(const EndpointMetadata&)`, `axtp::responseEndpointMetadata(const EndpointMetadata&)`, and `std::string axtp::endpointIdFromKey(std::string_view)`.

- [ ] **Step 1: Add failing model and canonical-vector tests**

Add assertions equivalent to:

```cpp
axtp::EndpointMetadata empty;
assert(!axtp::hasEndpointMetadata(empty));

axtp::EndpointMetadata request;
request.src = "ep_app";
request.dst = "ep_device";
const auto response = axtp::responseEndpointMetadata(request);
assert(response.src == request.dst);
assert(response.dst == request.src);

assert(axtp::endpointIdFromKey(
           "device:axtp:hid:1234:5678:SERIAL-1") ==
       "ep_3340a334b47934f471968db6b1470da6");
assert(axtp::endpointIdFromKey("app:nearcast:install-001") ==
       "ep_db5025fc9bd10de9f235ad2637585242");
assert(axtp::endpointIdFromKey("service:lab:receiver-slot-a") ==
       "ep_20d54d9fc87018d571995be978620d21");
bool emptyKeyRejected = false;
try {
    (void)axtp::endpointIdFromKey("");
} catch (const std::invalid_argument&) {
    emptyKeyRejected = true;
}
assert(emptyKeyRejected);
```

- [ ] **Step 2: Run the focused test and confirm the API is absent**

Run:

```bash
cmake -S . -B build/endpoint-relay -DAXTP_BUILD_JSON_RPC=ON
cmake --build build/endpoint-relay --target phase1_model_io_test
ctest --test-dir build/endpoint-relay -R '^phase1_model_io_test$' --output-on-failure
```

Expected: compilation fails because `EndpointMetadata` and `endpointIdFromKey` do not exist.

- [ ] **Step 3: Implement the metadata helpers**

Use this exact public shape in `endpoint_metadata.hpp` and include it from `payload.hpp`:

```cpp
struct EndpointMetadata {
    std::optional<std::string> src;
    std::optional<std::string> dst;
};

inline bool hasEndpointMetadata(const EndpointMetadata& metadata) {
    return metadata.src.has_value() || metadata.dst.has_value();
}

inline EndpointMetadata responseEndpointMetadata(const EndpointMetadata& request) {
    return EndpointMetadata{request.dst, request.src};
}
```

Append `EndpointMetadata endpoint;` to `PayloadMeta` without moving existing members.

- [ ] **Step 4: Implement portable SHA-256 and the public ID helper**

Keep the SHA-256 state, block transform, padding, and 64 round constants in `endpoint_identity.hpp` under `namespace axtp::detail`. Expose only:

```cpp
inline std::string endpointIdFromKey(std::string_view endpointKey) {
    if (endpointKey.empty()) {
        throw std::invalid_argument("endpoint key must not be empty");
    }
    const std::string canonical = "AXTP-ENDPOINT-v1|" + std::string(endpointKey);
    const std::array<std::uint8_t, 32> digest = detail::sha256(canonical);
    static constexpr char kHex[] = "0123456789abcdef";
    std::string id = "ep_";
    id.reserve(35);
    for (std::size_t index = 0; index < 16; ++index) {
        id.push_back(kHex[digest[index] >> 4U]);
        id.push_back(kHex[digest[index] & 0x0fU]);
    }
    return id;
}
```

The internal digest function must process UTF-8 bytes verbatim, use SHA-256 big-endian word loading, append the `0x80` bit and 64-bit bit length, and emit eight big-endian state words. Include `endpoint_identity.hpp` from `axtp_core.hpp` so `axtp::core` consumers get the helper.

- [ ] **Step 5: Run the focused test**

Run the three commands from Step 2 again.

Expected: `phase1_model_io_test` passes and all three fixed vectors match.

- [ ] **Step 6: Commit the model and identity unit**

```bash
git add include/core/protocol/model/endpoint_metadata.hpp \
  include/core/protocol/endpoint/endpoint_identity.hpp \
  include/core/protocol/model/payload.hpp include/axtp_core.hpp \
  tests/core/phase1_model_io_test.cpp
git commit -m "feat: add endpoint relay metadata model"
```

### Task 2: Object-RPC Metadata Codec with Legacy Golden Behavior

**Files:**
- Create: `include/core/protocol/wire/websocket_json_rpc/endpoint_metadata_codec.hpp`
- Modify: `include/core/protocol/wire/websocket_json_rpc/outbound/json_rpc_encoder.hpp`
- Modify: `include/core/protocol/wire/websocket_json_rpc/inbound/json_rpc_payload_decoder.hpp`
- Test: `tests/core/phase2_inbound_test.cpp`
- Test: `tests/core/phase3_outbound_test.cpp`

**Interfaces:**
- Consumes: `EndpointMetadata`, `hasEndpointMetadata`, nlohmann/json.
- Produces: `decodeEndpointMetadata(const nlohmann::json&)`, `addEndpointMetadata(nlohmann::json&, const EndpointMetadata&)`, and request/response/Event codec support for optional `m`.

- [ ] **Step 1: Add failing inbound metadata and session-liveness tests**

Add a table that feeds complete object-RPC messages through the current inbound processor. Assert:

```cpp
// Valid addressed request.
REQUIRE(request.meta.endpoint.src == "ep_app");
REQUIRE(request.meta.endpoint.dst == "ep_device");

// Unknown optional metadata remains usable.
feed(R"({"sid":"S","op":7,"m":{"src":"ep_app","dst":"ep_device","future":{"v":1}},"d":{"id":2,"method":"audio.getAlgorithmConfig","params":{}}})");
REQUIRE(sink.rpcs.size() == 1);

// Each malformed payload is dropped; the following legacy payload still decodes.
const std::vector<std::string> malformed = {
    R"({"sid":"S","op":7,"m":[],"d":{"id":3,"method":"audio.getAlgorithmConfig","params":{}}})",
    R"({"sid":"S","op":7,"m":{"src":"","dst":"ep_device"},"d":{"id":4,"method":"audio.getAlgorithmConfig","params":{}}})",
    R"({"sid":"S","op":7,"m":{"src":7,"dst":"ep_device"},"d":{"id":5,"method":"audio.getAlgorithmConfig","params":{}}})",
    R"({"sid":"S","op":7,"m":{"src":"ep_app","dst":[]},"d":{"id":6,"method":"audio.getAlgorithmConfig","params":{}}})",
};
for (const auto& bad : malformed) {
    sink.rpcs.clear();
    feed(bad);
    REQUIRE(sink.rpcs.empty());
    feed(validLegacyRequest);
    REQUIRE(sink.rpcs.size() == 1);
}
```

- [ ] **Step 2: Add failing outbound and byte-for-byte legacy tests**

Assert the parsed object contains `m.src`/`m.dst` for Request, RequestResponse, and Event. Also retain an exact golden string for an unaddressed request:

```cpp
REQUIRE(text ==
        R"({"d":{"id":41,"method":"audio.getAlgorithmConfig","params":{}},"op":7,"sid":"12345678"})");
REQUIRE(!nlohmann::json::parse(text).contains("m"));
```

Create an Event with only `src` and assert `m.dst` is absent; then copy it, assign one `dst`, and assert the output destination is a string.

- [ ] **Step 3: Run inbound and outbound tests and confirm failure**

```bash
cmake --build build/endpoint-relay --target phase2_inbound_test phase3_outbound_test
ctest --test-dir build/endpoint-relay -R '^(phase2_inbound_test|phase3_outbound_test)$' --output-on-failure
```

Expected: addressed payload assertions fail because the codec currently ignores and omits `m`.

- [ ] **Step 4: Implement the shared JSON metadata codec**

Use these rules in `endpoint_metadata_codec.hpp`:

```cpp
inline EndpointMetadata decodeEndpointMetadata(const nlohmann::json& object) {
    EndpointMetadata result;
    const auto metadata = object.find("m");
    if (metadata == object.end()) return result;
    if (!metadata->is_object()) throw std::invalid_argument("invalid m");
    const auto read = [&](const char* key) -> std::optional<std::string> {
        const auto value = metadata->find(key);
        if (value == metadata->end()) return std::nullopt;
        if (!value->is_string()) throw std::invalid_argument("invalid endpoint metadata");
        auto text = value->get<std::string>();
        if (text.empty()) throw std::invalid_argument("empty endpoint metadata");
        return text;
    };
    result.src = read("src");
    result.dst = read("dst");
    return result;
}

inline void addEndpointMetadata(nlohmann::json& object,
                                const EndpointMetadata& metadata) {
    if (!hasEndpointMetadata(metadata)) return;
    if ((metadata.src && metadata.src->empty()) ||
        (metadata.dst && metadata.dst->empty())) {
        throw std::invalid_argument("empty endpoint metadata");
    }
    auto wire = nlohmann::json::object();
    if (metadata.src) wire["src"] = *metadata.src;
    if (metadata.dst) wire["dst"] = *metadata.dst;
    object["m"] = std::move(wire);
}
```

Unknown keys are intentionally ignored. An array `dst` reaches the non-string branch and is rejected.

- [ ] **Step 5: Wire the helper into every object-RPC path**

In `fillJsonMeta`, assign `payload.meta.endpoint = decodeEndpointMetadata(object)`. In the encoder, call `addEndpointMetadata(object, payload.meta.endpoint)` after assigning `sid` and `op`, before assigning `d`, for Request, RequestResponse, RequestBatchResponse, and Event. Keep Hello/Identify/Identified unchanged and omit `m` for empty metadata.

- [ ] **Step 6: Run the focused codec tests**

Run the Step 3 commands.

Expected: both tests pass; the legacy golden output contains no `m`.

- [ ] **Step 7: Commit the codec unit**

```bash
git add include/core/protocol/wire/websocket_json_rpc/endpoint_metadata_codec.hpp \
  include/core/protocol/wire/websocket_json_rpc/outbound/json_rpc_encoder.hpp \
  include/core/protocol/wire/websocket_json_rpc/inbound/json_rpc_payload_decoder.hpp \
  tests/core/phase2_inbound_test.cpp tests/core/phase3_outbound_test.cpp
git commit -m "feat: encode endpoint relay rpc metadata"
```

### Task 3: Response Reversal Across Core, Broker, and SDK Server Paths

**Files:**
- Modify: `include/core/protocol/wire/websocket_json_rpc/inbound/json_rpc_payload_decoder.hpp`
- Modify: `include/core/runtime/core/rpc_dispatcher.hpp`
- Modify: `include/core/runtime/broker/business_router.hpp`
- Modify: `include/sdk/axtp_server.hpp`
- Test: `tests/core/phase4_core_test.cpp`
- Test: `tests/core/phase7_broker_test.cpp`
- Test: `tests/sdk/sdk_smoke_test.cpp`

**Interfaces:**
- Consumes: `responseEndpointMetadata(const EndpointMetadata&)`.
- Produces: reversed metadata for success, validation/business error, unknown method, batch-unsupported, direct dispatcher, and SDK server response paths; request metadata visible as `RpcContext::endpoint`.

- [ ] **Step 1: Add failing reversal tests for each response constructor**

Use a request with `src=ep_app`, `dst=ep_device`, and assert every returned response has:

```cpp
REQUIRE(response.meta.endpoint.src == "ep_device");
REQUIRE(response.meta.endpoint.dst == "ep_app");
```

Cover `RpcDispatcher`, `BusinessRouter` success, validator rejection, registered-but-unsupported method, and decoder-generated unknown-method/batch errors. Add a legacy request assertion that both response fields remain absent.

- [ ] **Step 2: Add failing handler-context preservation tests**

Register Raw and JSON handlers and assert:

```cpp
REQUIRE(context.endpoint.src == "ep_app");
REQUIRE(context.endpoint.dst == "ep_device");
```

For `AxtpServer::onRaw`, assert the reconstructed `RpcPayload` passed to the handler contains the same request metadata.

- [ ] **Step 3: Run the focused tests and confirm failure**

```bash
cmake --build build/endpoint-relay --target phase4_core_test phase7_broker_test cpp_sdk_smoke_test
ctest --test-dir build/endpoint-relay -R '^(phase4_core_test|phase7_broker_test|cpp_sdk_smoke_test)$' --output-on-failure
```

Expected: responses copy request addresses without reversal and handler context lacks Endpoint metadata.

- [ ] **Step 4: Replace response metadata copies with the reversal helper**

For each response constructor, preserve all existing `PayloadMeta` fields, then replace only Endpoint addressing:

```cpp
response.meta = request.meta;
response.meta.endpoint = responseEndpointMetadata(request.meta.endpoint);
```

Apply the same rule immediately after `fillJsonMeta` for decoder-generated unknown-method and batch-unsupported responses. Do not reverse inbound responses received from the wire.

- [ ] **Step 5: Preserve request addressing in broker and SDK handler context**

Append `EndpointMetadata endpoint;` to `RpcContext`, set it from `request.meta.endpoint`, and copy it when compatibility handlers reconstruct a request:

```cpp
context.endpoint = request.meta.endpoint;
payload.meta.endpoint = context.endpoint;
```

Update `AxtpServer::onRaw` to name the `RpcContext` argument and copy `context.endpoint` into the reconstructed `RpcPayload`.

- [ ] **Step 6: Run the focused tests**

Run the Step 3 commands.

Expected: all response paths reverse addressed requests and legacy paths remain unaddressed.

- [ ] **Step 7: Commit response semantics**

```bash
git add include/core/protocol/wire/websocket_json_rpc/inbound/json_rpc_payload_decoder.hpp \
  include/core/runtime/core/rpc_dispatcher.hpp \
  include/core/runtime/broker/business_router.hpp include/sdk/axtp_server.hpp \
  tests/core/phase4_core_test.cpp tests/core/phase7_broker_test.cpp \
  tests/sdk/sdk_smoke_test.cpp
git commit -m "feat: reverse endpoint addresses in rpc responses"
```

### Task 4: SDK Call Options and JSON_BINARY Compatibility Guard

**Files:**
- Modify: `include/sdk/call_options.hpp`
- Modify: `include/sdk/axtp_client.hpp`
- Modify: `include/core/protocol/wire/framed_binary/outbound/payload_encoder.hpp`
- Test: `tests/sdk/call_options_test.cpp`
- Test: `tests/core/phase3_outbound_test.cpp`

**Interfaces:**
- Consumes: `EndpointMetadata`, `hasEndpointMetadata`, `responseEndpointMetadata`.
- Produces: `sdk::CallOptions::endpoint`; dynamic and typed request propagation; local-handler response reversal; explicit JSON_BINARY rejection.

- [ ] **Step 1: Add failing default-compatibility and propagation tests**

Keep the existing four-field `legacyAggregateOptions` initializer compiling. Add:

```cpp
axtp::sdk::CallOptions addressed;
addressed.endpoint.src = "ep_app";
addressed.endpoint.dst = "ep_device";
addressed.progress = [&] {
    const auto dynamic = takeOutgoingRequest(*transportPtr);
    REQUIRE(dynamic.meta.endpoint.src == addressed.endpoint.src);
    REQUIRE(dynamic.meta.endpoint.dst == addressed.endpoint.dst);
};
addressed.cancelled = [] { return true; };
(void)client.callJson("audio.getAlgorithmConfig", "{}", addressed);
```

For the typed path, call the existing public `makeTypedRequest<MethodId>()` and assert its `meta.endpoint` equals the supplied options. Do not add a production testing hook.

- [ ] **Step 2: Add failing JSON_BINARY and local-handler tests**

Call `callRaw` with JSON_BINARY plus Endpoint metadata and assert `InvalidArgument` and zero outgoing packets. Then make a normal JSON request with a registered local handler and assert the returned response reverses addresses. Assert the current JSON_BINARY encoding length/offset golden remains unchanged for an unaddressed request.

- [ ] **Step 3: Run SDK/outbound tests and confirm failure**

```bash
cmake --build build/endpoint-relay --target cpp_sdk_call_options_test phase3_outbound_test
ctest --test-dir build/endpoint-relay -R '^(cpp_sdk_call_options_test|phase3_outbound_test)$' --output-on-failure
```

Expected: options do not propagate, JSON_BINARY metadata is silently dropped, or local responses retain request direction.

- [ ] **Step 4: Append and propagate the SDK option**

Append this member after `progress`:

```cpp
EndpointMetadata endpoint;
```

Set `payload.meta.endpoint = options.endpoint` in `makeTypedRequest` and `makeDynamicRequest`. Raw `RpcPayload` callers continue to own `request.meta.endpoint`; an empty `CallOptions::endpoint` must not erase it.

- [ ] **Step 5: Validate before allocating a request ID**

At the start of `normalizeRequest`, before inserting into `_usedRequestIds`, enforce:

```cpp
const auto invalidEndpoint = [](const EndpointMetadata& endpoint) {
    return (endpoint.src && endpoint.src->empty()) ||
           (endpoint.dst && endpoint.dst->empty());
};
if (invalidEndpoint(request.meta.endpoint)) return false;
if (isJsonBinaryRpcEncoding(request.encoding) &&
    hasEndpointMetadata(request.meta.endpoint)) return false;
```

Apply the same JSON_BINARY check at the start of
`PayloadEncoder::encodeRpc` and throw `std::invalid_argument`. This protects
direct Core users as well as SDK callers; the unaddressed fixed-header branch
must remain byte-for-byte unchanged.

For local handler and `makeErrorResponse`, copy the existing meta and replace only the Endpoint part with `responseEndpointMetadata(request.meta.endpoint)`.

- [ ] **Step 6: Run the focused tests**

Run the Step 3 commands.

Expected: propagation and reversal pass, the invalid combination sends no bytes, and legacy JSON_BINARY golden assertions remain unchanged.

- [ ] **Step 7: Commit SDK support**

```bash
git add include/sdk/call_options.hpp include/sdk/axtp_client.hpp \
  include/core/protocol/wire/framed_binary/outbound/payload_encoder.hpp \
  tests/sdk/call_options_test.cpp tests/core/phase3_outbound_test.cpp
git commit -m "feat: expose endpoint metadata in sdk calls"
```

### Task 5: Direct WebSocket Adapter Error Compatibility

**Files:**
- Modify: `include/json_rpc/websocket_json_rpc_adapter.hpp`
- Test: `tests/core/phase6_json_rpc_session_test.cpp`

**Interfaces:**
- Consumes: `decodeEndpointMetadata`, `responseEndpointMetadata`, `addEndpointMetadata`.
- Produces: addressed request-before-ready and other adapter-generated errors with reversed metadata, without changing legacy error JSON.

- [ ] **Step 1: Add failing adapter-generated error tests**

Send an addressed, structurally valid request before APP_READY and assert the error response preserves request ID and contains `m.src=ep_device`, `m.dst=ep_app`. Send the same request without `m` and compare its response against the existing legacy error golden with no `m`.

- [ ] **Step 2: Run the session test and confirm failure**

```bash
cmake --build build/endpoint-relay --target phase6_json_rpc_session_test
ctest --test-dir build/endpoint-relay -R '^phase6_json_rpc_session_test$' --output-on-failure
```

Expected: addressed adapter errors omit `m`.

- [ ] **Step 3: Carry valid request metadata through `sendError`**

Extend the private error helper to accept `EndpointMetadata endpoint = {}`. At the call site, parse `m` with `decodeEndpointMetadata`; pass `responseEndpointMetadata(requestEndpoint)` only when parsing succeeds. In `sendError`, call `addEndpointMetadata` before serializing. Invalid `m` remains a payload error and must not be echoed.

- [ ] **Step 4: Run the session test**

Run the Step 2 commands.

Expected: addressed errors reverse metadata and legacy errors are unchanged.

- [ ] **Step 5: Commit adapter error behavior**

```bash
git add include/json_rpc/websocket_json_rpc_adapter.hpp \
  tests/core/phase6_json_rpc_session_test.cpp
git commit -m "fix: preserve endpoint routing on json rpc errors"
```

### Task 6: Pinned Spec Upgrade and Endpoint Relay Conformance

**Files:**
- Modify: `AXTP_SPEC.lock.yaml`
- Modify: `package.json`
- Modify: `include/core/protocol/generated/**` only through the generator
- Modify: `generated/axtp_generated_manifest.json` only through the generator
- Modify: `devtools/conformance/runtime-profile.yaml`
- Modify: `devtools/conformance/conformance_runner.cpp`
- Modify: `README.md`
- Modify: `docs/AXTP_CPP_RUNTIME_PATTERNS.md`
- Test: axtp cases `rpc.endpoint_metadata_compatibility`, `rpc.endpoint_relay_addressing`, and `event.endpoint_fanout_addressing`.

**Interfaces:**
- Consumes: a released spec tag containing commit `2d88ff4c369411d8e3afbb551886d423bd63bb82`, runtime codec APIs from Tasks 1-5.
- Produces: reproducible `endpoint-relay` conformance support and an updated runtime spec lock.

- [ ] **Step 1: Prove a released tag contains the authority commit**

With `AXTP_SPEC_PATH` pointing at a full axtp checkout, run:

```bash
git -C "$AXTP_SPEC_PATH" fetch --tags origin
endpoint_spec_tag=$(git -C "$AXTP_SPEC_PATH" tag --contains \
  2d88ff4c369411d8e3afbb551886d423bd63bb82 \
  --list 'spec/v*' --sort=version:refname | head -n 1)
test -n "$endpoint_spec_tag"
```

Expected: the command succeeds and prints a released tag through `echo "$endpoint_spec_tag"`. If it fails, stop this task: unit implementation may remain on the runtime feature branch, but the runtime PR, Axent pin update, and conformance claim are not ready.

- [ ] **Step 2: Upgrade the runtime lock through the repository script**

```bash
devtools/scripts/upgrade-axtp-spec.sh "$endpoint_spec_tag"
devtools/scripts/check-axtp-spec-lock.sh
```

Expected: the lock's tag and peeled commit match the released tag; neither value is `main` or `unreleased`.

- [ ] **Step 3: Regenerate only through the supported generator**

```bash
pnpm --dir devtools/generators install --frozen-lockfile
pnpm --dir devtools/generators build
pnpm --dir devtools/generators test
AXTP_SPEC_PATH="$AXTP_SPEC_PATH" pnpm --dir devtools/generators generate:runtime
devtools/scripts/check-generated-version.sh
```

Expected: generator tests and version checks pass.

- [ ] **Step 4: Add failing conformance dispatch coverage**

Add `endpoint-relay` to `required_levels`. Add three explicit handlers in `executeLoadedCase` for the case IDs above. The compatibility handler must feed legacy then extended requests through one APP_READY adapter and verify liveness; the relay handler must verify a singular destination and reversed response; the Event handler must decode one source Event, encode two copies with different string destinations, and preserve the source.

- [ ] **Step 5: Run conformance and confirm the new cases fail**

```bash
AXTP_SPEC_PATH="$AXTP_SPEC_PATH" devtools/scripts/run-conformance.sh
```

Expected: each new Endpoint Relay case reports failed because its newly registered handler returns `false`; existing required cases remain passing.

- [ ] **Step 6: Implement the three conformance handlers**

Use the existing `MemoryJsonTransport`, `setupJsonAdapter`, `runCase`, and JSON comparison helpers. Dispatch by exact IDs:

```cpp
{"rpc.endpoint_metadata_compatibility", testEndpointMetadataCompatibility},
{"rpc.endpoint_relay_addressing", testEndpointRelayAddressing},
{"event.endpoint_fanout_addressing", testEndpointFanoutAddressing},
```

The runtime handler validates wire mechanics only. It must not introduce a provider registry, subscription policy, or multi-hop route model.

- [ ] **Step 7: Document compatibility and the new profile**

In README and runtime patterns, state that Endpoint Relay is additive, empty metadata omits `m`, legacy peers continue through unaddressed requests, response addressing reverses when present, and JSON_BINARY is unchanged.

- [ ] **Step 8: Run conformance and the full runtime suite**

```bash
AXTP_SPEC_PATH="$AXTP_SPEC_PATH" devtools/scripts/run-conformance.sh
cmake -S . -B build/endpoint-relay-clean -DAXTP_BUILD_JSON_RPC=ON
cmake --build build/endpoint-relay-clean
ctest --test-dir build/endpoint-relay-clean --output-on-failure
devtools/scripts/check-format-cpp.sh
git diff --check
```

Expected: all required Endpoint Relay cases pass, all CTests pass, format passes, and `git diff --check` is silent.

- [ ] **Step 9: Commit the reproducible conformance unit**

```bash
git add AXTP_SPEC.lock.yaml package.json include/core/protocol/generated \
  generated/axtp_generated_manifest.json devtools/conformance/runtime-profile.yaml \
  devtools/conformance/conformance_runner.cpp README.md \
  docs/AXTP_CPP_RUNTIME_PATTERNS.md
git commit -m "test: declare endpoint relay conformance"
```

### Task 7: Runtime Branch Review Gate

**Files:**
- Verify: every file changed by Tasks 1-6.

**Interfaces:**
- Consumes: all runtime commits and the pinned released spec.
- Produces: one reviewed runtime commit SHA suitable for Axent's submodule pin.

- [ ] **Step 1: Inspect scope and generated-file provenance**

```bash
git status --short
git diff origin/main...HEAD --stat
git diff origin/main...HEAD -- include/core/protocol/generated generated/axtp_generated_manifest.json
```

Expected: only planned files changed; generated diffs correspond to the locked spec upgrade and were not hand-edited.

- [ ] **Step 2: Re-run release-grade verification from a clean build directory**

```bash
cmake -S . -B build/endpoint-relay-final -DAXTP_BUILD_JSON_RPC=ON
cmake --build build/endpoint-relay-final
ctest --test-dir build/endpoint-relay-final --output-on-failure
AXTP_SPEC_PATH="$AXTP_SPEC_PATH" CONFORMANCE_BUILD_DIR=build/endpoint-relay-conformance-final \
  CONFORMANCE_RESULT_DIR=build/endpoint-relay-conformance-results-final \
  devtools/scripts/run-conformance.sh
devtools/scripts/check-axtp-spec-lock.sh
devtools/scripts/check-generated-version.sh
devtools/scripts/check-format-cpp.sh
git diff --check origin/main...HEAD
```

Expected: all commands pass.

- [ ] **Step 3: Record the pin candidate without merging Axent**

```bash
git rev-parse HEAD
git log --oneline origin/main..HEAD
```

Expected: the printed HEAD is the reviewed runtime pin candidate. Open the runtime PR; do not update Axent's submodule to an unreviewed working-tree commit.
