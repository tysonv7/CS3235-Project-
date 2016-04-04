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

#include <string.h>
#include <strings.h>
#include <pthread.h>

#include "srslte/srslte.h"
#include "common/log.h"
#include "phy/prach.h"
#include "phy/phy.h"
#include "common/phy_interface.h"

#define Error(fmt, ...)   if (SRSLTE_DEBUG_ENABLED) log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) if (SRSLTE_DEBUG_ENABLED) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    if (SRSLTE_DEBUG_ENABLED) log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   if (SRSLTE_DEBUG_ENABLED) log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

namespace srsue {
 
  
void prach::free_cell() 
{
  if (initiated) {
    for (int i=0;i<64;i++) {
      if (buffer[i]) {
        free(buffer[i]);    
      }      
    }
    if (signal_buffer) {
      free(signal_buffer);
    }
    srslte_cfo_free(&cfo_h);
    srslte_prach_free(&prach_obj);
  }
}

void prach::init(phy_params* params_db_, srslte::log* log_h_)
{
  log_h = log_h_; 
  params_db = params_db_; 
}

bool prach::init_cell(srslte_cell_t cell_)
{
  cell = cell_; 
  preamble_idx = -1; 

  Info("ConfigIdx=%d, RootSeq=%d, ZC=%d\n", 
       params_db->get_param(phy_interface_params::PRACH_CONFIG_INDEX),     
       params_db->get_param(phy_interface_params::PRACH_ROOT_SEQ_IDX),     
       params_db->get_param(phy_interface_params::PRACH_ZC_CONFIG));
  
  if (srslte_prach_init(&prach_obj, srslte_symbol_sz(cell.nof_prb), 
                        srslte_prach_get_preamble_format(params_db->get_param(phy_interface_params::PRACH_CONFIG_INDEX)), 
                        params_db->get_param(phy_interface_params::PRACH_ROOT_SEQ_IDX), 
                        params_db->get_param(phy_interface_params::PRACH_HIGH_SPEED_FLAG)?true:false, 
                        params_db->get_param(phy_interface_params::PRACH_ZC_CONFIG))) 
  {
    Error("Initiating PRACH library\n");
    return false; 
  }
  
  len = prach_obj.N_seq + prach_obj.N_cp;
  for (int i=0;i<64;i++) {
    buffer[i] = (cf_t*) srslte_vec_malloc(len*sizeof(cf_t));
    if(!buffer[i]) {
      return false; 
    }    
    if(srslte_prach_gen(&prach_obj, i, params_db->get_param(phy_interface_params::PRACH_FREQ_OFFSET), buffer[i])) {
      Error("Generating PRACH preamble %d\n", i);
      return false;
    }
  }
  srslte_cfo_init(&cfo_h, len);
  signal_buffer = (cf_t*) srslte_vec_malloc(len*sizeof(cf_t)); 
  initiated = signal_buffer?true:false; 
  transmitted_tti = -1; 
  Info("PRACH Initiated %s\n", initiated?"OK":"KO");
  return initiated;  
}

bool prach::prepare_to_send(uint32_t preamble_idx_, int allowed_subframe_, float target_power_dbm_)
{
  if (initiated && preamble_idx_ < 64) {
    preamble_idx = preamble_idx_;
    target_power_dbm = target_power_dbm_;
    allowed_subframe = allowed_subframe_; 
    transmitted_tti = -1; 
    Info("PRACH prepare to send preamble %d\n", preamble_idx);
    return true; 
  } else {
    if (!initiated) {
      Error("PRACH not initiated\n");
    } else if (preamble_idx_ >= 64) {
      Error("Invalid preamble %d\n", preamble_idx_);
    }
    return false; 
  }
}

bool prach::is_ready_to_send(uint32_t current_tti_) {
  if (initiated && preamble_idx >= 0 && preamble_idx < 64 && params_db != NULL) {
    // consider the number of subframes the transmission must be anticipated 
    uint32_t current_tti = (current_tti_ + tx_advance_sf)%10240;
    uint32_t config_idx = (uint32_t) params_db->get_param(phy_interface_params::PRACH_CONFIG_INDEX); 
    if (srslte_prach_send_tti(config_idx, current_tti, allowed_subframe)) {
      Info("PRACH Buffer: Ready to send at tti: %d (now is %d)\n", current_tti, current_tti_);
      transmitted_tti = current_tti; 
      return true; 
    }
  }
  return false;     
}

int prach::tx_tti() {
  return transmitted_tti; 
}

float prach::get_p0_preamble()
{
  return target_power_dbm; 
}


bool prach::send(srslte::radio *radio_handler, float cfo, float pathloss, srslte_timestamp_t tx_time)
{
  // Correct CFO before transmission
  srslte_cfo_correct(&cfo_h, buffer[preamble_idx], signal_buffer, cfo /srslte_symbol_sz(cell.nof_prb));            

  // If power control is not disabled, choose amplitude and power 
  if (params_db->get_param(phy_interface_params::PRACH_GAIN) < 0) {
    // Get PRACH transmission power 
    float tx_power = SRSLTE_MIN(SRSLTE_PC_MAX, pathloss + target_power_dbm);
    
    tx_power += (float) params_db->get_param(phy_interface_params::UL_PWR_CTRL_OFFSET);

    // Get output power for amplitude 1
    radio_handler->set_tx_power(tx_power);
        
    // Scale signal
    float digital_power = srslte_vec_avg_power_cf(signal_buffer, len);
    float scale = sqrtf(pow(10,tx_power/10)/digital_power);
    
    srslte_vec_sc_prod_cfc(signal_buffer, scale, signal_buffer, len);
    log_h->console("TX PRACH: Pathloss=%.2f dB, Target power %.2f dBm, TX_power %.2f dBm, TX_gain %.1f dB\n",
          pathloss, target_power_dbm, tx_power, radio_handler->get_tx_gain(), scale);
    
  } else {
    radio_handler->set_tx_gain((float) params_db->get_param(phy_interface_params::PRACH_GAIN));
    
    Info("TX PRACH: Power control for PRACH is disabled, setting gain to %.0f dB\n", 
      (float) params_db->get_param(phy_interface_params::PRACH_GAIN));
  }
    
  radio_handler->tx(signal_buffer, len, tx_time);
  radio_handler->tx_end();
  
  Debug("PRACH transmitted CFO: %f, preamble=%d, len=%d tx_time=%f\n", 
       cfo*15000, preamble_idx, len, tx_time.frac_secs);
  preamble_idx = -1; 

  // Set UL gain if power control for the rest of the channels is disabled
  if (params_db->get_param(phy_interface_params::UL_GAIN) > 0) {
    radio_handler->set_tx_gain((float) params_db->get_param(phy_interface_params::UL_GAIN));    
    Info("UL power control is disabled. Fixing TX gain to %.0f dB\n", (float) params_db->get_param(phy_interface_params::UL_GAIN));
  } 
  
}
  
} // namespace srsue

