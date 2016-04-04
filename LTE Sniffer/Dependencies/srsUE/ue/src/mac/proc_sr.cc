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

#define Error(fmt, ...)   log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#include "mac/proc_sr.h"
#include "mac/mac_params.h"



namespace srsue {

sr_proc::sr_proc() {
  initiated = false; 
}
  
void sr_proc::init(phy_interface* phy_h_, rrc_interface_phymac *rrc_, srslte::log* log_h_, mac_params* params_db_)
{
  log_h     = log_h_;
  rrc       = rrc_; 
  params_db = params_db_; 
  phy_h     = phy_h_;
  initiated = true; 
  do_ra = false; 
}
  
void sr_proc::reset()
{
  is_pending_sr = false;
}

void sr_proc::step(uint32_t tti)
{
  if (initiated) {
    if (is_pending_sr) {
      if (params_db->get_param(mac_interface_params::SR_PUCCH_CONFIGURED)) {
        if (sr_counter < dsr_transmax) {
          int last_tx_tti = phy_h->sr_last_tx_tti(); 
          if (last_tx_tti >= 0 && last_tx_tti + 4 < tti || sr_counter == 0) {
            sr_counter++;
            Info("SR signalling PHY. sr_counter=%d, PHY TTI=%d\n", sr_counter, phy_h->get_current_tti());
            phy_h->sr_send();
          }
        } else {
          Info("Releasing PUCCH/SRS resources, sr_counter=%d, dsr_transmax=%d\n", sr_counter, dsr_transmax);
          log_h->console("Scheduling request failed: releasing RRC connection...\n");
          rrc->connection_release();
          reset(); 
        }
      } else {
        do_ra = true; 
        reset();
      }
    }
  }
}

bool sr_proc::need_random_access() {
  if (initiated) {
    if (do_ra) {
      do_ra = false; 
      return true; 
    } else {
      return false; 
    }
  }
}

void sr_proc::start()
{
  if (initiated) {
    if (!is_pending_sr) {
      sr_counter = 0;
      is_pending_sr = true; 
    }
    dsr_transmax = params_db->get_param(mac_interface_params::SR_TRANS_MAX);
    Info("SR starting dsrTransMax=%d. sr_counter=%d\n", dsr_transmax, sr_counter);
  }
}

}

