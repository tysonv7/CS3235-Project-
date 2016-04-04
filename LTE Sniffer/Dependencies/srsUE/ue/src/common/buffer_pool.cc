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


#include "common/buffer_pool.h"
#include <stdio.h>

namespace srsue{

buffer_pool* buffer_pool::instance = NULL;
boost::mutex buffer_pool::instance_mutex;

buffer_pool* buffer_pool::get_instance(void)
{
  boost::lock_guard<boost::mutex> lock(instance_mutex);
  if(NULL == instance)
    instance = new buffer_pool();
  return instance;
}

void buffer_pool::cleanup(void)
{
  boost::lock_guard<boost::mutex> lock(instance_mutex);
  if(NULL != instance)
  {
    delete instance;
    instance = NULL;
  }
}

buffer_pool::buffer_pool()
{
  pool = new byte_buffer_t[POOL_SIZE];
  first_available = &pool[0];
  for(int i=0;i<POOL_SIZE-1;i++)
  {
    pool[i].set_next(&pool[i+1]);
  }
  pool[POOL_SIZE-1].set_next(NULL);
  allocated = 0;
}

byte_buffer_t* buffer_pool::allocate()
{
  boost::lock_guard<boost::mutex> lock(mutex);

  if(first_available == NULL)
  {
    printf("Error - buffer pool is empty");
    return NULL;
  }

  // Remove from available list
  byte_buffer_t* b = first_available;
  first_available = b->get_next();
  allocated++;

  return b;
}

void buffer_pool::deallocate(byte_buffer_t *b)
{
  boost::lock_guard<boost::mutex> lock(mutex);

  // Add to front of available list
  b->reset();
  b->set_next(first_available);
  first_available = b;
  allocated--;
}



} // namespace srsue
