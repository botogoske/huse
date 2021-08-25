// huse
// Copyright (c) 2021 Borislav Stanimirov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
#include "JsonSerializer.hpp"

#include <cmath>
#include <iostream>
#include <type_traits>

namespace huse
{

JsonSerializer::JsonSerializer(std::ostream& out, bool pretty)
    : m_out(out)
    , m_pretty(pretty)
{}

JsonSerializer::~JsonSerializer()
{
    assert(m_depth == 0);
}

void JsonSerializer::writeRawJson(std::string_view key, std::string_view json)
{
    pushKey(key);
    m_out << json;
    m_hasValue = true;
}

template <typename T>
void JsonSerializer::writeSimpleValue(T val)
{
    prepareWriteVal();
    m_out << val;
}

void JsonSerializer::write(bool val)
{
    static constexpr std::string_view t = "true", f = "false";
    writeSimpleValue(val ? t : f);
}

void JsonSerializer::write(short val) { writeSimpleValue(val); }
void JsonSerializer::write(unsigned short val) { writeSimpleValue(val); }
void JsonSerializer::write(int val) { writeSimpleValue(val); }
void JsonSerializer::write(unsigned int val) { writeSimpleValue(val); }

template <typename T>
void JsonSerializer::writePotentiallyBigIntegerValue(T val)
{
    if constexpr (sizeof(T) <= 4)
    {
        // true for long on windows
        writeSimpleValue(val);
    }
    else if constexpr (std::is_signed_v<T>)
    {
        // max integer which can be stored in a double
        constexpr int64_t MAX_D = 9007199254740992ll;
        constexpr int64_t MIN_D = -9007199254740992ll;

        if (val >= MIN_D && val <= MAX_D)
        {
            writeSimpleValue(val);
        }
        else
        {
            throwException("integer too big");
        }
    }
    else
    {
        constexpr uint64_t MAX_D = 9007199254740992ull;

        if (val <= MAX_D)
        {
            writeSimpleValue(val);
        }
        else
        {
            throwException("integer too big");
        }
    }

}

// some values may not fit json's numbers
void JsonSerializer::write(long val) { writePotentiallyBigIntegerValue(val); }
void JsonSerializer::write(unsigned long val) { writePotentiallyBigIntegerValue(val); }
void JsonSerializer::write(long long val) { writePotentiallyBigIntegerValue(val); }
void JsonSerializer::write(unsigned long long val) { writePotentiallyBigIntegerValue(val); }

template <typename T>
void JsonSerializer::writeFloatValue(T val)
{
    if (std::isfinite(val))
    {
        writeSimpleValue(val);
    }
    else
    {
        throwException("float not finite");
    }
}

void JsonSerializer::write(float val) { writeFloatValue(val); }
void JsonSerializer::write(double val) { writeFloatValue(val); }

void JsonSerializer::writeEscapedUTF8String(std::string_view str)
{
    m_out.put('\"');

    for (auto c : str)
    {
        switch (c) {
            // http://www.json.org/
        case '"':
            m_out << "\\\"";
            break;
        case '\\':
            m_out << "\\\\";
            break;
        case '\n':
            m_out << "\\n";
            break;
        case '\r':
            m_out << "\\r";
            break;
        case '\b':
            m_out << "\\b";
            break;
        case '\t':
            m_out << "\\t";
            break;
        case '\f':
            m_out << "\\f";
            break;
        default:
        {
            if (c >= ' ')
            {
                m_out.put(c);
            }
            else {
                const char* prefix = c < 16 ? "\\u000" : "\\u00";
                m_out << prefix << std::hex << c << std::dec;
            }
        }
        break;
        }
    }

    m_out.put('\"');
}

void JsonSerializer::write(std::string_view val)
{
    prepareWriteVal();
    writeEscapedUTF8String(val);
}

void JsonSerializer::pushKey(std::string_view k)
{
    if (m_hasValue) m_out << ',';
    newLine();
    writeEscapedUTF8String(k);
    m_out << ':';
    m_hasValue = false;
}

void JsonSerializer::open(char o)
{
    prepareWriteVal();
    m_out << o;
    m_hasValue = false;
    ++m_depth;
}

void JsonSerializer::close(char c)
{
    assert(m_depth);
    --m_depth;
    if (m_hasValue) newLine();
    m_out << c;
    m_hasValue = true;
}

void JsonSerializer::openObject() { open('{'); }
void JsonSerializer::closeObject() { close('}'); }
void JsonSerializer::openArray()
{
    open('[');
    m_arrayJustOpen = true;
}
void JsonSerializer::closeArray()
{
    close(']');
    m_arrayJustOpen = false;
}

void JsonSerializer::prepareWriteVal()
{
    if (m_hasValue)
    {
        m_out << ',';
        newLine();
    }
    else if (m_arrayJustOpen)
    {
        newLine();
        m_arrayJustOpen = false;
    }
    m_hasValue = true;
}

void JsonSerializer::newLine()
{
    if (!m_pretty) return; // not pretty

    static constexpr std::string_view indent = "  ";
    m_out << '\n';
    for (uint32_t i = 0; i < m_depth; ++i)
    {
        m_out << indent;
    }
}

void JsonSerializer::throwException(std::string text) const {}

}
