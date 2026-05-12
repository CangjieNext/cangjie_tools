// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <fstream>
#include <iostream>
#include <cinttypes>
#include <functional>
#include <sys/time.h>
#include "Data/Hprof.h"

// Static empty containers for when parser is null
const std::map<Hprof::ID, std::string> Hprof::m_emptyStrings;
const std::map<Hprof::ID, Hprof::Frame> Hprof::m_emptyFrames;
const std::map<Hprof::u4, Hprof::StackTrace> Hprof::m_emptyStackTraces;
const std::map<Hprof::ID, Hprof::Thread> Hprof::m_emptyThreads;
const std::map<Hprof::ID, Hprof::Class> Hprof::m_emptyClasses;
const std::map<Hprof::ID, Hprof::Instance> Hprof::m_emptyInstances;
const std::unordered_map<Hprof::ID, Hprof::Array> Hprof::m_emptyArrays;
const std::unordered_map<Hprof::ID, Hprof::Local> Hprof::m_emptyLocals;
const std::unordered_set<Hprof::ID> Hprof::m_emptyGlobals;
const std::unordered_set<Hprof::ID> Hprof::m_emptyUnknown;
const std::map<Hprof::u4, Hprof::Sample> Hprof::m_emptyCpuSamples;
const std::unordered_map<Hprof::ID, Hprof::u4> Hprof::m_emptyComponentNums;

#pragma pack(push, 1)

bool Hprof::Parse(const std::string &data, bool verbose)
{
    // Read file header to determine format version
    if (data.size() < sizeof(FileHeader)) {
        fprintf(stderr, "error: Invalid file contents or file format.\n");
        return false;
    }

    auto header = (FileHeader *)(data.data());
    std::string ident(header->ident);

    // Strategy pattern: create appropriate parser based on format version
    if (ident == m_headerFlagV2) {
        m_parser = std::make_unique<HprofParserV2>();
    } else if (ident == m_headerFlagV1) {
        m_parser = std::make_unique<HprofParserV1>();
    } else {
        fprintf(stderr, "error: Invalid file contents or file format.\n");
        return false;
    }

    // Initialize parser with data
    m_parser->SetData(data);
    m_parser->m_idSize = SwapEndian(header->idSize);
    m_parser->m_fileTime = header->timeHigh;
    m_parser->m_fileTime = (m_parser->m_fileTime << 32) + header->timeLow;
    m_parser->SetCurPos(sizeof(FileHeader));

    // Main parsing loop using strategy pattern
    std::map<Tag, std::function<void(bool)>> recordParsers = {
        { STACK_FRAME, std::bind(&Hprof::ParseStackFrame, this, std::placeholders::_1) },
        { STACK_TRACE, std::bind(&Hprof::ParseStackTrace, this, std::placeholders::_1) },
        { START_THREAD, std::bind(&Hprof::ParseStartThread, this, std::placeholders::_1) },
        { HEAP_DUMP, std::bind(&Hprof::ParseHeapDump, this, std::placeholders::_1) },
        { CPU_SAMPLES, std::bind(&Hprof::ParseCpuSamples, this, std::placeholders::_1) }
    };

    while (m_parser->GetCurPos() < data.size()) {
        auto tag = m_parser->ReadAs<Tag>();
        if (!tag) {
            fprintf(stderr, "error: Record was truncated at offset 0x%zx\n", m_parser->GetCurPos());
            return false;
        }
        m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(Tag));

        // CLASS record has no length field, delegate to parser
        if (*tag == CLASS) {
            m_parser->ParseClass(verbose);
            continue;
        }

        // STRING record has u4/u8 length, delegate to parser
        if (*tag == STRING) {
            m_parser->ParseString(verbose);
            continue;
        }

        // For other records, check if there's a parser
        auto it = recordParsers.find(*tag);
        if (it != recordParsers.end()) {
            it->second(verbose);
            continue;
        }

        // Unknown tag: read length (u8) and skip
        auto lengthRec = m_parser->ReadAs<u8>();
        if (!lengthRec) {
            fprintf(stderr, "error: Record length truncated at offset 0x%zx\n", m_parser->GetCurPos());
            return false;
        }
        m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u8));
        auto size = SwapEndian(*lengthRec);
        if (m_parser->GetCurPos() + size > data.size()) {
            fprintf(stderr, "error: Record length is too long at offset 0x%zx\n", m_parser->GetCurPos());
            return false;
        }

        if (verbose) {
            printf("[SKIPED@0x%zx] tag = %u\n", m_parser->GetCurPos() - sizeof(Tag) - sizeof(u8), *tag);
        }

        m_parser->SetCurPos(m_parser->GetCurPos() + size);
    }

    if (m_parser->GetCurPos() != data.size()) {
        fprintf(stderr, "error: Unknown record or the length of the last record is too long.\n");
        return false;
    }

    return true;
}

void Hprof::ParseStackFrame(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(Tag);
    auto lengthRec = m_parser->ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u8));
    auto length = SwapEndian(*lengthRec);

    ID id = m_parser->ReadId();
    ID name = m_parser->ReadId();
    ID fileName = m_parser->ReadId();

    auto lineRec = m_parser->ReadAs<u4>();
    if (lineRec == nullptr) {
        return;
    }
    u4 line = SwapEndian(*lineRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    m_parser->m_frames[id] = { name, fileName, line };

    if (verbose) {
        printf("[STACK FRAME@0x%zx] id = 0x%" PRIx64 ", method name id = 0x%" PRIx64 ", file name id = 0x%" PRIx64 ", line = %u\n",
            startPos, id, name, fileName, line);
    }
}

void Hprof::ParseStackTrace(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(Tag);
    auto lengthRec = m_parser->ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u8));
    auto length = SwapEndian(*lengthRec);

    auto idxRec = m_parser->ReadAs<u4>();
    if (idxRec == nullptr) {
        return;
    }
    u4 idx = SwapEndian(*idxRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto threadRec = m_parser->ReadAs<u4>();
    if (threadRec == nullptr) {
        return;
    }
    m_parser->m_stackTraces[idx].thread = SwapEndian(*threadRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto frameNumRec = m_parser->ReadAs<u4>();
    if (frameNumRec == nullptr) {
        return;
    }
    u4 frameNum = SwapEndian(*frameNumRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    for (size_t i = 0; i < frameNum; i++) {
        m_parser->m_stackTraces[idx].frames.push_back(m_parser->ReadId());
    }

    if (verbose) {
        printf("[STACK TRACE@0x%zx] idx = %u, thread idx = %u, frames = [", startPos, idx, m_parser->m_stackTraces[idx].thread);

        auto frames = m_parser->m_stackTraces[idx].frames;
        for (size_t i = 0; i < frames.size(); i++) {
            printf("0x%" PRIx64 "%s", frames[i], i < frames.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

void Hprof::ParseStartThread(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(Tag);
    auto lengthRec = m_parser->ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u8));
    auto length = SwapEndian(*lengthRec);

    auto idxRec = m_parser->ReadAs<u4>();
    if (idxRec == nullptr) {
        return;
    }
    u4 idx = SwapEndian(*idxRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    ID id = m_parser->ReadId();

    auto stackTraceIdxRec = m_parser->ReadAs<u4>();
    if (stackTraceIdxRec == nullptr) {
        return;
    }
    u4 stackTraceIdx = SwapEndian(*stackTraceIdxRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    ID name = m_parser->ReadId();

    m_parser->m_threads[id].idx = idx;
    m_parser->m_threads[id].stackTraceIdx = stackTraceIdx;
    m_parser->m_threads[id].name = name;

    if (verbose) {
        printf("[START THREAD@0x%zx] id = 0x%" PRIx64 ", thread idx = %u, stack trace idx = %u, name id = 0x%" PRIx64 "\n",
            startPos, id, idx, stackTraceIdx, name);
    }
}

void Hprof::ParseHeapDump(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(Tag);
    auto lengthRec = m_parser->ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u8));
    auto length = SwapEndian(*lengthRec);
    auto endPos = m_parser->GetCurPos() + length;

    std::unordered_map<HeapDumpSubTag, std::function<void(bool)>> subRecordParsers = {
        { ROOT_GLOBAL, std::bind(&Hprof::ParseHeapDumpRootGlobal, this, std::placeholders::_1) },
        { ROOT_LOCAL, std::bind(&Hprof::ParseHeapDumpRootLocal, this, std::placeholders::_1) },
        { CLASS_DUMP, std::bind(&Hprof::ParseHeapDumpClassDump, this, std::placeholders::_1) },
        { INSTANCE_DUMP, std::bind(&Hprof::ParseHeapDumpInstanceDump, this, std::placeholders::_1) },
        { OBJECT_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpObjectArrayDump, this, std::placeholders::_1) },
        { PRIMITIVE_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpPrimitiveArrayDump, this, std::placeholders::_1) },
        { STRUCT_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpStructArrayDump, this, std::placeholders::_1) },
        { PINNED_INSTANCE_DUMP, std::bind(&Hprof::ParseHeapDumpInstanceDump, this, std::placeholders::_1) },
        { LARGE_INSTANCE_DUMP, std::bind(&Hprof::ParseHeapDumpInstanceDump, this, std::placeholders::_1) },
        { LARGE_OBJECT_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpObjectArrayDump, this, std::placeholders::_1) },
        { LARGE_PRIMITIVE_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpPrimitiveArrayDump, this, std::placeholders::_1) },
        { LARGE_STRUCT_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpStructArrayDump, this, std::placeholders::_1) },
        { UNMOVABLE_INSTANCE_DUMP, std::bind(&Hprof::ParseHeapDumpInstanceDump, this, std::placeholders::_1) },
        { UNMOVABLE_OBJECT_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpObjectArrayDump, this, std::placeholders::_1) },
        { UNMOVABLE_PRIMITIVE_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpPrimitiveArrayDump, this, std::placeholders::_1) },
        { UNMOVABLE_STRUCT_ARRAY_DUMP, std::bind(&Hprof::ParseHeapDumpStructArrayDump, this, std::placeholders::_1) },
        { ROOT_UNKNOWN, std::bind(&Hprof::ParseHeapDumpRootUnknown, this, std::placeholders::_1) }
    };

    while (m_parser->GetCurPos() < endPos) {
        auto tag = m_parser->ReadAs<HeapDumpSubTag>();
        if (tag == nullptr) {
            return;
        }
        m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(HeapDumpSubTag));
        auto it = subRecordParsers.find(*tag);
        if (it != subRecordParsers.end()) {
            it->second(verbose);
        } else {
            fprintf(stderr, "error: Unknown sub-tag %u at offset 0x%zx\n", *tag, m_parser->GetCurPos() - sizeof(HeapDumpSubTag));
            return;
        }
    }
}

void Hprof::ParseHeapDumpRootGlobal(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(u1);
    ID id = m_parser->ReadId();
    m_parser->m_globals.insert(id);

    if (verbose) {
        printf("[ROOT GLOBAL@0x%zx] id = 0x%" PRIx64 "\n", startPos, id);
    }
}

void Hprof::ParseHeapDumpRootLocal(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(u1);
    ID id = m_parser->ReadId();

    auto threadRec = m_parser->ReadAs<u4>();
    if (threadRec == nullptr) {
        return;
    }
    u4 thread = SwapEndian(*threadRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto frameRec = m_parser->ReadAs<u4>();
    if (frameRec == nullptr) {
        return;
    }
    u4 frame = SwapEndian(*frameRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    m_parser->m_locals[id] = { thread, frame };

    if (verbose) {
        printf("[ROOT LOCAL@0x%zx] id = 0x%" PRIx64 ", thread idx = %u, frame = %u\n",
            startPos, id, m_parser->m_locals[id].thread, m_parser->m_locals[id].frame);
    }
}

void Hprof::ParseHeapDumpClassDump(bool verbose)
{
    // Delegate to parser strategy
    m_parser->ParseHeapDumpClassDump(verbose);
}

void Hprof::ParseHeapDumpInstanceDump(bool verbose)
{
    // Delegate to parser strategy
    m_parser->ParseHeapDumpInstanceDump(verbose);
}

void Hprof::ParseHeapDumpObjectArrayDump(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(u1);
    ID id = m_parser->ReadId();

    auto numRec = m_parser->ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    ID cls = m_parser->ReadId();

    m_parser->m_arrays[id].type = BasicType::OBJECT;
    m_parser->m_arrays[id].cls = cls;
    m_parser->m_arrays[id].num = num;
    for (size_t i = 0; i < num; i++) {
        m_parser->m_arrays[id].elements.push_back(m_parser->ReadId());
    }

    if (verbose) {
        printf("[OBJECT ARRAY DUMP@0x%zx] id = 0x%" PRIx64 ", class = 0x%" PRIx64 ", elements = [", startPos, id, cls);

        auto elements = m_parser->m_arrays[id].elements;
        for (size_t i = 0; i < num; i++) {
            printf("0x%" PRIx64 "%s", elements[i], i < elements.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

void Hprof::ParseHeapDumpStructArrayDump(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(u1);
    ID id = m_parser->ReadId();

    auto componentNumRec = m_parser->ReadAs<u4>();
    if (componentNumRec == nullptr) {
        return;
    }
    m_parser->m_componentNums[id] = SwapEndian(*componentNumRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto numRec = m_parser->ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    ID cls = m_parser->ReadId();

    m_parser->m_arrays[id].type = BasicType::OBJECT;
    m_parser->m_arrays[id].cls = cls;
    m_parser->m_arrays[id].num = num;
    for (size_t i = 0; i < num; i++) {
        m_parser->m_arrays[id].elements.push_back(m_parser->ReadId());
    }

    if (verbose) {
        printf("[STRUCT ARRAY DUMP@0x%zx] id = 0x%" PRIx64 ", class = 0x%" PRIx64 ", elements = [", startPos, id, cls);

        auto elements = m_parser->m_arrays[id].elements;
        for (size_t i = 0; i < num; i++) {
            printf("0x%" PRIx64 "%s", elements[i], i < elements.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

void Hprof::ParseHeapDumpPrimitiveArrayDump(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(u1);
    ID id = m_parser->ReadId();

    auto numRec = m_parser->ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    m_parser->m_arrays[id].num = SwapEndian(*numRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto typeRec = m_parser->ReadAs<u1>();
    if (typeRec == nullptr) {
        return;
    }
    m_parser->m_arrays[id].type = BasicType(*typeRec);
    m_parser->m_arrays[id].cls = 0;
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u1));

    if (verbose) {
        printf("[PRIMITIVE ARRAY DUMP@0x%zx] id = 0x%" PRIx64 ", num = %u, type = %u\n",
            startPos, id, m_parser->m_arrays[id].num, m_parser->m_arrays[id].type);
    }
}

void Hprof::ParseHeapDumpRootUnknown(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(u1);
    ID id = m_parser->ReadId();
    m_parser->m_unknown.insert(id);

    if (verbose) {
        printf("[ROOT UNKNOWN@0x%zx] id = 0x%" PRIx64 "\n", startPos, id);
    }
}

void Hprof::ParseCpuSamples(bool verbose)
{
    size_t startPos = m_parser->GetCurPos() - sizeof(Tag);
    auto lengthRec = m_parser->ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u8));
    auto length = SwapEndian(*lengthRec);

    auto periodRec = m_parser->ReadAs<u4>();
    if (periodRec == nullptr) {
        return;
    }
    auto period = SwapEndian(*periodRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto numRec = m_parser->ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

    auto &stackTraces = m_parser->m_cpuSamples[period].stackTraces;

    for (u4 i = 0; i < num; i++) {
        auto sampleNumRec = m_parser->ReadAs<u4>();
        if (sampleNumRec == nullptr) {
            return;
        }
        auto sampleNum = SwapEndian(*sampleNumRec);
        m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

        auto stackTraceIdxRec = m_parser->ReadAs<u4>();
        if (stackTraceIdxRec == nullptr) {
            return;
        }
        auto stackTraceIdx = SwapEndian(*stackTraceIdxRec);
        m_parser->SetCurPos(m_parser->GetCurPos() + sizeof(u4));

        stackTraces[stackTraceIdx] = sampleNum;
    }

    if (verbose) {
        printf("[CPU SAMPLES@0x%zx] period = %u, samples = [", startPos, period);
        for (auto it : stackTraces) {
            printf("[%u, %u]", it.first, it.second);
        }
        printf("]\n");
    }
}

const std::string &Hprof::GenData()
{
    // Generate V2 format by default
    m_data = std::string(m_headerFlagV2, sizeof(m_headerFlagV2));
    u4 idSize = sizeof(u4);
    m_data.append(std::string((const char *)&idSize, sizeof(u4)));

    timeval time = {0};
    gettimeofday(&time, nullptr);
    u8 us = static_cast<u8>(time.tv_sec * 1000000 + time.tv_usec);
    u8 swappedUs = SwapEndian(us);
    m_data.append(std::string((const char *)&swappedUs, sizeof(u8)));

    for (auto string : m_stringsMap) {
        GenString(string.second, string.first);
    }

    for (auto frame : m_framesMap) {
        GenStackFrame(frame.second, frame.first);
    }

    for (auto trace : m_stackTracesMap) {
        GenStackTrace(trace.second, trace.first);
    }

    if (m_parser) {
        for (auto sample : m_parser->m_cpuSamples) {
            GenCpuSample(sample.first, sample.second);
        }
    }

    return m_data;
}

void Hprof::PutCpuSample(const std::vector<std::string> &callChain, u4 period)
{
    StackTrace stackTrace = { .thread = 0 };
    for (auto func : callChain) {
        ID name = GetMapValue(m_stringsMap, func);
        Frame frame = { .name = name, .fileName = 0, .line = 0 };
        ID frameId = GetMapValue(m_framesMap, frame);
        stackTrace.frames.push_back(frameId);
    }

    if (m_parser) {
        m_parser->m_cpuSamples[period].stackTraces[GetMapValue(m_stackTracesMap, stackTrace)]++;
    }
}

void Hprof::GenString(ID id, const std::string &str)
{
    m_data.push_back(STRING);
    u4 length = sizeof(u4) + sizeof(u4) + str.size();
    u4 swappedLength = SwapEndian(length);
    m_data.append(std::string((const char *)&swappedLength, sizeof(u4)));
    u4 swappedId = SwapEndian(static_cast<u4>(id));
    m_data.append(std::string((const char *)&swappedId, sizeof(u4)));
    m_data.append(str);
}

void Hprof::GenStackFrame(ID id, const Frame &frame)
{
    m_data.push_back(STACK_FRAME);
    u8 length = sizeof(u4) * 4;  // id + name + fileName + line (V2 format uses u4)
    u8 swappedLength = SwapEndian(length);
    m_data.append(std::string((const char *)&swappedLength, sizeof(u8)));

    u4 swappedId = SwapEndian(static_cast<u4>(id));
    u4 swappedName = SwapEndian(static_cast<u4>(frame.name));
    u4 swappedFileName = SwapEndian(static_cast<u4>(frame.fileName));
    u4 swappedLine = SwapEndian(frame.line);

    m_data.append(std::string((const char *)&swappedId, sizeof(u4)));
    m_data.append(std::string((const char *)&swappedName, sizeof(u4)));
    m_data.append(std::string((const char *)&swappedFileName, sizeof(u4)));
    m_data.append(std::string((const char *)&swappedLine, sizeof(u4)));
}

void Hprof::GenStackTrace(u4 idx, const StackTrace &stackTrace)
{
    m_data.push_back(STACK_TRACE);
    u8 length = sizeof(u4) * 3 + sizeof(u4) * stackTrace.frames.size();
    u8 swappedLength = SwapEndian(length);
    m_data.append(std::string((const char *)&swappedLength, sizeof(u8)));

    u4 swappedIdx = SwapEndian(idx);
    u4 swappedThread = SwapEndian(stackTrace.thread);
    u4 swappedFrameNum = SwapEndian(static_cast<u4>(stackTrace.frames.size()));

    m_data.append(std::string((const char *)&swappedIdx, sizeof(u4)));
    m_data.append(std::string((const char *)&swappedThread, sizeof(u4)));
    m_data.append(std::string((const char *)&swappedFrameNum, sizeof(u4)));

    for (auto frameId : stackTrace.frames) {
        u4 swappedFrameId = SwapEndian(static_cast<u4>(frameId));
        m_data.append(std::string((const char *)&swappedFrameId, sizeof(u4)));
    }
}

void Hprof::GenCpuSample(u4 period, const Sample &sample)
{
    m_data.push_back(CPU_SAMPLES);
    u8 length = sizeof(u4) * 2 + sizeof(u4) * 2 * sample.stackTraces.size();
    u8 swappedLength = SwapEndian(length);
    m_data.append(std::string((const char *)&swappedLength, sizeof(u8)));

    u4 swappedPeriod = SwapEndian(period);
    u4 swappedNum = SwapEndian(static_cast<u4>(sample.stackTraces.size()));

    m_data.append(std::string((const char *)&swappedPeriod, sizeof(u4)));
    m_data.append(std::string((const char *)&swappedNum, sizeof(u4)));

    for (auto it : sample.stackTraces) {
        u4 swappedSampleNum = SwapEndian(it.second);
        u4 swappedStackTraceIdx = SwapEndian(it.first);
        m_data.append(std::string((const char *)&swappedSampleNum, sizeof(u4)));
        m_data.append(std::string((const char *)&swappedStackTraceIdx, sizeof(u4)));
    }
}

#pragma pack(pop)