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


#include "upper/rlc_am.h"

#define MOD 1024
#define RX_MOD_BASE(x) (x-vr_r)%1024
#define TX_MOD_BASE(x) (x-vt_a)%1024

using namespace srslte;

namespace srsue{

rlc_am::rlc_am()
{
  tx_sdu = NULL;
  rx_sdu = NULL;
  pool = buffer_pool::get_instance();

  vt_a    = 0;
  vt_ms   = RLC_AM_WINDOW_SIZE;
  vt_s    = 0;
  poll_sn = 0;

  vr_r    = 0;
  vr_mr   = RLC_AM_WINDOW_SIZE;
  vr_x    = 0;
  vr_ms   = 0;
  vr_h    = 0;

  pdu_without_poll  = 0;
  byte_without_poll = 0;

  poll_received = false;
  do_status     = false;
}

void rlc_am::init(srslte::log          *log_,
                  uint32_t              lcid_,
                  pdcp_interface_rlc   *pdcp_,
                  rrc_interface_rlc    *rrc_,
                  mac_interface_timers *mac_timers)
{
  log  = log_;
  lcid = lcid_;
  pdcp = pdcp_;
  rrc  = rrc_;
}

void rlc_am::configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg)
{
  t_poll_retx       = liblte_rrc_t_poll_retransmit_num[cnfg->ul_am_rlc.t_poll_retx];
  poll_pdu          = liblte_rrc_poll_pdu_num[cnfg->ul_am_rlc.poll_pdu];
  poll_byte         = liblte_rrc_poll_byte_num[cnfg->ul_am_rlc.poll_byte]*1000; // KB
  max_retx_thresh   = liblte_rrc_max_retx_threshold_num[cnfg->ul_am_rlc.max_retx_thresh];

  t_reordering      = liblte_rrc_t_reordering_num[cnfg->dl_am_rlc.t_reordering];
  t_status_prohibit = liblte_rrc_t_status_prohibit_num[cnfg->dl_am_rlc.t_status_prohibit];

  log->info("%s configured: t_poll_retx=%d, poll_pdu=%d, poll_byte=%d, max_retx_thresh=%d, "
            "t_reordering=%d, t_status_prohibit=%d\n",
            rb_id_text[lcid], t_poll_retx, poll_pdu, poll_byte, max_retx_thresh,
            t_reordering, t_status_prohibit);
}

void rlc_am::reset()
{
  reordering_timeout.reset();
  if(tx_sdu)
    tx_sdu->reset();
  if(rx_sdu)
    rx_sdu->reset();

  vt_a    = 0;
  vt_ms   = RLC_AM_WINDOW_SIZE;
  vt_s    = 0;
  poll_sn = 0;

  vr_r    = 0;
  vr_mr   = RLC_AM_WINDOW_SIZE;
  vr_x    = 0;
  vr_ms   = 0;
  vr_h    = 0;

  pdu_without_poll  = 0;
  byte_without_poll = 0;

  poll_received = false;
  do_status     = false;

  // Drop all messages in TX SDU queue
  byte_buffer_t *buf;
  while(tx_sdu_queue.size() > 0) {
    tx_sdu_queue.read(&buf);
    pool->deallocate(buf);
  }

  // Drop all messages in RX window
  std::map<uint32_t, rlc_amd_rx_pdu_t>::iterator rxit;
  for(rxit = rx_window.begin(); rxit != rx_window.end(); rxit++) {
    pool->deallocate(rxit->second.buf);
  }
  rx_window.clear();

  // Drop all messages in TX window
  std::map<uint32_t, rlc_amd_tx_pdu_t>::iterator txit;
  for(txit = tx_window.begin(); txit != tx_window.end(); txit++) {
    pool->deallocate(txit->second.buf);
  }
  tx_window.clear();

  // Drop all messages in RETX queue
  while(retx_queue.size() > 0)
    retx_queue.pop();

}

rlc_mode_t rlc_am::get_mode()
{
  return RLC_MODE_AM;
}

uint32_t rlc_am::get_bearer()
{
  return lcid;
}

/****************************************************************************
 * PDCP interface
 ***************************************************************************/

void rlc_am::write_sdu(byte_buffer_t *sdu)
{
  log->info_hex(sdu->msg, sdu->N_bytes, "%s Tx SDU", rb_id_text[lcid]);
  tx_sdu_queue.write(sdu);
}

/****************************************************************************
 * MAC interface
 ***************************************************************************/

uint32_t rlc_am::get_buffer_state()
{
  boost::lock_guard<boost::mutex> lock(mutex);

  // Bytes needed for status report
  check_reordering_timeout();
  if(do_status && !status_prohibited())
    return prepare_status();

  // Bytes needed for retx
  if(retx_queue.size() > 0)
    return tx_window[retx_queue.front()].buf->N_bytes;

  // Bytes needed for tx SDUs
  uint32_t n_sdus  = tx_sdu_queue.size();
  uint32_t n_bytes = tx_sdu_queue.size_bytes();
  if(tx_sdu)
  {
    n_sdus++;
    n_bytes += tx_sdu->N_bytes;
  }

  // Room needed for header extensions? (integer rounding)
  if(n_sdus > 1)
    n_bytes += ((n_sdus-1)*1.5)+0.5;

  // Room needed for fixed header?
  if(n_bytes > 0)
    n_bytes += 2;

  return n_bytes;
}

int rlc_am::read_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  boost::lock_guard<boost::mutex> lock(mutex);

  log->info("MAC opportunity - %d bytes\n", nof_bytes);

  // Tx STATUS if requested
  if(do_status && !status_prohibited())
    return build_status_pdu(payload, nof_bytes);

  // RETX if required
  if(retx_queue.size() > 0)
    return build_retx_pdu(payload, nof_bytes);

  // Build a PDU from SDUs
  return build_data_pdu(payload, nof_bytes);
}

void rlc_am::write_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  boost::lock_guard<boost::mutex> lock(mutex);

  if(rlc_am_is_control_pdu(payload))
  {
    handle_control_pdu(payload, nof_bytes);
  }else{
    handle_data_pdu(payload, nof_bytes);
  }
}

/****************************************************************************
 * Timer checks
 ***************************************************************************/

bool rlc_am::status_prohibited()
{
  return (status_prohibit_timeout.is_running() && !status_prohibit_timeout.expired());
}

bool rlc_am::poll_retx()
{
  return (poll_retx_timeout.is_running() && poll_retx_timeout.expired());
}

void rlc_am::check_reordering_timeout()
{
  if(reordering_timeout.is_running() && reordering_timeout.expired())
  {
    reordering_timeout.reset();
    log->debug("%s reordering timeout expiry - updating vr_ms\n", rb_id_text[lcid]);

    // 36.322 v10 Section 5.1.3.2.4
    vr_ms = vr_x;
    std::map<uint32_t, rlc_amd_rx_pdu_t>::iterator it = rx_window.find(vr_ms);
    while(rx_window.end() != it && it->second.pdu_complete)
    {
      vr_ms = (vr_ms + 1)%MOD;
      it = rx_window.find(vr_ms);
    }
    if(poll_received)
      do_status = true;

    if(RX_MOD_BASE(vr_h) > RX_MOD_BASE(vr_ms))
    {
      reordering_timeout.start(t_reordering);
      vr_x = vr_h;
    }

    debug_state();
  }
}

/****************************************************************************
 * Helpers
 ***************************************************************************/

bool rlc_am::poll_required()
{
  if(poll_pdu > 0 && pdu_without_poll > poll_pdu)
    return true;
  if(poll_byte > 0 && byte_without_poll > poll_byte)
    return true;
  if(poll_retx())
    return true;
  return false;
}

int rlc_am::prepare_status()
{
  status.N_nack = 0;
  status.ack_sn = vr_ms;

  uint32_t i = vr_r;
  while(RX_MOD_BASE(i) < RX_MOD_BASE(vr_ms))
  {
    if(rx_window.find(i) == rx_window.end())
      status.nack_sn[status.N_nack++] = i;
    i = (i + 1)%MOD;
  }

  return rlc_am_packed_length(&status);
}

int  rlc_am::build_status_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  int pdu_len = rlc_am_packed_length(&status);
  if(nof_bytes >= pdu_len)
  {
    log->info("%s Tx status PDU - %s\n",
              rb_id_text[lcid], rlc_am_to_string(&status).c_str());

    do_status     = false;
    poll_received = false;

    if(t_status_prohibit > 0)
      status_prohibit_timeout.start(t_status_prohibit);
    debug_state();
    return rlc_am_write_status_pdu(&status, payload);
  }else{
    log->warning("%s Cannot tx status PDU - %d bytes available, %d bytes required\n",
                 rb_id_text[lcid], nof_bytes, pdu_len);
    return 0;
  }
}

int  rlc_am::build_retx_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  uint32_t sn = retx_queue.front();
  if(tx_window[sn].buf->N_bytes <= nof_bytes)
  {
    pdu_without_poll++;
    byte_without_poll += tx_window[sn].buf->N_bytes;
    if(poll_required())
    {
      poll_sn           = vt_s;
      tx_window[sn].buf->msg[0] |= 1 << 5; // Set polling bit directly in PDU
      pdu_without_poll  = 0;
      byte_without_poll = 0;
      poll_retx_timeout.start(t_poll_retx);
    }else{
      tx_window[sn].buf->msg[0] &= ~(1 << 5); // Clear polling bit directly in PDU
    }
    memcpy(payload, tx_window[sn].buf->msg, tx_window[sn].buf->N_bytes);
    retx_queue.pop();
    tx_window[sn].retx_count++;
    if(tx_window[sn].retx_count >= max_retx_thresh)
      rrc->max_retx_attempted();
    log->info("%s Retx SN %d, retx count: %d\n",
              rb_id_text[lcid], sn, tx_window[sn].retx_count);
    debug_state();
    return tx_window[sn].buf->N_bytes;
  }else{
    //TODO: implement PDU resegmentation
    log->warning("%s Cannot retx SN %d - %d bytes available, %d bytes required\n",
                 rb_id_text[lcid], sn, nof_bytes, tx_window[sn].buf->N_bytes);
    return 0;
  }
}

int  rlc_am::build_data_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  if(!tx_sdu && tx_sdu_queue.size() == 0)
  {
    log->info("No data available to be sent");
    return 0;
  }

  byte_buffer_t *pdu = pool->allocate();
  rlc_amd_pdu_header_t header;
  header.dc   = RLC_DC_FIELD_DATA_PDU;
  header.rf   = 0;
  header.p    = 0;
  header.fi   = RLC_FI_FIELD_START_AND_END_ALIGNED;
  header.sn   = vt_s;
  header.lsf  = 0;
  header.so   = 0;
  header.N_li = 0;

  uint32_t head_len  = rlc_am_packed_length(&header);
  uint32_t to_move   = 0;
  uint32_t last_li   = 0;
  uint32_t pdu_space = nof_bytes;
  uint8_t *pdu_ptr   = pdu->msg;

  if(pdu_space <= head_len)
  {
    log->warning("%s Cannot build a PDU - %d bytes available, %d bytes required for header\n",
                 rb_id_text[lcid], nof_bytes, head_len);
    return 0;
  }

  // Check for SDU segment
  if(tx_sdu)
  {
    to_move = ((pdu_space-head_len) >= tx_sdu->N_bytes) ? tx_sdu->N_bytes : pdu_space-head_len;
    memcpy(pdu_ptr, tx_sdu->msg, to_move);
    last_li          = to_move;
    pdu_ptr         += to_move;
    pdu->N_bytes    += to_move;
    tx_sdu->N_bytes -= to_move;
    tx_sdu->msg     += to_move;
    if(tx_sdu->N_bytes == 0)
    {
      pool->deallocate(tx_sdu);
      tx_sdu = NULL;
    }
    pdu_space -= to_move;
    header.fi |= RLC_FI_FIELD_NOT_START_ALIGNED; // First byte does not correspond to first byte of SDU
  }

  // Pull SDUs from queue
  while(pdu_space > head_len && tx_sdu_queue.size() > 0)
  {
    if(last_li > 0)
      header.li[header.N_li++] = last_li;
    head_len = rlc_am_packed_length(&header);
    tx_sdu_queue.read(&tx_sdu);
    to_move = ((pdu_space-head_len) >= tx_sdu->N_bytes) ? tx_sdu->N_bytes : pdu_space-head_len;
    memcpy(pdu_ptr, tx_sdu->msg, to_move);
    last_li          = to_move;
    pdu_ptr         += to_move;
    pdu->N_bytes    += to_move;
    tx_sdu->N_bytes -= to_move;
    tx_sdu->msg     += to_move;
    if(tx_sdu->N_bytes == 0)
    {
      pool->deallocate(tx_sdu);
      tx_sdu = NULL;
    }
    pdu_space -= to_move;
  }

  if(tx_sdu)
    header.fi |= RLC_FI_FIELD_NOT_END_ALIGNED; // Last byte does not correspond to last byte of SDU

  // Set Poll bit
  pdu_without_poll++;
  byte_without_poll += (pdu->N_bytes + head_len);
  if(poll_required())
  {
    header.p          = 1;
    poll_sn           = vt_s;
    pdu_without_poll  = 0;
    byte_without_poll = 0;
    poll_retx_timeout.start(t_poll_retx);
  }

  // Set SN
  header.sn = vt_s;
  vt_s = (vt_s + 1)%MOD;

  // Add header, place PDU in tx_window and TX
  rlc_am_write_data_pdu_header(&header, pdu);
  tx_window[header.sn].buf        = pdu;
  tx_window[header.sn].header     = header;
  tx_window[header.sn].is_acked   = false;
  tx_window[header.sn].retx_count = 0;
  memcpy(payload, pdu->msg, pdu->N_bytes);

  debug_state();
  return pdu->N_bytes;
}

void rlc_am::handle_data_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  std::map<uint32_t, rlc_amd_rx_pdu_t>::iterator it;
  rlc_amd_pdu_header_t header;
  rlc_am_read_data_pdu_header(payload, nof_bytes, &header);

  log->info_hex(payload, nof_bytes, "%s Rx data PDU SN: %d",
                rb_id_text[lcid], header.sn);

  if(!inside_rx_window(header.sn))
  {
    if(header.p)
    {
      log->info("%s Status packet requested through polling bit\n", rb_id_text[lcid]);
      do_status = true;
    }
    log->info("%s SN: %d outside rx window [%d:%d] - discarding\n",
              rb_id_text[lcid], header.sn, vr_r, vr_mr);
    return;
  }
  it = rx_window.find(header.sn);
  if(rx_window.end() != it)
  {
    if(header.p)
    {
      log->info("%s Status packet requested through polling bit\n", rb_id_text[lcid]);
      do_status = true;
    }
    log->info("%s Discarding duplicate SN: %d\n",
              rb_id_text[lcid], header.sn);
    return;
  }

  // Write to rx window
  rlc_amd_rx_pdu_t pdu;
  pdu.buf = pool->allocate();
  memcpy(pdu.buf->msg, payload, nof_bytes);
  pdu.buf->N_bytes = nof_bytes;
  //Strip header from PDU
  int header_len = rlc_am_packed_length(&header);
  pdu.buf->msg += header_len;
  pdu.buf->N_bytes -= header_len;
  pdu.header = header;
  if(!pdu.header.rf)
    pdu.pdu_complete = true;
  rx_window[header.sn] = pdu;

  // Update vr_h
  if(RX_MOD_BASE(header.sn) >= RX_MOD_BASE(vr_h))
    vr_h  = (header.sn + 1)%MOD;

  // Update vr_ms
  it = rx_window.find(vr_ms);
  while(rx_window.end() != it && it->second.pdu_complete)
  {
    vr_ms = (vr_ms + 1)%MOD;
    it = rx_window.find(vr_ms);
  }

  // Check poll bit
  if(header.p)
  {
    log->info("%s Status packet requested through polling bit\n", rb_id_text[lcid]);
    poll_received = true;

    // 36.322 v10 Section 5.2.3
    if(RX_MOD_BASE(header.sn) < RX_MOD_BASE(vr_ms) ||
       RX_MOD_BASE(header.sn) >= RX_MOD_BASE(vr_mr))
    {
      do_status = true;
    }
    // else delay for reordering timer
  }

  // Reassemble and deliver SDUs
  reassemble_rx_sdus();

  // Update reordering variables and timers (36.322 v10.0.0 Section 5.1.3.2.3)
  if(reordering_timeout.is_running())
  {
    if(
       vr_x == vr_r ||
       (RX_MOD_BASE(vr_x) < RX_MOD_BASE(vr_r)  ||
        RX_MOD_BASE(vr_x) > RX_MOD_BASE(vr_mr) &&
        vr_x != vr_mr)
       )
    {
      reordering_timeout.reset();
    }
  }
  if(!reordering_timeout.is_running())
  {
    if(RX_MOD_BASE(vr_h) > RX_MOD_BASE(vr_r))
    {
      reordering_timeout.start(t_reordering);
      vr_x = vr_h;
    }
  }

  debug_state();
}

void rlc_am::handle_control_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  log->info_hex(payload, nof_bytes, "%s Rx control PDU", rb_id_text[lcid]);

  rlc_status_pdu_t status;
  rlc_am_read_status_pdu(payload, nof_bytes, &status);

  log->info("%s Rx Status PDU: %s\n", rb_id_text[lcid], rlc_am_to_string(&status).c_str());

  poll_retx_timeout.reset();

  // Handle ACKs and NACKs
  bool update_vt_a = true;
  uint32_t i = vt_a;
  while(TX_MOD_BASE(i) < TX_MOD_BASE(status.ack_sn) &&
        TX_MOD_BASE(i) < TX_MOD_BASE(vt_s))
  {
    std::map<uint32_t, rlc_amd_tx_pdu_t>::iterator it;
    if(rlc_am_status_has_nack(&status, i))
    {
      update_vt_a = false;
      it = tx_window.find(i);
      if(tx_window.end() != it)
      {
        retx_queue.push(i);
      }
    }else{
      it = tx_window.find(i);
      if(tx_window.end() != it)
      {
        tx_window[i].is_acked = true;
        if(update_vt_a)
        {
          pool->deallocate(tx_window[i].buf);
          tx_window.erase(i);
          vt_a = (vt_a + 1)%MOD;
          vt_ms = (vt_ms + 1)%MOD;
        }
      }
    }
    i = (i+1)%MOD;
  }

  debug_state();
}

void rlc_am::reassemble_rx_sdus()
{
  if(!rx_sdu)
    rx_sdu = pool->allocate();

  // Iterate through rx_window, assembling and delivering SDUs
  while(rx_window.end() != rx_window.find(vr_r))
  {
    // Handle any SDU segments
    for(int i=0; i<rx_window[vr_r].header.N_li; i++)
    {
      int len = rx_window[vr_r].header.li[i];
      memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_r].buf->msg, len);
      rx_sdu->N_bytes += len;
      rx_window[vr_r].buf->msg += len;
      rx_window[vr_r].buf->N_bytes -= len;
      log->info_hex(rx_sdu->msg, rx_sdu->N_bytes, "%s Rx SDU", rb_id_text[lcid]);
      pdcp->write_pdu(lcid, rx_sdu);
      rx_sdu = pool->allocate();
    }

    // Handle last segment
    memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_r].buf->msg, rx_window[vr_r].buf->N_bytes);
    rx_sdu->N_bytes += rx_window[vr_r].buf->N_bytes;
    if(rlc_am_end_aligned(rx_window[vr_r].header.fi))
    {
      log->info_hex(rx_sdu->msg, rx_sdu->N_bytes, "%s Rx SDU", rb_id_text[lcid]);
      pdcp->write_pdu(lcid, rx_sdu);
      rx_sdu = pool->allocate();
    }

    // Move the rx_window
    pool->deallocate(rx_window[vr_r].buf);
    rx_window.erase(vr_r);
    vr_r = (vr_r + 1)%MOD;
    vr_mr = (vr_mr + 1)%MOD;
  }
}

bool rlc_am::inside_tx_window(uint16_t sn)
{
  if(RX_MOD_BASE(sn) >= RX_MOD_BASE(vt_a) &&
     RX_MOD_BASE(sn) <  RX_MOD_BASE(vt_ms))
  {
    return true;
  }else{
    return false;
  }
}

bool rlc_am::inside_rx_window(uint16_t sn)
{
  if(RX_MOD_BASE(sn) >= RX_MOD_BASE(vr_r) &&
     RX_MOD_BASE(sn) <  RX_MOD_BASE(vr_mr))
  {
    return true;
  }else{
    return false;
  }
}

void rlc_am::debug_state()
{
  log->debug("%s vt_a = %d, vt_ms = %d, vt_s = %d, poll_sn = %d \n"
             "vr_r = %d, vr_mr = %d, vr_x = %d, vr_ms = %d, vr_h = %d\n",
             rb_id_text[lcid], vt_a, vt_ms, vt_s, poll_sn,
             vr_r, vr_mr, vr_x, vr_ms, vr_h);

}

/****************************************************************************
 * Header pack/unpack helper functions
 * Ref: 3GPP TS 36.322 v10.0.0 Section 6.2.1
 ***************************************************************************/

void rlc_am_read_data_pdu_header(byte_buffer_t *pdu, rlc_amd_pdu_header_t *header)
{
  rlc_am_read_data_pdu_header(pdu->msg, pdu->N_bytes, header);
}

void rlc_am_read_data_pdu_header(uint8_t *payload, uint32_t nof_bytes, rlc_amd_pdu_header_t *header)
{
  uint8_t  ext;
  uint8_t *ptr = payload;

  header->dc = (rlc_dc_field_t)((*ptr >> 7) & 0x01);

  if(RLC_DC_FIELD_DATA_PDU == header->dc)
  {
    // Fixed part
    header->rf =                 ((*ptr >> 6) & 0x01);
    header->p  =                 ((*ptr >> 5) & 0x01);
    header->fi = (rlc_fi_field_t)((*ptr >> 3) & 0x03);
    ext        =                 ((*ptr >> 2) & 0x01);
    header->sn =                 (*ptr & 0x03) << 8; // 2 bits SN
    ptr++;
    header->sn |=                (*ptr & 0xFF);     // 8 bits SN
    ptr++;

    if(header->rf)
    {
      header->lsf = ((*ptr >> 7) & 0x01);
      header->so  = (*ptr & 0x7F) << 8; // 7 bits of SO
      ptr++;
      header->so |= (*ptr & 0xFF);      // 8 bits of SO
      ptr++;
    }

    // Extension part
    header->N_li = 0;
    while(ext)
    {
      if(header->N_li%2 == 0)
      {
        ext = ((*ptr >> 7) & 0x01);
        header->li[header->N_li]  = (*ptr & 0x7F) << 4; // 7 bits of LI
        ptr++;
        header->li[header->N_li] |= (*ptr & 0xF0) >> 4; // 4 bits of LI
        header->N_li++;
      }
      else
      {
        ext = (*ptr >> 3) & 0x01;
        header->li[header->N_li] = (*ptr & 0x07) << 8; // 3 bits of LI
        ptr++;
        header->li[header->N_li] |= (*ptr & 0xFF);     // 8 bits of LI
        header->N_li++;
        ptr++;
      }
    }
  }
}

void rlc_am_write_data_pdu_header(rlc_amd_pdu_header_t *header, byte_buffer_t *pdu)
{
  uint32_t i;
  uint8_t ext = (header->N_li > 0) ? 1 : 0;

  // Make room for the header
  uint32_t len = rlc_am_packed_length(header);
  pdu->msg -= len;
  uint8_t *ptr = pdu->msg;

  // Fixed part
  *ptr  = (header->dc & 0x01) << 7;
  *ptr |= (header->rf & 0x01) << 6;
  *ptr |= (header->p  & 0x01) << 5;
  *ptr |= (header->fi & 0x03) << 3;
  *ptr |= (ext        & 0x01) << 2;

  *ptr |= (header->sn & 0x300) >> 8; // 2 bits SN
  ptr++;
  *ptr  = (header->sn & 0xFF);       // 8 bits SN
  ptr++;

  // Segment part
  if(header->rf)
  {
    *ptr  = (header->lsf & 0x01) << 7;
    *ptr |= (header->so  & 0x7F00) >> 8; // 7 bits of SO
    ptr++;
    *ptr = (header->so  & 0x00FF);       // 8 bits of SO
    ptr++;
  }

  // Extension part
  i = 0;
  while(i < header->N_li)
  {
    ext = ((i+1) == header->N_li) ? 0 : 1;
    *ptr  = (ext           &  0x01) << 7; // 1 bit header
    *ptr |= (header->li[i] & 0x7F0) >> 4; // 7 bits of LI
    ptr++;
    *ptr  = (header->li[i] & 0x00F) << 4; // 4 bits of LI
    i++;
    if(i < header->N_li)
    {
      ext = ((i+1) == header->N_li) ? 0 : 1;
      *ptr |= (ext           &  0x01) << 3; // 1 bit header
      *ptr |= (header->li[i] & 0x700) >> 8; // 3 bits of LI
      ptr++;
      *ptr  = (header->li[i] & 0x0FF);      // 8 bits of LI
      ptr++;
      i++;
    }
  }
  // Pad if N_li is odd
  if(header->N_li%2 == 1)
    ptr++;

  pdu->N_bytes += ptr-pdu->msg;
}

void rlc_am_read_status_pdu(byte_buffer_t *pdu, rlc_status_pdu_t *status)
{
  rlc_am_read_status_pdu(pdu->msg, pdu->N_bytes, status);
}

void rlc_am_read_status_pdu(uint8_t *payload, uint32_t nof_bytes, rlc_status_pdu_t *status)
{
  uint32_t i;
  uint8_t  ext1, ext2;
  bit_buffer_t tmp;
  uint8_t *ptr = tmp.msg;

  srslte_bit_unpack_vector(payload, tmp.msg, nof_bytes*8);
  tmp.N_bits = nof_bytes*8;

  rlc_dc_field_t dc = (rlc_dc_field_t)srslte_bit_pack(&ptr, 1);

  if(RLC_DC_FIELD_CONTROL_PDU == dc)
  {
    uint8_t cpt = srslte_bit_pack(&ptr, 3); // 3-bit Control PDU Type (0 == status)
    if(0 == cpt)
    {
      status->ack_sn  = srslte_bit_pack(&ptr, 10); // 10 bits ACK_SN
      ext1            = srslte_bit_pack(&ptr, 1);  // 1 bits E1
      status->N_nack  = 0;
      while(ext1)
      {
        status->nack_sn[status->N_nack++] = srslte_bit_pack(&ptr, 10);
        ext1 = srslte_bit_pack(&ptr, 1);  // 1 bits E1
        ext2 = srslte_bit_pack(&ptr, 1);  // 1 bits E2
        if(ext2)
        {
          // TODO: skipping resegmentation for now
          srslte_bit_pack(&ptr, 30); // 15-bit SOstart + 15-bit SOend
        }
      }
    }
  }
}

void rlc_am_write_status_pdu(rlc_status_pdu_t *status, byte_buffer_t *pdu )
{
  pdu->N_bytes = rlc_am_write_status_pdu(status, pdu->msg);
}

int rlc_am_write_status_pdu(rlc_status_pdu_t *status, uint8_t *payload)
{
  uint32_t i;
  uint8_t ext1;
  bit_buffer_t tmp;
  uint8_t *ptr = tmp.msg;

  srslte_bit_unpack(RLC_DC_FIELD_CONTROL_PDU, &ptr, 1);  // D/C
  srslte_bit_unpack(0,                        &ptr, 3);  // CPT (0 == STATUS)
  srslte_bit_unpack(status->ack_sn,           &ptr, 10); // 10 bit ACK_SN
  ext1 = (status->N_nack == 0) ? 0 : 1;
  srslte_bit_unpack(ext1,                     &ptr, 1);  // E1
  for(i=0;i<status->N_nack;i++)
  {
    srslte_bit_unpack(status->nack_sn[i],     &ptr, 10); // 10 bit NACK_SN
    ext1 = ((status->N_nack-1) == i) ? 0 : 1;
    srslte_bit_unpack(ext1,                   &ptr, 1);  // E1
    srslte_bit_unpack(0   ,                   &ptr, 1);  // E2
    // TODO: skipping resegmentation for now
  }

  // Pad
  tmp.N_bits = ptr - tmp.msg;
  uint8_t n_pad = 8 - (tmp.N_bits%8);
  srslte_bit_unpack(0, &ptr, n_pad);
  tmp.N_bits = ptr - tmp.msg;

  // Pack bits
  srslte_bit_pack_vector(tmp.msg, payload, tmp.N_bits);
  return tmp.N_bits/8;
}

uint32_t rlc_am_packed_length(rlc_amd_pdu_header_t *header)
{
  uint32_t len = 2;                 // Fixed part is 2 bytes
  if(header->rf) len += 2;          // Segment header is 2 bytes
  len += header->N_li * 1.5 + 0.5;  // Extension part - integer rounding up
  return len;
}

uint32_t rlc_am_packed_length(rlc_status_pdu_t *status)
{
  uint32_t len_bits = 15;                 // Fixed part is 15 bits
  len_bits += status->N_nack*12;          // Each nack is 12 bits (10 bits + 2 ext bits)
  return (len_bits+7)/8;                  // Convert to bytes - integer rounding up
}

bool rlc_am_is_control_pdu(byte_buffer_t *pdu)
{
  return rlc_am_is_control_pdu(pdu->msg);
}

bool rlc_am_is_control_pdu(uint8_t *payload)
{
  return ((*(payload) >> 7) & 0x01) == RLC_DC_FIELD_CONTROL_PDU;
}

bool rlc_am_status_has_nack(rlc_status_pdu_t *status, uint32_t sn)
{
  for(int i=0;i<status->N_nack;i++)
    if(status->nack_sn[i] == sn)
      return true;
  return false;
}

std::string rlc_am_to_string(rlc_status_pdu_t *status)
{
  std::stringstream ss;
  ss << "ACK_SN = " << status->ack_sn;
  ss << ", N_nack = " << status->N_nack;
  if(status->N_nack > 0)
  {
    ss << ", NACK_SN = ";
    for(int i=0; i<status->N_nack; i++)
    {
      ss << "[" << status->nack_sn[i] << "]";
    }
  }
  return ss.str();
}

bool rlc_am_start_aligned(uint8_t fi)
{
  return (fi == RLC_FI_FIELD_START_AND_END_ALIGNED || fi == RLC_FI_FIELD_NOT_END_ALIGNED);
}

bool rlc_am_end_aligned(uint8_t fi)
{
  return (fi == RLC_FI_FIELD_START_AND_END_ALIGNED || fi == RLC_FI_FIELD_NOT_START_ALIGNED);
}

} // namespace srsue
