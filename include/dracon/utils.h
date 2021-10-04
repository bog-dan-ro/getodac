/*
    Copyright (C) 2020, BogDan Vatra <bogdan@kde.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    AGPL EXCEPTION:
    The AGPL license applies only to this file itself.

    As a special exception, the copyright holders of this file give you permission
    to use it, regardless of the license terms of your work, and to copy and distribute
    them under terms of your choice.
    If you do any changes to this file, these changes must be published under AGPL.

*/

#pragma once

#include <sys/socket.h>
#include <netdb.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <list>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace dracon {

/*!
 * \brief The SpinLock class
 *
 * Spin lock mutex
 */
class spin_lock
{
public:
    inline void lock() noexcept
    {
        while (m_lock.test_and_set(std::memory_order_acquire))
            ;
    }

    inline void unlock() noexcept
    {
        m_lock.clear(std::memory_order_release);
    }

    inline bool try_lock()
    {
        return !m_lock.test_and_set(std::memory_order_acquire);
    }

private:
    std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};


/*!
 * \brief toHex
 *
 * \param ch
 * \return
 */
inline char fromHex(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return 10 + ch -'a';
    if (ch >= 'A' && ch <= 'F')
        return 10 + ch -'A';

    throw std::invalid_argument{"Bad hex value"};
}

/*!
 * \brief unEscapeUrl
 * \param in
 * \return
 */
inline std::string unescape_url(std::string_view in)
{
    std::string out;
    out.reserve(in.size());
    for (std::string::size_type i = 0 ; i < in.size(); ++i) {
        switch (in[i]) {
        case '%':
            if (i + 2 < in.size()) {
                // it's faster than std::stoi(in.substr(i + 1, 2)) ...
                out += static_cast<char>(fromHex(in[i + 1]) << 4 | fromHex(in[i + 2]));
                i += 2;
            } else {
                throw std::invalid_argument{"Malformated URL"};
            }
            break;
        case '+':
            out += ' ';
            break;
        default:
            out += in[i];
            break;
        }
    }
    return out;
}


/*!
 * \brief split
 *
 * Splits the str starting from pos nchars by ch character
 *
 * \param str string to search in
 * \param ch to search for
 * \param count max items to return
 *
 * \return a vector of pair<pos, nchars> substr chunks
 */
using SplitVector = std::vector<std::string_view>;
inline SplitVector split(std::string_view str, char ch, std::string::size_type count = std::string::npos)
{
    if (!str.size())
        return {};

    SplitVector ret;
    std::string::size_type pos = 0;
    for (auto nextPos = str.find(ch, pos); nextPos != std::string::npos && count--; nextPos = str.find(ch, pos)) {
        // Ignore empty strings
        if (pos != nextPos) {
            auto sz = nextPos - pos;
            ret.emplace_back(str.substr(pos,sz));
        }
        pos = nextPos + 1;
    }
    if (pos < str.size())
        ret.emplace_back(str.substr(pos));
    return ret;
}

template <typename K, typename V>
class lru_cache
{
    using KeyValue = std::pair<K, V>;
    using List = std::list<KeyValue>;
    using Iterator = typename List::iterator;

public:
    explicit lru_cache(size_t cacheSize)
        : m_cache_size(cacheSize) {}

    inline void put(const K &key, const V &value)
    {
        auto it = m_cache_hash.find(key);
        if (it != m_cache_hash.end())
            m_cache_items.erase(it->second);
        m_cache_items.emplace_front(key, value);
        m_cache_hash[key] = m_cache_items.begin();
        clean_cache();
    }

    inline V &reference(const K &key)
    {
        auto it = m_cache_hash.find(key);
        if (it == m_cache_hash.end())
            throw std::range_error{"Invalid key"};
        m_cache_items.splice(m_cache_items.begin(), m_cache_items, it->second);
        return it->second->second;
    }

    inline V value(const K &key)
    {
        auto it = m_cache_hash.find(key);
        if (it == m_cache_hash.end())
            return V{};
        m_cache_items.splice(m_cache_items.begin(), m_cache_items, it->second);
        return it->second->second;
    }

    inline bool exists(const K &key)
    {
        return m_cache_hash.find(key) != m_cache_hash.end();
    }

    void cache_size(size_t size)
    {
        m_cache_size = size;
        clean_cache();
    }

    void clear()
    {
        m_cache_items.clear();
        m_cache_hash.clear();
    }

    Iterator begin()
    {
        return m_cache_items.begin();
    }

    Iterator end()
    {
        return m_cache_items.end();
    }

    Iterator begin() const
    {
        return m_cache_items.begin();
    }

    Iterator end() const
    {
        return m_cache_items.end();
    }

    Iterator erase(Iterator it)
    {
        m_cache_hash.erase(it->first);
        return m_cache_items.erase(it);
    }

    size_t size() const
    {
        assert(m_cache_items.size() == m_cache_hash.size());
        return m_cache_hash.size();
    }
private:
    inline void clean_cache()
    {
        while (m_cache_hash.size() > m_cache_size) {
            auto it = m_cache_items.rbegin();
            m_cache_hash.erase(it->first);
            m_cache_items.pop_back();
        }
    }

private:
    List m_cache_items;
    std::unordered_map<K, Iterator> m_cache_hash;
    size_t m_cache_size;
};

/*!
 * \brief addrText
 *
 * Transforms \a addr sockaddr_storage struct to string
 */
inline std::string addr_text(const sockaddr_storage &addr)
{
    char hbuf[NI_MAXHOST];
    if (getnameinfo((const sockaddr *)&addr, sizeof(sockaddr_storage),
                    hbuf, sizeof(hbuf), nullptr, 0, NI_NUMERICHOST) == 0) {
        return hbuf;
    }
    return {};
}

/*!
 * \brief The SimpleTimer class
 */
class simple_timer
{
public:
    template<typename T>
    simple_timer(T callback, std::chrono::milliseconds timeout = std::chrono::seconds{1}, bool singleShot = false)
        : m_thread([callback, timeout, singleShot, this]{
        std::unique_lock<std::mutex> lock(m_lock);
        while (!m_waitCondition.wait_for(lock, timeout, [this]{return m_quit.load();})) {
            callback();
            if (__builtin_expect(singleShot, 0))
                m_quit.store(true);
        }
    })
    {}

    ~simple_timer()
    {
        m_quit.store(true);
        m_waitCondition.notify_one();
        if (m_thread.joinable())
            m_thread.join();
    }
private:
    std::condition_variable m_waitCondition;
    std::mutex m_lock;
    std::atomic_bool m_quit{false};
    std::thread m_thread;
};

template <typename T = char>
class buffer {
public:
    buffer() = default;
    buffer(size_t size)
        : m_buffer(std::make_unique<T[]>(size))
        , m_size(size)
    {}

    void reset()
    {
        m_current = m_buffer.get();
        m_end = m_current + m_size;
    }

    void resize(size_t size)
    {
        if (size == m_size) {
            reset();
            return;
        }
        auto tmp = std::make_unique<T[]>(size);
        std::memcpy(tmp.get(), m_buffer.get(), sizeof(T) * std::min(size, m_size));
        m_buffer = std::move(tmp);
        m_size = size;
        reset();
    }

    void advance(size_t size)
    {
        m_current += size;
        if (m_current > m_end)
            m_current = m_end;
    }

    void commit()
    {
        if (m_current == m_buffer.get())
            return;
        size_t size = m_end - m_current;
        std::memmove(m_buffer.get(), m_current, size);
        m_current = m_buffer.get();
        m_end = m_current + size;
    }

    const T *current_data() const
    {
        return m_current;
    }

    T *current_data()
    {
        return m_current;
    }
    size_t current_size() const
    {
        return m_end - m_current;
    }

    void set_current_size(size_t size)
    {
        m_end = m_current + size;
    }

    void set_current_data(size_t size)
    {
        m_current = m_buffer.get() + size;
    }

    const T *data() const
    {
        return m_buffer.get();
    }
    T *data()
    {
        return m_buffer.get();
    }

    size_t size() const
    {
        return m_size;
    }

    std::basic_string_view<T> current_string() const
    {
        return {m_current, size_t(m_end - m_current)};
    }

    std::basic_string_view<T> string() const
    {
        return std::basic_string_view<T>{m_buffer.get(), m_size};
    }

    buffer<T>& operator=(std::string_view buff)
    {
        const size_t size = buff.size();
        resize(size);
        memcpy(m_buffer.get(), buff.data(), size);
        set_current_size(size);
        return *this;
    }

    buffer<T>& operator *=(std::string_view buff)
    {
        const size_t size = buff.size();
        if (m_size <= size)
            return operator=(buff);
        reset();
        memcpy(m_buffer.get(), buff.data(), size);
        advance(size);
        return *this;
    }

    void clear()
    {
        m_buffer.reset();
        m_size = 0;
        reset();
    }
private:
    std::unique_ptr<T[]> m_buffer;
    size_t m_size = 0;
    T *m_current = nullptr;
    T *m_end = nullptr;
};

using char_buffer = buffer<char>;

} // namespace dracon
