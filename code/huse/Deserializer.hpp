// huse
// Copyright (c) 2021 Borislav Stanimirov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
#pragma once
#include "API.h"

#include "impl/UniqueStack.hpp"

#include <string_view>
#include <optional>

namespace huse
{

class DeserializerArray;
class DeserializerObject;
class Deserializer;

class DeserializerNode : public impl::UniqueStack
{
protected:
    DeserializerNode(Deserializer& d, impl::UniqueStack* parent)
        : impl::UniqueStack(parent)
        , m_deserializer(d)
    {}

    Deserializer& m_deserializer;
public:
    DeserializerNode(const DeserializerNode&) = delete;
    DeserializerNode operator=(const DeserializerNode&) = delete;
    DeserializerNode(DeserializerNode&&) = delete;
    DeserializerNode operator=(DeserializerNode&&) = delete;

    DeserializerObject obj();
    DeserializerArray ar();

    template <typename T>
    void val(T& v);

    struct Type
    {
    public:
        enum Value : int
        {
            True    = 0b01,
            False   = 0b10,
            Boolean = 0b11,
            Integer = 0b0100,
            Float   = 0b1000,
            Number  = 0b1100,
            String  = 0b10000,
            Object  = 0b100000,
            Array   = 0b1000000,
            Null    = 0b10000000,
        };

        Type(Value t) : m_t(t) {}

        bool is(Value mask) const { return m_t & mask; }
    private:
        Value m_t;
    };

    Type type() const;

protected:
    // number of elements in compound object
    int length() const;
};

class DeserializerArray : public DeserializerNode
{
public:
    DeserializerArray(Deserializer& d, impl::UniqueStack* parent = nullptr);
    ~DeserializerArray();

    using DeserializerNode::length;
    DeserializerNode& index(int index);

    // intentionally hiding parent
    Type type() const { return {Type::Array}; }
};

class DeserializerObject : private DeserializerNode
{
public:
    DeserializerObject(Deserializer& d, impl::UniqueStack* parent = nullptr);
    ~DeserializerObject();

    using DeserializerNode::length;

    DeserializerNode& key(std::string_view k);

    DeserializerObject obj(std::string_view k)
    {
        return key(k).obj();
    }
    DeserializerArray ar(std::string_view k)
    {
        return key(k).ar();
    }

    DeserializerNode* optkey(std::string_view k);

    template <typename T>
    void val(std::string_view k, T& v)
    {
        key(k).val(v);
    }

    template <typename T>
    void val(std::string_view k, std::optional<T>& v)
    {
        if (auto open = optkey(k))
        {
            open->val(v.emplace());
        }
        else
        {
            v.reset();
        }
    }

    template <typename T>
    void optval(std::string_view k, T& v)
    {
        if (auto open = optkey(k))
        {
            open->val(v);
        }
    }

    template <typename T>
    void optval(std::string_view k, std::optional<T>& v, const T& d)
    {
        if (auto open = optkey(k))
        {
            open->val(v.emplace());
        }
        else
        {
            v.reset(d);
        }
    }

    template <typename T>
    void flatval(T& v);

    struct KeyQuery
    {
        std::string_view name;
        DeserializerNode* node = nullptr;
        explicit operator bool() const { return node; }
        DeserializerNode* operator->() { return node; }
    };
    KeyQuery nextkey();

    // intentionally hiding parent
    Type type() const { return {Type::Object}; }
};

class HUSE_API Deserializer : public DeserializerNode
{
    friend class DeserializerNode;
    friend class DeserializerArray;
    friend class DeserializerObject;
public:
    Deserializer() : DeserializerNode(*this, nullptr) {}
    virtual ~Deserializer();

    struct HUSE_API Exception {};
    [[noreturn]] virtual void throwException(std::string msg) const = 0;

protected:
    // read interface
    virtual void read(bool& val) = 0;
    virtual void read(short& val) = 0;
    virtual void read(unsigned short& val) = 0;
    virtual void read(int& val) = 0;
    virtual void read(unsigned int& val) = 0;
    virtual void read(long& val) = 0;
    virtual void read(unsigned long& val) = 0;
    virtual void read(long long& val) = 0;
    virtual void read(unsigned long long& val) = 0;
    virtual void read(float& val) = 0;
    virtual void read(double& val) = 0;
    virtual void read(std::string_view& val) = 0;
    virtual void read(std::string& val) = 0;

    // implementation interface
    virtual void loadObject() = 0;
    virtual void unloadObject() = 0;

    virtual void loadArray() = 0;
    virtual void unloadArray() = 0;

    // number of sub-nodes in current node
    virtual int curLength() const = 0;

    // throw if no key
    virtual void loadKey(std::string_view key) = 0;

    // load and resturn true if key exists, otherwise return false
    // equivalent to (but more optimized than)
    // if (hasKey(k)) { loadKey(k); return true; } else return false;
    virtual bool tryLoadKey(std::string_view key) = 0;

    // throw if no index
    virtual void loadIndex(int index) = 0;

    virtual Type pendingType() const = 0;

    // noad the next key and return it or nullopt if there is no next key
    virtual std::optional<std::string_view> loadNextKey() = 0;
};

inline DeserializerObject DeserializerNode::obj()
{
    return DeserializerObject(m_deserializer, this);
}

inline DeserializerArray DeserializerNode::ar()
{
    return DeserializerArray(m_deserializer, this);
}

namespace impl
{
struct DeserializerReadHelper : public Deserializer {
    using Deserializer::read;
};

template <typename, typename = void>
struct HasDeserializerRead : std::false_type {};

template <typename T>
struct HasDeserializerRead<T, decltype(std::declval<DeserializerReadHelper>().read(std::declval<T&>()))> : std::true_type {};

template <typename, typename = void>
struct HasDeserializeMethod : std::false_type {};

template <typename T>
struct HasDeserializeMethod<T, decltype(std::declval<T>().huseDeserialize(std::declval<DeserializerNode&>()))> : std::true_type {};

template <typename, typename = void>
struct HasDeserializeFunc : std::false_type {};

template <typename T>
struct HasDeserializeFunc<T, decltype(huseDeserialize(std::declval<DeserializerNode&>(), std::declval<T&>()))> : std::true_type {};

template <typename, typename = void>
struct HasDeserializeFlatMethod : std::false_type {};

template <typename T>
struct HasDeserializeFlatMethod<T, decltype(std::declval<T>().huseDeserializeFlat(std::declval<DeserializerObject&>()))> : std::true_type {};

template <typename, typename = void>
struct HasDeserializeFlatFunc : std::false_type {};

template <typename T>
struct HasDeserializeFlatFunc<T, decltype(huseDeserializeFlat(std::declval<DeserializerObject&>(), std::declval<T&>()))> : std::true_type {};
} // namespace impl

template <typename T>
void DeserializerNode::val(T& v) {
    if constexpr (impl::HasDeserializeMethod<T>::value)
    {
        v.huseDeserialize(*this);
    }
    else if constexpr (impl::HasDeserializeFunc<T>::value)
    {
        huseDeserialize(*this, v);
    }
    else if constexpr (impl::HasDeserializerRead<T>::value)
    {
        m_deserializer.read(v);
    }
    else
    {
        cannot_deserialize(v);
    }
}

inline DeserializerNode::Type DeserializerNode::type() const
{
    return m_deserializer.pendingType();
}

inline int DeserializerNode::length() const
{
    return m_deserializer.curLength();
}

inline DeserializerArray::DeserializerArray(Deserializer& d, impl::UniqueStack* parent)
    : DeserializerNode(d, parent)
{
    m_deserializer.loadArray();
}

inline DeserializerArray::~DeserializerArray()
{
    m_deserializer.unloadArray();
}

inline DeserializerNode& DeserializerArray::index(int index)
{
    m_deserializer.loadIndex(index);
    return *this;
}

inline DeserializerObject::DeserializerObject(Deserializer& d, impl::UniqueStack* parent)
    : DeserializerNode(d, parent)
{
    m_deserializer.loadObject();
}
inline DeserializerObject::~DeserializerObject()
{
    m_deserializer.unloadObject();
}

inline DeserializerNode& DeserializerObject::key(std::string_view k)
{
    m_deserializer.loadKey(k);
    return *this;
}

inline DeserializerNode* DeserializerObject::optkey(std::string_view k)
{
    if (m_deserializer.tryLoadKey(k)) return this;
    return nullptr;
}

inline DeserializerObject::KeyQuery DeserializerObject::nextkey()
{
    auto name = m_deserializer.loadNextKey();
    if (!name) return {};
    return {*name, this};
}

template <typename T>
void DeserializerObject::flatval(T& v)
{
    if constexpr (impl::HasDeserializeFlatMethod<T>::value)
    {
        v.huseDeserializeFlat(*this);
    }
    else if constexpr (impl::HasDeserializeFlatFunc<T>::value)
    {
        huseDeserializeFlat(*this, v);
    }
    else
    {
        cannot_deserialize(v);
    }
}

} // namespace huse
