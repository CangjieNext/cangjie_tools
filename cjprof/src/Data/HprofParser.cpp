// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cinttypes>
#include "Data/HprofParser.h"

void HprofParserBase::SetData(const std::string &data)
{
    m_data = data;
    m_curPos = 0;
}

// ============================================================================
// HprofParserV2 Implementation (New Format 1.0.2)
// ============================================================================

HprofParserBase::ID HprofParserV2::ReadId()
{
    if (m_data.size() - m_curPos < m_idSize) {
        return 0;
    }
    // New format: ID is u4
    u4 val = *(u4 *)(m_data.data() + m_curPos);
    m_curPos += 4;
    return SwapEndian(val);
}

void HprofParserV2::WriteId(ID id)
{
    WriteAs(SwapEndian(static_cast<u4>(id)));
}

void HprofParserV2::ParseString(bool verbose)
{
    size_t startPos = m_curPos;

    // New format: length is u4, id is u4
    auto lengthRec = ReadAs<u4>();
    if (lengthRec == nullptr) {
        fprintf(stderr, "error: StringRecord length truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    auto length = SwapEndian(*lengthRec);
    m_curPos += sizeof(u4);

    auto idRec = ReadAs<u4>();
    if (idRec == nullptr) {
        fprintf(stderr, "error: StringRecord id truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(u4);

    if (length < sizeof(u4)) {
        fprintf(stderr, "error: StringRecord length too small at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    m_strings[id] = std::string(m_data.data() + m_curPos, length - sizeof(u4));
    m_curPos += length - sizeof(u4);

    if (verbose) {
        printf("[STRING@0x%zx] id = 0x%" PRIx64 ", str = \"%s\"\n", startPos, id, m_strings[id].c_str());
    }
}

void HprofParserV2::ParseClass(bool verbose)
{
    size_t startPos = m_curPos;

    // New format: no length field, id=u4, name=u4, size=u4
    auto idRec = ReadAs<u4>();
    if (idRec == nullptr) {
        fprintf(stderr, "error: ClassRecord id truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(u4);

    auto nameRec = ReadAs<u4>();
    if (nameRec == nullptr) {
        fprintf(stderr, "error: ClassRecord name truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    m_classes[id].name = SwapEndian(*nameRec);
    m_curPos += sizeof(u4);

    auto sizeRec = ReadAs<u4>();
    if (sizeRec == nullptr) {
        fprintf(stderr, "error: ClassRecord size truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    m_classes[id].size = SwapEndian(*sizeRec);
    m_curPos += sizeof(u4);

    if (verbose) {
        printf("[LOAD CLASS@0x%zx] id = 0x%" PRIx64 ", name id = 0x%" PRIx64 ", size = %u\n",
               startPos, id, m_classes[id].name, m_classes[id].size);
    }
}

void HprofParserV2::ParseHeapDumpClassDump(bool verbose)
{
    size_t startPos = m_curPos;

    // New format: id=u4, name=u4, size=u4
    auto idRec = ReadAs<u4>();
    if (idRec == nullptr) {
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(u4);

    auto nameRec = ReadAs<u4>();
    if (nameRec == nullptr) {
        return;
    }
    m_classes[id].name = SwapEndian(*nameRec);
    m_curPos += sizeof(u4);

    auto sizeRec = ReadAs<u4>();
    if (sizeRec == nullptr) {
        return;
    }
    m_classes[id].size = SwapEndian(*sizeRec);
    m_curPos += sizeof(u4);

    if (verbose) {
        printf("[CLASS DUMP@0x%zx] id = 0x%" PRIx64 ", name id = 0x%" PRIx64 ", size = %u\n",
               startPos, id, m_classes[id].name, m_classes[id].size);
    }
}

void HprofParserV2::ParseHeapDumpInstanceDump(bool verbose)
{
    size_t startPos = m_curPos;

    // New format: id and cls are u4 (ReadId)
    ID id = ReadId();
    ID cls = ReadId();

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

    // New format: values are u4
    for (size_t i = 0; i < num; i++) {
        auto val = ReadAs<u4>();
        if (val == nullptr) {
            return;
        }
        m_instances[id].fields.push_back(SwapEndian(*val));
        m_curPos += sizeof(u4);
    }
    m_instances[id].cls = cls;

    if (verbose) {
        printf("[INSTANCE DUMP@0x%zx] id = 0x%" PRIx64 ", class = 0x%" PRIx64 ", value = [", startPos, id, cls);

        auto fields = m_instances[id].fields;
        for (size_t i = 0; i < fields.size(); i++) {
            printf("0x%" PRIx64 "%s", fields[i], i < fields.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

// ============================================================================
// HprofParserV1 Implementation (Old Format 1.0.1)
// ============================================================================

HprofParserBase::ID HprofParserV1::ReadId()
{
    if (m_data.size() - m_curPos < m_idSize) {
        return 0;
    }
    // Old format: ID is u8
    u8 val = *(u8 *)(m_data.data() + m_curPos);
    m_curPos += 8;
    return SwapEndian(val);
}

void HprofParserV1::WriteId(ID id)
{
    WriteAs(SwapEndian(id));
}

void HprofParserV1::ParseString(bool verbose)
{
    size_t startPos = m_curPos;

    // Old format: length is u8, id is ID (u8)
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        fprintf(stderr, "error: StringRecord length truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    auto length = SwapEndian(*lengthRec);
    m_curPos += sizeof(u8);

    auto idRec = ReadAs<ID>();
    if (idRec == nullptr) {
        fprintf(stderr, "error: StringRecord id truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(ID);

    if (length < sizeof(ID)) {
        fprintf(stderr, "error: StringRecord length too small at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    m_strings[id] = std::string(m_data.data() + m_curPos, length - sizeof(ID));
    m_curPos += length - sizeof(ID);

    if (verbose) {
        printf("[STRING@0x%zx] id = 0x%" PRIx64 ", str = \"%s\"\n", startPos, id, m_strings[id].c_str());
    }
}

void HprofParserV1::ParseClass(bool verbose)
{
    size_t startPos = m_curPos;

    // Old format: has length(u8), id=ID, name=ID
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        fprintf(stderr, "error: ClassRecord length truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    m_curPos += sizeof(u8);

    auto idRec = ReadAs<ID>();
    if (idRec == nullptr) {
        fprintf(stderr, "error: ClassRecord id truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(ID);

    auto nameRec = ReadAs<ID>();
    if (nameRec == nullptr) {
        fprintf(stderr, "error: ClassRecord name truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();
        return;
    }
    m_classes[id].name = SwapEndian(*nameRec);
    m_curPos += sizeof(ID);

    if (verbose) {
        printf("[LOAD CLASS@0x%zx] id = 0x%" PRIx64 ", name id = 0x%" PRIx64 "\n", startPos, id, m_classes[id].name);
    }
}

void HprofParserV1::ParseHeapDumpClassDump(bool verbose)
{
    size_t startPos = m_curPos;

    // Old format: id=ID, size=u4
    auto idRec = ReadAs<ID>();
    if (idRec == nullptr) {
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(ID);

    auto sizeRec = ReadAs<u4>();
    if (sizeRec == nullptr) {
        return;
    }
    m_classes[id].size = SwapEndian(*sizeRec);
    m_curPos += sizeof(u4);

    if (verbose) {
        printf("[CLASS DUMP@0x%zx] id = 0x%" PRIx64 ", size = %u\n", startPos, id, m_classes[id].size);
    }
}

void HprofParserV1::ParseHeapDumpInstanceDump(bool verbose)
{
    size_t startPos = m_curPos;

    // Old format: id and cls are fixed ID (u8)
    auto idRec = ReadAs<ID>();
    if (idRec == nullptr) {
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(ID);

    auto clsRec = ReadAs<ID>();
    if (clsRec == nullptr) {
        return;
    }
    ID cls = SwapEndian(*clsRec);
    m_curPos += sizeof(ID);

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

    // Old format: values are ID (u8)
    for (size_t i = 0; i < num; i++) {
        auto val = ReadAs<ID>();
        if (val == nullptr) {
            return;
        }
        m_instances[id].fields.push_back(SwapEndian(*val));
        m_curPos += sizeof(ID);
    }
    m_instances[id].cls = cls;

    if (verbose) {
        printf("[INSTANCE DUMP@0x%zx] id = 0x%" PRIx64 ", class = 0x%" PRIx64 ", value = [", startPos, id, cls);

        auto fields = m_instances[id].fields;
        for (size_t i = 0; i < fields.size(); i++) {
            printf("0x%" PRIx64 "%s", fields[i], i < fields.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}