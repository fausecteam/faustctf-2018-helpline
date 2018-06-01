/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_Window32W32.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 *  window multiplication
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_Window32W32(
    WebRtc_Word32 *z,    /* Output */
    WebRtc_Word32 *x,    /* Input (same domain as Output)*/
    const WebRtc_Word32  *y,  /* Q31 Window */
    WebRtc_Word16 N     /* length to process */
) {
	WebRtc_Word16 i;
	WebRtc_Word16 x_low, x_hi, y_low, y_hi;
	WebRtc_Word16 left_shifts;
	WebRtc_Word32 temp;

	left_shifts = (WebRtc_Word16)WebRtcSpl_NormW32(x[0]);
	WebRtcSpl_VectorBitShiftW32(x, N, x, (WebRtc_Word16)(-left_shifts));


	/* The double precision numbers use a special representation:
	 * w32 = hi<<16 + lo<<1
	 */
	for (i = 0; i < N; i++) {
		/* Extract higher bytes */
		x_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(x[i], 16);
		y_hi = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(y[i], 16);

		/* Extract lower bytes, defined as (w32 - hi<<16)>>1 */
		temp = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)x_hi, 16);
		x_low = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32((x[i] - temp), 1);

		temp = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)y_hi, 16);
		y_low = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32((y[i] - temp), 1);

		/* Calculate z by a 32 bit multiplication using both low and high from x and y */
		temp = WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_16(x_hi, y_hi), 1);
		temp = (temp + (WEBRTC_SPL_MUL_16_16_RSFT(x_hi, y_low, 14)));

		z[i] = (temp + (WEBRTC_SPL_MUL_16_16_RSFT(x_low, y_hi, 14)));
	}

	WebRtcSpl_VectorBitShiftW32(z, N, z, left_shifts);

	return;
}
