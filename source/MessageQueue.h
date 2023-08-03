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

#include "IMessageQueue.h"
#include <condition_variable>
#include <deque>
#include <functional>
#include <gst/gst.h>
#include <memory>
#include <mutex>
#include <thread>

class CallInEventLoopMessage : public Message
{
public:
    explicit CallInEventLoopMessage(const std::function<void()> &func) : m_func(func), m_done{false} {}
    void handle() override
    {
        std::unique_lock<std::mutex> lock(m_callInEventLoopMutex);
        m_func();
        m_done = true;
        m_callInEventLoopCondVar.notify_all();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(m_callInEventLoopMutex);
        m_callInEventLoopCondVar.wait(lock, [this]() { return m_done; });
    }

private:
    const std::function<void()> m_func;
    std::mutex m_callInEventLoopMutex;
    std::condition_variable m_callInEventLoopCondVar;
    bool m_done;
};

class MessageQueue : public IMessageQueue
{
public:
    MessageQueue() : m_running(false) {}

    ~MessageQueue() override { doStop(); }

    void start() override
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

    void stop() override { doStop(); }

    void clear() override { doClear(); }

    // Wait for a message to appear on the queue.
    std::shared_ptr<Message> waitForMessage() override
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
    bool postMessage(const std::shared_ptr<Message> &msg) override
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

    void processMessages() override
    {
        do
        {
            std::shared_ptr<Message> message = waitForMessage();
            message->handle();
        } while (m_running);
    }

    bool callInEventLoop(const std::function<void()> &func) override
    {
        if (std::this_thread::get_id() != m_workerThread.get_id())
        {
            auto message = std::make_shared<CallInEventLoopMessage>(func);
            if (!postMessage(message))
            {
                return false;
            }
            message->wait();
        }
        else
        {
            func();
        }

        return true;
    }

protected:
    void doStop()
    {
        if (!m_running)
        {
            // queue is not running
            return;
        }
        callInEventLoop([this]() { m_running = false; });

        if (m_workerThread.joinable())
            m_workerThread.join();

        doClear();
    }

    void doClear()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.clear();
    }

protected:
    std::condition_variable m_condVar;
    std::mutex m_mutex;
    std::deque<std::shared_ptr<Message>> m_queue;
    std::thread m_workerThread;
    bool m_running;
};
