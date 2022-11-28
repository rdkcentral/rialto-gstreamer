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
        : mFunc(func), mCallInEventLoopMutex(callInEventLoopMutex), mCallInEventLoopCondVar(callInEventLoopCondVar)
    {
    }
    void handle()
    {
        std::unique_lock<std::mutex> lock(mCallInEventLoopMutex);
        mFunc();
        mCallInEventLoopCondVar.notify_all();
    }

private:
    const std::function<void()> mFunc;
    std::mutex &mCallInEventLoopMutex;
    std::condition_variable &mCallInEventLoopCondVar;
};

class MessageQueue
{
public:
    MessageQueue() : mRunning(false) {}

    virtual ~MessageQueue() { stop(); }

    void start()
    {
        if (mRunning)
        {
            // queue is running
            return;
        }
        mRunning = true;
        std::thread startThread(&MessageQueue::processMessages, this);
        mWorkerThread.swap(startThread);
    }

    void stop()
    {
        if (!mRunning)
        {
            // queue is not running
            return;
        }
        callInEventLoop([this]() { mRunning = false; });

        if (mWorkerThread.joinable())
            mWorkerThread.join();

        clear();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mQueue.clear();
    }

    // Wait for a message to appear on the queue.
    std::shared_ptr<Message> waitForMessage()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        while (mQueue.empty())
        {
            mCondVar.wait(lock);
        }
        std::shared_ptr<Message> message = mQueue.front();
        mQueue.pop_front();
        return message;
    }

    // Posts a message to the queue.
    bool postMessage(const std::shared_ptr<Message> &msg)
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        if (!mRunning)
        {
            GST_ERROR("Message queue is not running");
            return false;
        }
        mQueue.push_back(msg);
        mCondVar.notify_all();

        return true;
    }

    void processMessages()
    {
        do
        {
            std::shared_ptr<Message> message = waitForMessage();
            message->handle();
        } while (mRunning);
    }

    bool callInEventLoopImpl(const std::function<void()> &func)
    {
        if (std::this_thread::get_id() != mWorkerThread.get_id())
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
    std::condition_variable mCondVar;
    std::mutex mMutex;
    std::deque<std::shared_ptr<Message>> mQueue;
    std::thread mWorkerThread;
    bool mRunning;
};

class SetPositionMessage : public Message
{
public:
    SetPositionMessage(int64_t newPosition, int64_t &targetPosition)
        : mNewPosition(newPosition), mTargetPosition(targetPosition)
    {
    }
    void handle() { mTargetPosition = mNewPosition; }

private:
    int64_t mNewPosition;
    int64_t &mTargetPosition;
};

class SetDurationMessage : public Message
{
public:
    SetDurationMessage(int64_t newDuration, int64_t &targetDuration)
        : mNewDuration(newDuration), mTargetDuration(targetDuration)
    {
    }
    void handle() { mTargetDuration = mNewDuration; }

private:
    int64_t mNewDuration;
    int64_t &mTargetDuration;
};
