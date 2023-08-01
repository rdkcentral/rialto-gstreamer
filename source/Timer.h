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

#ifndef GSTTIMER_H
#define GSTTIMER_H

#pragma once

/**
 * @file Timer.h
 *
 * The definition of the Timer class.
 *
 * This file comprises the definition of the Timer class. This class
 * represents a single shot millisecond timer.
 */

#include <exception>
#include <string>

#include <deque>

#include <pthread.h>

/**
 * @brief Exception thrown when there is an exception in a timer
 */
class TimerException : public std::exception
{
public:
    /**
     * @brief The constructor
     *
     * @param[in] message : The exception message.
     */
    explicit TimerException(const std::string &message) : exception(), m_message(message){};

    /**
     * @brief The copy constructor
     *
     * @param[in] other : The exception to copy.
     */
    TimerException(const TimerException &other) : exception(), m_message(other.m_message){};

    /**
     * @brief The virtual destructor.
     */
    virtual ~TimerException() throw(){};

    /**
     * @brief Returns the explanatory string.
     */
    virtual const char *what() { return m_message.c_str(); };

    /**
     * @brief The copy operator.
     *
     * @param[in] other : The exception to copy.
     */
    TimerException &operator=(const TimerException &other)
    {
        m_message = other.m_message;
        return *this;
    }

private:
    /**
     * @brief The exception message
     */
    std::string m_message;
};

/**
 * @brief The timer callback function type.
 *
 * @param[in] data : User data set when the timer was created.
 */
typedef void (*TimerCallback)(void *data);

/**
 * @brief A simple timer based around pthreads.
 */
class Timer
{
public:
    /**
     * @brief Default constructor.
     *
     * @param[in] callbackFunction : The callback function.
     * @param[in] userData         : The user data.
     */
    Timer(TimerCallback callbackFunction, void *userData, const char *name);

    /**
     * @brief virtual destructor
     */
    virtual ~Timer();

    /**
     * @brief Cancels the timer.
     *
     * This method cancels any currently armed timer. The callback
     * function will not be called at any point after this method is
     * called.
     */
    void cancel();

    /**
     * @brief Arms the timer.
     *
     * This method arms the timer. After the specified period the
     * registered callback function will be called with the opaque user
     * data provided in the timer's constructor.
     *
     * This method MAY be called within the callback function. It is
     * legal to re-arm a timer without cancelling it.
     *
     * @param[in] timeout : the wait time in ms
     */
    void arm(long timeout);

protected:
    /**
     * @brief A message for the timer task message queue.
     */
    class TimerMessage
    {
    public:
        /**
         * @brief message types.
         */
        enum MessageType
        {
            TIMER_MSG_UNKNOWN, /**< Message type is unknown or undefined. */
            TIMER_MSG_ARM,     /**< A message to arm the timer.           */
            TIMER_MSG_CANCEL,  /**< A message to cancel the timer.        */
            TIMER_MSG_QUIT     /**< A message to quit the timer task.     */
        };

        /**
         * @brief Creates and returns a new ARM message.
         *
         * @param[in] timeout : the timeout in milliseconds.
         */
        static TimerMessage *createArmMessage(long timeout) { return new TimerMessage(TIMER_MSG_ARM, timeout); }

        /**
         * @brief Creates and returns a new CANCEL message.
         */
        static TimerMessage *createCancelMessage() { return new TimerMessage(TIMER_MSG_CANCEL); }

        /**
         * @brief Creates and returns a new QUIT message.
         */
        static TimerMessage *createQuitMessage() { return new TimerMessage(TIMER_MSG_QUIT); }

        /**
         * @brief Virtual destructor.
         */
        virtual ~TimerMessage(){};

        /**
         * @brief Returns the message type.
         *
         * @retval the message type.
         */
        MessageType getType() { return m_type; };

        /**
         * @brief Returns the message timeout value.
         *
         * @retval the message timeout value.
         */
        long getTimeout() { return m_timeout; };

    protected:
        /**
         * @brief The message type.
         */
        MessageType m_type;

        /**
         * @brief The timeout.
         */
        long m_timeout;

        /**
         * @brief Constructor.
         *
         * @param[in] type    : The message type.
         * @param[in] timeout : The timeout (default -1)
         */
        TimerMessage(MessageType type = TIMER_MSG_UNKNOWN, long timeout = -1) : m_type(type), m_timeout(timeout){};
    };

    /**
     * @brief The callback function
     */
    TimerCallback m_callbackFunction;

    /**
     * @brief The user data
     */
    void *m_userData;

    /**
     * @brief The message queue protection mutex.
     */
    pthread_mutex_t m_mutex;

    /**
     * @brief The message queue protection mutex attributes.
     */
    pthread_mutexattr_t m_mutexAttr;

    /**
     * @brief The message queue condition variable.
     */
    pthread_cond_t m_cond;

    /**
     * @brief The message queue condition variable attibutes.
     */
    pthread_condattr_t m_condAttr;

    /**
     * @brief The message queue.
     */
    std::deque<TimerMessage *> m_msgQueue;

    /**
     * @brief The timer thread.
     */
    pthread_t m_timerThread;

    /**
     * @brief The timer thread static function.
     *
     * @param[in] self : A void 'this' pointer.
     *
     * @retval not NULL if exit without error.
     */
    static void *timerThreadFunction(void *self);

    /**
     * @brief The timer thread function.
     *
     * @retval true if exit without error.
     */
    bool timerThread();

    /**
     * @brief Posts a timer message to the timer thread.
     *
     * This method posts a timer message to the timer thread. Set urgent to
     * true to post a message to the front of the queue. I.e. ensure it is
     * the next message handled.
     *
     * @param[in] message : The timer message.
     * @param[in] urgent  : The urgency flag
     */
    void postTimerMessage(TimerMessage *message, bool urgent = false);

    /**
     * @brief Timer name.
     */
    std::string m_name;
};

#endif // GSTTIMER_H
