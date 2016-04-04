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
 *  File:         params_db.h
 *  Description:  Generic database of parameters
 *  Reference:
 *****************************************************************************/

#ifndef PARAMS_H
#define PARAMS_H

#include <stdlib.h>

namespace srsue {

class params_db
{
public:
params_db(uint32_t nof_params_) {
  nof_params = nof_params_;
  db = (int64_t*) calloc(sizeof(int64_t), nof_params);
  for (int i=0;i<nof_params;i++) {
    db[i] = 0;
  }
}
~params_db() {
  free(db);
}
void    set_param(uint32_t param_idx, int64_t value) {
  if (param_idx < nof_params) {
    db[param_idx] = value;
  }
}
int64_t get_param(uint32_t param_idx) {
  if (param_idx < nof_params) {
    return db[param_idx];
  } else {
    return -1;
  }
}

private:
uint32_t nof_params;
int64_t *db;
};

} // namespace srsue


#endif // PARAMS_H
