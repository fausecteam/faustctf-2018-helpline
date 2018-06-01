/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pitch_estimator.h"
#include "os_specific_inline.h"

#include <stdlib.h>
#include <memory.h>
#include <math.h>

static const double kDampFilter[PITCH_DAMPORDER] = {-0.07, 0.25, 0.64, 0.25, -0.07};

/* interpolation coefficients; generated by design_pitch_filter.m */
static const double kIntrpCoef[PITCH_FRACS][PITCH_FRACORDER] = {
	{-0.02239172458614,  0.06653315052934, -0.16515880017569,  0.60701333734125, 0.64671399919202, -0.20249000396417,  0.09926548334755, -0.04765933793109,  0.01754159521746},
	{-0.01985640750434,  0.05816126837866, -0.13991265473714,  0.44560418147643, 0.79117042386876, -0.20266133815188,  0.09585268418555, -0.04533310458084,  0.01654127246314},
	{-0.01463300534216,  0.04229888475060, -0.09897034715253,  0.28284326017787, 0.90385267956632, -0.16976950138649,  0.07704272393639, -0.03584218578311,  0.01295781500709},
	{-0.00764851320885,  0.02184035544377, -0.04985561057281,  0.13083306574393, 0.97545011664662, -0.10177807997561,  0.04400901776474, -0.02010737175166,  0.00719783432422},
	{-0.00000000000000,  0.00000000000000, -0.00000000000001,  0.00000000000001, 0.99999999999999,  0.00000000000001, -0.00000000000001,  0.00000000000000, -0.00000000000000},
	{ 0.00719783432422, -0.02010737175166,  0.04400901776474, -0.10177807997562, 0.97545011664663,  0.13083306574393, -0.04985561057280,  0.02184035544377, -0.00764851320885},
	{ 0.01295781500710, -0.03584218578312,  0.07704272393640, -0.16976950138650, 0.90385267956634,  0.28284326017785, -0.09897034715252,  0.04229888475059, -0.01463300534216},
	{ 0.01654127246315, -0.04533310458085,  0.09585268418557, -0.20266133815190, 0.79117042386878,  0.44560418147640, -0.13991265473712,  0.05816126837865, -0.01985640750433}
};


void WebRtcIsac_PitchfilterPre(double *indat,
                               double *outdat,
                               PitchFiltstr *pfp,
                               double *lags,
                               double *gains)
{

	double ubuf[PITCH_INTBUFFSIZE];
	const double *fracoeff = NULL;
	double curgain, curlag, gaindelta, lagdelta;
	double sum, inystate[PITCH_DAMPORDER];
	double ftmp, oldlag, oldgain;
	int    k, n, m, pos, ind, pos2, Li, frc;

	Li = 0;
	/* Set up buffer and states */
	memcpy(ubuf, pfp->ubuf, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(inystate, pfp->ystate, sizeof(double) * PITCH_DAMPORDER);

	oldlag = *pfp->oldlagp;
	oldgain = *pfp->oldgainp;

	/* No interpolation if pitch lag step is big */
	if ((lags[0] > (PITCH_UPSTEP * oldlag)) || (lags[0] < (PITCH_DOWNSTEP * oldlag))) {
		oldlag = lags[0];
		oldgain = gains[0];
	}

	ind=0;
	for (k=0; k<PITCH_SUBFRAMES; k++) {

		/* Calculate interpolation steps */
		lagdelta=(lags[k]-oldlag) / PITCH_GRAN_PER_SUBFRAME;
		curlag=oldlag ;
		gaindelta=(gains[k]-oldgain) / PITCH_GRAN_PER_SUBFRAME;
		curgain=oldgain ;
		oldlag=lags[k];
		oldgain=gains[k];

		for (n=0; n<PITCH_SUBFRAME_LEN; n++) {
			if ((ind % PITCH_UPDATE) == 0) { /* Update parameters */
				curgain += gaindelta;
				curlag += lagdelta;
				Li = WebRtcIsac_lrint(curlag+PITCH_FILTDELAY + 0.5);
				ftmp = Li - (curlag+PITCH_FILTDELAY);
				frc = WebRtcIsac_lrint(PITCH_FRACS * ftmp - 0.5);
				fracoeff = kIntrpCoef[frc];
			}

			/* shift low pass filter state */
			for (m=PITCH_DAMPORDER-1; m>0; m--)
				inystate[m] = inystate[m-1];

			/* Filter to get fractional pitch */
			pos = ind + PITCH_BUFFSIZE;
			pos2 = pos - Li;
			sum=0;
			for (m=0; m<PITCH_FRACORDER; m++)
				sum += ubuf[pos2+m] * fracoeff[m];
			inystate[0] = curgain * sum;  /* Multiply with gain */

			/* Low pass filter */
			sum=0;
			for (m=0; m<PITCH_DAMPORDER; m++)
				sum += inystate[m] * kDampFilter[m];

			/* Subtract from input and update buffer */
			outdat[ind] = indat[ind] - sum;
			ubuf[pos] = indat[ind] + outdat[ind];
			ind++;
		}
	}

	/* Export buffer and states */
	memcpy(pfp->ubuf, ubuf+PITCH_FRAME_LEN, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(pfp->ystate, inystate, sizeof(double) * PITCH_DAMPORDER);

	*pfp->oldlagp = oldlag;
	*pfp->oldgainp = oldgain;

}


void WebRtcIsac_PitchfilterPre_la(double *indat,
                                  double *outdat,
                                  PitchFiltstr *pfp,
                                  double *lags,
                                  double *gains)
{
	double ubuf[PITCH_INTBUFFSIZE+QLOOKAHEAD];
	const double *fracoeff = NULL;
	double curgain, curlag, gaindelta, lagdelta;
	double sum, inystate[PITCH_DAMPORDER];
	double ftmp;
	double oldlag, oldgain;
	int    k, n, m, pos, ind, pos2, Li, frc;

	Li = 0;
	/* Set up buffer and states */
	memcpy(ubuf, pfp->ubuf, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(inystate, pfp->ystate, sizeof(double) * PITCH_DAMPORDER);

	oldlag = *pfp->oldlagp;
	oldgain = *pfp->oldgainp;

	/* No interpolation if pitch lag step is big */
	if ((lags[0] > (PITCH_UPSTEP * oldlag)) || (lags[0] < (PITCH_DOWNSTEP * oldlag))) {
		oldlag = lags[0];
		oldgain = gains[0];
	}


	ind=0;
	for (k=0; k<PITCH_SUBFRAMES; k++) {

		/* Calculate interpolation steps */
		lagdelta=(lags[k]-oldlag) / PITCH_GRAN_PER_SUBFRAME;
		curlag=oldlag ;
		gaindelta=(gains[k]-oldgain) / PITCH_GRAN_PER_SUBFRAME;
		curgain=oldgain ;
		oldlag=lags[k];
		oldgain=gains[k];

		for (n=0; n<PITCH_SUBFRAME_LEN; n++) {
			if ((ind % PITCH_UPDATE) == 0) {   /* Update parameters */
				curgain += gaindelta;
				curlag += lagdelta;
				Li = WebRtcIsac_lrint(curlag+PITCH_FILTDELAY + 0.5);
				ftmp = Li - (curlag+PITCH_FILTDELAY);
				frc = WebRtcIsac_lrint(PITCH_FRACS * ftmp - 0.5);
				fracoeff = kIntrpCoef[frc];
			}

			/* shift low pass filter state */
			for (m=PITCH_DAMPORDER-1; m>0; m--)
				inystate[m] = inystate[m-1];

			/* Filter to get fractional pitch */
			pos = ind + PITCH_BUFFSIZE;
			pos2 = pos - Li;
			sum=0.0;
			for (m=0; m<PITCH_FRACORDER; m++)
				sum += ubuf[pos2+m] * fracoeff[m];
			inystate[0] = curgain * sum; /* Multiply with gain */

			/* Low pass filter */
			sum=0.0;
			for (m=0; m<PITCH_DAMPORDER; m++)
				sum += inystate[m] * kDampFilter[m];

			/* Subtract from input and update buffer */
			outdat[ind] = indat[ind] - sum;
			ubuf[pos] = indat[ind] + outdat[ind];
			ind++;
		}
	}

	/* Export buffer and states */
	memcpy(pfp->ubuf, ubuf+PITCH_FRAME_LEN, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(pfp->ystate, inystate, sizeof(double) * PITCH_DAMPORDER);

	*pfp->oldlagp = oldlag;
	*pfp->oldgainp = oldgain;


	/* Filter look-ahead segment */
	for (n=0; n<QLOOKAHEAD; n++) {
		/* shift low pass filter state */
		for (m=PITCH_DAMPORDER-1; m>0; m--)
			inystate[m] = inystate[m-1];

		/* Filter to get fractional pitch */
		pos = ind + PITCH_BUFFSIZE;
		pos2 = pos - Li;
		sum=0.0;
		for (m=0; m<PITCH_FRACORDER; m++)
			sum += ubuf[pos2+m] * fracoeff[m];
		inystate[0] = curgain * sum; /* Multiply with gain */

		/* Low pass filter */
		sum=0.0;
		for (m=0; m<PITCH_DAMPORDER; m++)
			sum += inystate[m] * kDampFilter[m];

		/* Subtract from input and update buffer */
		outdat[ind] = indat[ind] - sum;
		ubuf[pos] = indat[ind] + outdat[ind];
		ind++;
	}
}


void WebRtcIsac_PitchfilterPre_gains(double *indat,
                                     double *outdat,
                                     double out_dG[][PITCH_FRAME_LEN + QLOOKAHEAD],
                                     PitchFiltstr *pfp,
                                     double *lags,
                                     double *gains)
{
	double ubuf[PITCH_INTBUFFSIZE+QLOOKAHEAD];
	double inystate_dG[4][PITCH_DAMPORDER];
	double gain_mult[4];
	const double *fracoeff = NULL;
	double curgain, curlag, gaindelta, lagdelta;
	double sum, sum2, inystate[PITCH_DAMPORDER];
	double ftmp, oldlag, oldgain;
	int    k, n, m, m_tmp, j, pos, ind, pos2, Li, frc;

	Li = 0;

	/* Set up buffer and states */
	memcpy(ubuf, pfp->ubuf, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(inystate, pfp->ystate, sizeof(double) * PITCH_DAMPORDER);

	/* clear some buffers */
	for (k = 0; k < 4; k++) {
		gain_mult[k] = 0.0;
		for (n = 0; n < PITCH_DAMPORDER; n++)
			inystate_dG[k][n] = 0.0;
	}

	oldlag = *pfp->oldlagp;
	oldgain = *pfp->oldgainp;

	/* No interpolation if pitch lag step is big */
	if ((lags[0] > (PITCH_UPSTEP * oldlag)) || (lags[0] < (PITCH_DOWNSTEP * oldlag))) {
		oldlag = lags[0];
		oldgain = gains[0];
		gain_mult[0] = 1.0;
	}


	ind=0;
	for (k=0; k<PITCH_SUBFRAMES; k++) {

		/* Calculate interpolation steps */
		lagdelta=(lags[k]-oldlag) / PITCH_GRAN_PER_SUBFRAME;
		curlag=oldlag ;
		gaindelta=(gains[k]-oldgain) / PITCH_GRAN_PER_SUBFRAME;
		curgain=oldgain ;
		oldlag=lags[k];
		oldgain=gains[k];

		for (n=0; n<PITCH_SUBFRAME_LEN; n++) {
			if ((ind % PITCH_UPDATE) == 0) {   /* Update parameters */
				curgain += gaindelta;
				curlag += lagdelta;
				Li = WebRtcIsac_lrint(curlag+PITCH_FILTDELAY + 0.5);
				ftmp = Li - (curlag+PITCH_FILTDELAY);
				frc = WebRtcIsac_lrint(PITCH_FRACS * ftmp - 0.5);
				fracoeff = kIntrpCoef[frc];
				gain_mult[k] += 0.2;
				if (gain_mult[k] > 1.0) gain_mult[k] = 1.0;
				if (k > 0) gain_mult[k-1] -= 0.2;
			}

			/* shift low pass filter states */
			for (m=PITCH_DAMPORDER-1; m>0; m--) {
				inystate[m] = inystate[m-1];
				for (j = 0; j < 4; j++)
					inystate_dG[j][m] = inystate_dG[j][m-1];
			}

			pos = ind + PITCH_BUFFSIZE;
			pos2 = pos - Li;

			/* Filter to get fractional pitch */
			sum=0.0;
			for (m=0; m<PITCH_FRACORDER; m++)
				sum += ubuf[pos2+m] * fracoeff[m];
			inystate[0] = curgain * sum;  /* Multiply with gain */
			m_tmp = (Li-ind > 0) ? Li-ind : 0;
			for (j = 0; j < k+1; j++) {
				/* filter */
				sum2 = 0.0;
				for (m = PITCH_FRACORDER-1; m >= m_tmp; m--)
					sum2 += out_dG[j][ind-Li + m] * fracoeff[m];
				inystate_dG[j][0] = gain_mult[j] * sum + curgain * sum2;
			}

			/* Low pass filter */
			sum=0.0;
			for (m=0; m<PITCH_DAMPORDER; m++)
				sum += inystate[m] * kDampFilter[m];

			/* Subtract from input and update buffer */
			outdat[ind] = indat[ind] - sum;
			ubuf[pos] = indat[ind] + outdat[ind];

			for (j = 0; j < k+1; j++) {
				sum = 0.0;
				for (m=0; m<PITCH_DAMPORDER; m++)
					sum -= inystate_dG[j][m] * kDampFilter[m];
				out_dG[j][ind] = sum;
			}
			for (j = k+1; j < 4; j++)
				out_dG[j][ind] = 0.0;


			ind++;
		}
	}

	/* Filter look-ahead segment */
	for (n=0; n<QLOOKAHEAD; n++) {
		/* shift low pass filter states */
		for (m=PITCH_DAMPORDER-1; m>0; m--) {
			inystate[m] = inystate[m-1];
			for (j = 0; j < 4; j++)
				inystate_dG[j][m] = inystate_dG[j][m-1];
		}

		pos = ind + PITCH_BUFFSIZE;
		pos2 = pos - Li;

		/* Filter to get fractional pitch */
		sum=0.0;
		for (m=0; m<PITCH_FRACORDER; m++)
			sum += ubuf[pos2+m] * fracoeff[m];
		inystate[0] = curgain * sum;  /* Multiply with gain */
		m_tmp = (Li-ind > 0) ? Li-ind : 0;
		for (j = 0; (j<k+1)&&(j<4); j++) {
			/* filter */
			sum2 = 0.0;
			for (m = PITCH_FRACORDER-1; m >= m_tmp; m--)
				sum2 += out_dG[j][ind-Li + m] * fracoeff[m];
			inystate_dG[j][0] = gain_mult[j] * sum + curgain * sum2;
		}

		/* Low pass filter */
		sum=0.0;
		for (m=0; m<PITCH_DAMPORDER; m++)
			sum += inystate[m] * kDampFilter[m];

		/* Subtract from input and update buffer */
		outdat[ind] = indat[ind] - sum;
		ubuf[pos] = indat[ind] + outdat[ind];

		for (j = 0; (j<k+1)&&(j<4); j++) {
			sum = 0.0;
			for (m=0; m<PITCH_DAMPORDER; m++)
				sum -= inystate_dG[j][m] * kDampFilter[m];
			out_dG[j][ind] = sum;
		}

		ind++;
	}
}


void WebRtcIsac_PitchfilterPost(double *indat,
                                double *outdat,
                                PitchFiltstr *pfp,
                                double *lags,
                                double *gains)
{

	double ubuf[PITCH_INTBUFFSIZE];
	const double *fracoeff = NULL;
	double curgain, curlag, gaindelta, lagdelta;
	double sum, inystate[PITCH_DAMPORDER];
	double ftmp, oldlag, oldgain;
	int    k, n, m, pos, ind, pos2, Li, frc;

	Li = 0;

	/* Set up buffer and states */
	memcpy(ubuf, pfp->ubuf, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(inystate, pfp->ystate, sizeof(double) * PITCH_DAMPORDER);

	oldlag = *pfp->oldlagp;
	oldgain = *pfp->oldgainp;

	/* make output more periodic */
	for (k=0; k<PITCH_SUBFRAMES; k++)
		gains[k] *= 1.3;

	/* No interpolation if pitch lag step is big */
	if ((lags[0] > (PITCH_UPSTEP * oldlag)) || (lags[0] < (PITCH_DOWNSTEP * oldlag))) {
		oldlag = lags[0];
		oldgain = gains[0];
	}


	ind=0;
	for (k=0; k<PITCH_SUBFRAMES; k++) {

		/* Calculate interpolation steps */
		lagdelta=(lags[k]-oldlag) / PITCH_GRAN_PER_SUBFRAME;
		curlag=oldlag ;
		gaindelta=(gains[k]-oldgain) / PITCH_GRAN_PER_SUBFRAME;
		curgain=oldgain ;
		oldlag=lags[k];
		oldgain=gains[k];

		for (n=0; n<PITCH_SUBFRAME_LEN; n++) {
			if ((ind % PITCH_UPDATE) == 0) {   /* Update parameters */
				curgain += gaindelta;
				curlag += lagdelta;
				Li = WebRtcIsac_lrint(curlag+PITCH_FILTDELAY + 0.5);
				ftmp = Li - (curlag+PITCH_FILTDELAY);
				frc = WebRtcIsac_lrint(PITCH_FRACS * ftmp - 0.5);
				fracoeff = kIntrpCoef[frc];
			}

			/* shift low pass filter state */
			for (m=PITCH_DAMPORDER-1; m>0; m--)
				inystate[m] = inystate[m-1];

			/* Filter to get fractional pitch */
			pos = ind + PITCH_BUFFSIZE;
			pos2 = pos - Li;
			sum=0.0;
			for (m=0; m<PITCH_FRACORDER; m++)
				sum += ubuf[pos2+m] * fracoeff[m];
			inystate[0] = curgain * sum; /* Multiply with gain */

			/* Low pass filter */
			sum=0.0;
			for (m=0; m<PITCH_DAMPORDER; m++)
				sum += inystate[m] * kDampFilter[m];

			/* Add to input and update buffer */
			outdat[ind] = indat[ind] + sum;
			ubuf[pos] = indat[ind] + outdat[ind];
			ind++;
		}
	}

	/* Export buffer and states */
	memcpy(pfp->ubuf, ubuf+PITCH_FRAME_LEN, sizeof(double) * PITCH_BUFFSIZE);
	memcpy(pfp->ystate, inystate, sizeof(double) * PITCH_DAMPORDER);

	*pfp->oldlagp = oldlag;
	*pfp->oldgainp = oldgain;

}
