#include "quip_config.h"

/*
 * Maximum liklihood Weibull fits
 * See appendix in Watson, "Probability summation over time"
 * Vis. Res. v. 19 pp. 515-522
 */

#ifdef HAVE_MATH_H
#include <math.h>
#endif

#include "stc.h"
#include "debug.h"

/*
 * Two methods for Weibull fit optimization:
 *
 * Original method called stepit (fortran).
 *
 * For greater portability, we now also support simplex method
 * from numerical recipes in C.
 */

#include "optimize.h"

static double	alpha,		/* threshold parameter */
		beta,		/* slope parameter */
		w_gamma;	/* lower asymptote or chance rate */

#define ALPHA_NAME	"alpha"
#define BETA_NAME	"beta"

#define DELTA		0.01		/* default finger error rate */
#define MIN_DELTA	0.0001		/* minumum finger error rate */
static double error_rate=DELTA;

static Data_Tbl *the_dtp;

#define ALPHA_INDEX	0
#define BETA_INDEX	1
#define N_WPARMS	2	/* number of variable paramters */

/* local prototypes */
static void weibull_fit(QSP_ARG_DECL  Data_Tbl *dp, int ntrac);

static float w_likelihood(SINGLE_QSP_ARG_DECL)		/* called from optimize; return likelihood of guess */
{

	float lh=0.0,lhinc;
	int i;
	int ntt,		/* number of total trials */
	    nc;			/* number "correct" */
	float pc,xv;

	float t_alpha, t_beta;	/* trial slope and int */
	Opt_Param *opp;

	/* compute the likelihood for this guess */

	opp=get_opt_param(QSP_ARG  ALPHA_NAME);
//#ifdef CAUTIOUS
//	if( opp==NO_OPT_PARAM ){
//		ERROR1("CAUTIOUS:  missing alpha param");
//		IOS_RETURN_VAL(0)
//	}
//#endif
	assert( opp != NO_OPT_PARAM );

	t_alpha = opp->ans;

	opp=get_opt_param(QSP_ARG  BETA_NAME);
//#ifdef CAUTIOUS
//	if( opp==NO_OPT_PARAM ){
//		ERROR1("CAUTIOUS:  missing beta param");
//		IOS_RETURN_VAL(0)
//	}
//#endif
	assert( opp != NO_OPT_PARAM );

	t_beta = opp->ans;

	for(i=0;i<_nvals;i++){

		/* calculate theoretical percent correct with this guess */

		if( (ntt=DATUM_NTOTAL(DTBL_ENTRY(the_dtp,i)) ) <= 0 )
			continue;

		nc=DATUM_NCORR( DTBL_ENTRY(the_dtp,i) );
		xv = xval_array[ i ];

		if( xv == 0.0 ) pc = (float) w_gamma;
		else {
			pc = (float) (1 - (1-w_gamma)*exp(-pow( xv/t_alpha, t_beta ) ));
			if( pc > (1-error_rate) ) pc =(float) ( 1-error_rate);
		}

		/* pc is the theoretical % correct at this xval */

		lhinc = (float) (nc * log( pc ) + ( ntt - nc ) * log( 1 - pc ));
		lh -= lhinc;
	}

	/* return the answer */

	return(lh);
}

static void weibull_fit(QSP_ARG_DECL  Data_Tbl *dp,int ntrac)		/** maximum liklihood fit */
{
	Opt_Param op1, *alpha_opp=(&op1);
	Opt_Param op2, *beta_opp=(&op2);

	/* initialize global */

	the_dtp = dp;

	/* initialize the parameters */

	delete_opt_params(SINGLE_QSP_ARG);	/* clear any existing parameters */

	alpha_opp->op_name = ALPHA_NAME;
	alpha_opp->ans = xval_array[ _nvals/2 ];
	if( xval_array[0] < xval_array[_nvals-1] ){
		alpha_opp->maxv =  xval_array[_nvals-1];
		alpha_opp->minv =  xval_array[0];
	} else {
		alpha_opp->maxv = xval_array[0];
		alpha_opp->minv = xval_array[_nvals-1];
	}
	if( alpha_opp->minv < 0.0 ){
		WARN("wiebull fit will blow up for negative x values");
		return;
	}
	alpha_opp->delta = (float) fabs( xval_array[_nvals/2] - xval_array[ (_nvals/2)+1 ] );
	alpha_opp->mindel = (float) 1.0e-30;

	add_opt_param(QSP_ARG  alpha_opp);


	beta_opp->op_name = BETA_NAME;
	beta_opp->ans = 2;
	beta_opp->maxv = 10000.0;
	beta_opp->minv = 0.0;
	beta_opp->delta = 0.5;
	beta_opp->mindel = (float) 1.0e-30;

	add_opt_param(QSP_ARG  beta_opp);

	if( fc_flag ){
		w_gamma = 0.5;
	} else {
		w_gamma = error_rate;
	}


	optimize(QSP_ARG  w_likelihood);

	alpha_opp=get_opt_param(QSP_ARG  ALPHA_NAME);
//#ifdef CAUTIOUS
//	if( alpha_opp==NO_OPT_PARAM ){
//		ERROR1("CAUTIOUS:  missing alpha param");
//		IOS_RETURN
//	}
//#endif
	assert( alpha_opp != NO_OPT_PARAM );

	alpha = alpha_opp->ans;

	beta_opp=get_opt_param(QSP_ARG  BETA_NAME);
//#ifdef CAUTIOUS
//	if( beta_opp==NO_OPT_PARAM ){
//		ERROR1("CAUTIOUS:  missing beta param");
//		IOS_RETURN
//	}
//#endif
	assert( beta_opp != NO_OPT_PARAM );

	beta = beta_opp->ans;

	/* clean up */
	del_opt_param(QSP_ARG  beta_opp);
	del_opt_param(QSP_ARG  alpha_opp);
}

void w_analyse( QSP_ARG_DECL  Trial_Class *tcp )		/** do a regression on the ith table */
{
	int ntrac=(-1);

	weibull_fit( QSP_ARG  CLASS_DATA_TBL(tcp), ntrac );
}

void weibull_out(QSP_ARG_DECL  Trial_Class * tcp)			/** verbose analysis report */
{
        sprintf(msg_str,"\nTrial_Class %s\n",CLASS_NAME(tcp));
	prt_msg(msg_str);

	sprintf(msg_str,"alpha (threshold):\t\t%f",alpha);
	prt_msg(msg_str);
	sprintf(msg_str,"beta (slope):\t\t%f",beta);
	prt_msg(msg_str);
	/* BUG print out chi-square like statistic */
}

void w_tersout(QSP_ARG_DECL  Trial_Class * tcp)
{
	sprintf(msg_str,"%s\t%f\t%f",CLASS_NAME(tcp),alpha,beta);
	prt_msg(msg_str);
	/* BUG should print out chi sq stat */
}

void w_set_error_rate(double er)
{
	if( er < 0 ){
		NWARN("error rate must be non-negative");
		return;
	} else if( er >= 1 ){
		NWARN("error rate cannot be >= 1");
		return;
	} else if( er < MIN_DELTA ){
		if( verbose ){
			sprintf(DEFAULT_ERROR_STRING,
	"Setting error rate to minimum permissable value:  %g", MIN_DELTA);
			NADVISE(DEFAULT_ERROR_STRING);
		}
		er = MIN_DELTA;
	}
	error_rate=er;
}


