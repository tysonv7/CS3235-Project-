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

#include <unistd.h>
#include <string.h>
#include "phy/phch_worker.h"
#include "common/mac_interface.h"
#include "common/phy_interface.h"

#define Error(fmt, ...)   if (SRSLTE_DEBUG_ENABLED) phy->log_h->error_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warning(fmt, ...) if (SRSLTE_DEBUG_ENABLED) phy->log_h->warning_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...)    if (SRSLTE_DEBUG_ENABLED) phy->log_h->info_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Debug(fmt, ...)   if (SRSLTE_DEBUG_ENABLED) phy->log_h->debug_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)


namespace srsue {


phch_worker::phch_worker() : tr_exec(10240)
{
  phy = NULL; 
  signal_buffer = NULL; 
  
  cell_initiated  = false; 
  pregen_enabled  = false; 
  rar_cqi_request = false; 
  rnti_is_set     = false; 
  trace_enabled   = false; 
  cfi = 0;
  
  bzero(&dl_metrics, sizeof(dl_metrics_t));
  bzero(&ul_metrics, sizeof(ul_metrics_t));
  reset_ul_params();
  
}

void phch_worker::set_common(phch_common* phy_)
{
  phy = phy_;   
}
    
bool phch_worker::init_cell(srslte_cell_t cell_)
{
  memcpy(&cell, &cell_, sizeof(srslte_cell_t));
  
  // ue_sync in phy.cc requires a buffer for 2 subframes 
  signal_buffer = (cf_t*) srslte_vec_malloc(2 * sizeof(cf_t) * SRSLTE_SF_LEN_PRB(cell.nof_prb));
  if (!signal_buffer) {
    Error("Allocating memory\n");
    return false; 
  }
  if (srslte_ue_dl_init(&ue_dl, cell)) {    
    Error("Initiating UE DL\n");
    return false; 
  }
  
  if (srslte_ue_ul_init(&ue_ul, cell)) {  
    Error("Initiating UE UL\n");
    return false; 
  }
  srslte_ue_ul_set_normalization(&ue_ul, true);
  srslte_ue_ul_set_cfo_enable(&ue_ul, true);
  
  /* Set decoder iterations */
  if (phy->params_db->get_param(phy_interface_params::PDSCH_MAX_ITS) > 0) {
    srslte_sch_set_max_noi(&ue_dl.pdsch.dl_sch, phy->params_db->get_param(phy_interface_params::PDSCH_MAX_ITS));
  }
  
  cell_initiated = true; 
  
  return true; 
}

void phch_worker::free_cell()
{
  if (cell_initiated) {
    if (signal_buffer) {
      free(signal_buffer);
    }
    srslte_ue_dl_free(&ue_dl);
    srslte_ue_ul_free(&ue_ul);
  }
}

cf_t* phch_worker::get_buffer()
{
  return signal_buffer; 
}

void phch_worker::set_tti(uint32_t tti_, uint32_t tx_tti_)
{
  tti    = tti_; 
  tx_tti = tx_tti_;
}

void phch_worker::set_cfo(float cfo_)
{
  cfo = cfo_;
}

void phch_worker::set_crnti(uint16_t rnti)
{
  srslte_ue_dl_set_rnti(&ue_dl, rnti);
  srslte_ue_ul_set_rnti(&ue_ul, rnti);
  rnti_is_set = true; 
}

void phch_worker::work_imp()
{
  if (!cell_initiated) {
    return; 
  }
  
  Debug("TTI %d running\n", tti);

#ifdef LOG_EXECTIME
  gettimeofday(&logtime_start[1], NULL);
#endif

  tr_log_start();
  
  reset_uci();

  bool ul_grant_available = false; 
  bool dl_ack = false; 
  
  mac_interface_phy::mac_grant_t    dl_mac_grant;
  mac_interface_phy::tb_action_dl_t dl_action; 
  bzero(&dl_action, sizeof(mac_interface_phy::tb_action_dl_t));

  mac_interface_phy::mac_grant_t    ul_mac_grant;
  mac_interface_phy::tb_action_ul_t ul_action; 
  bzero(&ul_action, sizeof(mac_interface_phy::tb_action_ul_t));

  /* Do FFT and extract PDCCH LLR, or quit if no actions are required in this subframe */
  if (extract_fft_and_pdcch_llr()) {
    
    
    /***** Downlink Processing *******/
    
    /* PDCCH DL + PDSCH */
    if(decode_pdcch_dl(&dl_mac_grant)) {
      /* Send grant to MAC and get action for this TB */
      phy->mac->new_grant_dl(dl_mac_grant, &dl_action);
      
      /* Decode PDSCH if instructed to do so */
      dl_ack = dl_action.default_ack; 
      if (dl_action.decode_enabled) {
        dl_ack = decode_pdsch(&dl_action.phy_grant.dl, dl_action.payload_ptr, 
                              dl_action.softbuffer, dl_action.rv, dl_action.rnti, 
                              dl_mac_grant.pid);              
      }
      if (dl_action.generate_ack_callback && dl_action.decode_enabled) {
        phy->mac->tb_decoded(dl_ack, dl_mac_grant.rnti_type, dl_mac_grant.pid);
        dl_ack = dl_action.generate_ack_callback(dl_action.generate_ack_callback_arg);
        Info("Calling generate ACK callback returned=%d\n", dl_ack);
      }
      if (dl_action.generate_ack) {
        set_uci_ack(dl_ack);
      }
    }

    // Decode PHICH 
    bool ul_ack; 
    bool ul_ack_available = decode_phich(&ul_ack); 
    
    /***** Uplink Processing + Transmission *******/
    
    /* Generate SR if required*/
    set_uci_sr();    
    
    /* Generate periodic CQI reports if required */
    set_uci_periodic_cqi();
    
    
    /* Check if we have UL grant. ul_phy_grant will be overwritten by new grant */
    ul_grant_available = decode_pdcch_ul(&ul_mac_grant);   
    
    /* Send UL grant or HARQ information (from PHICH) to MAC */
    if (ul_grant_available         && ul_ack_available)  {    
      phy->mac->new_grant_ul_ack(ul_mac_grant, ul_ack, &ul_action);      
    } else if (ul_grant_available  && !ul_ack_available) {
      phy->mac->new_grant_ul(ul_mac_grant, &ul_action);
    } else if (!ul_grant_available && ul_ack_available)  {    
      phy->mac->harq_recv(tti, ul_ack, &ul_action);        
    }

    /* Set UL CFO before transmission */  
    srslte_ue_ul_set_cfo(&ue_ul, cfo);
  }
  
  /* Transmit PUSCH, PUCCH or SRS */
  bool signal_ready = false; 
  if (ul_action.tx_enabled) {
    encode_pusch(&ul_action.phy_grant.ul, ul_action.payload_ptr, ul_action.current_tx_nb, 
                 ul_action.softbuffer, ul_action.rv, ul_action.rnti, ul_mac_grant.is_from_rar);          
    signal_ready = true; 
    if (ul_action.expect_ack) {
      phy->set_pending_ack(tti + 8, ue_ul.pusch_cfg.grant.n_prb_tilde[0], ul_action.phy_grant.ul.ncs_dmrs);
    }

  } else if (dl_action.generate_ack || uci_data.scheduling_request || uci_data.uci_cqi_len > 0) {
    encode_pucch();
    signal_ready = true; 
  } else if (srs_is_ready_to_send()) {
    encode_srs();
    signal_ready = true; 
  } 

  tr_log_end();
  
  phy->worker_end(tx_tti, signal_ready, signal_buffer, SRSLTE_SF_LEN_PRB(cell.nof_prb), tx_time);
  
  if (dl_action.decode_enabled && !dl_action.generate_ack_callback) {
    if (dl_mac_grant.rnti_type == SRSLTE_RNTI_PCH) {
      phy->mac->pch_decoded_ok(dl_mac_grant.n_bytes);
    } else {
      phy->mac->tb_decoded(dl_ack, dl_mac_grant.rnti_type, dl_mac_grant.pid);
    }
  }

  update_measurements();
}


bool phch_worker::extract_fft_and_pdcch_llr() {
  bool decode_pdcch = false; 
  if (phy->get_ul_rnti(tti) || phy->get_dl_rnti(tti) || phy->get_pending_rar(tti)) {
    decode_pdcch = true; 
  } 
  
  /* Without a grant, we might need to do fft processing if need to decode PHICH */
  if (phy->get_pending_ack(tti) || decode_pdcch) {
    if (srslte_ue_dl_decode_fft_estimate(&ue_dl, signal_buffer, tti%10, &cfi) < 0) {
      Error("Getting PDCCH FFT estimate\n");
      return false; 
    }        
    chest_done = true; 
  } else {
    chest_done = false; 
  }
  if (decode_pdcch || (tti%5) == 0) { /* and not in DRX mode */
    
    float noise_estimate = phy->avg_noise;
    
    if (phy->params_db->get_param(phy_interface_params::EQUALIZER_COEFF) >= 0) {
      noise_estimate = phy->params_db->get_param(phy_interface_params::EQUALIZER_COEFF);
    }
    
    if (srslte_pdcch_extract_llr(&ue_dl.pdcch, ue_dl.sf_symbols, ue_dl.ce, noise_estimate, tti%10, cfi)) {
      Error("Extracting PDCCH LLR\n");
      return false; 
    }
  }
  return (decode_pdcch || phy->get_pending_ack(tti));
}
  








/********************* Downlink processing functions ****************************/

bool phch_worker::decode_pdcch_dl(srsue::mac_interface_phy::mac_grant_t* grant)
{
  char timestr[64];
  timestr[0]='\0';

  dl_rnti = phy->get_dl_rnti(tti); 
  if (dl_rnti) {
    
    srslte_rnti_type_t type = phy->get_dl_rnti_type();

    srslte_dci_msg_t dci_msg; 
    srslte_ra_dl_dci_t dci_unpacked;
    
    Debug("Looking for RNTI=0x%x\n", dl_rnti);
    
    if (srslte_ue_dl_find_dl_dci_type(&ue_dl, &dci_msg, cfi, tti%10, dl_rnti, type) != 1) {
      return false; 
    }
    
    if (srslte_dci_msg_to_dl_grant(&dci_msg, dl_rnti, cell.nof_prb, &dci_unpacked, &grant->phy_grant.dl)) {
      Error("Converting DCI message to DL grant\n");
      return false;   
    }
    
    /* Fill MAC grant structure */
    grant->ndi = dci_unpacked.ndi;
    grant->pid = dci_unpacked.harq_process;
    grant->n_bytes = grant->phy_grant.dl.mcs.tbs/8;
    grant->tti = tti; 
    grant->rv  = dci_unpacked.rv_idx;
    grant->rnti = dl_rnti; 
    grant->rnti_type = type; 
    
    last_dl_pdcch_ncce = srslte_ue_dl_get_ncce(&ue_dl);

#ifdef LOG_EXECTIME
  gettimeofday(&logtime_start[2], NULL);
  get_time_interval(logtime_start);
  snprintf(timestr, 64, ", partial_time=%4d us", (int) logtime_start[0].tv_usec);
#endif

    Info("PDCCH: DL DCI %s cce_index=%2d, n_data_bits=%d%s\n", srslte_ra_dl_dci_string(&dci_unpacked), 
         ue_dl.last_n_cce, dci_msg.nof_bits, timestr);
    
    return true; 
  } else {
    return false; 
  }
}

bool phch_worker::decode_pdsch(srslte_ra_dl_grant_t *grant, uint8_t *payload, 
                               srslte_softbuffer_rx_t* softbuffer, uint32_t rv, uint16_t rnti, uint32_t harq_pid)
{
  char timestr[64];
  timestr[0]='\0';
  
  Debug("DL Buffer TTI %d: Decoding PDSCH\n", tti);

  /* Setup PDSCH configuration for this CFI, SFIDX and RVIDX */
  if (!srslte_ue_dl_cfg_grant(&ue_dl, grant, cfi, tti%10, rv)) {
    if (ue_dl.pdsch_cfg.grant.mcs.mod > 0 && ue_dl.pdsch_cfg.grant.mcs.tbs >= 0) {
      
      float noise_estimate = 1./srslte_chest_dl_get_snr(&ue_dl.chest);
      
      if (phy->params_db->get_param(phy_interface_params::EQUALIZER_COEFF) >= 0) {
        noise_estimate = phy->params_db->get_param(phy_interface_params::EQUALIZER_COEFF);
      }
      
#ifdef LOG_EXECTIME
      struct timeval t[3];
      gettimeofday(&t[1], NULL);
#endif
      
      bool ack = srslte_pdsch_decode_rnti(&ue_dl.pdsch, &ue_dl.pdsch_cfg, softbuffer, ue_dl.sf_symbols, 
                                    ue_dl.ce, noise_estimate, rnti, payload) == 0;
#ifdef LOG_EXECTIME
      gettimeofday(&t[2], NULL);
      get_time_interval(t);
      snprintf(timestr, 64, ", dec_time=%4d us", (int) t[0].tv_usec);
#endif
            
      Info("PDSCH: l_crb=%2d, harq=%d, tbs=%d, mcs=%d, rv=%d, crc=%s, snr=%.1f dB, n_iter=%d%s\n", 
             grant->nof_prb, harq_pid, 
             grant->mcs.tbs/8, grant->mcs.idx, rv, 
             ack?"OK":"KO", 
             10*log10(srslte_chest_dl_get_snr(&ue_dl.chest)), 
             srslte_pdsch_last_noi(&ue_dl.pdsch),
             timestr);

      // Store metrics
      dl_metrics.mcs    = grant->mcs.idx;
      
      return ack; 
    } else {
      Warning("Received grant for TBS=0\n");
    }
  } else {
    Error("Error configuring DL grant\n"); 
  }
  return true; 
}

bool phch_worker::decode_phich(bool *ack)
{
  uint32_t I_lowest, n_dmrs; 
  if (phy->get_pending_ack(tti, &I_lowest, &n_dmrs)) {
    if (ack) {
      *ack = srslte_ue_dl_decode_phich(&ue_dl, tti%10, I_lowest, n_dmrs);     
      Info("PHICH: hi=%d, I_lowest=%d, n_dmrs=%d\n", *ack, I_lowest, n_dmrs);
    }
    phy->reset_pending_ack(tti);
    return true; 
  } else {
    return false; 
  }
}




/********************* Uplink processing functions ****************************/

bool phch_worker::decode_pdcch_ul(mac_interface_phy::mac_grant_t* grant)
{
  char timestr[64];
  timestr[0]='\0';

  phy->reset_pending_ack(tti + 8); 

  srslte_dci_msg_t dci_msg; 
  srslte_ra_ul_dci_t dci_unpacked;
  srslte_dci_rar_grant_t rar_grant;
  srslte_rnti_type_t type = phy->get_ul_rnti_type();
  
  bool ret = false; 
  if (phy->get_pending_rar(tti, &rar_grant)) {

    Info("Pending RAR UL grant\n");
    if (srslte_dci_rar_to_ul_grant(&rar_grant, cell.nof_prb, pusch_hopping.hopping_offset, 
      &dci_unpacked, &grant->phy_grant.ul)) 
    {
      Error("Converting RAR message to UL grant\n");
      return false; 
    } 
    grant->rnti_type = SRSLTE_RNTI_TEMP;
    grant->is_from_rar = true; 
    Info("RAR grant found for TTI=%d\n", tti);
    rar_cqi_request = rar_grant.cqi_request;    
    ret = true;  
  } else {
    ul_rnti = phy->get_ul_rnti(tti);
    if (ul_rnti) {
      if (srslte_ue_dl_find_ul_dci(&ue_dl, &dci_msg, cfi, tti%10, ul_rnti) != 1) {
        return false; 
      }
      if (srslte_dci_msg_to_ul_grant(&dci_msg, cell.nof_prb, pusch_hopping.hopping_offset, 
        &dci_unpacked, &grant->phy_grant.ul, tti)) 
      {
        Error("Converting DCI message to UL grant\n");
        return false;   
      }
      grant->rnti_type = type; 
      grant->is_from_rar = false; 
      ret = true; 
      
#ifdef LOG_EXECTIME
  gettimeofday(&logtime_start[2], NULL);
  get_time_interval(logtime_start);
  snprintf(timestr, 64, ", partial_time=%4d us", (int) logtime_start[0].tv_usec);
#endif

      Info("PDCCH: UL DCI Format0 cce_index=%d, L=%d, n_data_bits=%d, TBS=%d%s\n", 
           ue_dl.last_location.ncce, (1<<ue_dl.last_location.L), dci_msg.nof_bits, grant->phy_grant.ul.mcs.tbs, timestr);
      
      if (grant->phy_grant.ul.mcs.tbs==0) {
        srslte_vec_fprint_hex(stdout, dci_msg.data, dci_msg.nof_bits);
      }
    }
  }
  
  /* Limit UL modulation if not supported by the UE or disabled by higher layers */
  if (!phy->params_db->get_param(phy_interface_params::FORCE_ENABLE_64QAM) && !phy->params_db->get_param(phy_interface_params::PUSCH_EN_64QAM)) {
    if (grant->phy_grant.ul.mcs.mod == SRSLTE_MOD_64QAM) {
      grant->phy_grant.ul.mcs.mod = SRSLTE_MOD_16QAM;
      grant->phy_grant.ul.Qm      = 4;
    }
  }
  
  if (ret) {    
    grant->ndi = dci_unpacked.ndi;
    grant->pid = 0; // This is computed by MAC from TTI 
    grant->n_bytes = grant->phy_grant.ul.mcs.tbs/8;
    grant->tti = tti; 
    grant->rnti = ul_rnti; 
    grant->rv = dci_unpacked.rv_idx;
    if (SRSLTE_VERBOSE_ISINFO()) {
      srslte_ra_pusch_fprint(stdout, &dci_unpacked, cell.nof_prb);
    }
    
    return true;     
  } else {
    return false; 
  }    
}

void phch_worker::reset_uci()
{
  bzero(&uci_data, sizeof(srslte_uci_data_t));
}

void phch_worker::set_uci_ack(bool ack)
{
  uci_data.uci_ack = ack?1:0;
  uci_data.uci_ack_len = 1; 
}

void phch_worker::set_uci_sr()
{
  uci_data.scheduling_request = false; 
  if (phy->sr_enabled) {
    // Get I_sr parameter
    if (srslte_ue_ul_sr_send_tti(I_sr, (tti+4)%10240)) {
      Info("SR transmission at TTI=%d\n", (tti+4)%10240);
      uci_data.scheduling_request = true; 
      phy->sr_last_tx_tti = (tti+4)%10240; 
      phy->sr_enabled = false;
    }
  } 
}

void phch_worker::set_uci_periodic_cqi()
{
  if ((period_cqi.configured || rar_cqi_request) && rnti_is_set) {
    if (srslte_cqi_send(period_cqi.pmi_idx, (tti+4)%10240)) {
      srslte_cqi_value_t cqi_report;
      if (period_cqi.format_is_subband) {
        // TODO: Implement subband periodic reports
        cqi_report.type = SRSLTE_CQI_TYPE_SUBBAND;
        cqi_report.subband.subband_cqi = srslte_cqi_from_snr(phy->avg_snr_db);
        cqi_report.subband.subband_label = 0;
        phy->log_h->console("Warning: Subband CQI periodic reports not implemented\n");
        Info("CQI: subband snr=%.1f dB, cqi=%d\n", phy->avg_snr_db, cqi_report.subband.subband_cqi);
      } else {
        cqi_report.type = SRSLTE_CQI_TYPE_WIDEBAND;
        cqi_report.wideband.wideband_cqi = srslte_cqi_from_snr(phy->avg_snr_db);        
        if (cqi_report.wideband.wideband_cqi > 15) {
          cqi_report.wideband.wideband_cqi = 15;
        }
        Info("CQI: wideband snr=%.1f dB, cqi=%d\n", phy->avg_snr_db, cqi_report.wideband.wideband_cqi);
      }
      uci_data.uci_cqi_len = srslte_cqi_value_pack(&cqi_report, uci_data.uci_cqi);
      rar_cqi_request = false;       
    }
  }
}

bool phch_worker::srs_is_ready_to_send() {
  if (srs_cfg.configured) {
    if (srslte_refsignal_srs_send_cs(srs_cfg.subframe_config, (tti+4)%10) == 1 && 
        srslte_refsignal_srs_send_ue(srs_cfg.I_srs, (tti+4)%10240)        == 1)
    {
      return true; 
    }
  }
  return false; 
}

void phch_worker::set_tx_time(srslte_timestamp_t _tx_time)
{
  memcpy(&tx_time, &_tx_time, sizeof(srslte_timestamp_t));
}

void phch_worker::encode_pusch(srslte_ra_ul_grant_t *grant, uint8_t *payload, uint32_t current_tx_nb, 
                               srslte_softbuffer_tx_t* softbuffer, uint32_t rv, uint16_t rnti, bool is_from_rar)
{
  char timestr[64];
  timestr[0]='\0';
  
  if (srslte_ue_ul_cfg_grant(&ue_ul, grant, (tti+4)%10240, rv, current_tx_nb)) {
    Error("Configuring UL grant\n");
  }
    
  if (srslte_ue_ul_pusch_encode_rnti_softbuffer(&ue_ul, 
                                                payload, uci_data, 
                                                softbuffer,
                                                rnti, 
                                                signal_buffer)) 
  {
    Error("Encoding PUSCH\n");
  }
    
  float p0_preamble = 0; 
  if (is_from_rar) {
    p0_preamble = phy->p0_preamble;
  }
  float tx_power = srslte_ue_ul_pusch_power(&ue_ul, phy->pathloss, p0_preamble);
  float gain = set_power(tx_power);

  // Save PUSCH power for PHR calculation  
  phy->cur_pusch_power = tx_power; 
  
#ifdef LOG_EXECTIME
  gettimeofday(&logtime_start[2], NULL);
  get_time_interval(logtime_start);
  snprintf(timestr, 64, ", total_time=%4d us", (int) logtime_start[0].tv_usec);
#endif

  Info("PUSCH: power=%.2f dBm, tti_tx=%d, n_prb=%d, rb_start=%d, tbs=%d, mod=%d, mcs=%d, rv_idx=%d, ack=%s%s\n", 
         tx_power, (tti+4)%10240,
         grant->L_prb, grant->n_prb[0], 
         grant->mcs.tbs/8, grant->mcs.mod, grant->mcs.idx, rv,
         uci_data.uci_ack_len>0?(uci_data.uci_ack?"1":"0"):"no",
         timestr);

  // Store metrics
  ul_metrics.mcs   = grant->mcs.idx;
  ul_metrics.power = tx_power;
}

void phch_worker::encode_pucch()
{
  char timestr[64];
  timestr[0]='\0';

  if (uci_data.scheduling_request || uci_data.uci_ack_len > 0 || uci_data.uci_cqi_len > 0) 
  {
    
    // Drop CQI if there is collision with ACK 
    if (!period_cqi.simul_cqi_ack && uci_data.uci_ack_len > 0 && uci_data.uci_cqi_len > 0) {
      uci_data.uci_cqi_len = 0; 
    }

#ifdef LOG_EXECTIME
    struct timeval t[3];
    gettimeofday(&t[1], NULL);
#endif

    if (srslte_ue_ul_pucch_encode(&ue_ul, uci_data, last_dl_pdcch_ncce, (tti+4)%10240, signal_buffer)) {
      Error("Encoding PUCCH\n");
    }

#ifdef LOG_EXECTIME
  gettimeofday(&logtime_start[2], NULL);
  memcpy(&t[2], &logtime_start[2], sizeof(struct timeval));
  get_time_interval(logtime_start);
  get_time_interval(t);
  snprintf(timestr, 64, ", enc_time=%d, total_time=%d us", (int) t[0].tv_usec, (int) logtime_start[0].tv_usec);
#endif

  float tx_power = srslte_ue_ul_pucch_power(&ue_ul, phy->pathloss, ue_ul.last_pucch_format, uci_data.uci_cqi_len, uci_data.uci_ack_len);
  float gain = set_power(tx_power);  
  
  Info("PUCCH: power=%.2f dBm, tti_tx=%d, n_cce=%3d, ack=%s, sr=%s, shortened=%s%s\n", 
         tx_power, (tti+4)%10240, 
         last_dl_pdcch_ncce, uci_data.uci_ack_len>0?(uci_data.uci_ack?"1":"0"):"no",uci_data.scheduling_request?"yes":"no", 
         ue_ul.pucch.shortened?"yes":"no", timestr);        
  }   
  
  if (uci_data.scheduling_request) {
    phy->sr_enabled = false; 
  }
}

void phch_worker::encode_srs()
{
  char timestr[64];
  timestr[0]='\0';
  
  if (srslte_ue_ul_srs_encode(&ue_ul, (tti+4)%10240, signal_buffer)) 
  {
    Error("Encoding SRS\n");
  }

#ifdef LOG_EXECTIME
  gettimeofday(&logtime_start[2], NULL);
  get_time_interval(logtime_start);
  snprintf(timestr, 64, ", total_time=%4d us", (int) logtime_start[0].tv_usec);
#endif
  
  float tx_power = srslte_ue_ul_srs_power(&ue_ul, phy->pathloss);  
  float gain = set_power(tx_power);
  uint32_t fi = srslte_vec_max_fi((float*) signal_buffer, SRSLTE_SF_LEN_PRB(cell.nof_prb));
  float *f = (float*) signal_buffer;
  Info("SRS: power=%.2f dBm, tti_tx=%d%s\n", tx_power, (tti+4)%10240, timestr);
  
}

void phch_worker::enable_pregen_signals(bool enabled)
{
  pregen_enabled = enabled; 
}

void phch_worker::reset_ul_params() 
{
  bzero(&dmrs_cfg, sizeof(srslte_refsignal_dmrs_pusch_cfg_t));    
  bzero(&pusch_hopping, sizeof(srslte_pusch_hopping_cfg_t));
  bzero(&uci_cfg, sizeof(srslte_uci_cfg_t));
  bzero(&pucch_cfg, sizeof(srslte_pucch_cfg_t));
  bzero(&pucch_sched, sizeof(srslte_pucch_sched_t));
  bzero(&srs_cfg, sizeof(srslte_refsignal_srs_cfg_t));
  bzero(&period_cqi, sizeof(srslte_cqi_periodic_cfg_t));
  I_sr = 0; 
}

void phch_worker::set_ul_params()
{

  /* PUSCH DMRS signal configuration */
  bzero(&dmrs_cfg, sizeof(srslte_refsignal_dmrs_pusch_cfg_t));    
  dmrs_cfg.group_hopping_en    = (bool)     phy->params_db->get_param(phy_interface_params::DMRS_GROUP_HOPPING_EN);
  dmrs_cfg.sequence_hopping_en = (bool)     phy->params_db->get_param(phy_interface_params::DMRS_SEQUENCE_HOPPING_EN);
  dmrs_cfg.cyclic_shift        = (uint32_t) phy->params_db->get_param(phy_interface_params::PUSCH_RS_CYCLIC_SHIFT);
  dmrs_cfg.delta_ss            = (uint32_t) phy->params_db->get_param(phy_interface_params::PUSCH_RS_GROUP_ASSIGNMENT);
  
  /* PUSCH Hopping configuration */
  bzero(&pusch_hopping, sizeof(srslte_pusch_hopping_cfg_t));
  pusch_hopping.n_sb           = (uint32_t) phy->params_db->get_param(phy_interface_params::PUSCH_HOPPING_N_SB);
  pusch_hopping.hop_mode       = (uint32_t) phy->params_db->get_param(phy_interface_params::PUSCH_HOPPING_INTRA_SF) ? 
                                  pusch_hopping.SRSLTE_PUSCH_HOP_MODE_INTRA_SF : 
                                  pusch_hopping.SRSLTE_PUSCH_HOP_MODE_INTER_SF; 
  pusch_hopping.hopping_offset = (uint32_t) phy->params_db->get_param(phy_interface_params::PUSCH_HOPPING_OFFSET);

  /* PUSCH UCI configuration */
  bzero(&uci_cfg, sizeof(srslte_uci_cfg_t));
  uci_cfg.I_offset_ack         = (uint32_t) phy->params_db->get_param(phy_interface_params::UCI_I_OFFSET_ACK);
  uci_cfg.I_offset_cqi         = (uint32_t) phy->params_db->get_param(phy_interface_params::UCI_I_OFFSET_CQI);
  uci_cfg.I_offset_ri          = (uint32_t) phy->params_db->get_param(phy_interface_params::UCI_I_OFFSET_RI);
  
  /* PUCCH configuration */  
  bzero(&pucch_cfg, sizeof(srslte_pucch_cfg_t));
  pucch_cfg.delta_pucch_shift  = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_DELTA_SHIFT);
  pucch_cfg.N_cs               = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_CYCLIC_SHIFT);
  pucch_cfg.n_rb_2             = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_RB_2);
  pucch_cfg.srs_configured     = (bool)     phy->params_db->get_param(phy_interface_params::SRS_IS_CONFIGURED)?true:false;
  pucch_cfg.srs_cs_subf_cfg    = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_CS_SFCFG);
  pucch_cfg.srs_simul_ack      = (bool)     phy->params_db->get_param(phy_interface_params::SRS_CS_ACKNACKSIMUL)?true:false;
  
  /* PUCCH Scheduling configuration */
  bzero(&pucch_sched, sizeof(srslte_pucch_sched_t));
  pucch_sched.n_pucch_1[0]     = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_1_0);
  pucch_sched.n_pucch_1[1]     = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_1_1);
  pucch_sched.n_pucch_1[2]     = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_1_2);
  pucch_sched.n_pucch_1[3]     = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_1_3);
  pucch_sched.N_pucch_1        = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_1);
  pucch_sched.n_pucch_2        = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_2);
  pucch_sched.n_pucch_sr       = (uint32_t) phy->params_db->get_param(phy_interface_params::PUCCH_N_PUCCH_SR);

  /* SRS Configuration */
  bzero(&srs_cfg, sizeof(srslte_refsignal_srs_cfg_t));
  srs_cfg.configured           = (bool)     phy->params_db->get_param(phy_interface_params::SRS_IS_CONFIGURED)?true:false;
  srs_cfg.subframe_config      = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_CS_SFCFG);
  srs_cfg.bw_cfg               = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_CS_BWCFG);
  srs_cfg.I_srs                = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_UE_CONFIGINDEX);
  srs_cfg.B                    = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_UE_BW);
  srs_cfg.b_hop                = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_UE_HOP);
  srs_cfg.n_rrc                = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_UE_NRRC);
  srs_cfg.k_tc                 = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_UE_TXCOMB);
  srs_cfg.n_srs                = (uint32_t) phy->params_db->get_param(phy_interface_params::SRS_UE_CYCLICSHIFT);

  /* UL power control configuration */
  bzero(&power_ctrl, sizeof(srslte_ue_ul_powerctrl_t));
  power_ctrl.p0_nominal_pusch  = (float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_P0_NOMINAL_PUSCH);
  power_ctrl.alpha             =((float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_ALPHA))/10;
  power_ctrl.p0_nominal_pucch  = (float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_P0_NOMINAL_PUCCH);
  for (int i=0;i<5;i++) {
    power_ctrl.delta_f_pucch[i]= (float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_DELTA_PUCCH_F1+i);
  }
  power_ctrl.delta_preamble_msg3=(float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_DELTA_MSG3);
  
  power_ctrl.p0_ue_pusch       = (float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_P0_UE_PUSCH);
  power_ctrl.delta_mcs_based   = (bool)     phy->params_db->get_param(phy_interface_params::PWRCTRL_DELTA_MCS_EN)?true:false;
  power_ctrl.acc_enabled       = (bool)     phy->params_db->get_param(phy_interface_params::PWRCTRL_ACC_EN)?true:false;
  power_ctrl.p0_ue_pucch       = (float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_P0_UE_PUCCH);
  power_ctrl.p_srs_offset      = (float)    phy->params_db->get_param(phy_interface_params::PWRCTRL_SRS_OFFSET);
  
  srslte_ue_ul_set_cfg(&ue_ul, &dmrs_cfg, &srs_cfg, &pucch_cfg, &pucch_sched, &uci_cfg, &pusch_hopping, &power_ctrl);

  /* CQI configuration */
  bzero(&period_cqi, sizeof(srslte_cqi_periodic_cfg_t));
  period_cqi.configured        = (bool)     phy->params_db->get_param(phy_interface_params::CQI_PERIODIC_CONFIGURED)?true:false;
  period_cqi.pmi_idx           = (uint32_t) phy->params_db->get_param(phy_interface_params::CQI_PERIODIC_PMI_IDX); 
  period_cqi.simul_cqi_ack     = (bool)     phy->params_db->get_param(phy_interface_params::CQI_PERIODIC_SIMULT_ACK)?true:false;
  period_cqi.format_is_subband = (bool)     phy->params_db->get_param(phy_interface_params::CQI_PERIODIC_FORMAT_SUBBAND)?true:false;
  period_cqi.subband_size      = (uint32_t) phy->params_db->get_param(phy_interface_params::CQI_PERIODIC_FORMAT_SUBBAND_K);
  
  /* SR configuration */
  I_sr                         = (uint32_t) phy->params_db->get_param(phy_interface_params::SR_CONFIG_INDEX);
  

  if (pregen_enabled) { 
    Info("Pre-generating UL signals\n");
    srslte_ue_ul_pregen_signals(&ue_ul);
  }  
}

float phch_worker::set_power(float tx_power) {
  float gain = 0; 
  /* Check if UL power control is enabled */
  if(phy->params_db->get_param(phy_interface_params::UL_GAIN) < 0) {
    
    /* Adjust maximum power if it changes significantly */
    //if (tx_power < phy->cur_radio_power - 5 || tx_power > phy->cur_radio_power + 5) {
      phy->cur_radio_power = tx_power; 
      /* Add an optional offset to the power set to the RF frontend */
      float radio_tx_power = phy->cur_radio_power;
      radio_tx_power += (float) phy->params_db->get_param(phy_interface_params::UL_PWR_CTRL_OFFSET);
      gain = phy->get_radio()->set_tx_power(radio_tx_power);  
    //}    
  }
  return gain;
}







/**************************** Measurements **************************/

void phch_worker::update_measurements() 
{
  if (chest_done) {
    /* Compute ADC/RX gain offset every 20 ms */
    if ((tti%20) == 0 || phy->rx_gain_offset == 0) {
      float rx_gain_offset = 0; 
      if (phy->get_radio()->has_rssi()) {
        float rssi_all_signal = srslte_chest_dl_get_rssi(&ue_dl.chest);          
        if (rssi_all_signal) {
          rx_gain_offset = 10*log10(rssi_all_signal)-phy->get_radio()->get_rssi();
        } else {
          rx_gain_offset = 0; 
        }
      } else {
        if (phy->params_db->get_param(phy_interface_params::RX_GAIN_OFFSET) > 0) {
          rx_gain_offset = (float) phy->params_db->get_param(phy_interface_params::RX_GAIN_OFFSET);
        } else {
          rx_gain_offset = phy->get_radio()->get_rx_gain();
        }
      }
      if (phy->rx_gain_offset) {
        phy->rx_gain_offset = SRSLTE_VEC_EMA(phy->rx_gain_offset, rx_gain_offset, 0.1);
      } else {
        phy->rx_gain_offset = rx_gain_offset; 
      }
    }
    
    // Adjust measurements with RX gain offset    
    if (phy->rx_gain_offset) {
      float cur_rsrq = 10*log10(srslte_chest_dl_get_rsrq(&ue_dl.chest));
      if (isnormal(cur_rsrq)) {
        phy->avg_rsrq_db = SRSLTE_VEC_EMA(phy->avg_rsrq_db, cur_rsrq, SNR_FILTER_COEFF);
      }
      
      float cur_rsrp = srslte_chest_dl_get_rsrp(&ue_dl.chest);
      if (isnormal(cur_rsrp)) {
        phy->avg_rsrp = SRSLTE_VEC_EMA(phy->avg_rsrp, cur_rsrp, SNR_FILTER_COEFF);
      }
      
      /* Correct absolute power measurements by RX gain offset */
      float rsrp = 10*log10(srslte_chest_dl_get_rsrp(&ue_dl.chest)) + 30 - phy->rx_gain_offset;
      float rssi = 10*log10(srslte_chest_dl_get_rssi(&ue_dl.chest)) + 30 - phy->rx_gain_offset;
      
      // TODO: Send UE measurements to RRC where filtering is done. Now do filtering here
      if (isnormal(rsrp)) {
        if (!phy->avg_rsrp_db) {
          phy->avg_rsrp_db = rsrp;
        } else {
          uint32_t k = 4; // Set by RRC reconfiguration message
          float coeff = pow(0.5,(float) k/4);
          phy->avg_rsrp_db = SRSLTE_VEC_EMA(phy->avg_rsrp_db, rsrp, coeff);          
        }    
      }
      // Compute PL
      float tx_crs_power = (float) phy->params_db->get_param(phy_interface_params::PDSCH_RSPOWER);
      phy->pathloss = tx_crs_power - phy->avg_rsrp_db;

      // Average noise in subframes 0 and 5
      if ((tti%5) == 0) {
        float cur_noise = srslte_chest_dl_get_noise_estimate(&ue_dl.chest);
        if (isnormal(cur_noise)) {
          if (!phy->avg_noise) {
            phy->avg_noise = cur_noise;
          } else {
            phy->avg_noise = SRSLTE_VEC_EMA(phy->avg_noise, cur_noise, SNR_FILTER_COEFF);            
          }
        }
      }
      
      // Compute SNR
      float cur_snr = 10*log10(phy->avg_rsrp/phy->avg_noise);      
      if (isnormal(cur_snr)) {
        if (!phy->avg_snr_db) {
          phy->avg_snr_db = cur_snr; 
        } else if (isnormal(cur_snr)) {
          phy->avg_snr_db = SRSLTE_VEC_EMA(phy->avg_snr_db, cur_snr, SNR_FILTER_COEFF);        
        }        
      }
      
      // Store metrics
      dl_metrics.n      = phy->avg_noise;
      dl_metrics.rsrp   = phy->avg_rsrp_db;
      dl_metrics.rsrq   = phy->avg_rsrq_db;
      dl_metrics.rssi   = rssi;
      dl_metrics.pathloss = phy->pathloss;
      dl_metrics.sinr   = phy->avg_snr_db;
      dl_metrics.turbo_iters = srslte_pdsch_last_noi(&ue_dl.pdsch);
      phy->set_dl_metrics(dl_metrics);
      
      phy->set_ul_metrics(ul_metrics);

    }
  }
}







/********** Execution time trace function ************/

void phch_worker::start_trace() {
  trace_enabled = true; 
}

void phch_worker::write_trace(std::string filename) {
  tr_exec.writeToBinary(filename + ".exec");
}

void phch_worker::tr_log_start()
{
  if (trace_enabled) {
    gettimeofday(&tr_time[1], NULL);
  }
}

void phch_worker::tr_log_end()
{
  if (trace_enabled) {
    gettimeofday(&tr_time[2], NULL);
    get_time_interval(tr_time);
    tr_exec.push(tti, tr_time[0].tv_usec);
  }
}


}
