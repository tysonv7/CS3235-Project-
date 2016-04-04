/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

/******************************************************************************
 *  File:         msg_queue.h
 *  Description:  Thread-safe bounded circular buffer of srsue_byte_buffer pointers.
 *  Reference:
 *****************************************************************************/

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include "common/common.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

namespace srsue {

class msg_queue
{
public:
  msg_queue(uint32_t capacity_ = 128)
    :head(0)
    ,tail(0)
    ,unread(0)
    ,unread_bytes(0)
    ,capacity(capacity_)
  {
    buf = new byte_buffer_t*[capacity];
  }

  ~msg_queue()
  {
    delete [] buf;
  }

  void write(byte_buffer_t *msg)
  {
    boost::mutex::scoped_lock lock(mutex);
    while(is_full()) not_full.wait(lock);
    buf[head] = msg;
    head = (head+1)%capacity;
    unread++;
    unread_bytes += msg->N_bytes;
    lock.unlock();
    not_empty.notify_one();
  }

  void read(byte_buffer_t **msg)
  {
    boost::mutex::scoped_lock lock(mutex);
    while(is_empty()) not_empty.wait(lock);
    *msg = buf[tail];
    tail = (tail+1)%capacity;
    unread--;
    unread_bytes -= (*msg)->N_bytes;
    lock.unlock();
    not_full.notify_one();
  }

  bool try_read(byte_buffer_t **msg)
  {
    boost::mutex::scoped_lock lock(mutex);
    if(is_empty())
    {
      return false;
    }else{
      *msg = buf[tail];
      tail = (tail+1)%capacity;
      unread--;
      unread_bytes -= (*msg)->N_bytes;
      lock.unlock();
      not_full.notify_one();
      return true;
    }
  }

  uint32_t size()
  {
    boost::mutex::scoped_lock lock(mutex);
    return unread;
  }

  uint32_t size_bytes()
  {
    boost::mutex::scoped_lock lock(mutex);
    return unread_bytes;
  }

  uint32_t size_tail_bytes()
  {
    boost::mutex::scoped_lock lock(mutex);
    return buf[tail]->N_bytes;
  }

private:
  bool     is_empty() const { return unread == 0; }
  bool     is_full() const { return unread == capacity; }

  boost::condition      not_empty;
  boost::condition      not_full;
  boost::mutex          mutex;
  byte_buffer_t **buf;
  uint32_t              capacity;
  uint32_t              unread;
  uint32_t              unread_bytes;
  uint32_t              head;
  uint32_t              tail;
};

} // namespace srsue


#endif // MSG_QUEUE_H
