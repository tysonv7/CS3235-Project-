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

#include "mac/proc_phr.h"
#include "mac/mac.h"
#include "mac/mux.h"
#include "common/phy_interface.h"


  namespace srsue {
    
phr_proc::phr_proc()
{
  initiated = false; 
  timer_periodic = -2; 
  timer_prohibit = -2;
  dl_pathloss_change = -2; 
  phr_is_triggered = false; 
}

void phr_proc::init(phy_interface* phy_h_, srslte::log* log_h_, mac_params* params_db_, srslte::timers *timers_db_)
{
  phy_h     = phy_h_;
  log_h     = log_h_; 
  params_db = params_db_;
  timers_db = timers_db_; 
  initiated = true;
}

void phr_proc::reset()
{
  phr_is_triggered = false; 
}

/* Trigger PHR when timers exire */
void phr_proc::timer_expired(uint32_t timer_id) {
  switch(timer_id) {
    case mac::PHR_TIMER_PERIODIC:
      phr_is_triggered = true; 
      break;
    case mac::PHR_TIMER_PROHIBIT:
      if (abs(params_db->get_param(mac_interface_params::PHR_PATHLOSS_DB) - cur_pathloss_db) > 
              params_db->get_param(mac_interface_params::PHR_DL_PATHLOSS_CHANGE)) 
      {
        phr_is_triggered = true; 
      }
      break;      
  }
}

void phr_proc::step(uint32_t tti)
{
  if (!initiated) {
    return;
  }  

  // Setup timers and trigger PHR when configuration changed by higher layers
  if (timer_periodic != params_db->get_param(mac_interface_params::PHR_TIMER_PERIODIC) &&
      params_db->get_param(mac_interface_params::PHR_TIMER_PERIODIC)                   && 
      params_db->get_param(mac_interface_params::PHR_TIMER_PERIODIC) != -1) 
  {
    timer_periodic = params_db->get_param(mac_interface_params::PHR_TIMER_PERIODIC); 
    timers_db->get(mac::PHR_TIMER_PERIODIC)->set(this, params_db->get_param(mac_interface_params::PHR_TIMER_PERIODIC));
    phr_is_triggered = true; 
  }

  if (timer_prohibit != params_db->get_param(mac_interface_params::PHR_TIMER_PROHIBIT) &&
      params_db->get_param(mac_interface_params::PHR_TIMER_PROHIBIT)                   && 
      params_db->get_param(mac_interface_params::PHR_TIMER_PROHIBIT) != -1) 
  {
    timer_prohibit = params_db->get_param(mac_interface_params::PHR_TIMER_PROHIBIT); 
    timers_db->get(mac::PHR_TIMER_PROHIBIT)->set(this, params_db->get_param(mac_interface_params::PHR_TIMER_PROHIBIT));
    phr_is_triggered = true; 
  }  
  if (abs(params_db->get_param(mac_interface_params::PHR_PATHLOSS_DB) - cur_pathloss_db) > 
          params_db->get_param(mac_interface_params::PHR_DL_PATHLOSS_CHANGE) && 
          timers_db->get(mac::PHR_TIMER_PROHIBIT)->is_expired()) 
  {
   phr_is_triggered = true;        
  }
}

bool phr_proc::generate_phr_on_ul_grant(float *phr) 
{
  
  if (phr_is_triggered) {
    if (phr) {
      *phr = phy_h->get_phr();
    }
    
    timers_db->get(mac::PHR_TIMER_PERIODIC)->reset();
    timers_db->get(mac::PHR_TIMER_PROHIBIT)->reset();
    timers_db->get(mac::PHR_TIMER_PERIODIC)->run();
    timers_db->get(mac::PHR_TIMER_PROHIBIT)->run();
    
    phr_is_triggered = false; 
    
    return true; 
  } else {
    return false; 
  }
}

}
