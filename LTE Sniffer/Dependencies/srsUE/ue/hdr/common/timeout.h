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
 *  File:         timeout.h
 *  Description:  Millisecond resolution timeouts. Uses a dedicated thread to
 *                call an optional callback function upon timeout expiry.
 *  Reference:
 *****************************************************************************/

#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace srsue {
  
class timeout_callback
{
  public: 
    virtual void timeout_expired(uint32_t timeout_id) = 0;
}; 
  
class timeout
{
public:
  timeout():running(false),callback(NULL){}
  ~timeout()
  {
    if(running && callback)
      pthread_join(thread, NULL);
  }
  void start(int duration_msec_, uint32_t timeout_id_=0,timeout_callback *callback_=NULL)
  {
    if(duration_msec_ < 0)
      return;
    reset();
    stop_time     = boost::posix_time::microsec_clock::local_time() + boost::posix_time::milliseconds(duration_msec_);
    running       = true;
    timeout_id    = timeout_id_;
    callback      = callback_;
    if(callback)
      pthread_create(&thread, NULL, &thread_start, this);
  }
  void reset()
  {
    if(callback)
      pthread_cancel(thread);
    running = false;
  }
  static void* thread_start(void *t_)
  {
    timeout *t = (timeout*)t_;
    t->thread_func();
  }
  void thread_func()
  {
    boost::posix_time::time_duration diff;
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    diff = stop_time - now;
    int32_t usec = diff.total_microseconds();
    if(usec > 0)
      usleep(usec);
    if(callback && running)
        callback->timeout_expired(timeout_id);
  }
  bool expired()
  {
    if(running)
      return boost::posix_time::microsec_clock::local_time() > stop_time;
    else
      return false;
  }
  bool is_running()
  {
    return running;
  }

private:
  boost::posix_time::ptime  stop_time;
  pthread_t                 thread;
  uint32_t                  timeout_id;
  timeout_callback         *callback;
  bool                      running;
};

} // namespace srsue
  
#endif // TIMEOUT_H
