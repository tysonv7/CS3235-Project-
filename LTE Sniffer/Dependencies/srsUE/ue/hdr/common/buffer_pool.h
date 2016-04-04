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

#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include "common/common.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

namespace srsue{

/******************************************************************************
 * Buffer pool
 *
 * Preallocates a large number of srsue_byte_buffer_t and provides allocate and
 * deallocate functions. Provides quick object creation and deletion as well
 * as object reuse. Uses a linked list to keep track of available buffers.
 * Singleton class - only one exists for the UE.
 *****************************************************************************/
class buffer_pool{
public:
  // Singleton
  static buffer_pool   *instance;

  static buffer_pool*   get_instance(void);
  static void           cleanup(void);

  byte_buffer_t*        allocate();
  void                  deallocate(byte_buffer_t *b);

private:
  buffer_pool();
  ~buffer_pool(){ delete [] pool; }
  buffer_pool(buffer_pool const&);    // Disabled
  void operator=(buffer_pool const&); // Disabled

  static const int      POOL_SIZE = 2048;
  byte_buffer_t        *pool;
  byte_buffer_t        *first_available;
  boost::mutex          mutex;
  static boost::mutex   instance_mutex;
  int                   allocated;
};


} // namespace srsue

#endif // BUFFER_POOL_H
