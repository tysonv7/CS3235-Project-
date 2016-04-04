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

#include <string>
#include <sstream>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#include "srslte/srslte.h"

#include "common/threads.h"
#include "common/log.h"
#include "phy/phy.h"
#include "phy/phch_worker.h"

#define Error(fmt, ...)   if (SRSLTE_DEBUG_ENABLED) log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) if (SRSLTE_DEBUG_ENABLED) log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    if (SRSLTE_DEBUG_ENABLED) log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   if (SRSLTE_DEBUG_ENABLED) log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)



using namespace std; 


namespace srsue {

phy::phy() : workers_pool(MAX_WORKERS), 
             workers(MAX_WORKERS), 
             workers_common(phch_recv::MUTEX_X_WORKER*MAX_WORKERS)
{
}

bool phy::init(srslte::radio* radio_handler_, mac_interface_phy *mac, rrc_interface_phymac *rrc, 
               srslte::log *log_h_, uint32_t nof_workers_)
{

  mlockall(MCL_CURRENT | MCL_FUTURE);
  
  log_h = log_h_; 
  radio_handler = radio_handler_;
  nof_workers = nof_workers_; 
  
  // Add workers to workers pool and start threads
  for (int i=0;i<nof_workers;i++) {
    workers[i].set_common(&workers_common);
    workers_pool.init_worker(i, &workers[i], WORKERS_THREAD_PRIO);    
  }

  prach_buffer.init(&params_db, log_h);
  workers_common.init(&params_db, log_h, radio_handler, mac);
  
  // Warning this must be initialized after all workers have been added to the pool
  sf_recv.init(radio_handler, mac, rrc, &prach_buffer, &workers_pool, &workers_common, log_h, SF_RECV_THREAD_PRIO);

  // Enable UL signal pregeneration by default
  enable_pregen_signals(true);

  return true; 
}

void phy::set_agc_enable(bool enabled)
{
  sf_recv.set_agc_enable(enabled);
}

void phy::start_trace()
{
  for (int i=0;i<nof_workers;i++) {
    workers[i].start_trace();
  }
  printf("trace started\n");
}

void phy::write_trace(std::string filename)
{
  for (int i=0;i<nof_workers;i++) {
    string i_str = static_cast<ostringstream*>( &(ostringstream() << i) )->str();
    workers[i].write_trace(filename + "_" + i_str);
  }
}

void phy::stop()
{  
  sf_recv.stop();
  workers_pool.stop();
}

void phy::get_metrics(phy_metrics_t &m) {
  workers_common.get_dl_metrics(m.dl);
  workers_common.get_ul_metrics(m.ul);
  workers_common.get_sync_metrics(m.sync);
  m.mabr = srslte_ra_tbs_from_idx(srslte_ra_tbs_idx_from_mcs(m.dl.mcs), workers_common.get_nof_prb());

  // Estimate IP-layer MABR as 75% of MAC-layer MABR
  m.mabr = m.mabr*4/5;
}

void phy::set_timeadv_rar(uint32_t ta_cmd) {
  n_ta = srslte_N_ta_new_rar(ta_cmd);
  sf_recv.set_time_adv_sec(((float) n_ta)*SRSLTE_LTE_TS);
  Info("Set TA RAR: ta_cmd: %d, n_ta: %d, ta_usec: %.1f\n", ta_cmd, n_ta, ((float) n_ta)*SRSLTE_LTE_TS*1e6);
}

void phy::set_timeadv(uint32_t ta_cmd) {
  n_ta = srslte_N_ta_new(n_ta, ta_cmd);
  sf_recv.set_time_adv_sec(((float) n_ta)*SRSLTE_LTE_TS);  
  Info("Set TA: ta_cmd: %d, n_ta: %d, ta_usec: %.1f\n", ta_cmd, n_ta, ((float) n_ta)*SRSLTE_LTE_TS*1e6);
}

void phy::set_param(phy_interface_params::phy_param_t param, int64_t value) {
  params_db.set_param((uint32_t) param, value);
}

int64_t phy::get_param(phy_interface_params::phy_param_t param) {
  return params_db.get_param((uint32_t) param);
}

void phy::configure_prach_params()
{
  if (sf_recv.status_is_sync()) {
    Info("Configuring PRACH parameters\n");
    srslte_cell_t cell; 
    sf_recv.get_current_cell(&cell);
    if (!prach_buffer.init_cell(cell)) {
      Error("Configuring PRACH parameters\n");
    } else {
      Info("Done\n");
    }
  } else {
    Error("Cell is not synchronized\n");
  }
}

void phy::configure_ul_params()
{
  Info("Configuring UL parameters\n");
  for (int i=0;i<nof_workers;i++) {
    workers[i].set_ul_params();
  }
}

float phy::get_phr()
{
  float phr = radio_handler->get_max_tx_power() - workers_common.cur_pusch_power; 
  return phr; 
}

void phy::pdcch_ul_search(srslte_rnti_type_t rnti_type, uint16_t rnti, int tti_start, int tti_end)
{
  workers_common.set_ul_rnti(rnti_type, rnti, tti_start, tti_end);
}

void phy::pdcch_dl_search(srslte_rnti_type_t rnti_type, uint16_t rnti, int tti_start, int tti_end)
{
  workers_common.set_dl_rnti(rnti_type, rnti, tti_start, tti_end);
}

void phy::pdcch_dl_search_reset()
{
  workers_common.set_dl_rnti(SRSLTE_RNTI_USER, 0);
}

void phy::pdcch_ul_search_reset()
{
  workers_common.set_ul_rnti(SRSLTE_RNTI_USER, 0);
}

void phy::get_current_cell(srslte_cell_t *cell)
{
  sf_recv.get_current_cell(cell);
}

void phy::prach_send(uint32_t preamble_idx, int allowed_subframe, float target_power_dbm)
{
  
  if (!prach_buffer.prepare_to_send(preamble_idx, allowed_subframe, target_power_dbm)) {
    Error("Preparing PRACH to send\n");
  }
}

int phy::prach_tx_tti()
{
  return prach_buffer.tx_tti();
}

void phy::reset()
{
  // TODO 
}

uint32_t phy::get_current_tti()
{
  return sf_recv.get_current_tti();
}

void phy::sr_send()
{
  workers_common.sr_enabled = true;
  workers_common.sr_last_tx_tti = -1;
}

int phy::sr_last_tx_tti()
{
  return workers_common.sr_last_tx_tti;
}

bool phy::status_is_sync()
{
  return sf_recv.status_is_sync();
}

void phy::sync_start()
{
  sf_recv.sync_start();
}

void phy::sync_stop()
{
  sf_recv.sync_stop();
}

void phy::set_rar_grant(uint32_t tti, uint8_t grant_payload[SRSLTE_RAR_GRANT_LEN])
{
  workers_common.set_rar_grant(tti, grant_payload);
}

void phy::set_crnti(uint16_t rnti) {
  for(uint32_t i=0;i<nof_workers;i++) {
    workers[i].set_crnti(rnti);
  }    
}

void phy::enable_pregen_signals(bool enable)
{
  for(uint32_t i=0;i<nof_workers;i++) {
    workers[i].enable_pregen_signals(enable);
  }
}


uint32_t phy::tti_to_SFN(uint32_t tti) {
  return tti/10; 
}

uint32_t phy::tti_to_subf(uint32_t tti) {
  return tti%10; 
}

  
}
