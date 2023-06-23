/*
 * Copyright (C) 2022 Sky UK
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <gst/gst.h>
#include <memory>
#include <mutex>
#include <thread>

class Message
{
public:
    virtual ~Message() {}
    virtual void handle() = 0;
};

class CallInEventLoopMessage : public Message
{
public:
    CallInEventLoopMessage(const std::function<void()> &func, std::mutex &callInEventLoopMutex,
                           std::condition_variable &callInEventLoopCondVar)
        : m_func(func), m_callInEventLoopMutex(callInEventLoopMutex), m_callInEventLoopCondVar(callInEventLoopCondVar)
    {
    }
    void handle()
    {
        std::unique_lock<std::mutex> lock(m_callInEventLoopMutex);
        m_func();
        m_callInEventLoopCondVar.notify_all();
    }

private:
    const std::function<void()> m_func;
    std::mutex &m_callInEventLoopMutex;
    std::condition_variable &m_callInEventLoopCondVar;
};

class MessageQueue
{
public:
    MessageQueue() : m_running(false) {}

    virtual ~MessageQueue() { stop(); }

    void start()
    {
        if (m_running)
        {
            // queue is running
            return;
        }
        m_running = true;
        std::thread startThread(&MessageQueue::processMessages, this);
        m_workerThread.swap(startThread);
    }

    void stop()
    {
        if (!m_running)
        {
            // queue is not running
            return;
        }
        callInEventLoop([this]() { m_running = false; });

        if (m_workerThread.joinable())
            m_workerThread.join();

        clear();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.clear();
    }

    // Wait for a message to appear on the queue.
    std::shared_ptr<Message> waitForMessage()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty())
        {
            m_condVar.wait(lock);
        }
        std::shared_ptr<Message> message = m_queue.front();
        m_queue.pop_front();
        return message;
    }

    // Posts a message to the queue.
    bool postMessage(const std::shared_ptr<Message> &msg)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running)
        {
            GST_ERROR("Message queue is not running");
            return false;
        }
        m_queue.push_back(msg);
        m_condVar.notify_all();

        return true;
    }

    void processMessages()
    {
        do
        {
            std::shared_ptr<Message> message = waitForMessage();
            message->handle();
        } while (m_running);
    }

    bool callInEventLoopImpl(const std::function<void()> &func)
    {
        if (std::this_thread::get_id() != m_workerThread.get_id())
        {
            std::mutex callInEventLoopMutex;
            std::unique_lock<std::mutex> lock(callInEventLoopMutex);
            std::condition_variable callInEventLoopCondVar;

            if (!postMessage(std::make_shared<CallInEventLoopMessage>(func, callInEventLoopMutex, callInEventLoopCondVar)))
            {
                return false;
            }

            callInEventLoopCondVar.wait(lock);
        }
        else
        {
            func();
        }

        return true;
    }

    template <class Function, class... Args> bool callInEventLoop(Function &&f, Args &&...args)
    {
        return callInEventLoopImpl(std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
    }

protected:
    std::condition_variable m_condVar;
    std::mutex m_mutex;
    std::deque<std::shared_ptr<Message>> m_queue;
    std::thread m_workerThread;
    bool m_running;
};

class SetPositionMessage : public Message
{
public:
    SetPositionMessage(int64_t newPosition, int64_t &targetPosition)
        : m_newPosition(newPosition), m_targetPosition(targetPosition)
    {
    }
    void handle() { m_targetPosition = m_newPosition; }

private:
    int64_t m_newPosition;
    int64_t &m_targetPosition;
};

class SetDurationMessage : public Message
{
public:
    SetDurationMessage(int64_t newDuration, int64_t &targetDuration)
        : m_newDuration(newDuration), m_targetDuration(targetDuration)
    {
    }
    void handle() { m_targetDuration = m_newDuration; }

private:
    int64_t m_newDuration;
    int64_t &m_targetDuration;
};
