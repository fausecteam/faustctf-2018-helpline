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

 WebRtcIlbcfix_CbConstruct.c

******************************************************************/

#include "defines.h"
#include "gain_dequant.h"
#include "get_cd_vec.h"

/*----------------------------------------------------------------*
 *  Construct decoded vector from codebook and gains.
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_CbConstruct(
    WebRtc_Word16 *decvector,  /* (o) Decoded vector */
    WebRtc_Word16 *index,   /* (i) Codebook indices */
    WebRtc_Word16 *gain_index,  /* (i) Gain quantization indices */
    WebRtc_Word16 *mem,   /* (i) Buffer for codevector construction */
    WebRtc_Word16 lMem,   /* (i) Length of buffer */
    WebRtc_Word16 veclen   /* (i) Length of vector */
) {
	int j;
	WebRtc_Word16 gain[CB_NSTAGES];
	/* Stack based */
	WebRtc_Word16 cbvec0[SUBL];
	WebRtc_Word16 cbvec1[SUBL];
	WebRtc_Word16 cbvec2[SUBL];
	WebRtc_Word32 a32;
	WebRtc_Word16 *gainPtr;

	/* gain de-quantization */

	gain[0] = WebRtcIlbcfix_GainDequant(gain_index[0], 16384, 0);
	gain[1] = WebRtcIlbcfix_GainDequant(gain_index[1], gain[0], 1);
	gain[2] = WebRtcIlbcfix_GainDequant(gain_index[2], gain[1], 2);

	/* codebook vector construction and construction of total vector */

	/* Stack based */
	WebRtcIlbcfix_GetCbVec(cbvec0, mem, index[0], lMem, veclen);
	WebRtcIlbcfix_GetCbVec(cbvec1, mem, index[1], lMem, veclen);
	WebRtcIlbcfix_GetCbVec(cbvec2, mem, index[2], lMem, veclen);

	gainPtr = &gain[0];
	for (j=0; j<veclen; j++) {
		a32  = WEBRTC_SPL_MUL_16_16(*gainPtr++, cbvec0[j]);
		a32 += WEBRTC_SPL_MUL_16_16(*gainPtr++, cbvec1[j]);
		a32 += WEBRTC_SPL_MUL_16_16(*gainPtr, cbvec2[j]);
		gainPtr -= 2;
		decvector[j] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(a32 + 8192, 14);
	}

	return;
}
