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


#include <stdio.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include "common/timeout.h"

using namespace srsue;

class callback
    : public timeout_callback
{
public:
  callback(){}
  void timeout_expired(uint32_t timeout_id)
  {
    boost::mutex::scoped_lock lock(mut);
    end_time = boost::posix_time::microsec_clock::local_time();
    finished = true;
    cond.notify_one();
  }
  void wait()
  {
    boost::mutex::scoped_lock lock(mut);
    while(!finished) cond.wait(lock);
  }
  boost::posix_time::ptime start_time, end_time;
private:
  bool              finished;
  boost::condition  cond;
  boost::mutex      mut;
};

int main(int argc, char **argv) {
  bool result;
  boost::posix_time::ptime  start_time, end_time;
  uint32_t id       = 0;
  uint32_t duration_msec = 5;

  callback c;
  timeout t;

  c.start_time = boost::posix_time::microsec_clock::local_time();
  t.start(0, duration_msec, &c);
  c.wait();

  boost::posix_time::time_duration diff = c.end_time - c.start_time;
  uint32_t diff_ms = diff.total_milliseconds();
  printf("Target duration: %dms, started: %s, ended: %s, actual duration %dms\n",
         duration_msec,
         std::string(boost::posix_time::to_simple_string(c.start_time),12,26).c_str(),
         std::string(boost::posix_time::to_simple_string(c.end_time),12,26).c_str(),
         diff_ms);

  result = (diff_ms == duration_msec);

  if(result) {
    printf("Passed\n");
    exit(0);
  }else{
    printf("Failed\n;");
    exit(1);
  }
}
