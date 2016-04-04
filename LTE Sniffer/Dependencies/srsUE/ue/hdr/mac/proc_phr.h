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

#ifndef PROCPHR_H
#define PROCPHR_H

#include <stdint.h>
#include "mac/proc.h"
#include "mac_params.h"
#include "common/timers.h"
#include "common/phy_interface.h"
#include "common/log.h"

/* Power headroom report procedure */


namespace srsue {

class phr_proc : public proc, srslte::timer_callback
{
public:
  phr_proc();  
  void init(phy_interface* phy_h, srslte::log* log_h_, mac_params* params_db_, srslte::timers *timers_db_);
  
  void step(uint32_t tti);
  void reset();
  
  bool generate_phr_on_ul_grant(float *phr);
  void timer_expired(uint32_t timer_id);
  
private:
  srslte::log* log_h;
  mac_params* params_db; 
  phy_interface* phy_h; 
  srslte::timers* timers_db;
  bool initiated;
  int timer_prohibit;
  int timer_periodic;
  int dl_pathloss_change; 
  int cur_pathloss_db;
  bool phr_is_triggered;
};

} // namespace srsue

#endif // PROCPHR_H
