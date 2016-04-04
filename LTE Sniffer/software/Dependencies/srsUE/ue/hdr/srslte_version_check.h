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

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define REQ_SRSLTE_VMAJOR  001
#define REQ_SRSLTE_VMINOR  001
#define REQ_SRSLTE_VPATCH  000

#include <srslte/srslte.h>

#if !(SRSLTE_VERSION_CHECK(REQ_SRSLTE_VMAJOR, REQ_SRSLTE_VMINOR, REQ_SRSLTE_VPATCH))
  #pragma message "Error: SRSLTE version " \
  STR(REQ_SRSLTE_VMAJOR) "." STR(REQ_SRSLTE_VMINOR) "." STR(REQ_SRSLTE_VPATCH) " required. " \
  "SRSLTE version " SRSLTE_VERSION_STRING " found."
  #error "SRSLTE version mismatch."
#endif
