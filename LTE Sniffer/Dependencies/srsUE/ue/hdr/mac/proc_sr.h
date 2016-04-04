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

#ifndef PROCSR_H
#define PROCSR_H

#include <stdint.h>

#include "mac/proc.h"
#include "phy/phy.h"
#include "mac/mac_params.h"

/* Scheduling Request procedure as defined in 5.4.4 of 36.321 */


namespace srsue {

class sr_proc : public proc
{
public:
  sr_proc();
  void init(phy_interface *phy_h, rrc_interface_phymac *rrc, srslte::log *log_h, mac_params *params_db);
  void step(uint32_t tti);  
  void reset();
  void start();
  bool need_random_access(); 
  
private:
  uint32_t      sr_counter;
  uint32_t      dsr_transmax; 
  bool          is_pending_sr;
  mac_params    *params_db; 
  
  rrc_interface_phymac *rrc;
  phy_interface *phy_h; 
  srslte::log   *log_h;
  bool          initiated;
  bool          do_ra;
};

} // namespace srsue

#endif // PROCSR_H
