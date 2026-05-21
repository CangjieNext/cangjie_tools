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

// ReadId and WriteId are based on m_idSize (set from file header)
HprofParserBase::ID HprofParserBase::ReadId()
{
    if (m_data.size() - m_curPos < m_idSize) {
        return 0;
    }
    if (m_idSize == 4) {
        u4 val = *(u4 *)(m_data.data() + m_curPos);
        m_curPos += 4;
        return SwapEndian(val);
    } else if (m_idSize == 8) {
        u8 val = *(u8 *)(m_data.data() + m_curPos);
        m_curPos += 8;
        return SwapEndian(val);
    }
    return 0;
}

void HprofParserBase::WriteId(ID id)
{
    if (m_idSize == 4) {
        WriteAs(SwapEndian(static_cast<u4>(id)));
    } else if (m_idSize == 8) {
        WriteAs(SwapEndian(static_cast<u8>(id)));
    }
}

// ============================================================================
// HprofParserV2 Implementation (Format 1.0.2)
// ============================================================================
//
// V2 Format: ID is dynamic (based on idSize from file header, currently u4)
//

// V2 STRING Format: u4 length, u4 id, u1 str[]
void HprofParserV2::ParseString(bool verbose)
{
    size_t startPos = m_curPos;
    // V2: length is u4, id is u4
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

// V2 CLASS (LoadClass) Format: u4 id, u4 name, u4 size (no length field)
void HprofParserV2::ParseClass(bool verbose)
{
    size_t startPos = m_curPos;
    // V2: no length field, id=u4, name=u4, size=u4
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

// V2 CLASS_DUMP Format: u4 id, u4 name, u4 size
void HprofParserV2::ParseHeapDumpClassDump(bool verbose)
{
    size_t startPos = m_curPos;
    // V2: id=u4, name=u4, size=u4
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

// V2 INSTANCE_DUMP Format: ID id, ID cls, u4 num, u4 values[]
void HprofParserV2::ParseHeapDumpInstanceDump(bool verbose)
{
    size_t startPos = m_curPos;
    // V2: id and cls are dynamic ID (ReadId), values are u4
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
// HprofParserV1 Implementation (Format 1.0.1)
// ============================================================================
//
// V1 Format: ID is fixed u8 (8 bytes)
//

// V1 STRING Format: u8 length, ID(u8) id, u1 str[]
void HprofParserV1::ParseString(bool verbose)
{
    size_t startPos = m_curPos;

    // V1: length is u8, id is ID (u8)
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

// V1 CLASS (LoadClass) Format: u8 length, ID(u8) id, ID(u8) name (no size)
void HprofParserV1::ParseClass(bool verbose)
{
    size_t startPos = m_curPos;

    // V1: has length(u8), id=ID, name=ID
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

// V1 CLASS_DUMP Format: ID(u8) id, u4 size (no name)
void HprofParserV1::ParseHeapDumpClassDump(bool verbose)
{
    size_t startPos = m_curPos;

    // V1: id=ID(u8), size=u4 (no name field)
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

// V1 INSTANCE_DUMP Format: ID(u8) id, ID(u8) cls, u4 num, ID(u8) values[]
void HprofParserV1::ParseHeapDumpInstanceDump(bool verbose)
{
    size_t startPos = m_curPos;

    // V1: id and cls are fixed ID (u8), values are ID(u8)
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
    // V1: values are ID (u8)
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