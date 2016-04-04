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

#ifndef UEPRACH_H
#define UEPRACH_H

#include "srslte/srslte.h"
#include "radio/radio.h"
#include "common/log.h"
#include "common/phy_interface.h"
#include "phy/phy_params.h"

namespace srsue {

  class prach {
  public: 
    prach() {
      params_db = NULL; 
      initiated = false; 
      signal_buffer = NULL; 
    }
    void           init(phy_params *params_db, srslte::log *log_h);
    bool           init_cell(srslte_cell_t cell);
    void           free_cell();
    bool           prepare_to_send(uint32_t preamble_idx, int allowed_subframe = -1, float target_power_dbm = -1);
    bool           is_ready_to_send(uint32_t current_tti);
    int            tx_tti();
    
    bool           send(srslte::radio* radio_handler, float cfo, float pathloss, srslte_timestamp_t rx_time);
    float          get_p0_preamble();
    
  private: 
    static const uint32_t tx_advance_sf = 4; // Number of subframes to advance transmission
    phy_params    *params_db; 
    srslte::log   *log_h;
    int            preamble_idx;  
    int            allowed_subframe; 
    bool           initiated;   
    uint32_t       len; 
    cf_t          *buffer[64]; 
    srslte_prach_t prach_obj; 
    int            transmitted_tti;
    srslte_cell_t  cell;
    cf_t          *signal_buffer;
    srslte_cfo_t   cfo_h; 
    float target_power_dbm;
    
  };

} // namespace srsue

#endif // UEPRACH_H
