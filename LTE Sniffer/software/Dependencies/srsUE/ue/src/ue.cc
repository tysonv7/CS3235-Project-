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


#include <boost/algorithm/string.hpp>
#include <boost/thread/mutex.hpp>
#include "ue.h"
#include "srslte_version_check.h"
#include "srslte/srslte.h"

namespace srsue{

ue*           ue::instance = NULL;
boost::mutex  ue_instance_mutex;

bool ue::check_srslte_version(void) {
  bool ret = (0 != srslte_check_version(REQ_SRSLTE_VMAJOR, REQ_SRSLTE_VMINOR, REQ_SRSLTE_VPATCH));
  if(!ret) {
    fprintf(stderr, "srsLTE version mismatch. Minimum required version is %d.%d.%d Found version %s\n",
            REQ_SRSLTE_VMAJOR, REQ_SRSLTE_VMINOR, REQ_SRSLTE_VPATCH,
            srslte_get_version());
  } else {
    fprintf(stdout, "Using srsLTE version %s\n", srslte_get_version());
  }
  return ret;
}

ue* ue::get_instance(void)
{
    boost::mutex::scoped_lock lock(ue_instance_mutex);
    if(NULL == instance) {
        instance = new ue();
    }
    return(instance);
}
void ue::cleanup(void)
{
    boost::mutex::scoped_lock lock(ue_instance_mutex);
    if(NULL != instance) {
        delete instance;
        instance = NULL;
    }
}

ue::ue()
    :started(false)
{
  pool = buffer_pool::get_instance();
}

ue::~ue()
{
  buffer_pool::cleanup();
}

bool ue::init(all_args_t *args_)
{
  args     = args_;

  if (!check_srslte_version()) {
    return false; 
  }
  
  logger.init(args->log.filename);
  rf_log.init("RF ", &logger);
  phy_log.init("PHY ", &logger, true);
  mac_log.init("MAC ", &logger, true);
  rlc_log.init("RLC ", &logger);
  pdcp_log.init("PDCP", &logger);
  rrc_log.init("RRC ", &logger);
  nas_log.init("NAS ", &logger);
  gw_log.init("GW  ", &logger);
  usim_log.init("USIM", &logger);

  // Init logs
  logger.log("\n\n");
  rf_log.set_level(srslte::LOG_LEVEL_INFO);
  phy_log.set_level(level(args->log.phy_level));
  mac_log.set_level(level(args->log.mac_level));
  rlc_log.set_level(level(args->log.rlc_level));
  pdcp_log.set_level(level(args->log.pdcp_level));
  rrc_log.set_level(level(args->log.rrc_level));
  nas_log.set_level(level(args->log.nas_level));
  gw_log.set_level(level(args->log.gw_level));
  usim_log.set_level(level(args->log.usim_level));

  phy_log.set_hex_limit(args->log.phy_hex_limit);
  mac_log.set_hex_limit(args->log.mac_hex_limit);
  rlc_log.set_hex_limit(args->log.rlc_hex_limit);
  pdcp_log.set_hex_limit(args->log.pdcp_hex_limit);
  rrc_log.set_hex_limit(args->log.rrc_hex_limit);
  nas_log.set_hex_limit(args->log.nas_hex_limit);
  gw_log.set_hex_limit(args->log.gw_hex_limit);
  usim_log.set_hex_limit(args->log.usim_hex_limit);

  // Set up pcap and trace
  if(args->pcap.enable)
  {
    mac_pcap.open(args->pcap.filename.c_str());
    mac.start_pcap(&mac_pcap);
  }
  if(args->trace.enable)
  {
    phy.start_trace();
    radio.start_trace();
  }
  
  // Set up expert mode parameters
  set_expert_parameters();

  // Init layers
  
  /* Start Radio */
  char *dev_name = NULL;
  if (args->rf.device_name.compare("auto")) {
    dev_name = (char*) args->rf.device_name.c_str();
  }
  
  char *dev_args = NULL;
  if (args->rf.device_args.compare("auto")) {
    dev_args = (char*) args->rf.device_args.c_str();
  }
  
  if(!radio.init(dev_args, dev_name))
  {
    printf("Failed to find device %s with args %s\n",
           args->rf.device_name.c_str(), args->rf.device_args.c_str());
    return false;
  }    
  
  // Set RF options
  if (args->rf.time_adv_nsamples.compare("auto")) {
    radio.set_tx_adv(atoi(args->rf.time_adv_nsamples.c_str()));
  }  
  if (args->rf.burst_preamble.compare("auto")) {
    radio.set_burst_preamble(atof(args->rf.burst_preamble.c_str()));    
  }
  
  radio.set_manual_calibration(&args->rf_cal);
  
  phy.init(&radio, &mac, &rrc, &phy_log, args->expert.nof_phy_threads);
  
  if (args->rf.rx_gain < 0) {
    radio.start_agc(false);    
    radio.set_tx_rx_gain_offset(10);
    phy.set_agc_enable(true);
  } else {
    radio.set_rx_gain(args->rf.rx_gain);
    if (args->rf.tx_gain < 0) {
      radio.set_tx_gain(args->rf.rx_gain);
    }
  }
  if (args->rf.tx_gain > 0) {
    radio.set_tx_gain(args->rf.tx_gain);
  } else {
    std::cout << std::endl << 
                "Warning: TX gain was not set. " << 
                "Using open-loop power control (not working properly)" << std::endl << std::endl; 
  }

  radio.register_error_handler(rf_msg);

  radio.set_rx_freq(args->rf.dl_freq);
  radio.set_tx_freq(args->rf.ul_freq);

  phy_log.console("Setting frequency: DL=%.1f Mhz, UL=%.1f MHz\n", args->rf.dl_freq/1e6, args->rf.ul_freq/1e6);

  mac.init(&phy, &rlc, &rrc, &mac_log);
  rlc.init(&pdcp, &rrc, this, &rlc_log, &mac);
  pdcp.init(&rlc, &rrc, &gw, &pdcp_log);
  rrc.init(&phy, &mac, &rlc, &pdcp, &nas, &usim, &rrc_log);
  nas.init(&usim, &rrc, &gw, &nas_log);
  gw.init(&pdcp, &rrc, this, &gw_log);
  usim.init(&args->usim, &usim_log);

  started = true;
  return true;
}

void ue::set_expert_parameters() {
  phy.set_param(phy_interface_params::CELLSEARCH_TIMEOUT_MIB_NFRAMES, args->expert.sync_find_max_frames);
  phy.set_param(phy_interface_params::CELLSEARCH_TIMEOUT_PSS_NFRAMES, args->expert.sync_find_max_frames);
  if (args->expert.sync_find_th > 1.0) {
    phy.set_param(phy_interface_params::CELLSEARCH_TIMEOUT_PSS_CORRELATION_THRESHOLD, args->expert.sync_find_th*10);
  } else {
    phy.set_param(phy_interface_params::CELLSEARCH_TIMEOUT_PSS_CORRELATION_THRESHOLD, 160);
  }
  
  phy.set_param(phy_interface_params::SYNC_TRACK_THRESHOLD, 10*args->expert.sync_track_th);
  phy.set_param(phy_interface_params::SYNC_TRACK_AVG_COEFF, 100*args->expert.sync_track_avg_coef);

  if (args->rf.tx_gain > 0) {
    phy.set_param(phy_interface_params::PRACH_GAIN, args->rf.tx_gain);
    phy.set_param(phy_interface_params::UL_GAIN,    args->rf.tx_gain);
  } else {
    phy.set_param(phy_interface_params::PRACH_GAIN, args->expert.prach_gain);
    phy.set_param(phy_interface_params::UL_GAIN,    args->expert.ul_gain);
    std::cout << std::endl << 
                "Warning: TX gain was not set. " << 
                "Using open-loop power control (not working properly)" << std::endl << std::endl; 
  }
  
  phy.set_param(phy_interface_params::UL_PWR_CTRL_OFFSET, args->expert.ul_pwr_ctrl_offset);
  
  phy.set_param(phy_interface_params::RX_GAIN_OFFSET, args->expert.rx_gain_offset);
  
  phy.set_param(phy_interface_params::FORCE_ENABLE_64QAM, args->expert.enable_64qam_attach?1:0);

  phy.set_param(phy_interface_params::CONTINUOUS_TX, args->expert.continuous_tx?1:0);
  phy.set_param(phy_interface_params::PDSCH_MAX_ITS, args->expert.pdsch_max_its);
    
  if (!args->expert.equalizer_mode.compare("zf")) {
    phy.set_param(phy_interface_params::EQUALIZER_COEFF, 0);
  } else if (!args->expert.equalizer_mode.compare("mmse")) {
    phy.set_param(phy_interface_params::EQUALIZER_COEFF, -1);    
  } else {
    phy.set_param(phy_interface_params::EQUALIZER_COEFF, atof(args->expert.equalizer_mode.c_str()));
  }
}

void ue::stop()
{
  if(started)
  {
    phy.stop();
    mac.stop();
    rlc.stop();
    pdcp.stop();
    rrc.stop();
    nas.stop();
    gw.stop();
    usim.stop();

    sleep(1);
    if(args->pcap.enable)
    {
       mac_pcap.close();
    }
    if(args->trace.enable)
    {
      phy.write_trace(args->trace.phy_filename);
      radio.write_trace(args->trace.radio_filename);
    }
    started = false;
  }
}

bool ue::get_metrics(ue_metrics_t &m)
{
  m.rf = rf_metrics;
  bzero(&rf_metrics, sizeof(rf_metrics_t));
  rf_metrics.rf_error = false; // Reset error flag

  if(EMM_STATE_REGISTERED == nas.get_state()) {
    if(RRC_STATE_RRC_CONNECTED == rrc.get_state()) {
      phy.get_metrics(m.phy);
      mac.get_metrics(m.mac);
      return true;
    }
  }
  return false;
}

void ue::rf_msg(srslte_rf_error_t error)
{
  ue *u = ue::get_instance();
  u->handle_rf_msg(error);
}

void ue::handle_rf_msg(srslte_rf_error_t error)
{
  if(error.type == srslte_rf_error_t::SRSLTE_RF_ERROR_OVERFLOW) {
    rf_metrics.rf_o++;
    rf_metrics.rf_error = true;
  }else if(error.type == srslte_rf_error_t::SRSLTE_RF_ERROR_UNDERFLOW) {
    rf_metrics.rf_u++;
    rf_metrics.rf_error = true;
  } else if(error.type == srslte_rf_error_t::SRSLTE_RF_ERROR_LATE) {
    rf_metrics.rf_l++;
    rf_metrics.rf_error = true;
  } else if (error.type == srslte_rf_error_t::SRSLTE_RF_ERROR_OTHER) {
    std::string str(error.msg);
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
    str.push_back('\n');
    rf_log.info(str);
  }
}

srslte::LOG_LEVEL_ENUM ue::level(std::string l)
{
  boost::to_upper(l);
  if("NONE" == l){
    return srslte::LOG_LEVEL_NONE;
  }else if("ERROR" == l){
    return srslte::LOG_LEVEL_ERROR;
  }else if("WARNING" == l){
    return srslte::LOG_LEVEL_WARNING;
  }else if("INFO" == l){
    return srslte::LOG_LEVEL_INFO;
  }else if("DEBUG" == l){
    return srslte::LOG_LEVEL_DEBUG;
  }else{
    return srslte::LOG_LEVEL_NONE;
  }
}

} // namespace srsue
