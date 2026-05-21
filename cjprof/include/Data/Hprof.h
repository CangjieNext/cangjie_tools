// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef HPROF_H
#define HPROF_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Data/HprofParser.h"

class Hprof {
public:
    // Inherit types from HprofParserBase for consistency
    using u1 = HprofParserBase::u1;
    using u2 = HprofParserBase::u2;
    using u4 = HprofParserBase::u4;
    using u8 = HprofParserBase::u8;
    using ID = HprofParserBase::ID;
    using BasicType = HprofParserBase::BasicType;
    using Class = HprofParserBase::Class;
    using Instance = HprofParserBase::Instance;
    using Array = HprofParserBase::Array;
    using Frame = HprofParserBase::Frame;
    using StackTrace = HprofParserBase::StackTrace;
    using Thread = HprofParserBase::Thread;
    using Local = HprofParserBase::Local;
    using Sample = HprofParserBase::Sample;

    // Format version selection:
    // - V1 (1.0.1): Old format with fixed 8-byte IDs, header "CANGJIE PROFILE 1.0.1"
    // - V2 (1.0.2): New format with dynamic IDs (based on idSize), header "CANGJIE PROFILE 1.0.2"
    // Parser is selected automatically based on file header identification string.

    Hprof() = default;
    bool Parse(const std::string &data, bool verbose = false);
    const std::string &GenData();
    void PutCpuSample(const std::vector<std::string> &callChain, u4 period);

    // Getters - delegate to parser
    const std::map<ID, std::string> &GetStrings() const
    {
        return m_parser ? m_parser->m_strings : m_emptyStrings;
    }

    const std::map<ID, Frame> &GetFrames() const
    {
        return m_parser ? m_parser->m_frames : m_emptyFrames;
    }

    const std::map<u4, StackTrace> &GetStackTraces() const
    {
        return m_parser ? m_parser->m_stackTraces : m_emptyStackTraces;
    }

    const std::map<ID, Thread> &GetThreads() const
    {
        return m_parser ? m_parser->m_threads : m_emptyThreads;
    }

    const std::map<ID, Class> &GetClasses() const
    {
        return m_parser ? m_parser->m_classes : m_emptyClasses;
    }

    const std::map<ID, Instance> &GetInstances() const
    {
        return m_parser ? m_parser->m_instances : m_emptyInstances;
    }

    const std::unordered_map<ID, Array> &GetArrays() const
    {
        return m_parser ? m_parser->m_arrays : m_emptyArrays;
    }

    const std::unordered_map<ID, Local> &GetLocalsRoots() const
    {
        return m_parser ? m_parser->m_locals : m_emptyLocals;
    }

    const std::unordered_set<ID> &GetGlobalsRoots() const
    {
        return m_parser ? m_parser->m_globals : m_emptyGlobals;
    }

    const std::unordered_set<ID> &GetUnknownRoots() const
    {
        return m_parser ? m_parser->m_unknown : m_emptyUnknown;
    }

    const std::map<u4, Sample> &GetCpuSamples() const
    {
        return m_parser ? m_parser->m_cpuSamples : m_emptyCpuSamples;
    }

    const std::unordered_map<ID, u4> &GetComponentNums() const
    {
        return m_parser ? m_parser->m_componentNums : m_emptyComponentNums;
    }

    const u8 GetFileTime() const
    {
        return m_parser ? m_parser->m_fileTime : 0;
    }

    const u4 GetIdSize() const
    {
        return m_parser ? m_parser->m_idSize : 0;
    }

private:
    enum Tag : u1 {
        STRING = 0x01,
        CLASS = 0x02,
        STACK_FRAME = 0x04,
        STACK_TRACE = 0x05,
        START_THREAD = 0x0a,
        HEAP_DUMP = 0x0c,
        CPU_SAMPLES = 0x0d
    };

    enum HeapDumpSubTag : u1 {
        ROOT_GLOBAL = 0x01,
        ROOT_LOCAL = 0x02,
        CLASS_DUMP = 0x20,
        INSTANCE_DUMP = 0x21,
        OBJECT_ARRAY_DUMP = 0x22,
        PRIMITIVE_ARRAY_DUMP = 0x23,
        STRUCT_ARRAY_DUMP = 0x24,
        PINNED_INSTANCE_DUMP = 0x25,
        LARGE_INSTANCE_DUMP = 0x26,
        LARGE_OBJECT_ARRAY_DUMP = 0x27,
        LARGE_PRIMITIVE_ARRAY_DUMP = 0x28,
        LARGE_STRUCT_ARRAY_DUMP = 0x29,
        UNMOVABLE_INSTANCE_DUMP = 0x2A,
        UNMOVABLE_OBJECT_ARRAY_DUMP = 0x2B,
        UNMOVABLE_PRIMITIVE_ARRAY_DUMP = 0x2C,
        UNMOVABLE_STRUCT_ARRAY_DUMP = 0x2D,
        ROOT_UNKNOWN = 0xff
    };

#pragma pack(push, 1)
    struct FileHeader {
        char ident[22];
        u4 idSize;
        u4 timeHigh;
        u4 timeLow;
    };
#pragma pack(pop)

    // Common parsing methods (format-independent)
    void ParseHeapDump(bool verbose);
    void ParseHeapDumpRootGlobal(bool verbose);
    void ParseHeapDumpRootLocal(bool verbose);
    void ParseHeapDumpClassDump(bool verbose);
    void ParseHeapDumpInstanceDump(bool verbose);
    void ParseHeapDumpObjectArrayDump(bool verbose);
    void ParseHeapDumpPrimitiveArrayDump(bool verbose);
    void ParseHeapDumpStructArrayDump(bool verbose);
    void ParseHeapDumpRootUnknown(bool verbose);
    void ParseCpuSamples(bool verbose);

    // Generation methods
    void GenString(ID id, const std::string &str);
    void GenStackFrame(ID id, const Frame &frame);
    void GenStackTrace(u4 idx, const StackTrace &stackTrace);
    void GenCpuSample(u4 period, const Sample &sample);

    template <typename T1, typename T2>
    T2 GetMapValue(std::map<T1, T2> &map, const T1 &key)
    {
        static T2 val = 1;

        auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        }

        auto id = val++;
        map[key] = id;

        return id;
    }

private:
    static constexpr char m_headerFlagV2[] = "CANGJIE PROFILE 1.0.2";
    static constexpr char m_headerFlagV1[] = "CANGJIE PROFILE 1.0.1";

    // Strategy pattern: parser selected based on format version
    std::unique_ptr<HprofParserBase> m_parser;

    // Data for generation (not parsed from file)
    std::string m_data;
    std::map<std::string, ID> m_stringsMap;
    std::map<Frame, ID> m_framesMap;
    std::map<StackTrace, u4> m_stackTracesMap;

    // Empty containers for when parser is null
    static const std::map<ID, std::string> m_emptyStrings;
    static const std::map<ID, Frame> m_emptyFrames;
    static const std::map<u4, StackTrace> m_emptyStackTraces;
    static const std::map<ID, Thread> m_emptyThreads;
    static const std::map<ID, Class> m_emptyClasses;
    static const std::map<ID, Instance> m_emptyInstances;
    static const std::unordered_map<ID, Array> m_emptyArrays;
    static const std::unordered_map<ID, Local> m_emptyLocals;
    static const std::unordered_set<ID> m_emptyGlobals;
    static const std::unordered_set<ID> m_emptyUnknown;
    static const std::map<u4, Sample> m_emptyCpuSamples;
    static const std::unordered_map<ID, u4> m_emptyComponentNums;
};

#endif // HPROF_H