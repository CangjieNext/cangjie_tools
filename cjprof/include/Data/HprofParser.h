// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef HPROF_PARSER_H
#define HPROF_PARSER_H

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

        u4 GetFixedSize() { return 16; }
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

            for (size_t i = 0; i < frames.size() && i < rhs.frames.size(); i++) {
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
    std::unordered_map<ID, u4> m_componentNums;

    u8 m_fileTime;
    u4 m_idSize;

    HprofParserBase() : m_fileTime(0), m_idSize(0) {}
    virtual ~HprofParserBase() = default;

    // Virtual parsing methods - implemented by concrete parsers
    virtual void ParseString(bool verbose) = 0;
    virtual void ParseClass(bool verbose) = 0;
    virtual void ParseHeapDumpClassDump(bool verbose) = 0;
    virtual void ParseHeapDumpInstanceDump(bool verbose) = 0;

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

    // Endian swap functions
    inline u2 SwapEndian(u2 val)
    {
        return static_cast<short unsigned int>((val & 0xff00U) >> 8)
            | static_cast<short unsigned int>((val & 0xffU) << 8);
    }

    inline u4 SwapEndian(u4 val)
    {
        return ((val & 0xff000000U) >> 24) | ((val & 0xff0000U) >> 8) |
            ((val & 0xff00U) << 8) | ((val & 0xffU) << 24);
    }

    inline u8 SwapEndian(u8 val)
    {
        return ((val & 0xff00000000000000ULL) >> 56) | ((val & 0xff000000000000ULL) >> 40) |
            ((val & 0xff0000000000ULL) >> 24) | ((val & 0xff00000000ULL) >> 8) |
            ((val & 0xff000000ULL) << 8) | ((val & 0xff0000ULL) << 24) |
            ((val & 0xff00ULL) << 40) | ((val & 0xffULL) << 56);
    }

    virtual ID ReadId() = 0;
    virtual void WriteId(ID id) = 0;

protected:
    std::string m_data;
    std::string m_outputData;
    size_t m_curPos;
};

// New format parser V2 (1.0.2)
class HprofParserV2 : public HprofParserBase {
public:
    HprofParserV2() { m_idSize = 4; }

    void ParseString(bool verbose) override;
    void ParseClass(bool verbose) override;
    void ParseHeapDumpClassDump(bool verbose) override;
    void ParseHeapDumpInstanceDump(bool verbose) override;

    ID ReadId() override;
    void WriteId(ID id) override;
};

// Old format parser V1 (1.0.1)
class HprofParserV1 : public HprofParserBase {
public:
    HprofParserV1() { m_idSize = 8; }

    void ParseString(bool verbose) override;
    void ParseClass(bool verbose) override;
    void ParseHeapDumpClassDump(bool verbose) override;
    void ParseHeapDumpInstanceDump(bool verbose) override;

    ID ReadId() override;
    void WriteId(ID id) override;
};

#endif // HPROF_PARSER_H