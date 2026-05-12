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

#pragma pack(push, 1)

bool Hprof::Parse(const std::string &data, bool verbose)
{
    m_data = data;
    m_curPos = 0;

    auto header = ReadAs<FileHeader>();
    if (!header || (std::string(header->ident) != m_headerFlag)) {
        fprintf(stderr, "error: Invalid file contents or file format.\n");
        return false;
    }

    m_idSize = SwapEndian(header->idSize);
    m_fileTime = header->timeHigh;
    m_fileTime = (m_fileTime << 32) + header->timeLow;

    m_curPos += sizeof(FileHeader);

    std::map<Tag, std::function<void(bool)>> recordParsers = {
        { STACK_FRAME, std::bind(&Hprof::ParseStackFrame, this, std::placeholders::_1) },
        { STACK_TRACE, std::bind(&Hprof::ParseStackTrace, this, std::placeholders::_1) },
        { START_THREAD, std::bind(&Hprof::ParseStartThread, this, std::placeholders::_1) },
        { HEAP_DUMP, std::bind(&Hprof::ParseHeapDump, this, std::placeholders::_1) },
        { CPU_SAMPLES, std::bind(&Hprof::ParseCpuSamples, this, std::placeholders::_1) }
    };
    while (m_curPos < m_data.size()) {
        auto tag = ReadAs<Tag>();
        if (!tag) {
            fprintf(stderr, "error: Record was truncated at offset 0x%zx\n", m_curPos);
            return false;
        }
        m_curPos += sizeof(Tag);  // advance past tag

        // CLASS record has no length field, handle separately
        if (*tag == CLASS) {
            ParseClass(verbose);
            continue;
        }

        // STRING record has u4 length, handle separately
        if (*tag == STRING) {
            ParseString(verbose);
            continue;
        }

        // For other records, check if there's a parser
        auto it = recordParsers.find(*tag);
        if (it != recordParsers.end()) {
            it->second(verbose);
            continue;
        }

        // Unknown tag: read length (u8) and skip
        auto lengthRec = ReadAs<u8>();
        if (!lengthRec) {
            fprintf(stderr, "error: Record length truncated at offset 0x%zx\n", m_curPos);
            return false;
        }
        m_curPos += sizeof(u8);
        auto size = SwapEndian(*lengthRec);
        if (m_curPos + size > m_data.size()) {
            fprintf(stderr, "error: Record length is too long at offset 0x%zx\n", m_curPos);
            return false;
        }

        if (verbose) {
            printf("[SKIPED@0x%zx] tag = %u\n", m_curPos - sizeof(Tag) - sizeof(u8), *tag);
        }

        m_curPos += size;
    }

    if (m_curPos != m_data.size()) {
        fprintf(stderr, "error: Unknown record or the length of the last record is too long.\n");
        return false;
    }

    return true;
}

void Hprof::ParseString(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was already read in main loop
    auto lengthRec = ReadAs<u4>();
    if (lengthRec == nullptr) {
        fprintf(stderr, "error: StringRecord length truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();  // stop parsing
        return;
    }
    auto length = SwapEndian(*lengthRec);
    m_curPos += sizeof(u4);

    auto idRec = ReadAs<u4>();
    if (idRec == nullptr) {
        fprintf(stderr, "error: StringRecord id truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();  // stop parsing
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(u4);

    if (length < sizeof(u4)) {
        fprintf(stderr, "error: StringRecord length too small at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();  // stop parsing
        return;
    }
    m_strings[id] = std::string(m_data.data() + m_curPos, length - sizeof(u4));
    m_curPos += length - sizeof(u4);

    if (verbose) {
        printf("[STRING@0x%zx] id = 0x%" PRIx64 ", str = \"%s\"\n", startPos, id, m_strings[id].c_str());
    }
}

void Hprof::ParseClass(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was already read in main loop
    auto idRec = ReadAs<u4>();
    if (idRec == nullptr) {
        fprintf(stderr, "error: ClassRecord id truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();  // stop parsing
        return;
    }
    ID id = SwapEndian(*idRec);
    m_curPos += sizeof(u4);

    auto nameRec = ReadAs<u4>();
    if (nameRec == nullptr) {
        fprintf(stderr, "error: ClassRecord name truncated at offset 0x%zx\n", startPos);
        m_curPos = m_data.size();  // stop parsing
        return;
    }
    m_classes[id].name = SwapEndian(*nameRec);
    m_curPos += sizeof(u4);

    if (verbose) {
        printf("[LOAD CLASS@0x%zx] id = 0x%" PRIx64 ", name id = 0x%" PRIx64 "\n", startPos, id, m_classes[id].name);
    }
}

void Hprof::ParseStackFrame(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was read in main loop
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_curPos += sizeof(u8);
    auto length = SwapEndian(*lengthRec);

    ID id = ReadId();
    ID name = ReadId();
    ID fileName = ReadId();

    auto lineRec = ReadAs<u4>();
    if (lineRec == nullptr) {
        return;
    }
    u4 line = SwapEndian(*lineRec);
    m_curPos += sizeof(u4);

    m_frames[id] = { name, fileName, line };

    if (verbose) {
        printf("[STACK FRAME@0x%zx] id = 0x%" PRIx64 ", method name id = 0x%" PRIx64 ", file name id = 0x%" PRIx64 ", line = %u\n",
            startPos, id, name, fileName, line);
    }
}

void Hprof::ParseStackTrace(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was read in main loop
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_curPos += sizeof(u8);
    auto length = SwapEndian(*lengthRec);

    auto idxRec = ReadAs<u4>();
    if (idxRec == nullptr) {
        return;
    }
    u4 idx = SwapEndian(*idxRec);
    m_curPos += sizeof(u4);

    auto threadRec = ReadAs<u4>();
    if (threadRec == nullptr) {
        return;
    }
    m_stackTraces[idx].thread = SwapEndian(*threadRec);
    m_curPos += sizeof(u4);

    auto frameNumRec = ReadAs<u4>();
    if (frameNumRec == nullptr) {
        return;
    }
    u4 frameNum = SwapEndian(*frameNumRec);
    m_curPos += sizeof(u4);

    for (size_t i = 0; i < frameNum; i++) {
        m_stackTraces[idx].frames.push_back(ReadId());
    }

    if (verbose) {
        printf("[STACK TRACE@0x%zx] idx = %u, thread idx = %u, frames = [", startPos, idx, m_stackTraces[idx].thread);

        auto frames = m_stackTraces[idx].frames;
        for (size_t i = 0; i < frames.size(); i++) {
            printf("0x%" PRIx64 "%s", frames[i], i < frames.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

void Hprof::ParseStartThread(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was read in main loop
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_curPos += sizeof(u8);
    auto length = SwapEndian(*lengthRec);

    auto idxRec = ReadAs<u4>();
    if (idxRec == nullptr) {
        return;
    }
    u4 idx = SwapEndian(*idxRec);
    m_curPos += sizeof(u4);

    ID id = ReadId();

    auto stackTraceIdxRec = ReadAs<u4>();
    if (stackTraceIdxRec == nullptr) {
        return;
    }
    u4 stackTraceIdx = SwapEndian(*stackTraceIdxRec);
    m_curPos += sizeof(u4);

    ID name = ReadId();

    m_threads[id].idx = idx;
    m_threads[id].stackTraceIdx = stackTraceIdx;
    m_threads[id].name = name;

    if (verbose) {
        printf("[START THREAD@0x%zx] id = 0x%" PRIx64 ", thread idx = %u, stack trace idx = %u, name id = 0x%" PRIx64 "\n",
            startPos, id, idx, stackTraceIdx, name);
    }
}

void Hprof::ParseHeapDump(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was read in main loop
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_curPos += sizeof(u8);
    auto length = SwapEndian(*lengthRec);
    auto endPos = m_curPos + length;

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

    while (m_curPos < endPos) {
        auto tag = ReadAs<HeapDumpSubTag>();
        if (tag == nullptr) {
            return;
        }
        m_curPos += sizeof(HeapDumpSubTag);  // consume tag
        auto it = subRecordParsers.find(*tag);
        if (it != subRecordParsers.end()) {
            it->second(verbose);
        } else {
            fprintf(stderr, "error: Unknown sub-tag %u at offset 0x%zx\n", *tag, m_curPos - sizeof(HeapDumpSubTag));
            return;  // stop parsing on unknown tag
        }
    }
}

void Hprof::ParseHeapDumpRootGlobal(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
    ID id = ReadId();
    m_globals.insert(id);

    if (verbose) {
        printf("[ROOT GLOBAL@0x%zx] id = 0x%" PRIx64 "\n", startPos, id);
    }
}

void Hprof::ParseHeapDumpRootLocal(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
    ID id = ReadId();

    auto threadRec = ReadAs<u4>();
    if (threadRec == nullptr) {
        return;
    }
    u4 thread = SwapEndian(*threadRec);
    m_curPos += sizeof(u4);

    auto frameRec = ReadAs<u4>();
    if (frameRec == nullptr) {
        return;
    }
    u4 frame = SwapEndian(*frameRec);
    m_curPos += sizeof(u4);

    m_locals[id] = { thread, frame };

    if (verbose) {
        printf("[ROOT LOCAL@0x%zx] id = 0x%" PRIx64 ", thread idx = %u, frame = %u\n",
            startPos, id, m_locals[id].thread, m_locals[id].frame);
    }
}

void Hprof::ParseHeapDumpClassDump(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
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
        printf("[CLASS DUMP@0x%zx] id = 0x%" PRIx64 ", name id = 0x%" PRIx64 ", size = %u\n", startPos, id, m_classes[id].name, m_classes[id].size);
    }
}

void Hprof::ParseHeapDumpInstanceDump(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump

    ID id = ReadId();
    ID cls = ReadId();

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

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

void Hprof::ParseHeapDumpObjectArrayDump(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
    ID id = ReadId();

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

    ID cls = ReadId();

    m_arrays[id].type = OBJECT;
    m_arrays[id].cls = cls;
    m_arrays[id].num = num;
    for (size_t i = 0; i < num; i++) {
        m_arrays[id].elements.push_back(ReadId());
    }

    if (verbose) {
        printf("[OBJECT ARRAY DUMP@0x%zx] id = 0x%" PRIx64 ", class = 0x%" PRIx64 ", elements = [", startPos, id, cls);

        auto elements = m_arrays[id].elements;
        for (size_t i = 0; i < m_arrays[id].num; i++) {
            printf("0x%" PRIx64 "%s", elements[i], i < elements.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

void Hprof::ParseHeapDumpStructArrayDump(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
    ID id = ReadId();

    auto componentNumRec = ReadAs<u4>();
    if (componentNumRec == nullptr) {
        return;
    }
    m_componentNums[id] = SwapEndian(*componentNumRec);
    m_curPos += sizeof(u4);

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

    ID cls = ReadId();

    m_arrays[id].type = OBJECT;
    m_arrays[id].cls = cls;
    m_arrays[id].num = num;
    for (size_t i = 0; i < num; i++) {
        m_arrays[id].elements.push_back(ReadId());
    }

    if (verbose) {
        printf("[STRUCT ARRAY DUMP@0x%zx] id = 0x%" PRIx64 ", class = 0x%" PRIx64 ", elements = [", startPos, id, cls);

        auto elements = m_arrays[id].elements;
        for (size_t i = 0; i < m_arrays[id].num; i++) {
            printf("0x%" PRIx64 "%s", elements[i], i < elements.size() - 1 ? ", " : "");
        }

        printf("]\n");
    }
}

void Hprof::ParseHeapDumpPrimitiveArrayDump(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
    ID id = ReadId();

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    m_arrays[id].num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

    auto typeRec = ReadAs<u1>();
    if (typeRec == nullptr) {
        return;
    }
    m_arrays[id].type = BasicType(*typeRec);
    m_arrays[id].cls = 0;
    m_curPos += sizeof(u1);

    if (verbose) {
        printf("[PRIMITIVE ARRAY DUMP@0x%zx] id = 0x%" PRIx64 ", num = %u, type = %u\n",
            startPos, id, m_arrays[id].num, m_arrays[id].type);
    }
}

void Hprof::ParseHeapDumpRootUnknown(bool verbose)
{
    size_t startPos = m_curPos - sizeof(u1);  // tag was already consumed by ParseHeapDump
    ID id = ReadId();
    m_unknown.insert(id);

    if (verbose) {
        printf("[ROOT UNKNOWN@0x%zx] id = 0x%" PRIx64 "\n", startPos, id);
    }
}

void Hprof::ParseCpuSamples(bool verbose)
{
    size_t startPos = m_curPos - sizeof(Tag);  // tag was read in main loop
    auto lengthRec = ReadAs<u8>();
    if (lengthRec == nullptr) {
        return;
    }
    m_curPos += sizeof(u8);
    auto length = SwapEndian(*lengthRec);

    auto periodRec = ReadAs<u4>();
    if (periodRec == nullptr) {
        return;
    }
    auto period = SwapEndian(*periodRec);
    m_curPos += sizeof(u4);

    auto numRec = ReadAs<u4>();
    if (numRec == nullptr) {
        return;
    }
    auto num = SwapEndian(*numRec);
    m_curPos += sizeof(u4);

    auto &stackTraces = m_cpuSamples[period].stackTraces;

    for (u4 i = 0; i < num; i++) {
        auto sampleNumRec = ReadAs<u4>();
        if (sampleNumRec == nullptr) {
            return;
        }
        auto sampleNum = SwapEndian(*sampleNumRec);
        m_curPos += sizeof(u4);

        auto stackTraceIdxRec = ReadAs<u4>();
        if (stackTraceIdxRec == nullptr) {
            return;
        }
        auto stackTraceIdx = SwapEndian(*stackTraceIdxRec);
        m_curPos += sizeof(u4);

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
    m_data = std::string(m_headerFlag, sizeof(m_headerFlag));
    WriteAs(SwapEndian((u4)sizeof(ID)));

    timeval time = {0};
    gettimeofday(&time, nullptr);
    u8 us = static_cast<u8>(time.tv_sec * 1000000 + time.tv_usec);
    WriteAs(SwapEndian(us));

    for (auto string : m_stringsMap) {
        GenString(string.second, string.first);
    }

    for (auto frame : m_framesMap) {
        GenStackFrame(frame.second, frame.first);
    }

    for (auto trace : m_stackTracesMap) {
        GenStackTrace(trace.second, trace.first);
    }

    for (auto sample : m_cpuSamples) {
        GenCpuSample(sample.first, sample.second);
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

    m_cpuSamples[period].stackTraces[GetMapValue(m_stackTracesMap, stackTrace)]++;
}

void Hprof::GenString(ID id, const std::string &str)
{
    StringRecord rec;
    rec.tag = STRING;
    rec.length = SwapEndian((u4)(sizeof(rec.tag) + sizeof(rec.length) + sizeof(rec.id) + str.size()));
    rec.id = SwapEndian(static_cast<u4>(id));
    WriteAs(rec);
    m_data.append(str);
}

void Hprof::GenStackFrame(ID id, const Frame &frame)
{
    StackFrameRecord rec;
    rec.tag = STACK_FRAME;
    rec.length = SwapEndian((u8)(sizeof(StackFrameRecord) - sizeof(RecordHeader)));
    rec.id = SwapEndian(id);
    rec.name = SwapEndian(frame.name);
    rec.fileName = SwapEndian(frame.fileName);
    rec.line = SwapEndian(frame.line);
    WriteAs(rec);
}

void Hprof::GenStackTrace(u4 idx, const StackTrace &stackTrace)
{
    StackTraceRecord rec;
    rec.tag = STACK_TRACE;
    rec.length = SwapEndian(
        (u8)(sizeof(StackTraceRecord) + sizeof(ID) * stackTrace.frames.size() - sizeof(RecordHeader)));
    rec.idx = SwapEndian(idx);
    rec.thread = SwapEndian(stackTrace.thread);
    rec.frameNum = SwapEndian((u4)stackTrace.frames.size());
    WriteAs(rec);
    for (auto id : stackTrace.frames) {
        WriteAs(SwapEndian(id));
    }
}

void Hprof::GenCpuSample(u4 period, const Sample &sample)
{
    CpuSamplesRecord rec;
    rec.tag = CPU_SAMPLES;
    rec.length = SwapEndian(
        (u8)(sizeof(CpuSamplesRecord) + sizeof(rec.samples[0]) * sample.stackTraces.size() - sizeof(RecordHeader)));
    rec.period = SwapEndian(period);
    rec.num = SwapEndian((u4)sample.stackTraces.size());
    WriteAs(rec);
    for (auto it : sample.stackTraces) {
        WriteAs(SwapEndian(it.second));
        WriteAs(SwapEndian(it.first));
    }
}

#pragma pack(pop)