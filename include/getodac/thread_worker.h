/*
    Copyright (C) 2018, BogDan Vatra <bogdan@kde.org>

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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <stack>
#include <thread>
#include <vector>

namespace Getodac {

/*!
 * \brief The ThreadWorker class
 *
 * Helper class to sync run tasks on worker thread(s)
 */
class ThreadWorker
{
public:
    /*!
     * \brief ThreadWorker
     *
     * \param workers the number of threads that will be used to run the tasks
     */
    ThreadWorker(uint32_t workers = 1)
    {
        workers = std::max(uint32_t(1), workers);
        m_quit.store(false);
        m_workers.reserve(workers);
        for (uint32_t i = 0; i < workers; ++i)
            m_workers.emplace_back([this]{
                while (!m_quit) {
                    auto task = nextTask();
                    if (task)
                        task();
                    else
                        break;
                }
            });
    }

    ~ThreadWorker()
    {
        m_quit.store(true);
        m_waitCondition.notify_all();
        for (auto & m_worker : m_workers)
            m_worker.join();
    }

    /*!
     * \brief insertTask
     *
     * Enqueue a new task. The task will be run on one of the worker threads
     *
     * \param task to execute
     */
    void insertTask(const std::function<void()> &task)
    {
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_pendingTasks.push(task);
        }
        m_waitCondition.notify_one();
    }

private:
    /*!
     * \brief nextTask
     *
     * Waits until a new task is available and then returns it to any available worker thread
     *
     * \return the next task to run
     */
    inline std::function<void()> nextTask()
    {
        std::unique_lock<std::mutex> lock(m_lock);
        m_waitCondition.wait(lock, [this]{return m_quit || !m_pendingTasks.empty();});
        if (m_quit)
            return std::function<void()>();
        std::function<void()> ret = m_pendingTasks.top();
        m_pendingTasks.pop();
        return ret;
    }

private:
    std::atomic_bool m_quit{false};
    std::stack<std::function<void()>> m_pendingTasks;
    std::mutex m_lock;
    std::condition_variable m_waitCondition;
    std::vector<std::thread> m_workers;
};

} // namespace Getodac
