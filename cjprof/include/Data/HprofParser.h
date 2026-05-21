// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef HPROF_PARSER_H
#define HPROF_PARSER_H

// ============================================================================
// Hprof File Format Documentation
// ============================================================================
//
// V1 Format (1.0.1) - Old Version
// ┌───────────────────────┬────────────────────────────────────────────┐
// │ ID Type: Fixed u8 (8 bytes)                                        │
// ├───────────────────────┬────────────────────────────────────────────┤
// │ Record                │ Format                                     │
// ├───────────────────────┼────────────────────────────────────────────┤
// │ STRING                │ u8 length, u8 id, u1 str[]                 │
// │ CLASS (LoadClass)     │ u8 length, u8 id, u8 name                  │
// │ CLASS_DUMP            │ u8 id, u4 size                             │
// │ INSTANCE_DUMP         │ u8 id, u8 cls, u4 num, u8 vals[]           │
// │ STACK_FRAME           │ u8 length, u8 id, u8 name, u8 file, u4 line│
// │ STACK_TRACE           │ u8 length, u4 idx, u4 thread, u4 num, u8[] │
// │ START_THREAD          │ u8 length, u4 idx, u8 id, u4 stIdx, u8 name│
// │ ROOT_GLOBAL           │ u8 id                                      │
// │ ROOT_LOCAL            │ u8 id, u4 thread, u4 frame                 │
// │ ROOT_UNKNOWN          │ u8 id                                      │
// │ OBJECT_ARRAY_DUMP     │ u8 id, u4 num, u8 cls, u8 elements[]       │
// │ PRIMITIVE_ARRAY_DUMP  │ u8 id, u4 num, u1 type                     │
// │ STRUCT_ARRAY_DUMP     │ u8 id, u4 compNum, u4 num, u8 cls, u8[]    │
// │ CPU_SAMPLES           │ u8 length, u4 period, u4 num, {u4,u4}[]    │
// └───────────────────────┴────────────────────────────────────────────┘
//
// V2 Format (1.0.2) - New Version
// ┌───────────────────────┬────────────────────────────────────────────┐
// │ ID Type: Dynamic (based on idSize from file header, 4 or 8)       │
// ├───────────────────────┬────────────────────────────────────────────┤
// │ Record                │ Format                                     │
// ├───────────────────────┼────────────────────────────────────────────┤
// │ STRING                │ u4 length, u4 id, u1 str[]                 │
// │ CLASS (LoadClass)     │ u4 id, u4 name, u4 size (no length field)  │
// │ CLASS_DUMP            │ u4 id, u4 name, u4 size                    │
// │ INSTANCE_DUMP         │ ID id, u4 cls, u4 num, ID values[]         │
// │ STACK_FRAME           │ u8 length, u4 id, u4 name, u4 file, u4 line│
// │ STACK_TRACE           │ u8 length, u4 idx, u4 thread, u4 num, u4[] │
// │ START_THREAD          │ u8 length, u4 idx, ID id, u4 stIdx, ID name│
// │ ROOT_GLOBAL           │ ID id                                      │
// │ ROOT_LOCAL            │ ID id, u4 thread, u4 frame                 │
// │ ROOT_UNKNOWN          │ ID id                                      │
// │ OBJECT_ARRAY_DUMP     │ ID id, u4 num, u4 cls, ID elements[]       │
// │ PRIMITIVE_ARRAY_DUMP  │ ID id, u4 num, u1 type                     │
// │ STRUCT_ARRAY_DUMP     │ ID id, u4 compNum, u4 num, u4 cls, ID[]    │
// │ CPU_SAMPLES           │ u8 length, u4 period, u4 num, {u4,u4}[]    │
// └───────────────────────┴────────────────────────────────────────────┘
//
// ============================================================================

#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

// Forward declaration
class Hprof;

class HprofParserBase {
public:
    using u1 = uint8_t;
    using u2 = uint16_t;
    using u4 = uint32_t;
    using u8 = uint64_t;
    using ID = uint64_t;

    enum BasicType : u1 {
        OBJECT = 2,
        BOOLEAN = 4,
        SHORT = 9,
        INT = 10,
        LONG = 11
    };

    struct Class {
        ID name;
        u4 size;
    };

    struct Instance {
        ID cls;
        std::vector<ID> fields;
    };

    struct Array {
        BasicType type;
        ID cls;
        u4 num;
        std::vector<ID> elements;
        /* Fixed members ('_typeinfo', 'size') size of a raw array. */
        u4 GetFixedSize() const { return 16; }
    };

    struct Frame {
        ID name;
        ID fileName;
        u4 line;

        bool operator<(const Frame &rhs) const
        {
            if (name != rhs.name) return name < rhs.name;
            if (fileName != rhs.fileName) return fileName < rhs.fileName;
            return line < rhs.line;
        };
    };

    struct StackTrace {
        u4 thread;
        std::vector<ID> frames;
        bool operator<(const StackTrace &rhs) const
        {
            if (thread != rhs.thread) {
                return thread < rhs.thread;
            }
            if (frames.size() != rhs.frames.size()) {
                return frames.size() < rhs.frames.size();
            }
            for (size_t i = 0; i < frames.size(); i++) {
                if (frames[i] != rhs.frames[i]) {
                    return frames[i] < rhs.frames[i];
                }
            }
            return false;
        };
    };

    struct Thread {
        ID name;
        u4 idx;
        u4 stackTraceIdx;
    };

    struct Local {
        u4 thread;
        u4 frame;
    };

    struct Sample {
        std::map<u4, u4> stackTraces;
    };

    // Data containers - shared with Hprof class
    std::map<ID, std::string> m_strings;
    std::map<ID, Frame> m_frames;
    std::map<u4, StackTrace> m_stackTraces;
    std::map<ID, Thread> m_threads;
    std::map<ID, Class> m_classes;
    std::map<ID, Instance> m_instances;
    std::unordered_map<ID, Array> m_arrays;
    std::unordered_map<ID, Local> m_locals;
    std::unordered_set<ID> m_globals;
    std::unordered_set<ID> m_unknown;
    std::map<u4, Sample> m_cpuSamples;
    // componentNum from STRUCT_ARRAY_DUMP: number of struct components in the array
    std::unordered_map<ID, u4> m_componentNums;
    u8 m_fileTime;
    u4 m_idSize;

    HprofParserBase() : m_fileTime(0), m_idSize(0), m_curPos(0) {}
    virtual ~HprofParserBase() = default;

    // Virtual parsing methods - implemented by concrete parsers
    virtual void ParseString(bool verbose) = 0;
    virtual void ParseClass(bool verbose) = 0;
    virtual void ParseStackFrame(bool verbose) = 0;
    virtual void ParseStackTrace(bool verbose) = 0;
    virtual void ParseStartThread(bool verbose) = 0;
    virtual void ParseHeapDumpClassDump(bool verbose) = 0;
    virtual void ParseHeapDumpInstanceDump(bool verbose) = 0;
    virtual void ParseHeapDumpObjectArrayDump(bool verbose) = 0;
    virtual void ParseHeapDumpStructArrayDump(bool verbose) = 0;

    // Common methods
    void SetData(const std::string &data);
    size_t GetCurPos() const { return m_curPos; }
    void SetCurPos(size_t pos) { m_curPos = pos; }

    template <typename T>
    T *ReadAs()
    {
        return m_data.size() - m_curPos >= sizeof(T) ? (T *)(m_data.data() + m_curPos) : nullptr;
    }

    template <typename T>
    void WriteAs(const T &data)
    {
        m_outputData.append(std::string((const char *)&data, sizeof(T)));
    }

    // Static endian swap functions - can be called without object instance
    static inline u2 SwapEndian(u2 val)
    {
        return static_cast<short unsigned int>((val & 0xff00U) >> 8)
            | static_cast<short unsigned int>((val & 0xffU) << 8);
    }

    static inline u4 SwapEndian(u4 val)
    {
        return ((val & 0xff000000U) >> 24) | ((val & 0xff0000U) >> 8) |
            ((val & 0xff00U) << 8) | ((val & 0xffU) << 24);
    }

    static inline u8 SwapEndian(u8 val)
    {
        return ((val & 0xff00000000000000ULL) >> 56) | ((val & 0xff000000000000ULL) >> 40) |
            ((val & 0xff0000000000ULL) >> 24) | ((val & 0xff00000000ULL) >> 8) |
            ((val & 0xff000000ULL) << 8) | ((val & 0xff0000ULL) << 24) |
            ((val & 0xff00ULL) << 40) | ((val & 0xffULL) << 56);
    }

    // ReadId and WriteId are based on m_idSize, implemented in base class
    ID ReadId();
    void WriteId(ID id);

    // Template helper: read, swap endian, advance position in one call
    template <typename T>
    T ReadAndSwap()
    {
        auto ptr = ReadAs<T>();
        if (!ptr) {
            return 0;
        }
        m_curPos += sizeof(T);
        return SwapEndian(*ptr);
    }

    // Helper: read multiple IDs into a vector
    void ReadIdVector(std::vector<ID>& vec, size_t count)
    {
        for (size_t i = 0; i < count; i++) {
            vec.push_back(ReadId());
        }
    }

protected:
    std::string m_data;        // input data for parsing
    std::string m_outputData;  // output data for generation (WriteAs)
    size_t m_curPos;
};

// New format parser V2 (1.0.2)
class HprofParserV2 : public HprofParserBase {
public:
    void ParseString(bool verbose) override;
    void ParseClass(bool verbose) override;
    void ParseStackFrame(bool verbose) override;
    void ParseStackTrace(bool verbose) override;
    void ParseStartThread(bool verbose) override;
    void ParseHeapDumpClassDump(bool verbose) override;
    void ParseHeapDumpInstanceDump(bool verbose) override;
    void ParseHeapDumpObjectArrayDump(bool verbose) override;
    void ParseHeapDumpStructArrayDump(bool verbose) override;
};

// Old format parser V1 (1.0.1)
class HprofParserV1 : public HprofParserBase {
public:
    HprofParserV1() { m_idSize = 8; }
    void ParseString(bool verbose) override;
    void ParseClass(bool verbose) override;
    void ParseStackFrame(bool verbose) override;
    void ParseStackTrace(bool verbose) override;
    void ParseStartThread(bool verbose) override;
    void ParseHeapDumpClassDump(bool verbose) override;
    void ParseHeapDumpInstanceDump(bool verbose) override;
    void ParseHeapDumpObjectArrayDump(bool verbose) override;
    void ParseHeapDumpStructArrayDump(bool verbose) override;
};

#endif // HPROF_PARSER_H