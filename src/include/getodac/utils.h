/*
    Copyright (C) 2016, BogDan Vatra <bogdan@kde.org>

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
*/

#ifndef UTILS_H
#define UTILS_H

#include <atomic>
#include <string>
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
 * \brief findInSubstr
 *
 * Find the ch in str starting from pos to nchars
 *
 * \param str string to search in
 * \param pos in str from where to search
 * \param nchars to search
 * \param ch to serch
 * \return the position of the ch in str or npos if not found
 */
inline std::string::size_type findInSubstr(const std::string &str, std::string::size_type pos, std::string::size_type nchars, char ch)
{
    for (; nchars; ++pos, --nchars)
        if (str[pos] == ch)
            return pos;
    return std::string::npos;
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
inline std::vector<std::pair<std::string::size_type, std::string::size_type>> split(const std::string &str, char ch, std::string::size_type pos = 0, std::string::size_type nchars = std::string::npos)
{
    std::vector<std::pair<std::string::size_type, std::string::size_type>> ret;
    if (!nchars)
        return ret;

    if (nchars == std::string::npos)
        nchars = str.size() - pos;

    for (auto nextPos = findInSubstr(str, pos, nchars, ch); nextPos != std::string::npos; nextPos = findInSubstr(str, pos, nchars, ch)) {
        // Ignore empty strings
        if (pos != nextPos) {
            auto sz = nextPos - pos;
            ret.emplace_back(std::make_pair(pos, sz));
            nchars -= sz + 1;
        }
        pos = nextPos + 1;
    }
    if (nchars)
        ret.emplace_back(std::make_pair(pos, nchars));
    return ret;
}

} // namespace Getodac

#endif // UTILS_H
