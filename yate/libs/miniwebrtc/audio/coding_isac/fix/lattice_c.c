/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Contains the core loop function for the lattice filter AR routine
 * for iSAC codec.
 *
 */

#include "settings.h"
#include "signal_processing_library.h"
#include "typedefs.h"

/* Filter ar_g_Q0[] and ar_f_Q0[] through an AR filter with coefficients
 * cth_Q15[] and sth_Q15[].
 */
void WebRtcIsacfix_FilterArLoop(int16_t* ar_g_Q0,     // Input samples
                                int16_t* ar_f_Q0,     // Input samples
                                int16_t* cth_Q15,     // Filter coefficients
                                int16_t* sth_Q15,     // Filter coefficients
                                int16_t order_coef) { // order of the filter
	int n = 0;

	for (n = 0; n < HALF_SUBFRAMELEN - 1; n++) {
		int k = 0;
		int16_t tmpAR = 0;
		int32_t tmp32 = 0;
		int32_t tmp32_2 = 0;

		tmpAR = ar_f_Q0[n + 1];
		for (k = order_coef - 1; k >= 0; k--) {
			tmp32 = WEBRTC_SPL_RSHIFT_W32(((WEBRTC_SPL_MUL_16_16(cth_Q15[k], tmpAR))
			                               - (WEBRTC_SPL_MUL_16_16(sth_Q15[k], ar_g_Q0[k])) + 16384), 15);
			tmp32_2 = WEBRTC_SPL_RSHIFT_W32(((WEBRTC_SPL_MUL_16_16(sth_Q15[k], tmpAR))
			                                 + (WEBRTC_SPL_MUL_16_16(cth_Q15[k], ar_g_Q0[k])) + 16384), 15);
			tmpAR   = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp32);
			ar_g_Q0[k + 1] = (WebRtc_Word16)WebRtcSpl_SatW32ToW16(tmp32_2);
		}
		ar_f_Q0[n + 1] = tmpAR;
		ar_g_Q0[0] = tmpAR;
	}
}
