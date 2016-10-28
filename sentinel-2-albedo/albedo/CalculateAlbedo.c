/* 
 * !Description
 *  Landsat spectral albedo calculation and conversion from narrow to broad bands
 *              using Tao He's 2012 N2B coefficients
 *      
 * !Usage
 *   parseParameters (filename, ptr_LndSensor, ptr_MODIS)
 * 
 * !Developer
 *  Initial version (V1.0.0) Yanmin Shuai (Yanmin.Shuai@nasa.gov)
 *                                              and numerous other contributors
 *                                              (thanks to all)
 *
 * !Revision History
 * 
 *
 *!Team-unique header:
 * This system (research version) was developed for NASA TE project "Albedo  
 *         Trends Related to Land CoverChange and Disturbance: A Multi-sensor   
 *         Approach", and is adopted by USGS Landsat science team (2012-2017) 
 *         to produce the long-term land surface albedo dataset over North America
 *         at U-Mass Boston (PI: Crystal B. Schaaf, Co-Is:Zhongping Lee and Yanmin Shuai). 
 * 
 * !References and Credits
 * Principal Investigator: Jeffrey G. masek (Jeffrey.G.Masek@nasa.gov)
 *                                       Code 618, Biospheric Science Laboratory, NASA/GSFC.
 *
 * Please contact us with a simple description of your application, if you have any interest.
 */

#include "lndsr_albedo.h"

int
CalculateAlbedo (DIS_LANDSAT * sensor, double *lndPixAlbedo, int row, int col,
								 int8 * QA, int snow_flag)
/*  
 Calculate the spectral Black Sky albedo and White Sky albedo using the BSA&WSA_ANratio
 Narrow-to-broad band conversion by Shunlin's coefficents simulated by the Santa Barbara 
 DISORT Atmospheric Radiative Transfer code. 
*/
{
	int icls, BRDFcls, k;
	int iband;
	double SZA, SAA, pix_VZA, pix_VAA;

	int TMBandIndx[6] = { 2, 3, 0, 1, 5, 6 };	/*TM band(0-4, 6) Index in the related MODIS band order */
	int TM_Indx;									/*LandSat Thermal band written as the last band in LEDAPS output */

	/* Coefficients in Landsat band series from band 1-5 & band 7 plus the intercept */
	/*Yanmin 201209 changed to new N2B TM/ETM coefficients generated by Tao He using ~250 spectrum libarary -USGS & ASTER */
	double ETM_coefficents_sw[7] = { 0.3141, 0.000, 0.1607, 0.3694, 0.1160, 0.0456, -0.0057 };	/* for BB-SW using ETM */
	double ETM_coefficents_vis[7] = { 0.5610, 0.2404, 0.2012, 0.000, 0.000, 0.000, -0.0026 };	/* for BB-SW using ETM */
	double ETM_coefficents_nir[7] = { 0.000, 0.000, 0.000, 0.6668, 0.2861, 0.0572, -0.0042 };	/* for BB-SW using ETM */

	double _TM_coefficents_sw[7] = { 0.3206, 0.000, 0.1572, 0.3666, 0.1162, 0.0457, -0.0063 };	/* for BB-SW using TM */
	double TM_coefficents_vis[7] = { 0.6000, 0.2204, 0.1828, 0.000, 0.000, 0.000, -0.0033 };	/* for BB-SW using TM */
	double TM_coefficents_nir[7] = { 0.000, 0.000, 0.000, 0.6646, 0.2859, 0.0566, -0.0037 };	/* for BB-SW using TM */

	//double LC8_coefficents_sw[7] = {.0763975, .1601541, .229087, .3140529, .0671073, .1197409, -.001175};

	double LC8_n2b_lib[7] =
		{ 0.2453421, 0.050843, 0.1803945, 0.3080635, 0.1331847, 0.0521349,
		0.0011052
	};
	double LC8_n2b_lib_snow[7] =
		{ 1.22416, -.431845, -.3446429, .3367926, .1834496, .2554519, -.0052154 };

	// Sentinel-2 MSI, n2b, 2016 April 29, from Qingsong Sun, Band 2, 3, 4, 8A, 11, and 12
	double MSI_n2b_lib_sw[7] = { .2687617, .0361839, .1501418, .3044542, .164433, .0356021, -.0048673 };	// inherent
	double MSI_n2b_lib_sw_snow[7] = { -.1992158, 2.300191, -1.912122, .6714989, -2.272847, 1.934139, -.0001144 };	//apparent

	/*double *TM_coefficents_sw = LC8_coefficents_sw; */
	double *TM_coefficents_sw = _TM_coefficents_sw;

	int lndSR_flag;

	double VZACrFct = 0.0;
	int16 tmp_BRDF[3];
	double tmp_ANR_wsa, tmp_ANR_bsa;
	int BRDFava_flg;

	int16 tmpQA[6];								/*Yanmin 2012/06 intermediate result for the QA determination */

	lndSR_flag = 0;
	SZA = sensor->SZA;
	SAA = sensor->SAA;
	pix_VZA = sensor->pix_VZA;		/*CalPixVZA (sensor, row, col); */
	pix_VAA = sensor->pix_VAA;		/* CalPixAzimuth(sensor,row,col); */


	if (sensor->scene.instrument == _OLI_)
		{
			TM_coefficents_sw = LC8_n2b_lib;
			if (snow_flag != 0)
				{
					TM_coefficents_sw = LC8_n2b_lib_snow;
				}
			//TM_coefficents_sw = LC8_n2b_lib_snow;
		}
	else if (sensor->scene.instrument == MSI)
		{
			if (snow_flag != 0)
				{
					TM_coefficents_sw = MSI_n2b_lib_sw_snow;
				}
			else
				{
					TM_coefficents_sw = MSI_n2b_lib_sw;
				}
		}
	else
		{
			TM_coefficents_sw = _TM_coefficents_sw;
		}

	icls = sensor->Clsdata[col];
	if ((icls == sensor->Cls_fillValue) || (icls > sensor->actu_ncls)
			|| (icls < 0))
		return FAILURE;

#ifdef DEBUG
	if (row == DEBUG_irow && col == DEBUG_icol)
		{
			printf ("SZA = %f, iVZA = %f, icls = %d\n", SZA, pix_VZA, icls);
		}
#endif

	/*if (loadSensorSRPixel(sensor, row, col)==FAILURE){
	   fprintf(stderr, "\nfailed to read the lndsr value for pixel: %d,%d!",row,col);
	   return FAILURE;
	   } */

	for (k = 0; k < 6; k++)
		tmpQA[k] = 0;

	for (iband = 0; iband < sensor->nbands; iband++)
		{

			TM_Indx = TMBandIndx[iband];

			/* Yanmin 2012/06 determine if the BRDF is available for given icls, 
			   otherwise seeking the closest class in the spectral space--based on the Among-classes-distance */
			BRDFava_flg = 0;
			if ((sensor->BRDF_paras[icls][TM_Indx][0] == 0)
					&& (sensor->BRDF_paras[icls][TM_Indx][1] == 0)
					&& (sensor->BRDF_paras[icls][TM_Indx][2] == 0))
				BRDFava_flg = 1;

			if (BRDFava_flg == 0)
				{
					BRDFcls = icls;
					if (sensor->pure_thrsh[icls] > PUREPIX_thredhold * HIS_BIN)	/*have enough pure-pixel for this BRDF average */
						tmpQA[0]++;
					if (sensor->pure_thrsh[icls] == PUREPIX_thredhold * HIS_BIN)	/*still have some pure-pixel but the number is less than top 15% */
						tmpQA[1]++;
				}

			if (BRDFava_flg != 0)
				{
					BRDFcls = getClosestCls (sensor, icls, TM_Indx);
					if (BRDFcls == -1)
						{
							//fprintf (stderr, "\nFailed to find the closest class for icls=%d at pixel(%d,%d) !", icls, row, col);
							return FAILURE;
						}
					tmpQA[2]++;
				}

			tmp_BRDF[0] = (int16) (sensor->BRDF_paras[BRDFcls][TM_Indx][0] + 0.5);
			tmp_BRDF[1] = (int16) (sensor->BRDF_paras[BRDFcls][TM_Indx][1] + 0.5);
			tmp_BRDF[2] = (int16) (sensor->BRDF_paras[BRDFcls][TM_Indx][2] + 0.5);

#ifdef DEBUG
			if (row == DEBUG_irow && col == DEBUG_icol)
				{
					printf ("iband=%d, TM_Indx=%d, BRDFcls=%d, tmp_BRDF=%d,%d,%d\n",
									iband, TM_Indx, BRDFcls, tmp_BRDF[0], tmp_BRDF[1],
									tmp_BRDF[2]);
				}
#endif

			if ((sensor->OneRowlndSR[iband][col] != sensor->SR_fillValue)
					&& (tmp_BRDF[0] != 0))
				{
					lndSR_flag += 0;
					if (CalculateANratio
							(tmp_BRDF, SZA, SAA, pix_VZA, pix_VAA, &tmp_ANR_wsa,
							 &tmp_ANR_bsa) != SUCCESS)
						{
							fprintf (stderr,
											 "\nCalLndAlbedoQA(): fail to calcualte the ANratio via CalculateANratio().\n");
							exit (1);
						};

					/*VZA correction of the spectral LndSR */
					/*VZACrFct=GetVZARefCorrectionFactor(tmp_BRDF,SZA,iVZA);
					   if(VZACrFct==FAILURE){
					   printf(stderr,"\nCalAlbedo(): fail to calcualte the VZA effect-factor via GetVZARefCorrectionFactor().\n");
					   VZACrFct=1.0;    
					   }; */

					/*Calculate the spectral narrow band albedos for six LandSat bands, no thermal band */
					/*Yanmin 2012/06 constrain the negative reflectance (probably caused by over atmo-correction) to zero */
					if (sensor->OneRowlndSR[iband][col] < 0)
						sensor->OneRowlndSR[iband][col] = 0;
					/*Yanmin 2012/06 constrain the high value over 12000 to 9900 */
					if (sensor->OneRowlndSR[iband][col] >= (1.2 / sensor->lndSR_ScaFct))
						sensor->OneRowlndSR[iband][col] = (0.99 / sensor->lndSR_ScaFct);

					/*spectral black sky albedo */
					lndPixAlbedo[iband * 2 + 0] =
						sensor->lndSR_ScaFct * (double) sensor->OneRowlndSR[iband][col] *
						tmp_ANR_bsa /**VZACrFct*/ ;
					/*spectral white sky albedo */
					lndPixAlbedo[iband * 2 + 1] =
						sensor->lndSR_ScaFct * (double) sensor->OneRowlndSR[iband][col] *
						tmp_ANR_wsa /**VZACrFct*/ ;

					if ((lndPixAlbedo[iband * 2 + 0] < 0)
							|| (lndPixAlbedo[iband * 2 + 1] < 0))
						{
							tmpQA[3]++;
							/*Yanmin 2012/06 apply the lambertian assumption to the pixel which has negative narrow BSA or WSA */
							lndPixAlbedo[iband * 2 + 0] =
								sensor->lndSR_ScaFct *
								(double) sensor->OneRowlndSR[iband][col];
							lndPixAlbedo[iband * 2 + 1] =
								sensor->lndSR_ScaFct *
								(double) sensor->OneRowlndSR[iband][col];
						}

				}

			/* treat it as an isotropic pixel if no-available BRDF can be used */
			if ((tmp_BRDF[0] == 0)
					&& (sensor->OneRowlndSR[iband][col] != sensor->SR_fillValue))
				{
					lndPixAlbedo[iband * 2 + 0] =
						sensor->lndSR_ScaFct * (double) sensor->OneRowlndSR[iband][col];
					lndPixAlbedo[iband * 2 + 1] =
						sensor->lndSR_ScaFct * (double) sensor->OneRowlndSR[iband][col];
					tmpQA[4]++;
				}
			/*non-available lndSR */
			if (sensor->OneRowlndSR[iband][col] == sensor->SR_fillValue)
				{
					tmpQA[5]++;
					lndSR_flag += 1;
				}

#ifdef DEBUG
			if (row == DEBUG_irow && col == DEBUG_icol)
				{
					printf
						("BAND=%d, ANR_WSA=%f, ANR_BSA=%f, LNDSR=%d, bsa=%f, wsa=%f\n",
						 iband, tmp_ANR_wsa, tmp_ANR_bsa, sensor->OneRowlndSR[iband][col],
						 lndPixAlbedo[iband * 2 + 0], lndPixAlbedo[iband * 2 + 1]);
				}
#endif

		}														/*end for (iband=0... spectral albedo calculation */

	if (lndSR_flag != 0)
		{
			*QA = -1;									/*Yanmin 2012/06: return fillvalue in lndPixAlbedo[] */
			return FAILURE;
		}

	if (tmpQA[0] == sensor->nbands)	/*with good concurrent BRDFs from enough pure-pixels */
		*QA = 0;
	else if (tmpQA[0] + tmpQA[1] == sensor->nbands)	/*with concurrent BRDFs */
		*QA = 1;
	else if (tmpQA[2] + tmpQA[0] + tmpQA[1] == sensor->nbands)	/*with some borrowed BRDFs from the closest class */
		*QA = 2;
	else if (tmpQA[3] > 0)				/*with an isotropic assumption */
		*QA = 3;

#ifdef DEBUG
	if (row == DEBUG_irow && col == DEBUG_icol)
		{
			printf ("QA=%d\n", *QA);
		}
#endif

	/*calculate the shortwave band albedo */
	if (lndSR_flag == 0)
		{
			lndPixAlbedo[12] = 0.0;
			lndPixAlbedo[13] = 0.0;
			lndPixAlbedo[14] = 0.0;
			lndPixAlbedo[15] = 0.0;
			lndPixAlbedo[16] = 0.0;
			lndPixAlbedo[17] = 0.0;

			for (iband = 0; iband < sensor->nbands; iband++)
				{
					/*narrow-to-broad band conversion by shunlin's coefficents for TM SW albedo */
					/*Landsat Broad band ShortWave White Sky Albedo using Liang's 2000 TM coefficients */
					lndPixAlbedo[12] +=
						TM_coefficents_sw[iband] * lndPixAlbedo[iband * 2 + 0];
					/*Landsat Broad band ShortWave Black Sky Albedo using Liang's 2000 TM coefficients */
					lndPixAlbedo[13] +=
						TM_coefficents_sw[iband] * lndPixAlbedo[iband * 2 + 1];
					/* visible BB */
					lndPixAlbedo[14] +=
						TM_coefficents_vis[iband] * lndPixAlbedo[iband * 2 + 0];
					lndPixAlbedo[15] +=
						TM_coefficents_vis[iband] * lndPixAlbedo[iband * 2 + 1];
					/* Near InfraRed BB */
					lndPixAlbedo[16] +=
						TM_coefficents_nir[iband] * lndPixAlbedo[iband * 2 + 0];
					lndPixAlbedo[17] +=
						TM_coefficents_nir[iband] * lndPixAlbedo[iband * 2 + 1];
				}

			/*apply the adjustment for the narrow-to-broad band conversion */
			lndPixAlbedo[12] += TM_coefficents_sw[6];
			lndPixAlbedo[13] += TM_coefficents_sw[6];

			lndPixAlbedo[14] += TM_coefficents_vis[6];
			lndPixAlbedo[15] += TM_coefficents_vis[6];

			lndPixAlbedo[16] += TM_coefficents_nir[6];
			lndPixAlbedo[17] += TM_coefficents_nir[6];

			/*Yanmin Shuai 2012/06 recalculate the BSA & WSA with the lambertian assumption if the A/N-ratio based albedos are negative. */
			if (lndPixAlbedo[12] <= 0 || lndPixAlbedo[13] <= 0)
				{
					for (iband = 0; iband < sensor->nbands; iband++)
						{
							lndPixAlbedo[12] +=
								TM_coefficents_sw[iband] *
								(double) sensor->OneRowlndSR[iband][col] *
								sensor->lndSR_ScaFct;
							lndPixAlbedo[13] +=
								TM_coefficents_sw[iband] *
								(double) sensor->OneRowlndSR[iband][col] *
								sensor->lndSR_ScaFct;
						}

					/*apply the adjustment for the narrow-to-broad band conversion */
					lndPixAlbedo[12] += TM_coefficents_sw[6];
					lndPixAlbedo[13] += TM_coefficents_sw[6];
					*QA = 4;
				}

			/* for visible band */
			if (lndPixAlbedo[14] <= 0 || lndPixAlbedo[15] <= 0)
				{
					for (iband = 0; iband < sensor->nbands; iband++)
						{
							lndPixAlbedo[14] +=
								TM_coefficents_vis[iband] *
								(double) sensor->OneRowlndSR[iband][col] *
								sensor->lndSR_ScaFct;
							lndPixAlbedo[15] +=
								TM_coefficents_vis[iband] *
								(double) sensor->OneRowlndSR[iband][col] *
								sensor->lndSR_ScaFct;
						}

					/*apply the adjustment for the narrow-to-broad band conversion */
					lndPixAlbedo[14] += TM_coefficents_vis[6];
					lndPixAlbedo[15] += TM_coefficents_vis[6];
					*QA = 4;
				}
			/* for NIR band */
			if (lndPixAlbedo[16] <= 0 || lndPixAlbedo[17] <= 0)
				{
					for (iband = 0; iband < sensor->nbands; iband++)
						{
							lndPixAlbedo[16] +=
								TM_coefficents_nir[iband] *
								(double) sensor->OneRowlndSR[iband][col] *
								sensor->lndSR_ScaFct;
							lndPixAlbedo[17] +=
								TM_coefficents_nir[iband] *
								(double) sensor->OneRowlndSR[iband][col] *
								sensor->lndSR_ScaFct;
						}

					/*apply the adjustment for the narrow-to-broad band conversion */
					lndPixAlbedo[16] += TM_coefficents_nir[6];
					lndPixAlbedo[17] += TM_coefficents_nir[6];
					*QA = 4;
				}
			return SUCCESS;
		}

}
