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
#include <sstream>
#include <fstream>
#include <iostream>

#include "upper/rrc.h"
#include <srslte/utils/bit.h>

using namespace srslte;

using namespace std;

ofstream output;

namespace srsue{

rrc::rrc()
  :state(RRC_STATE_IDLE)
  ,drb_up(false)
{}

void rrc::init(phy_interface_rrc     *phy_,
               mac_interface_rrc     *mac_,
               rlc_interface_rrc     *rlc_,
               pdcp_interface_rrc    *pdcp_,
               nas_interface_rrc     *nas_,
               usim_interface_rrc    *usim_,
               srslte::log           *rrc_log_)
{
  pool    = buffer_pool::get_instance();
  phy     = phy_;
  mac     = mac_;
  rlc     = rlc_;
  pdcp    = pdcp_;
  nas     = nas_;
  usim    = usim_;
  rrc_log = rrc_log_;

  transaction_id = 0;
  output.open("ue.txt", ios::out | ios::app);
}

void rrc::stop()
{}

rrc_state_t rrc::get_state()
{
  return state;
}

/*******************************************************************************
  NAS interface
*******************************************************************************/

void rrc::write_sdu(uint32_t lcid, byte_buffer_t *sdu)
{
  rrc_log->info_hex(sdu->msg, sdu->N_bytes, "UL %s SDU", rb_id_text[lcid]);

  switch(state)
  {
  case RRC_STATE_COMPLETING_SETUP:
    send_con_setup_complete(sdu);
    break;
  case RRC_STATE_RRC_CONNECTED:
    send_ul_info_transfer(lcid, sdu);
    break;
  default:
    rrc_log->error("SDU received from NAS while RRC state = %s", rrc_state_text[state]);
    break;
  }
}

uint16_t rrc::get_mcc()
{
  if(sib1.N_plmn_ids > 0)
    return sib1.plmn_id[0].id.mcc;
  else
    return 0;
}

uint16_t rrc::get_mnc()
{
  if(sib1.N_plmn_ids > 0)
    return sib1.plmn_id[0].id.mnc;
  else
    return 0;
}

/*******************************************************************************
  PHY/MAC interface
*******************************************************************************/
/* Forces a UE-initiated connection release after a synchronization error or SR timeout */
void rrc::connection_release()
{
  if (state == RRC_STATE_RRC_CONNECTED) {
    rrc_connection_release(); 
  }
}


/*******************************************************************************
  GW interface
*******************************************************************************/

bool rrc::rrc_connected()
{
  if(RRC_STATE_RRC_CONNECTED == state) {
    return true;
  }
  if(RRC_STATE_IDLE == state) {
    rrc_log->info("RRC in IDLE state - sending connection request.\n");
    state = RRC_STATE_WAIT_FOR_CON_SETUP;
    send_con_request();
  }
  return false;
}

bool rrc::have_drb()
{
  return drb_up;
}

/*******************************************************************************
  PDCP interface
*******************************************************************************/

void rrc::write_pdu(uint32_t lcid, byte_buffer_t *pdu)
{
  rrc_log->info_hex(pdu->msg, pdu->N_bytes, "DL %s PDU", rb_id_text[lcid]);

  switch(lcid)
  {
  case RB_ID_SRB0:
    parse_dl_ccch(pdu);
    break;
  case RB_ID_SRB1:
  case RB_ID_SRB2:
    parse_dl_dcch(lcid, pdu);
    break;
  default:
    rrc_log->error("DL PDU with invalid bearer id: %s", lcid);
    break;
  }

}

void rrc::write_pdu_bcch_bch(byte_buffer_t *pdu)
{
  // Unpack the MIB
  rrc_log->info_hex(pdu->msg, pdu->N_bytes, "BCCH BCH message received.");
  srslte_bit_unpack_vector(pdu->msg, bit_buf.msg, pdu->N_bytes*8);
  bit_buf.N_bits = pdu->N_bytes*8;
  pool->deallocate(pdu);
  liblte_rrc_unpack_bcch_bch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &mib);
  rrc_log->info("MIB received BW=%s MHz\n", liblte_rrc_dl_bandwidth_text[mib.dl_bw]);
  rrc_log->console("MIB received BW=%s MHz\n", liblte_rrc_dl_bandwidth_text[mib.dl_bw]);

  // Start the SIB search state machine
  state = RRC_STATE_SIB1_SEARCH;
  pthread_create(&sib_search_thread, NULL, &rrc::start_sib_thread, this);
}

void rrc::write_pdu_bcch_dlsch(byte_buffer_t *pdu)
{
  rrc_log->info_hex(pdu->msg, pdu->N_bytes, "BCCH DLSCH message received.");
  LIBLTE_RRC_BCCH_DLSCH_MSG_STRUCT dlsch_msg;
  srslte_bit_unpack_vector(pdu->msg, bit_buf.msg, pdu->N_bytes*8);
  bit_buf.N_bits = pdu->N_bytes*8;
  pool->deallocate(pdu);
  liblte_rrc_unpack_bcch_dlsch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &dlsch_msg);

  if (dlsch_msg.N_sibs > 0) {
    if (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1 == dlsch_msg.sibs[0].sib_type && RRC_STATE_SIB1_SEARCH == state) {
      // Handle SIB1
      memcpy(&sib1, &dlsch_msg.sibs[0].sib.sib1, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_1_STRUCT));
      rrc_log->info("SIB1 received, CellID=%d, si_window=%d, sib2_period=%d\n",
                    sib1.cell_id&0xfff,
                    liblte_rrc_si_window_length_num[sib1.si_window_length],
                    liblte_rrc_si_periodicity_num[sib1.sched_info[0].si_periodicity]);
      std::stringstream ss;
      for(int i=0;i<sib1.N_plmn_ids;i++){
        ss << " PLMN Id: MCC " << sib1.plmn_id[i].id.mcc << " MNC " << sib1.plmn_id[i].id.mnc;
        output << "MCC: " << sib1.plmn_id[i].id.mcc;
        output << ", MNC: " << sib1.plmn_id[i].id.mnc;
      }
      rrc_log->console("SIB1 received, CellID=%d, %s\n",
                       sib1.cell_id&0xfff,
                       ss.str().c_str());
      state = RRC_STATE_SIB2_SEARCH;
      mac->bcch_stop_rx();
      //TODO: Use all SIB1 info

    } else if (LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2 == dlsch_msg.sibs[0].sib_type && RRC_STATE_SIB2_SEARCH == state) {
      // Handle SIB2
      memcpy(&sib2, &dlsch_msg.sibs[0].sib.sib2, sizeof(LIBLTE_RRC_SYS_INFO_BLOCK_TYPE_2_STRUCT));
      rrc_log->console("SIB2 received\n");
      rrc_log->info("SIB2 received\n");
      state = RRC_STATE_WAIT_FOR_CON_SETUP;
      mac->bcch_stop_rx();
      apply_sib2_configs();
      send_con_request();
    }
  }
}

void rrc::write_pdu_pcch(byte_buffer_t *pdu)
{
  rrc_log->info_hex(pdu->msg, pdu->N_bytes, "PCCH message received %d bytes\n", pdu->N_bytes);
  LIBLTE_RRC_PCCH_MSG_STRUCT pcch_msg;
  srslte_bit_unpack_vector(pdu->msg, bit_buf.msg, pdu->N_bytes*8);
  bit_buf.N_bits = pdu->N_bytes*8;
  pool->deallocate(pdu);
  liblte_rrc_unpack_pcch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &pcch_msg);
  
  if (pcch_msg.paging_record_list_size > LIBLTE_RRC_MAX_PAGE_REC) {
    pcch_msg.paging_record_list_size = LIBLTE_RRC_MAX_PAGE_REC;     
  }

  LIBLTE_RRC_S_TMSI_STRUCT s_tmsi;
  if(!nas->get_s_tmsi(&s_tmsi)) {
    rrc_log->info("No S-TMSI present in NAS\n");
    return;
  }
  
  LIBLTE_RRC_S_TMSI_STRUCT *s_tmsi_paged;
  for (int i=0;i<pcch_msg.paging_record_list_size;i++) {
    s_tmsi_paged = &pcch_msg.paging_record_list[i].ue_identity.s_tmsi;
    rrc_log->info("Received paging (%d/%d) for UE 0x%x\n", i+1, pcch_msg.paging_record_list_size,
                  pcch_msg.paging_record_list[i].ue_identity.s_tmsi);
    rrc_log->console("Received paging (%d/%d) for UE 0x%x\n", i+1, pcch_msg.paging_record_list_size,
                     pcch_msg.paging_record_list[i].ue_identity.s_tmsi);
    if(s_tmsi.mmec == s_tmsi_paged->mmec && s_tmsi.m_tmsi == s_tmsi_paged->m_tmsi) {
      rrc_log->info("S-TMSI match in paging message\n");
      rrc_log->console("S-TMSI match in paging message\n");
      mac->pcch_stop_rx();
      if(RRC_STATE_IDLE == state) {
        rrc_log->info("RRC in IDLE state - sending connection request.\n");
        state = RRC_STATE_WAIT_FOR_CON_SETUP;
        send_con_request();
      }
    }
  }
}

/*******************************************************************************
  RLC interface
*******************************************************************************/

void rrc::max_retx_attempted()
{
  //TODO: Handle the radio link failure
}

/*******************************************************************************
  Senders
*******************************************************************************/

void rrc::send_con_request()
{
  rrc_log->debug("Preparing RRC Connection Request");
  LIBLTE_RRC_UL_CCCH_MSG_STRUCT ul_ccch_msg;
  LIBLTE_RRC_S_TMSI_STRUCT      s_tmsi;

  // Prepare ConnectionRequest packet
  ul_ccch_msg.msg_type = LIBLTE_RRC_UL_CCCH_MSG_TYPE_RRC_CON_REQ;
  if(nas->get_s_tmsi(&s_tmsi)) {
    ul_ccch_msg.msg.rrc_con_req.ue_id_type = LIBLTE_RRC_CON_REQ_UE_ID_TYPE_S_TMSI;
    ul_ccch_msg.msg.rrc_con_req.ue_id.s_tmsi = s_tmsi;
  } else {
    ul_ccch_msg.msg.rrc_con_req.ue_id_type = LIBLTE_RRC_CON_REQ_UE_ID_TYPE_RANDOM_VALUE;
    ul_ccch_msg.msg.rrc_con_req.ue_id.random = 1000;
  }
  ul_ccch_msg.msg.rrc_con_req.cause = LIBLTE_RRC_CON_REQ_EST_CAUSE_MO_SIGNALLING;
  liblte_rrc_pack_ul_ccch_msg(&ul_ccch_msg, (LIBLTE_BIT_MSG_STRUCT*)&bit_buf);

  // Byte align and pack the message bits for PDCP
  if((bit_buf.N_bits % 8) != 0)
  {
    for(int i=0; i<8-(bit_buf.N_bits % 8); i++)
      bit_buf.msg[bit_buf.N_bits + i] = 0;
    bit_buf.N_bits += 8 - (bit_buf.N_bits % 8);
  }
  byte_buffer_t *pdcp_buf = pool->allocate();
  srslte_bit_pack_vector(bit_buf.msg, pdcp_buf->msg, bit_buf.N_bits);
  pdcp_buf->N_bytes = bit_buf.N_bits/8;

  // Set UE contention resolution ID in MAC
  uint64_t uecri=0;
  uint8_t *ue_cri_ptr = (uint8_t*) &uecri;
  uint32_t nbytes = 6;
  for (int i=0;i<nbytes;i++) {
    ue_cri_ptr[nbytes-i-1] = pdcp_buf->msg[i];
  }
  rrc_log->debug("Setting UE contention resolution ID: %d\n", uecri);
  mac->set_param(srsue::mac_interface_params::CONTENTION_ID, uecri);

  rrc_log->info("Sending RRC Connection Request on SRB0\n");
  state = RRC_STATE_WAIT_FOR_CON_SETUP;
  pdcp->write_sdu(RB_ID_SRB0, pdcp_buf);
}

void rrc::send_con_setup_complete(byte_buffer_t *nas_msg)
{
  rrc_log->debug("Preparing RRC Connection Setup Complete\n");
  LIBLTE_RRC_UL_DCCH_MSG_STRUCT ul_dcch_msg;

  // Prepare ConnectionSetupComplete packet
  ul_dcch_msg.msg_type = LIBLTE_RRC_UL_DCCH_MSG_TYPE_RRC_CON_SETUP_COMPLETE;
  ul_dcch_msg.msg.rrc_con_setup_complete.registered_mme_present = false;
  ul_dcch_msg.msg.rrc_con_setup_complete.rrc_transaction_id = transaction_id;
  ul_dcch_msg.msg.rrc_con_setup_complete.selected_plmn_id = 1;
  memcpy(ul_dcch_msg.msg.rrc_con_setup_complete.dedicated_info_nas.msg, nas_msg->msg, nas_msg->N_bytes);
  ul_dcch_msg.msg.rrc_con_setup_complete.dedicated_info_nas.N_bytes = nas_msg->N_bytes;
  liblte_rrc_pack_ul_dcch_msg(&ul_dcch_msg, (LIBLTE_BIT_MSG_STRUCT*)&bit_buf);

  // Byte align and pack the message bits for PDCP
  if((bit_buf.N_bits % 8) != 0)
  {
    for(int i=0; i<8-(bit_buf.N_bits % 8); i++)
      bit_buf.msg[bit_buf.N_bits + i] = 0;
    bit_buf.N_bits += 8 - (bit_buf.N_bits % 8);
  }
  byte_buffer_t *pdcp_buf = pool->allocate();
  srslte_bit_pack_vector(bit_buf.msg, pdcp_buf->msg, bit_buf.N_bits);
  pdcp_buf->N_bytes = bit_buf.N_bits/8;

  state = RRC_STATE_RRC_CONNECTED;
  rrc_log->console("RRC Connected\n");
  rrc_log->info("Sending RRC Connection Setup Complete\n");
  pdcp->write_sdu(RB_ID_SRB1, pdcp_buf);
}

void rrc::send_ul_info_transfer(uint32_t lcid, byte_buffer_t *sdu)
{
  rrc_log->debug("Preparing UL Info Transfer\n");
  LIBLTE_RRC_UL_DCCH_MSG_STRUCT ul_dcch_msg;

  // Prepare UL INFO packet
  ul_dcch_msg.msg_type                                 = LIBLTE_RRC_UL_DCCH_MSG_TYPE_UL_INFO_TRANSFER;
  ul_dcch_msg.msg.ul_info_transfer.dedicated_info_type = LIBLTE_RRC_UL_INFORMATION_TRANSFER_TYPE_NAS;
  memcpy(ul_dcch_msg.msg.ul_info_transfer.dedicated_info.msg, sdu->msg, sdu->N_bytes);
  ul_dcch_msg.msg.ul_info_transfer.dedicated_info.N_bytes = sdu->N_bytes;
  liblte_rrc_pack_ul_dcch_msg(&ul_dcch_msg, (LIBLTE_BIT_MSG_STRUCT*)&bit_buf);

  // Reset and reuse sdu buffer
  byte_buffer_t *pdu = sdu;
  pdu->reset();

  // Byte align and pack the message bits for PDCP
  if((bit_buf.N_bits % 8) != 0)
  {
    for(int i=0; i<8-(bit_buf.N_bits % 8); i++)
      bit_buf.msg[bit_buf.N_bits + i] = 0;
    bit_buf.N_bits += 8 - (bit_buf.N_bits % 8);
  }
  srslte_bit_pack_vector(bit_buf.msg, pdu->msg, bit_buf.N_bits);
  pdu->N_bytes = bit_buf.N_bits/8;

  rrc_log->info("Sending UL Info Transfer\n");
  pdcp->write_sdu(lcid, pdu);
}

void rrc::send_security_mode_complete(uint32_t lcid, byte_buffer_t *pdu)
{
  rrc_log->debug("Preparing Security Mode Complete\n");
  LIBLTE_RRC_UL_DCCH_MSG_STRUCT ul_dcch_msg;
  ul_dcch_msg.msg_type = LIBLTE_RRC_UL_DCCH_MSG_TYPE_SECURITY_MODE_COMPLETE;
  ul_dcch_msg.msg.security_mode_complete.rrc_transaction_id = transaction_id;
  liblte_rrc_pack_ul_dcch_msg(&ul_dcch_msg, (LIBLTE_BIT_MSG_STRUCT*)&bit_buf);

  // Byte align and pack the message bits for PDCP
  if((bit_buf.N_bits % 8) != 0)
  {
    for(int i=0; i<8-(bit_buf.N_bits % 8); i++)
      bit_buf.msg[bit_buf.N_bits + i] = 0;
    bit_buf.N_bits += 8 - (bit_buf.N_bits % 8);
  }
  srslte_bit_pack_vector(bit_buf.msg, pdu->msg, bit_buf.N_bits);
  pdu->N_bytes = bit_buf.N_bits/8;

  rrc_log->info("Sending Security Mode Complete\n");
  pdcp->write_sdu(lcid, pdu);
}

void rrc::send_rrc_con_reconfig_complete(uint32_t lcid, byte_buffer_t *pdu)
{
  rrc_log->debug("Preparing RRC Connection Reconfig Complete\n");
  LIBLTE_RRC_UL_DCCH_MSG_STRUCT ul_dcch_msg;

  ul_dcch_msg.msg_type = LIBLTE_RRC_UL_DCCH_MSG_TYPE_RRC_CON_RECONFIG_COMPLETE;
  ul_dcch_msg.msg.rrc_con_reconfig_complete.rrc_transaction_id = transaction_id;
  liblte_rrc_pack_ul_dcch_msg(&ul_dcch_msg, (LIBLTE_BIT_MSG_STRUCT*)&bit_buf);

  // Byte align and pack the message bits for PDCP
  if((bit_buf.N_bits % 8) != 0)
  {
    for(int i=0; i<8-(bit_buf.N_bits % 8); i++)
      bit_buf.msg[bit_buf.N_bits + i] = 0;
    bit_buf.N_bits += 8 - (bit_buf.N_bits % 8);
  }
  srslte_bit_pack_vector(bit_buf.msg, pdu->msg, bit_buf.N_bits);
  pdu->N_bytes = bit_buf.N_bits/8;

  rrc_log->info("Sending RRC Connection Reconfig Complete\n");
  pdcp->write_sdu(lcid, pdu);
}

void rrc::enable_capabilities()
{
  phy->set_param(srsue::phy_interface_params::PUSCH_EN_64QAM,
                 sib2.rr_config_common_sib.pusch_cnfg.enable_64_qam);  
}

void rrc::send_rrc_ue_cap_info(uint32_t lcid, byte_buffer_t *pdu)
{
  rrc_log->debug("Preparing UE Capability Info\n");
  LIBLTE_RRC_UL_DCCH_MSG_STRUCT ul_dcch_msg;

  ul_dcch_msg.msg_type = LIBLTE_RRC_UL_DCCH_MSG_TYPE_UE_CAPABILITY_INFO;
  ul_dcch_msg.msg.ue_capability_info.rrc_transaction_id = transaction_id;

  LIBLTE_RRC_UE_CAPABILITY_INFORMATION_STRUCT *info = &ul_dcch_msg.msg.ue_capability_info;
  info->N_ue_caps = 1;
  info->ue_capability_rat[0].rat_type = LIBLTE_RRC_RAT_TYPE_EUTRA;

  LIBLTE_RRC_UE_EUTRA_CAPABILITY_STRUCT *cap = &info->ue_capability_rat[0].eutra_capability;
  cap->access_stratum_release = LIBLTE_RRC_ACCESS_STRATUM_RELEASE_REL9;
  cap->ue_category = SRSUE_UE_CATEGORY;

  cap->pdcp_params.max_rohc_ctxts_present = false;
  cap->pdcp_params.supported_rohc_profiles[0] = false;
  cap->pdcp_params.supported_rohc_profiles[1] = false;
  cap->pdcp_params.supported_rohc_profiles[2] = false;
  cap->pdcp_params.supported_rohc_profiles[3] = false;
  cap->pdcp_params.supported_rohc_profiles[4] = false;
  cap->pdcp_params.supported_rohc_profiles[5] = false;
  cap->pdcp_params.supported_rohc_profiles[6] = false;
  cap->pdcp_params.supported_rohc_profiles[7] = false;
  cap->pdcp_params.supported_rohc_profiles[8] = false;

  cap->phy_params.specific_ref_sigs_supported = false;
  cap->phy_params.tx_antenna_selection_supported = false;

  //TODO: Generate this from user input?
  cap->rf_params.N_supported_band_eutras = 3;
  cap->rf_params.supported_band_eutra[0].band_eutra = 3;
  cap->rf_params.supported_band_eutra[0].half_duplex = false;
  cap->rf_params.supported_band_eutra[1].band_eutra = 7;
  cap->rf_params.supported_band_eutra[1].half_duplex = false;
  cap->rf_params.supported_band_eutra[2].band_eutra = 20;
  cap->rf_params.supported_band_eutra[2].half_duplex = false;

  cap->meas_params.N_band_list_eutra = 3;
  cap->meas_params.band_list_eutra[0].N_inter_freq_need_for_gaps = 3;
  cap->meas_params.band_list_eutra[0].inter_freq_need_for_gaps[0] = true;
  cap->meas_params.band_list_eutra[0].inter_freq_need_for_gaps[1] = true;
  cap->meas_params.band_list_eutra[0].inter_freq_need_for_gaps[2] = true;
  cap->meas_params.band_list_eutra[1].N_inter_freq_need_for_gaps = 3;
  cap->meas_params.band_list_eutra[1].inter_freq_need_for_gaps[0] = true;
  cap->meas_params.band_list_eutra[1].inter_freq_need_for_gaps[1] = true;
  cap->meas_params.band_list_eutra[1].inter_freq_need_for_gaps[2] = true;
  cap->meas_params.band_list_eutra[2].N_inter_freq_need_for_gaps = 3;
  cap->meas_params.band_list_eutra[2].inter_freq_need_for_gaps[0] = true;
  cap->meas_params.band_list_eutra[2].inter_freq_need_for_gaps[1] = true;
  cap->meas_params.band_list_eutra[2].inter_freq_need_for_gaps[2] = true;

  cap->feature_group_indicator_present         = true;
  cap->feature_group_indicator                 = 0x62001000;
  cap->inter_rat_params.utra_fdd_present       = false;
  cap->inter_rat_params.utra_tdd128_present    = false;
  cap->inter_rat_params.utra_tdd384_present    = false;
  cap->inter_rat_params.utra_tdd768_present    = false;
  cap->inter_rat_params.geran_present          = false;
  cap->inter_rat_params.cdma2000_hrpd_present  = false;
  cap->inter_rat_params.cdma2000_1xrtt_present = false;

  liblte_rrc_pack_ul_dcch_msg(&ul_dcch_msg, (LIBLTE_BIT_MSG_STRUCT*)&bit_buf);

  // Byte align and pack the message bits for PDCP
  if((bit_buf.N_bits % 8) != 0)
  {
    for(int i=0; i<8-(bit_buf.N_bits % 8); i++)
      bit_buf.msg[bit_buf.N_bits + i] = 0;
    bit_buf.N_bits += 8 - (bit_buf.N_bits % 8);
  }
  srslte_bit_pack_vector(bit_buf.msg, pdu->msg, bit_buf.N_bits);
  pdu->N_bytes = bit_buf.N_bits/8;

  rrc_log->info("Sending UE Capability Info\n");
  pdcp->write_sdu(lcid, pdu);
}

/*******************************************************************************
  Parsers
*******************************************************************************/

void rrc::parse_dl_ccch(byte_buffer_t *pdu)
{
  srslte_bit_unpack_vector(pdu->msg, bit_buf.msg, pdu->N_bytes*8);
  bit_buf.N_bits = pdu->N_bytes*8;
  pool->deallocate(pdu);
  bzero(&dl_ccch_msg, sizeof(LIBLTE_RRC_DL_CCCH_MSG_STRUCT));
  liblte_rrc_unpack_dl_ccch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &dl_ccch_msg);

  rrc_log->info("SRB0 - Received %s\n",
                liblte_rrc_dl_ccch_msg_type_text[dl_ccch_msg.msg_type]);

  switch(dl_ccch_msg.msg_type)
  {
  case LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_REJ:
    rrc_log->info("Connection Reject received. Wait time: %d\n",
                  dl_ccch_msg.msg.rrc_con_rej.wait_time);
    state = RRC_STATE_IDLE;
    break;
  case LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_SETUP:
    rrc_log->info("Connection Setup received\n");
    handle_con_setup(&dl_ccch_msg.msg.rrc_con_setup);
    rrc_log->info("Notifying NAS of connection setup\n");
    state = RRC_STATE_COMPLETING_SETUP;
    nas->notify_connection_setup();
    break;
  case LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_REEST:
    rrc_log->error("Not handling Connection Reestablishment message");
    break;
  case LIBLTE_RRC_DL_CCCH_MSG_TYPE_RRC_CON_REEST_REJ:
    rrc_log->error("Not handling Connection Reestablishment Reject message");
    break;
  default:
    break;
  }
}

void rrc::parse_dl_dcch(uint32_t lcid, byte_buffer_t *pdu)
{
  srslte_bit_unpack_vector(pdu->msg, bit_buf.msg, pdu->N_bytes*8);
  bit_buf.N_bits = pdu->N_bytes*8;
  liblte_rrc_unpack_dl_dcch_msg((LIBLTE_BIT_MSG_STRUCT*)&bit_buf, &dl_dcch_msg);

  rrc_log->info("%s - Received %s\n",
                rb_id_text[lcid],
                liblte_rrc_dl_dcch_msg_type_text[dl_dcch_msg.msg_type]);

  // Reset and reuse pdu buffer if possible
  pdu->reset();

  switch(dl_dcch_msg.msg_type)
  {
  case LIBLTE_RRC_DL_DCCH_MSG_TYPE_DL_INFO_TRANSFER:
    memcpy(pdu->msg, dl_dcch_msg.msg.dl_info_transfer.dedicated_info.msg, dl_dcch_msg.msg.dl_info_transfer.dedicated_info.N_bytes);
    pdu->N_bytes = dl_dcch_msg.msg.dl_info_transfer.dedicated_info.N_bytes;
    nas->write_pdu(lcid, pdu);
    break;
  case LIBLTE_RRC_DL_DCCH_MSG_TYPE_SECURITY_MODE_COMMAND:
    transaction_id =  dl_dcch_msg.msg.security_mode_cmd.rrc_transaction_id;

    // TODO: Set algorithms correctly in PDCP
    // TODO: We currently only support EEA0 and EIA2 & they're hardcoded in PDCP
    //LIBLTE_RRC_CIPHERING_ALGORITHM_ENUM ciph        = dl_dcch_msg.msg.security_mode_cmd.sec_algs.cipher_alg;
    //LIBLTE_RRC_INTEGRITY_PROT_ALGORITHM_ENUM integ  = dl_dcch_msg.msg.security_mode_cmd.sec_algs.int_alg;

    // Configure PDCP for security
    usim->generate_as_keys(nas->get_ul_count(), k_rrc_enc, k_rrc_int, k_up_enc, k_up_int);
    pdcp->config_security(lcid, k_rrc_enc, k_rrc_int);
    send_security_mode_complete(lcid, pdu);
    break;
  case LIBLTE_RRC_DL_DCCH_MSG_TYPE_RRC_CON_RECONFIG:
    transaction_id = dl_dcch_msg.msg.security_mode_cmd.rrc_transaction_id;
    handle_rrc_con_reconfig(lcid, &dl_dcch_msg.msg.rrc_con_reconfig, pdu);
    break;
  case LIBLTE_RRC_DL_DCCH_MSG_TYPE_UE_CAPABILITY_ENQUIRY:
    transaction_id = dl_dcch_msg.msg.ue_cap_enquiry.rrc_transaction_id;
    for(int i=0; i<dl_dcch_msg.msg.ue_cap_enquiry.N_ue_cap_reqs; i++)
    {
      if(LIBLTE_RRC_RAT_TYPE_EUTRA == dl_dcch_msg.msg.ue_cap_enquiry.ue_capability_request[i])
      {
        send_rrc_ue_cap_info(lcid, pdu);
        break;
      }
    }
    break;
  case LIBLTE_RRC_DL_DCCH_MSG_TYPE_RRC_CON_RELEASE:
    rrc_connection_release();
    break;
  default:
    break;
  }
}

/*******************************************************************************
  Helpers
*******************************************************************************/

void rrc::rrc_connection_release() {
    drb_up = false;
    state  = RRC_STATE_IDLE;
    mac->reset();
    rlc->reset();
    pdcp->reset();
    rrc_log->console("RRC Connection released.\n");
    mac->pcch_start_rx();
}

void* rrc::start_sib_thread(void *rrc_)
{
  rrc *r = (rrc*)rrc_;
  r->sib_search();
}

void rrc::sib_search()
{
  bool      searching = true;
  uint32_t  tti ;
  uint32_t  si_win_start, si_win_len;
  uint16_t  period;

  while(searching)
  {
    switch(state)
    {
    case RRC_STATE_SIB1_SEARCH:
      // Instruct MAC to look for SIB1
      while(!phy->status_is_sync()){
        usleep(50000);
      }
      usleep(10000); 
      tti          = mac->get_current_tti();
      si_win_start = sib_start_tti(tti, 2, 5);
      mac->bcch_start_rx(si_win_start, 1);
      rrc_log->debug("Instructed MAC to search for SIB1, win_start=%d, win_len=%d\n",
                     si_win_start, 1);

      break;
    case RRC_STATE_SIB2_SEARCH:
      // Instruct MAC to look for SIB2
      usleep(10000);
      tti          = mac->get_current_tti();
      period       = liblte_rrc_si_periodicity_num[sib1.sched_info[0].si_periodicity];
      si_win_start = sib_start_tti(tti, period, 0);
      si_win_len   = liblte_rrc_si_window_length_num[sib1.si_window_length];

      mac->bcch_start_rx(si_win_start, si_win_len);
      rrc_log->debug("Instructed MAC to search for SIB2, win_start=%d, win_len=%d\n",
                     si_win_start, si_win_len);

      break;
    default:
      searching = false;
      break;
    }
    usleep(100000);
  }
}

// Determine SI messages scheduling as in 36.331 5.2.3 Acquisition of an SI message
uint32_t rrc::sib_start_tti(uint32_t tti, uint32_t period, uint32_t x) {
  return (period*10*(1+tti/(period*10))+x)%10240; // the 1 means next opportunity
}

void rrc::apply_sib2_configs()
{
  if(RRC_STATE_WAIT_FOR_CON_SETUP != state){
    rrc_log->error("State must be RRC_STATE_WAIT_FOR_CON_SETUP to handle SIB2. Actual state: %s\n",
                   rrc_state_text[state]);
    return;
  }

  // RACH-CONFIGCOMMON
  if (sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.present) {
    mac->set_param(srsue::mac_interface_params::RA_NOFGROUPAPREAMBLES,
                   liblte_rrc_message_size_group_a_num[sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.size_of_ra]);
    mac->set_param(srsue::mac_interface_params::RA_MESSAGESIZEA,
                   liblte_rrc_message_size_group_a_num[sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.msg_size]);
    mac->set_param(srsue::mac_interface_params::RA_MESSAGEPOWEROFFSETB,
                   liblte_rrc_message_power_offset_group_b_num[sib2.rr_config_common_sib.rach_cnfg.preambles_group_a_cnfg.msg_pwr_offset_group_b]);
  }
  mac->set_param(srsue::mac_interface_params::RA_NOFPREAMBLES,
                 liblte_rrc_number_of_ra_preambles_num[sib2.rr_config_common_sib.rach_cnfg.num_ra_preambles]);
  mac->set_param(srsue::mac_interface_params::RA_POWERRAMPINGSTEP,
                 liblte_rrc_power_ramping_step_num[sib2.rr_config_common_sib.rach_cnfg.pwr_ramping_step]);
  mac->set_param(srsue::mac_interface_params::RA_INITRECEIVEDPOWER,
                 liblte_rrc_preamble_initial_received_target_power_num[sib2.rr_config_common_sib.rach_cnfg.preamble_init_rx_target_pwr]);
  mac->set_param(srsue::mac_interface_params::RA_PREAMBLETRANSMAX,
                 liblte_rrc_preamble_trans_max_num[sib2.rr_config_common_sib.rach_cnfg.preamble_trans_max]);
  mac->set_param(srsue::mac_interface_params::RA_RESPONSEWINDOW,
                 liblte_rrc_ra_response_window_size_num[sib2.rr_config_common_sib.rach_cnfg.ra_resp_win_size]);
  mac->set_param(srsue::mac_interface_params::RA_CONTENTIONTIMER,
                 liblte_rrc_mac_contention_resolution_timer_num[sib2.rr_config_common_sib.rach_cnfg.mac_con_res_timer]);
  mac->set_param(srsue::mac_interface_params::HARQ_MAXMSG3TX,
                 sib2.rr_config_common_sib.rach_cnfg.max_harq_msg3_tx);

  rrc_log->info("Set RACH ConfigCommon: NofPreambles=%d, ResponseWindow=%d, ContentionResolutionTimer=%d ms\n",
         liblte_rrc_number_of_ra_preambles_num[sib2.rr_config_common_sib.rach_cnfg.num_ra_preambles],
         liblte_rrc_ra_response_window_size_num[sib2.rr_config_common_sib.rach_cnfg.ra_resp_win_size],
         liblte_rrc_mac_contention_resolution_timer_num[sib2.rr_config_common_sib.rach_cnfg.mac_con_res_timer]);

  // PDSCH ConfigCommon
  phy->set_param(srsue::phy_interface_params::PDSCH_RSPOWER,
                 sib2.rr_config_common_sib.pdsch_cnfg.rs_power);
  phy->set_param(srsue::phy_interface_params::PDSCH_PB,
                 sib2.rr_config_common_sib.pdsch_cnfg.p_b);

  // PUSCH ConfigCommon
  phy->set_param(srsue::phy_interface_params::PUSCH_EN_64QAM, 0); // This will be set after attach
  phy->set_param(srsue::phy_interface_params::PUSCH_HOPPING_OFFSET,
                 sib2.rr_config_common_sib.pusch_cnfg.pusch_hopping_offset);
  phy->set_param(srsue::phy_interface_params::PUSCH_HOPPING_N_SB,
                 sib2.rr_config_common_sib.pusch_cnfg.n_sb);
  phy->set_param(srsue::phy_interface_params::PUSCH_HOPPING_INTRA_SF,
                 sib2.rr_config_common_sib.pusch_cnfg.hopping_mode == LIBLTE_RRC_HOPPING_MODE_INTRA_AND_INTER_SUBFRAME?1:0);
  phy->set_param(srsue::phy_interface_params::DMRS_GROUP_HOPPING_EN,
                 sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_hopping_enabled?1:0);
  phy->set_param(srsue::phy_interface_params::DMRS_SEQUENCE_HOPPING_EN,
                 sib2.rr_config_common_sib.pusch_cnfg.ul_rs.sequence_hopping_enabled?1:0);
  phy->set_param(srsue::phy_interface_params::PUSCH_RS_CYCLIC_SHIFT,
                 sib2.rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift);
  phy->set_param(srsue::phy_interface_params::PUSCH_RS_GROUP_ASSIGNMENT,
                 sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch);

  rrc_log->info("Set PUSCH ConfigCommon: HopOffset=%d, RSGroup=%d, RSNcs=%d, N_sb=%d\n",
    sib2.rr_config_common_sib.pusch_cnfg.pusch_hopping_offset,
    sib2.rr_config_common_sib.pusch_cnfg.ul_rs.group_assignment_pusch,
    sib2.rr_config_common_sib.pusch_cnfg.ul_rs.cyclic_shift,
    sib2.rr_config_common_sib.pusch_cnfg.n_sb);

  // PUCCH ConfigCommon
  phy->set_param(srsue::phy_interface_params::PUCCH_DELTA_SHIFT,
                 liblte_rrc_delta_pucch_shift_num[sib2.rr_config_common_sib.pucch_cnfg.delta_pucch_shift]);
  phy->set_param(srsue::phy_interface_params::PUCCH_CYCLIC_SHIFT,
                 sib2.rr_config_common_sib.pucch_cnfg.n_cs_an);
  phy->set_param(srsue::phy_interface_params::PUCCH_N_PUCCH_1,
                 sib2.rr_config_common_sib.pucch_cnfg.n1_pucch_an);
  phy->set_param(srsue::phy_interface_params::PUCCH_N_RB_2,
                 sib2.rr_config_common_sib.pucch_cnfg.n_rb_cqi);

  rrc_log->info("Set PUCCH ConfigCommon: DeltaShift=%d, CyclicShift=%d, N1=%d, NRB=%d\n",
         liblte_rrc_delta_pucch_shift_num[sib2.rr_config_common_sib.pucch_cnfg.delta_pucch_shift],
         sib2.rr_config_common_sib.pucch_cnfg.n_cs_an,
         sib2.rr_config_common_sib.pucch_cnfg.n1_pucch_an,
         sib2.rr_config_common_sib.pucch_cnfg.n_rb_cqi);

  // UL Power control config ConfigCommon
    phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_NOMINAL_PUSCH, sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pusch);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_ALPHA, 
                round(10*liblte_rrc_ul_power_control_alpha_num[sib2.rr_config_common_sib.ul_pwr_ctrl.alpha]));
  phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_NOMINAL_PUCCH, sib2.rr_config_common_sib.ul_pwr_ctrl.p0_nominal_pucch);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F1, 
                liblte_rrc_delta_f_pucch_format_1_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1]);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F1B, 
                liblte_rrc_delta_f_pucch_format_1b_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_1b]);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F2, 
                liblte_rrc_delta_f_pucch_format_2_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2]);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F2A, 
                liblte_rrc_delta_f_pucch_format_2a_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2a]);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_PUCCH_F2B, 
                liblte_rrc_delta_f_pucch_format_2b_num[sib2.rr_config_common_sib.ul_pwr_ctrl.delta_flist_pucch.format_2b]);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_MSG3, sib2.rr_config_common_sib.ul_pwr_ctrl.delta_preamble_msg3);

  
  // PRACH Configcommon
  phy->set_param(srsue::phy_interface_params::PRACH_ROOT_SEQ_IDX,
                 sib2.rr_config_common_sib.prach_cnfg.root_sequence_index);
  phy->set_param(srsue::phy_interface_params::PRACH_HIGH_SPEED_FLAG,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag?1:0);
  phy->set_param(srsue::phy_interface_params::PRACH_FREQ_OFFSET,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset);
  phy->set_param(srsue::phy_interface_params::PRACH_ZC_CONFIG,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config);
  phy->set_param(srsue::phy_interface_params::PRACH_CONFIG_INDEX,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index);

  rrc_log->info("Set PRACH ConfigCommon: SeqIdx=%d, HS=%d, FreqOffset=%d, ZC=%d, ConfigIndex=%d\n",
                 sib2.rr_config_common_sib.prach_cnfg.root_sequence_index,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.high_speed_flag?1:0,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_freq_offset,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.zero_correlation_zone_config,
                 sib2.rr_config_common_sib.prach_cnfg.prach_cnfg_info.prach_config_index);

  // SRS ConfigCommon
  if (sib2.rr_config_common_sib.srs_ul_cnfg.present) {
    phy->set_param(srsue::phy_interface_params::SRS_CS_BWCFG, sib2.rr_config_common_sib.srs_ul_cnfg.bw_cnfg);
    phy->set_param(srsue::phy_interface_params::SRS_CS_SFCFG, sib2.rr_config_common_sib.srs_ul_cnfg.subfr_cnfg);
    phy->set_param(srsue::phy_interface_params::SRS_CS_ACKNACKSIMUL, sib2.rr_config_common_sib.srs_ul_cnfg.ack_nack_simul_tx);
  }

  rrc_log->info("Set SRS ConfigCommon: BW-Configuration=%d, SF-Configuration=%d, ACKNACK=%d\n",
                sib2.rr_config_common_sib.srs_ul_cnfg.bw_cnfg,
                sib2.rr_config_common_sib.srs_ul_cnfg.subfr_cnfg,
                sib2.rr_config_common_sib.srs_ul_cnfg.ack_nack_simul_tx);

  phy->configure_ul_params();
}

void rrc::handle_con_setup(LIBLTE_RRC_CONNECTION_SETUP_STRUCT *setup)
{
  // PHY CONFIG DEDICATED Defaults (3GPP 36.331 v10 9.2.4)
  phy->set_param(srsue::phy_interface_params::UCI_I_OFFSET_ACK, 10);
  phy->set_param(srsue::phy_interface_params::UCI_I_OFFSET_CQI, 15);
  phy->set_param(srsue::phy_interface_params::UCI_I_OFFSET_RI, 12);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_UE_PUSCH, 0);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_MCS_EN, 0);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_ACC_EN, 0);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_UE_PUCCH, 0);
  phy->set_param(srsue::phy_interface_params::PWRCTRL_SRS_OFFSET, 7);

  LIBLTE_RRC_RR_CONFIG_DEDICATED_STRUCT *cnfg = &setup->rr_cnfg;
  if(cnfg->phy_cnfg_ded_present)
  {
      // PHY CONFIG DEDICATED
      LIBLTE_RRC_PHYSICAL_CONFIG_DEDICATED_STRUCT *phy_cnfg = &cnfg->phy_cnfg_ded;
      if(phy_cnfg->pucch_cnfg_ded_present)
      {
        //TODO
      }
      if(phy_cnfg->pusch_cnfg_ded_present)
      {
        phy->set_param(srsue::phy_interface_params::UCI_I_OFFSET_ACK,
                       phy_cnfg->pusch_cnfg_ded.beta_offset_ack_idx);
        phy->set_param(srsue::phy_interface_params::UCI_I_OFFSET_CQI,
                       phy_cnfg->pusch_cnfg_ded.beta_offset_cqi_idx);
        phy->set_param(srsue::phy_interface_params::UCI_I_OFFSET_RI,
                       phy_cnfg->pusch_cnfg_ded.beta_offset_ri_idx);
      }
      if(phy_cnfg->ul_pwr_ctrl_ded_present)
      {
        phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_UE_PUSCH, phy_cnfg->ul_pwr_ctrl_ded.p0_ue_pusch);
        phy->set_param(srsue::phy_interface_params::PWRCTRL_DELTA_MCS_EN, 
                      phy_cnfg->ul_pwr_ctrl_ded.delta_mcs_en==LIBLTE_RRC_DELTA_MCS_ENABLED_EN0?0:1);
        phy->set_param(srsue::phy_interface_params::PWRCTRL_ACC_EN, 
                      phy_cnfg->ul_pwr_ctrl_ded.accumulation_en==false?0:1);
        phy->set_param(srsue::phy_interface_params::PWRCTRL_P0_UE_PUCCH, phy_cnfg->ul_pwr_ctrl_ded.p0_ue_pucch);
        phy->set_param(srsue::phy_interface_params::PWRCTRL_SRS_OFFSET, phy_cnfg->ul_pwr_ctrl_ded.p_srs_offset);
      }
      if(phy_cnfg->tpc_pdcch_cnfg_pucch_present)
      {
          //TODO
      }
      if(phy_cnfg->tpc_pdcch_cnfg_pusch_present)
      {
          //TODO
      }
      if(phy_cnfg->cqi_report_cnfg_present)
      {
        if (phy_cnfg->cqi_report_cnfg.report_periodic_present) {
          phy->set_param(srsue::phy_interface_params::PUCCH_N_PUCCH_2,
                        phy_cnfg->cqi_report_cnfg.report_periodic.pucch_resource_idx);
          phy->set_param(srsue::phy_interface_params::CQI_PERIODIC_PMI_IDX,
                        phy_cnfg->cqi_report_cnfg.report_periodic.pmi_cnfg_idx);
          phy->set_param(srsue::phy_interface_params::CQI_PERIODIC_SIMULT_ACK,
                        phy_cnfg->cqi_report_cnfg.report_periodic.simult_ack_nack_and_cqi?1:0);
          phy->set_param(srsue::phy_interface_params::CQI_PERIODIC_FORMAT_SUBBAND,
                        phy_cnfg->cqi_report_cnfg.report_periodic.format_ind_periodic ==
                        LIBLTE_RRC_CQI_FORMAT_INDICATOR_PERIODIC_SUBBAND_CQI);
          phy->set_param(srsue::phy_interface_params::CQI_PERIODIC_FORMAT_SUBBAND_K,
                        phy_cnfg->cqi_report_cnfg.report_periodic.format_ind_periodic_subband_k);

          phy->set_param(srsue::phy_interface_params::CQI_PERIODIC_CONFIGURED, 1);
        }
      }
      if(phy_cnfg->srs_ul_cnfg_ded_present && phy_cnfg->srs_ul_cnfg_ded.setup_present)
      {
          phy->set_param(srsue::phy_interface_params::SRS_UE_CS,
                         phy_cnfg->srs_ul_cnfg_ded.cyclic_shift);
          phy->set_param(srsue::phy_interface_params::SRS_UE_DURATION,
                         phy_cnfg->srs_ul_cnfg_ded.duration);
          phy->set_param(srsue::phy_interface_params::SRS_UE_NRRC,
                         phy_cnfg->srs_ul_cnfg_ded.freq_domain_pos);
          phy->set_param(srsue::phy_interface_params::SRS_UE_BW,
                         phy_cnfg->srs_ul_cnfg_ded.srs_bandwidth);
          phy->set_param(srsue::phy_interface_params::SRS_UE_CONFIGINDEX,
                         phy_cnfg->srs_ul_cnfg_ded.srs_cnfg_idx);
          phy->set_param(srsue::phy_interface_params::SRS_UE_HOP,
                         phy_cnfg->srs_ul_cnfg_ded.srs_hopping_bandwidth);
          phy->set_param(srsue::phy_interface_params::SRS_UE_CYCLICSHIFT,
                         phy_cnfg->srs_ul_cnfg_ded.cyclic_shift);
          phy->set_param(srsue::phy_interface_params::SRS_UE_TXCOMB,
                         phy_cnfg->srs_ul_cnfg_ded.tx_comb);
          phy->set_param(srsue::phy_interface_params::SRS_IS_CONFIGURED, 1);
      }
      if(phy_cnfg->antenna_info_present)
      {
          //TODO
      }
      if(phy_cnfg->sched_request_cnfg_present)
      {
          if(phy_cnfg->sched_request_cnfg.setup_present)
          {
              phy->set_param(srsue::phy_interface_params::PUCCH_N_PUCCH_SR,
                             phy_cnfg->sched_request_cnfg.sr_pucch_resource_idx);
              phy->set_param(srsue::phy_interface_params::SR_CONFIG_INDEX,
                             phy_cnfg->sched_request_cnfg.sr_cnfg_idx);
              mac->set_param(srsue::mac_interface_params::SR_TRANS_MAX,
                            liblte_rrc_dsr_trans_max_num[phy_cnfg->sched_request_cnfg.dsr_trans_max]);
              mac->set_param(srsue::mac_interface_params::SR_PUCCH_CONFIGURED, 1);
          }
      }
      if(phy_cnfg->pdsch_cnfg_ded_present)
      {
          //TODO
      }

      phy->configure_ul_params();

      rrc_log->info("Set PHY config ded: SR-n_pucch=%d, SR-ConfigIndex=%d, SR-TransMax=%d, SRS-ConfigIndex=%d, SRS-bw=%d, SRS-Nrcc=%d, SRS-hop=%d, SRS-Ncs=%d\n",
                   phy_cnfg->sched_request_cnfg.sr_pucch_resource_idx,
                   phy_cnfg->sched_request_cnfg.sr_cnfg_idx,
                   liblte_rrc_dsr_trans_max_num[phy_cnfg->sched_request_cnfg.dsr_trans_max],
                   phy_cnfg->srs_ul_cnfg_ded.srs_cnfg_idx,
                   phy_cnfg->srs_ul_cnfg_ded.srs_bandwidth,
                   phy_cnfg->srs_ul_cnfg_ded.freq_domain_pos,
                   phy_cnfg->srs_ul_cnfg_ded.srs_hopping_bandwidth,
                   phy_cnfg->srs_ul_cnfg_ded.cyclic_shift);
  }

  // MAC MAIN CONFIG Defaults (3GPP 36.331 v10 9.2.2)
  mac->set_param(srsue::mac_interface_params::HARQ_MAXTX, 5);
  mac->set_param(srsue::mac_interface_params::BSR_TIMER_PERIODIC, -1);
  mac->set_param(srsue::mac_interface_params::BSR_TIMER_RETX, 2560);

  if(cnfg->mac_main_cnfg_present && !cnfg->mac_main_cnfg.default_value)
  {
    // MAC MAIN CONFIG
    LIBLTE_RRC_MAC_MAIN_CONFIG_STRUCT *mac_cnfg = &cnfg->mac_main_cnfg.explicit_value;
    if(mac_cnfg->ulsch_cnfg_present)
    {
      if(mac_cnfg->ulsch_cnfg.max_harq_tx_present)
      {
        mac->set_param(srsue::mac_interface_params::HARQ_MAXTX,
                       liblte_rrc_max_harq_tx_num[mac_cnfg->ulsch_cnfg.max_harq_tx]);
      }
      if(mac_cnfg->ulsch_cnfg.periodic_bsr_timer_present)
      {
        mac->set_param(srsue::mac_interface_params::BSR_TIMER_PERIODIC,
                       liblte_rrc_periodic_bsr_timer_num[mac_cnfg->ulsch_cnfg.periodic_bsr_timer]);
      }
      mac->set_param(srsue::mac_interface_params::BSR_TIMER_RETX,
                     liblte_rrc_retransmission_bsr_timer_num[mac_cnfg->ulsch_cnfg.retx_bsr_timer]);
      //TODO: tti_bundling?
    }
    if(mac_cnfg->drx_cnfg_present)
    {
      //TODO
    }
    if(mac_cnfg->phr_cnfg_present)
    {
      mac->set_param(srsue::mac_interface_params::PHR_TIMER_PERIODIC, liblte_rrc_periodic_phr_timer_num[mac_cnfg->phr_cnfg.periodic_phr_timer]);
      mac->set_param(srsue::mac_interface_params::PHR_TIMER_PROHIBIT, liblte_rrc_prohibit_phr_timer_num[mac_cnfg->phr_cnfg.prohibit_phr_timer]);
      mac->set_param(srsue::mac_interface_params::PHR_DL_PATHLOSS_CHANGE, liblte_rrc_dl_pathloss_change_num[mac_cnfg->phr_cnfg.dl_pathloss_change]);
    }
    //TODO: time_alignment_timer?

    rrc_log->info("Set MAC main config: harq-MaxReTX=%d, bsr-TimerReTX=%d, bsr-TimerPeriodic=%d\n",
                 liblte_rrc_max_harq_tx_num[mac_cnfg->ulsch_cnfg.max_harq_tx],
                 liblte_rrc_retransmission_bsr_timer_num[mac_cnfg->ulsch_cnfg.retx_bsr_timer],
                 liblte_rrc_periodic_bsr_timer_num[mac_cnfg->ulsch_cnfg.periodic_bsr_timer]);
  }

  if(setup->rr_cnfg.sps_cnfg_present)
  {
    //TODO
  }
  if(setup->rr_cnfg.rlf_timers_and_constants_present)
  {
    //TODO
  }
  for(int i=0; i<setup->rr_cnfg.srb_to_add_mod_list_size; i++)
  {
    // TODO: handle SRB modification
    add_srb(&setup->rr_cnfg.srb_to_add_mod_list[i]);
  }
  for(int i=0; i<setup->rr_cnfg.drb_to_add_mod_list_size; i++)
  {
    // TODO: handle DRB modification
    add_drb(&setup->rr_cnfg.drb_to_add_mod_list[i]);
  }
}

void rrc::handle_rrc_con_reconfig(uint32_t lcid, LIBLTE_RRC_CONNECTION_RECONFIGURATION_STRUCT *reconfig, byte_buffer_t *pdu)
{
  uint32_t i;

  if(reconfig->meas_cnfg_present)
  {
    //TODO: handle meas_cnfg
  }
  if(reconfig->mob_ctrl_info_present)
  {
    //TODO: handle mob_ctrl_info
  }

  if(reconfig->rr_cnfg_ded_present)
  {
    uint32_t n_srb = reconfig->rr_cnfg_ded.srb_to_add_mod_list_size;
    for(i=0; i<n_srb; i++)
    {
      add_srb(&reconfig->rr_cnfg_ded.srb_to_add_mod_list[i]);
    }

    uint32_t n_drb_rel = reconfig->rr_cnfg_ded.drb_to_release_list_size;
    for(i=0; i<n_drb_rel; i++)
    {
      release_drb(reconfig->rr_cnfg_ded.drb_to_release_list[i]);
    }

    uint32_t n_drb = reconfig->rr_cnfg_ded.drb_to_add_mod_list_size;
    for(i=0; i<n_drb; i++)
    {
      add_drb(&reconfig->rr_cnfg_ded.drb_to_add_mod_list[i]);
    }
  }

  send_rrc_con_reconfig_complete(lcid, pdu);

  byte_buffer_t *nas_sdu;
  for(i=0;i<reconfig->N_ded_info_nas;i++)
  {
    nas_sdu = pool->allocate();
    memcpy(nas_sdu->msg, &reconfig->ded_info_nas_list[i].msg, reconfig->ded_info_nas_list[i].N_bytes);
    nas_sdu->N_bytes = reconfig->ded_info_nas_list[i].N_bytes;
    nas->write_pdu(lcid, nas_sdu);
  }
}

void rrc::add_srb(LIBLTE_RRC_SRB_TO_ADD_MOD_STRUCT *srb_cnfg)
{
  // Setup PDCP
  pdcp->add_bearer(srb_cnfg->srb_id);
  if(RB_ID_SRB2 == srb_cnfg->srb_id)
    pdcp->config_security(srb_cnfg->srb_id, k_rrc_enc, k_rrc_int);

  // Setup RLC
  if(srb_cnfg->rlc_cnfg_present)
  {
    if(srb_cnfg->rlc_default_cnfg_present)
    {
      rlc->add_bearer(srb_cnfg->srb_id);
    }else{
      rlc->add_bearer(srb_cnfg->srb_id, &srb_cnfg->rlc_explicit_cnfg);
    }
  }

  // Setup MAC
  uint8_t  log_chan_group       =  0;
  uint8_t  priority             =  1;
  int      prioritized_bit_rate = -1;
  int      bucket_size_duration = -1;

  if(srb_cnfg->lc_cnfg_present)
  {
    if(srb_cnfg->lc_default_cnfg_present)
    {
      if(RB_ID_SRB2 == srb_cnfg->srb_id)
        priority = 3;
    }else{
      if(srb_cnfg->lc_explicit_cnfg.log_chan_sr_mask_present)
      {
        //TODO
      }
      if(srb_cnfg->lc_explicit_cnfg.ul_specific_params_present)
      {
        if(srb_cnfg->lc_explicit_cnfg.ul_specific_params.log_chan_group_present)
          log_chan_group      = srb_cnfg->lc_explicit_cnfg.ul_specific_params.log_chan_group;

        priority              = srb_cnfg->lc_explicit_cnfg.ul_specific_params.priority;
        prioritized_bit_rate  = liblte_rrc_prioritized_bit_rate_num[srb_cnfg->lc_explicit_cnfg.ul_specific_params.prioritized_bit_rate];
        bucket_size_duration  = liblte_rrc_bucket_size_duration_num[srb_cnfg->lc_explicit_cnfg.ul_specific_params.bucket_size_duration];
      }
    }
    mac->setup_lcid(srb_cnfg->srb_id, log_chan_group, priority, prioritized_bit_rate, bucket_size_duration);
  }

  srbs[srb_cnfg->srb_id] = *srb_cnfg;
  rrc_log->info("Added radio bearer %s\n", rb_id_text[srb_cnfg->srb_id]);
}

void rrc::add_drb(LIBLTE_RRC_DRB_TO_ADD_MOD_STRUCT *drb_cnfg)
{

  if(!drb_cnfg->pdcp_cnfg_present ||
     !drb_cnfg->rlc_cnfg_present  ||
     !drb_cnfg->lc_cnfg_present)
  {
    rrc_log->error("Cannot add DRB - incomplete configuration\n");
    return;
  }

  uint32_t lcid = RB_ID_SRB2 + drb_cnfg->drb_id;

  // Setup PDCP
  pdcp->add_bearer(lcid, &drb_cnfg->pdcp_cnfg);
  // TODO: setup PDCP security (using k_up_enc)

  // Setup RLC
  rlc->add_bearer(lcid, &drb_cnfg->rlc_cnfg);

  // Setup MAC
  uint8_t  log_chan_group       =  0;
  uint8_t  priority             =  1;
  int      prioritized_bit_rate = -1;
  int      bucket_size_duration = -1;
  if(drb_cnfg->lc_cnfg.ul_specific_params_present)
  {
    if(drb_cnfg->lc_cnfg.ul_specific_params.log_chan_group_present)
      log_chan_group      = drb_cnfg->lc_cnfg.ul_specific_params.log_chan_group;

    priority              = drb_cnfg->lc_cnfg.ul_specific_params.priority;
    prioritized_bit_rate  = liblte_rrc_prioritized_bit_rate_num[drb_cnfg->lc_cnfg.ul_specific_params.prioritized_bit_rate];
    bucket_size_duration  = liblte_rrc_bucket_size_duration_num[drb_cnfg->lc_cnfg.ul_specific_params.bucket_size_duration];
  }
  //mac->setup_lcid(lcid, log_chan_group, priority, prioritized_bit_rate, bucket_size_duration);
  mac->setup_lcid(lcid, 3, 2, prioritized_bit_rate, bucket_size_duration);

  drbs[lcid] = *drb_cnfg;
  drb_up     = true;
  rrc_log->info("Added radio bearer %s\n", rb_id_text[lcid]);
}

void rrc::release_drb(uint8_t lcid)
{
  // TODO
}

} // namespace srsue
