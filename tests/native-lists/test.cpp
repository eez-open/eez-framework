#include <eez/core/assets.h>
#include <eez/core/alloc.h>
#include <eez/core/action.h>
#include <eez/core/vars.h>
#include <eez/flow/flow.h>
#include <eez/flow/private.h>
#include <eez/flow/expression.h>
#include <eez/flow/hooks.h>
#include <eez/libs/lz4/lz4.h>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression); \
            std::abort(); \
        } \
    } while (false)

using namespace eez;
using namespace eez::flow;

namespace eez { ActionExecFunc g_actionExecFunctions[] = { nullptr }; }
static int nativeInteger;
static int getNativeInteger() { return nativeInteger; }
static void setNativeInteger(int value) { nativeInteger = value; }
native_var_t native_vars[] = {
    { NATIVE_VAR_TYPE_NONE, nullptr, nullptr },
    { NATIVE_VAR_TYPE_INTEGER, (void *)getNativeInteger, (void *)setNativeInteger }
};

template<class T> static void setList(ListOfAssetsPtr<T> &list, AssetsPtr<T> *items, uint32_t count) {
    list.count = count;
    int32_t offset = (uint8_t *)items - ((uint8_t *)&list + 4);
    std::memcpy((uint8_t *)&list + 4, &offset, sizeof(offset));
}
struct Fixture {
    Assets assets;
    FlowDefinition definition;
    Flow flow;
    AssetsPtr<Value> globalPointers[7];
    Value globals[7];
    Value locals[1];
    FlowState state{};
    Fixture() {
        assets.flowDefinition = &definition;
        assets.reserved = sizeof(*this);
        setList(definition.globalVariables, globalPointers, 7);
        for (int i = 0; i < 7; ++i) globalPointers[i] = &globals[i];
        globals[0] = BooleanValue(true);
        globals[1] = IntegerValue(10);
        globals[2] = IntegerValue(20);
        globals[3] = StringValue("alpha");
        globals[4] = StringValue("beta");
        globals[5] = Value::makeArrayRef(2, defs_v3::ARRAY_TYPE_INTEGER, 0x1c349554);
        globals[5].getArray()->values[0] = IntegerValue(4);
        globals[5].getArray()->values[1] = IntegerValue(5);
        globals[6] = Value::makeArrayRef(1, defs_v3::ARRAY_TYPE_STRING, 0x1c349555);
        state.assets = &assets; state.flow = &flow; state.values = locals;
    }
};
static void word(std::vector<uint8_t> &bytes, uint16_t value) {
    bytes.push_back(value & 255); bytes.push_back(value >> 8);
}
static std::vector<uint8_t> list(const std::vector<std::vector<uint16_t>> &items) {
    std::vector<uint8_t> bytes;
    word(bytes, EXPR_EVAL_ASSIGNABLE_LIST); word(bytes, items.size()); word(bytes, 0); word(bytes, 0);
    for (const auto &item : items) { word(bytes, item.size()); for (auto value : item) word(bytes, value); }
    bytes[4] = bytes.size() & 255; bytes[5] = bytes.size() >> 8;
    return bytes;
}
static bool evaluate(Fixture &fixture, const std::vector<uint8_t> &bytes, Value &value) {
    return evalAssignableListExpression(&fixture.state, -1, bytes.data(), bytes.size(), value, FlowError::Plain("test"));
}
static unsigned checks;
static unsigned errors;
static void onError(FlowState *, int, const char *) { ++errors; }
static void onStop() {}

struct PropertyFixture : Fixture {
    Component component{};
    AssetsPtr<Component> components[1];
    AssetsPtr<Property> properties[1];
    uint8_t instructions[32]{};

    PropertyFixture() {
        assets.reserved = sizeof(*this);
        setList(flow.components, components, 1);
        components[0] = &component;
        setList(component.properties, properties, 1);
        properties[0] = reinterpret_cast<Property *>(instructions);
        globals[1] = IntegerValue(42);
    }

    void setInstructions(const std::vector<uint8_t> &bytes) {
        CHECK(bytes.size() <= sizeof(instructions));
        std::memcpy(instructions, bytes.data(), bytes.size());
    }
};

static void checkPropertyDispatch(PropertyFixture &fixture, bool hasAssetSize) {
    const auto error = FlowError::Plain("property dispatch");
    const uint16_t global = EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR | 1;
    const auto end = EXPR_EVAL_INSTRUCTION_TYPE_END;
    std::vector<uint8_t> expression;
    word(expression, global);
    word(expression, end);
    fixture.setInstructions(expression);
    Value result;
    CHECK(evalProperty(&fixture.state, 0, 0, result, error));
    CHECK(result.isInt32() && result.getInt() == 42);

    fixture.setInstructions(list({ { global, end } }));
    CHECK(evalProperty(&fixture.state, 0, 0, result, error) == hasAssetSize);
    if (hasAssetSize) {
        CHECK(result.isArray() && result.getArray()->arraySize == 1);
        CHECK(result.getArray()->values[0].getValue().getInt() == 42);
    } else {
        CHECK(result.getType() == VALUE_TYPE_UNDEFINED);
    }
    ++checks;
}

static void rejected(Fixture &fixture, std::vector<uint8_t> bytes) {
    // Allocate exactly this many bytes: ASan must see reads beyond the supplied
    // span, rather than reads hidden inside a vector's spare capacity.
    auto input = bytes.empty() ? nullptr : new uint8_t[bytes.size()];
    if (input) std::memcpy(input, bytes.data(), bytes.size());
    Value result(123);
    CHECK(!evalAssignableListExpression(&fixture.state, -1, input, bytes.size(), result, FlowError::Plain("malformed")));
    CHECK(result.getType() == VALUE_TYPE_UNDEFINED);
    delete[] input;
    ++checks;
}
#ifdef EEZ_HAS_COMPILER_FIXTURE
#include "compiler-fixture.h"
#endif

int main() {
    Fixture fixture;
    g_globalVariables = nullptr;
    enableThrowError(false);
    const uint16_t global = EXPR_EVAL_INSTRUCTION_TYPE_PUSH_GLOBAL_VAR;
    const uint16_t end = EXPR_EVAL_INSTRUCTION_TYPE_END;
    const uint16_t typedEnd = EXPR_EVAL_INSTRUCTION_TYPE_END_WITH_DST_VALUE_TYPE;
    const uint16_t jump = EXPR_EVAL_INSTRUCTION_JUMP;
    Value result;
    auto valid = list({ { (uint16_t)(global | 1), typedEnd, 7, 0 }, { (uint16_t)(global | 3), typedEnd, 13, 0 } });
    CHECK(evaluate(fixture, valid, result));
    CHECK(result.getArray()->arraySize == 2);
    CHECK(result.getArray()->values[0].getType() == VALUE_TYPE_VALUE_PTR);
    g_executeActionFlowState = &fixture.state; g_executeActionComponentIndex = -1;
    setUserPropertyReference(result.getArray()->values[0], IntegerValue(31));
    setUserPropertyReference(result.getArray()->values[1], StringValue("changed"));
    CHECK(fixture.globals[1].getInt() == 31);
    CHECK(std::string(fixture.globals[3].getString()) == "changed");
    ++checks;
    Value native(1, VALUE_TYPE_NATIVE_VARIABLE);
    setUserPropertyReference(native, IntegerValue(51));
    CHECK(nativeInteger == 51);
    Value arrayElement = Value::makeArrayElementRef(fixture.globals[5], 0, 0x1c349556);
    setUserPropertyReference(arrayElement, IntegerValue(61));
    CHECK(fixture.globals[5].getArray()->values[0].getInt() == 61);
    Value local(&fixture.locals[0], VALUE_TYPE_VALUE_PTR);
    setUserPropertyReference(local, IntegerValue(71));
    CHECK(fixture.locals[0].getInt() == 71);
    ++checks;

#ifdef EEZ_HAS_COMPILER_FIXTURE
    for (bool flag : { false, true }) {
        fixture.globals[0] = BooleanValue(flag);
        CHECK(evaluate(fixture, compiledCase(1), result));
        auto &integerReference = result.getArray()->values[0];
        CHECK(integerReference.getValue().isInt32());
        setUserPropertyReference(integerReference, IntegerValue(flag ? 101 : 202));
        CHECK(fixture.globals[flag ? 1 : 2].getInt() == (flag ? 101 : 202));
        CHECK(evaluate(fixture, compiledCase(2), result));
        setUserPropertyReference(result.getArray()->values[0], StringValue(flag ? "true branch" : "false branch"));
        CHECK(std::string(fixture.globals[flag ? 3 : 4].getString()) == (flag ? "true branch" : "false branch"));
        ++checks;
    }
#endif
    // Public API invalid-target behavior and out-of-native-call no-op.
    setUserPropertyReference(IntegerValue(123), IntegerValue(999));
    CHECK(fixture.locals[0].getInt() == 71);
    onFlowErrorHook = onError;
    stopScriptHook = onStop;
    enableThrowError(true);
    g_executeActionFlowState = &fixture.state;
    setUserPropertyReference(IntegerValue(1), IntegerValue(2));
    CHECK(errors == 1);
    auto invalidElement = Value::makeArrayElementRef(fixture.globals[5], 99, 0x18257e56);
    setUserPropertyReference(invalidElement, IntegerValue(99));
    CHECK(errors == 2 && fixture.globals[1].isInt32());
    enableThrowError(false);
    g_executeActionFlowState = nullptr;
    setUserPropertyReference(local, IntegerValue(999));
    CHECK(fixture.locals[0].getInt() == 71);
    ++checks;

    // Every shorter prefix, forged lengths, truncated typed END, missing END,
    // invalid opcode, and jumps outside or into operands.
    for (size_t i = 0; i < valid.size(); ++i) rejected(fixture, {valid.begin(), valid.begin() + i});
    auto wrong = valid; wrong[4] = 255; rejected(fixture, wrong);
    wrong = valid; wrong[8] = 255; rejected(fixture, wrong);
    rejected(fixture, list({ { (uint16_t)(global | 1) } }));
    rejected(fixture, list({ { (uint16_t)(global | 1), typedEnd } }));
    rejected(fixture, list({ { (uint16_t)(global | 1), typedEnd, 7 } }));
    rejected(fixture, list({ { (uint16_t)(global | 1), 0 }, { (uint16_t)(global | 3), end } }));
    rejected(fixture, list({ { (uint16_t)(global | 1), 0xe555 } }));
    rejected(fixture, list({ { (uint16_t)(global | 1), (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_OPERATION | 0x1fff), end } }));
    rejected(fixture, list({ { (uint16_t)(EXPR_EVAL_INSTRUCTION_TYPE_PUSH_CONSTANT | 0x1fff), end } }));
    rejected(fixture, list({ { jump, 0xffff, (uint16_t)(global | 1), end } }));
    rejected(fixture, list({ { jump, 1, typedEnd, 7, 0 } }));
    rejected(fixture, list({ { (uint16_t)(jump | 1), 0xffff, end } }));
    rejected(fixture, list({ { (uint16_t)(jump | 1), 0, (uint16_t)(global | 1), end } }));
    CHECK(evaluate(fixture, valid, result)); // No stack contamination after failures.
    ++checks;

    // A branch in an item longer than the inline boundary map still uses the
    // same validation and skips the unused instructions.
    constexpr uint16_t skippedWords = 128;
    std::vector<uint16_t> longItem{jump, skippedWords};
    longItem.insert(longItem.end(), skippedWords, global | 2);
    longItem.push_back(global | 1);
    longItem.push_back(end);
    CHECK(evaluate(fixture, list({longItem}), result));
    CHECK(result.getArray()->values[0].getValue().getInt() == fixture.globals[1].getInt());
    ++checks;
    longItem[1] += 2;
    rejected(fixture, list({longItem}));

    // Legacy external assets may have no registered length. The new list
    // requires a bounded span even when an ordinary property is readable.
    PropertyFixture externalProperties;
    checkPropertyDispatch(externalProperties, true);
    externalProperties.assets.reserved = 0;
    checkPropertyDispatch(externalProperties, false);

    // ROM assets get their length from the loader, ignoring the stored field.
    alignas(PropertyFixture) uint8_t rom[sizeof(PropertyFixture) + alignof(PropertyFixture)]{};
    auto romFixture = new (rom + alignof(PropertyFixture)) PropertyFixture;
    auto romData = reinterpret_cast<uint8_t *>(romFixture) - sizeof(uint32_t);
    const uint32_t tag = HEADER_TAG;
    std::memcpy(romData, &tag, sizeof(tag));
    romFixture->assets.reserved = UINT32_MAX;
    loadMainAssets(romData, sizeof(PropertyFixture) + sizeof(tag));
    CHECK(getAssetsDataSize(&romFixture->assets) == sizeof(PropertyFixture));
    CHECK(getAssetsRemainingSize(&romFixture->assets, rom + sizeof(rom) - 1) == 1);
    CHECK(getAssetsRemainingSize(&romFixture->assets, rom + sizeof(rom)) == 0);
    checkPropertyDispatch(*romFixture, true);

    // A zero byte count in the built-in loader is an empty span, even if an
    // older property is still present in the buffer.
    std::vector<uint8_t> oldProperty;
    word(oldProperty, global | 1);
    word(oldProperty, end);
    romFixture->setInstructions(oldProperty);
    loadMainAssets(romData, sizeof(tag));
    CHECK(!evalProperty(&romFixture->state, 0, 0, result, FlowError::Plain("empty ROM span")));
    ++checks;
    g_mainAssets = nullptr;
    romFixture->~PropertyFixture();
    ++checks;

    // External compressed assets record actual decompressed bytes only on a
    // successful safe decompression; allocation capacity is not the boundary.
    uint8_t plain[64]{};
    uint8_t compressed[256]{};
    auto header = (Header *)compressed;
    header->tag = HEADER_TAG_COMPRESSED; header->decompressedSize = sizeof(plain);
    int compressedSize = LZ4_compress_default((const char *)plain, (char *)compressed + sizeof(Header), sizeof(plain), sizeof(compressed) - sizeof(Header));
    CHECK(compressedSize > 0);
    alignas(8) uint8_t expanded[512]{};
    auto external = new (expanded) Assets;
    CHECK(decompressAssetsData(compressed, sizeof(Header) + compressedSize, external, sizeof(expanded), nullptr));
    const auto payloadOffset = reinterpret_cast<std::uintptr_t>(&external->settings) -
                               reinterpret_cast<std::uintptr_t>(expanded);
    CHECK(getAssetsDataSize(external) == payloadOffset + sizeof(plain));
    CHECK(getAssetsRemainingSize(external, expanded + getAssetsDataSize(external)) == 0);
    ++checks;

    // A failed reload invalidates the old size, and an undersized destination
    // must not be written at all.
    CHECK(!decompressAssetsData(compressed, sizeof(Header) - 1, external, sizeof(expanded), nullptr));
    CHECK(getAssetsDataSize(external) == 0);
    CHECK(decompressAssetsData(compressed, sizeof(Header) + compressedSize, external, sizeof(expanded), nullptr));
    CHECK(getAssetsDataSize(external) == payloadOffset + sizeof(plain));
    alignas(Assets) uint8_t undersized[1]{0xa5};
    CHECK(!decompressAssetsData(compressed, sizeof(Header) + compressedSize,
        reinterpret_cast<Assets *>(undersized), sizeof(undersized), nullptr));
    CHECK(undersized[0] == 0xa5);
    ++checks;
    std::printf("PASS: %u native reference/bounds checks (real framework)\n", checks);
}
