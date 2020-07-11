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
#include <list>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Getodac {

/*!
 * \brief The SpinLock class
 *
 * Spin lock mutex
 */
class SpinLock
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

    throw std::runtime_error{"Bad hex value"};
}

/*!
 * \brief unEscapeUrl
 * \param in
 * \return
 */
inline std::string unEscapeUrl(std::string_view in)
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
                throw std::runtime_error{"Malformated URL"};
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
 * \param pos in str to sart search
 * \param nchars nchars in str to search for
 *
 * \return a vector of pair<pos, nchars> substr chunks
 */
using SplitVector = std::vector<std::string_view>;
inline SplitVector split(std::string_view str, char ch)
{
    if (!str.size())
        return {};

    SplitVector ret;
    std::string::size_type pos = 0;
    for (auto nextPos = str.find(ch, pos); nextPos != std::string::npos; nextPos = str.find(ch, pos)) {
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
class LRUCache
{
    using KeyValue = std::pair<K, V>;
    using List = std::list<KeyValue>;
    using Iterator = typename List::iterator;

public:
    explicit LRUCache(size_t cacheSize)
        : m_cacheSize(cacheSize) {}

    inline void put(const K &key, const V &value)
    {
        auto it = m_cacheHash.find(key);
        if (it != m_cacheHash.end())
            m_cacheItems.erase(it->second);
        m_cacheItems.emplace_front(key, value);
        m_cacheHash[key] = m_cacheItems.begin();
        cleanCache();
    }

    inline V &getReference(const K &key)
    {
        auto it = m_cacheHash.find(key);
        if (it == m_cacheHash.end())
            throw std::range_error{"Invalid key"};
        m_cacheItems.splice(m_cacheItems.begin(), m_cacheItems, it->second);
        return it->second->second;
    }

    inline V getValue(const K &key)
    {
        auto it = m_cacheHash.find(key);
        if (it == m_cacheHash.end())
            return V{};
        m_cacheItems.splice(m_cacheItems.begin(), m_cacheItems, it->second);
        return it->second->second;
    }

    inline bool exists(const K &key)
    {
        return m_cacheHash.find(key) != m_cacheHash.end();
    }

    void setCacheSize(size_t size)
    {
        m_cacheSize = size;
        cleanCache();
    }

    void clear()
    {
        m_cacheItems.clear();
        m_cacheHash.clear();
    }

    Iterator begin()
    {
        return m_cacheItems.begin();
    }

    Iterator end()
    {
        return m_cacheItems.end();
    }

    Iterator begin() const
    {
        return m_cacheItems.begin();
    }

    Iterator end() const
    {
        return m_cacheItems.end();
    }

    Iterator erase(Iterator it)
    {
        m_cacheHash.erase(it->first);
        return m_cacheItems.erase(it);
    }

    size_t size() const
    {
        assert(m_cacheItems.size() == m_cacheHash.size());
        return m_cacheHash.size();
    }
private:
    inline void cleanCache()
    {
        while (m_cacheHash.size() > m_cacheSize) {
            auto it = m_cacheItems.rbegin();
            m_cacheHash.erase(it->first);
            m_cacheItems.pop_back();
        }
    }

private:
    List m_cacheItems;
    std::unordered_map<K, Iterator> m_cacheHash;
    size_t m_cacheSize;
};

/*!
 * \brief addrText
 *
 * Transforms \a addr sockaddr_storage struct to string
 */
inline std::string addrText(const sockaddr_storage &addr)
{
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    if (getnameinfo((const sockaddr *)&addr, sizeof(sockaddr_storage),
                    hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
        return hbuf;
    }
    return {};
}

/*!
 * \brief The SimpleTimer class
 */
class SimpleTimer
{
public:
    template<typename T>
    SimpleTimer(T callback, std::chrono::milliseconds timeout = std::chrono::seconds{1}, bool singleShot = false)
        : m_thread([=]{
        std::unique_lock<std::mutex> lock(m_lock);
        while (!m_waitCondition.wait_for(lock, timeout, [this]{return m_quit.load();})) {
            callback();
            if (__builtin_expect(singleShot, 0))
                m_quit.store(true);
        };
    })
    {};

    ~SimpleTimer()
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

} // namespace Getodac
