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

#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>

#include "common/log.h"
#include "mac/mac.h"
#include "mac/pcap.h"


namespace srsue {

mac::mac() : ttisync(10240), 
             timers_db((uint32_t) NOF_MAC_TIMERS), 
             pdu_process_thread(&demux_unit)
{
  started = false;  
  pcap    = NULL;   
  signals_pregenerated = false; 
}
  
bool mac::init(phy_interface *phy, rlc_interface_mac *rlc, rrc_interface_phymac *rrc, srslte::log *log_h_)
{
  started = false; 
  phy_h = phy;
  rlc_h = rlc;   
  log_h = log_h_; 
  tti = 0; 
  is_synchronized = false;   
  last_temporal_crnti = 0; 
  phy_rnti = 0; 
  
  srslte_softbuffer_rx_init(&pch_softbuffer, 100);
  
  bsr_procedure.init(       rlc_h, log_h, &params_db, &timers_db);
  phr_procedure.init(phy_h,        log_h, &params_db, &timers_db);
  mux_unit.init     (       rlc_h, log_h,                         &bsr_procedure, &phr_procedure);
  demux_unit.init   (phy_h, rlc_h, log_h,             &timers_db);
  ra_procedure.init (phy_h,        log_h, &params_db, &timers_db, &mux_unit, &demux_unit);
  sr_procedure.init (phy_h, rrc,   log_h, &params_db);
  ul_harq.init      (              log_h, &params_db, &timers_db, &mux_unit);
  dl_harq.init      (              log_h, &params_db, &timers_db, &demux_unit);

  reset();
  
  started = true; 
  start(MAC_MAIN_THREAD_PRIO);
  
  
  return started; 
}

void mac::stop()
{
  started = false;   
  ttisync.increase();
  upper_timers_thread.stop();
  pdu_process_thread.stop();
  wait_thread_finish();
}

void mac::start_pcap(mac_pcap* pcap_)
{
  pcap = pcap_; 
  dl_harq.start_pcap(pcap);
  ul_harq.start_pcap(pcap);
  ra_procedure.start_pcap(pcap);
}

// Implement Section 5.8
void mac::reconfiguration()
{

}

// Implement Section 5.9
void mac::reset()
{
  bzero(&metrics, sizeof(mac_metrics_t));
 
  Info("Resetting MAC\n");
  
  timers_db.stop_all();
  upper_timers_thread.reset();
  
  timeAlignmentTimerExpire(); 
  ul_harq.reset_ndi();
  
  mux_unit.msg3_flush();
  mux_unit.reset();
  
  ra_procedure.stop();
  ra_procedure.reset();    
  sr_procedure.stop();
  sr_procedure.reset();
  bsr_procedure.stop();
  bsr_procedure.reset();
  phr_procedure.stop();
  phr_procedure.reset();
  
  dl_harq.reset();
  phy_h->pdcch_dl_search_reset();
  phy_h->pdcch_ul_search_reset();
  
  signals_pregenerated = false; 
  is_first_ul_grant = true; 
  
  params_db.set_param(mac_interface_params::SR_PUCCH_CONFIGURED, 0);
}

void mac::run_thread() {
  setup_timers();

  Info("Waiting PHY to synchronize with cell\n");  
  phy_h->sync_start();
  while(!phy_h->get_current_tti() && started) {
    usleep(50000);
  }
  Debug("Setting ttysync to %d\n", phy_h->get_current_tti());
  ttisync.set_producer_cntr(phy_h->get_current_tti());
     
  while(started) {

    /* Warning: Here order of invocation of procedures is important!! */
    tti = ttisync.wait();
    
    if (started) {
      log_h->step(tti);
        
      // Step all procedures 
      bsr_procedure.step(tti);
      phr_procedure.step(tti);
      
      // Check if BSR procedure need to start SR 
      
      if (bsr_procedure.need_to_send_sr()) {
        Debug("Starting SR procedure by BSR request, PHY TTI=%d\n", phy_h->get_current_tti());
        sr_procedure.start();
      }
      if (bsr_procedure.need_to_reset_sr()) {
        Debug("Resetting SR procedure by BSR request\n");
        sr_procedure.reset();
      }
      sr_procedure.step(tti);

      // Check SR if we need to start RA 
      if (sr_procedure.need_random_access()) {
        ra_procedure.start_mac_order();
      }
      ra_procedure.step(tti);

      if (ra_procedure.is_successful() && !signals_pregenerated) {
        // Configure PHY to look for UL C-RNTI grants
        uint16_t crnti = params_db.get_param(mac_interface_params::RNTI_C);
        phy_h->pdcch_ul_search(SRSLTE_RNTI_USER, crnti);
        phy_h->pdcch_dl_search(SRSLTE_RNTI_USER, crnti);
        
        // Pregenerate UL signals and C-RNTI scrambling sequences
        Info("Pre-computing C-RNTI scrambling sequences for C-RNTI=0x%x\n", crnti);
        ((phy*) phy_h)->set_crnti(crnti);
        signals_pregenerated = true; 
      }
      
      timers_db.step_all();          
    }
  }  
}

void mac::bcch_start_rx()
{
  bcch_start_rx(tti, -1);
}

void mac::bcch_start_rx(int si_window_start, int si_window_length)
{
  if (si_window_length >= 0 && si_window_start >= 0) {
    dl_harq.set_si_window_length(si_window_length);
    phy_h->pdcch_dl_search(SRSLTE_RNTI_SI, SRSLTE_SIRNTI, si_window_start, si_window_start+si_window_length);
  } else {
    phy_h->pdcch_dl_search(SRSLTE_RNTI_SI, SRSLTE_SIRNTI, si_window_start);
  }
  Info("Searching for DL grant for SI-RNTI window_st=%d, window_len=%d\n", si_window_start, si_window_length);  
}

void mac::bcch_stop_rx()
{
  phy_h->pdcch_dl_search_reset();
}

void mac::pcch_start_rx()
{
  phy_h->pdcch_dl_search(SRSLTE_RNTI_PCH, SRSLTE_PRNTI);
  Info("Searching for DL grant for P-RNTI\n");  
}

void mac::pcch_stop_rx()
{
  phy_h->pdcch_dl_search_reset();
}


void mac::tti_clock(uint32_t tti)
{
  ttisync.increase();
  upper_timers_thread.tti_clock();
}

void mac::bch_decoded_ok(uint8_t* payload, uint32_t len)
{
  // Send MIB to RLC 
  rlc_h->write_pdu_bcch_bch(payload, len);
  
  if (pcap) {
    pcap->write_dl_bch(payload, len, true, phy_h->get_current_tti());
  }
}

void mac::pch_decoded_ok(uint32_t len)
{
  // Send PCH payload to RLC 
  rlc_h->write_pdu_pcch(pch_payload_buffer, len);
  
  if (pcap) {
    pcap->write_dl_pch(pch_payload_buffer, len, true, phy_h->get_current_tti());
  }
}

void mac::harq_recv(uint32_t tti, bool ack, mac_interface_phy::tb_action_ul_t* action)
{
  int tbs = ul_harq.get_current_tbs(tti);
  ul_harq.harq_recv(tti, ack, action);
  if (!ack) {
    metrics.tx_errors++;
  } else {
    metrics.tx_brate += tbs;
  }
}

void mac::new_grant_dl(mac_interface_phy::mac_grant_t grant, mac_interface_phy::tb_action_dl_t* action)
{
  if (grant.rnti_type == SRSLTE_RNTI_RAR) {
    ra_procedure.new_grant_dl(grant, action);
  } else if (grant.rnti_type == SRSLTE_RNTI_PCH) {

    memcpy(&action->phy_grant, &grant.phy_grant, sizeof(srslte_phy_grant_t));
    action->generate_ack = false; 
    action->decode_enabled = true; 
    srslte_softbuffer_rx_reset_cb(&pch_softbuffer, 1);
    action->payload_ptr = pch_payload_buffer;
    action->softbuffer  = &pch_softbuffer;
    action->rnti = grant.rnti;
    action->rv   = grant.rv; 
    if (grant.n_bytes > pch_payload_buffer_sz) {
      Error("Received grant for PCH (%d bytes) exceeds buffer (%d bytes)\n", grant.n_bytes, pch_payload_buffer_sz);
      action->decode_enabled = false; 
    }
  } else {
    // If PDCCH for C-RNTI and RA procedure in Contention Resolution, notify it
    if (grant.rnti_type == SRSLTE_RNTI_USER) {
      if (ra_procedure.is_contention_resolution()) {
        ra_procedure.pdcch_to_crnti(false);
      }
    }
    dl_harq.new_grant_dl(grant, action);
    metrics.rx_pkts++;
  }
}

uint32_t mac::get_current_tti()
{
  return phy_h->get_current_tti();
}

void mac::new_grant_ul(mac_interface_phy::mac_grant_t grant, mac_interface_phy::tb_action_ul_t* action)
{
  /* Start PHR Periodic timer on first UL grant */
  if (is_first_ul_grant) {
    is_first_ul_grant = false; 
    timers_db.get(mac::PHR_TIMER_PERIODIC)->run();
  }
  if (grant.rnti_type == SRSLTE_RNTI_USER) {
    if (ra_procedure.is_contention_resolution()) {
      ra_procedure.pdcch_to_crnti(true);
    }
  }
  ul_harq.new_grant_ul(grant, action);
  metrics.tx_pkts++;
}

void mac::new_grant_ul_ack(mac_interface_phy::mac_grant_t grant, bool ack, mac_interface_phy::tb_action_ul_t* action)
{
  int tbs = ul_harq.get_current_tbs(tti);
  ul_harq.new_grant_ul_ack(grant, ack, action);
  if (!ack) {
    metrics.tx_errors++;
  } else {
    metrics.tx_brate += tbs;
  }
}

void mac::tb_decoded(bool ack, srslte_rnti_type_t rnti_type, uint32_t harq_pid)
{
  if (rnti_type == SRSLTE_RNTI_RAR) {
    if (ack) {
      ra_procedure.tb_decoded_ok();
    }
  } else {
    dl_harq.tb_decoded(ack, rnti_type, harq_pid);
    if (ack) {
      pdu_process_thread.notify();
      metrics.rx_brate += dl_harq.get_current_tbs(harq_pid);
    } else {
      metrics.rx_errors++;
    }
  }
}


void mac::setup_timers()
{
  if (params_db.get_param(mac_interface_params::TIMER_TIMEALIGN) > 0) {
    timers_db.get(TIME_ALIGNMENT)->set(this, params_db.get_param(mac_interface_params::TIMER_TIMEALIGN));
  }
}

void mac::timer_expired(uint32_t timer_id)
{
  switch(timer_id) {
    case TIME_ALIGNMENT:
      timeAlignmentTimerExpire();
      break;
    default: 
      break;
  }
}

/* Function called on expiry of TimeAlignmentTimer */
void mac::timeAlignmentTimerExpire() 
{
  dl_harq.reset();
  ul_harq.reset();
}

void mac::set_param(mac_interface_params::mac_param_t param, int64_t value)
{
  params_db.set_param((uint32_t) param, value);
}

int64_t mac::get_param(mac_interface_params::mac_param_t param)
{
  return params_db.get_param((uint32_t) param);
}

void mac::setup_lcid(uint32_t lcid, uint32_t lcg, uint32_t priority, int PBR_x_tti, uint32_t BSD)
{
  Info("Logical Channel Setup: LCID=%d, LCG=%d, priority=%d, PBR=%d, BSd=%d\n", 
       lcid, lcg, priority, PBR_x_tti, BSD);
  mux_unit.set_priority(lcid, priority, PBR_x_tti, BSD);
  bsr_procedure.setup_lcg(lcid, lcg);
  bsr_procedure.set_priority(lcid, priority);
}

uint32_t mac::get_unique_id()
{
  return upper_timers_thread.get_unique_id();
}

/* Front-end to upper-layer timers */
srslte::timers::timer* mac::get(uint32_t timer_id)
{
  return upper_timers_thread.get(timer_id);
}


void mac::get_metrics(mac_metrics_t &m)
{
  metrics.ul_buffer = (int) bsr_procedure.get_buffer_state();
  m = metrics;  
  bzero(&metrics, sizeof(mac_metrics_t));
}


/********************************************************
 *
 * Class to run upper-layer timers with normal priority 
 *
 *******************************************************/
void mac::upper_timers::run_thread()
{
  running=true; 
  ttisync.set_producer_cntr(0);
  ttisync.resync();
  while(running) {
    ttisync.wait();
    timers_db.step_all();
  }
}
srslte::timers::timer* mac::upper_timers::get(uint32_t timer_id)
{
  return timers_db.get(timer_id%MAC_NOF_UPPER_TIMERS);
}

uint32_t mac::upper_timers::get_unique_id()
{
  return timers_db.get_unique_id();
}

void mac::upper_timers::stop()
{
  running=false;
  ttisync.increase();
  wait_thread_finish();
}
void mac::upper_timers::reset()
{
  timers_db.stop_all();
}

void mac::upper_timers::tti_clock()
{
  ttisync.increase();
}




/********************************************************
 *
 * Class that runs a thread to process DL MAC PDUs from
 * DEMU unit
 *
 *******************************************************/
mac::pdu_process::pdu_process(demux *demux_unit_)
{
  demux_unit = demux_unit_;
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cvar, NULL);
  have_data = false; 
  start(MAC_PDU_THREAD_PRIO);  
}

void mac::pdu_process::stop()
{
  pthread_mutex_lock(&mutex);
  running = false; 
  pthread_cond_signal(&cvar);
  pthread_mutex_unlock(&mutex);
  
  wait_thread_finish();
}

void mac::pdu_process::notify()
{
  pthread_mutex_lock(&mutex);
  have_data = true; 
  pthread_cond_signal(&cvar);
  pthread_mutex_unlock(&mutex);
}

void mac::pdu_process::run_thread()
{
  running = true; 
  while(running) {
    have_data = demux_unit->process_pdus();
    if (!have_data) {
      pthread_mutex_lock(&mutex);
      while(!have_data && running) {
        pthread_cond_wait(&cvar, &mutex);
      }
      pthread_mutex_unlock(&mutex);
    }
  }
}







}



