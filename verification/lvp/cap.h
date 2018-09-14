/*************************************************************************
 *
 *  (c) 1996-2018 Rajit Manohar
 *
 *************************************************************************/
#ifndef __CAP_H__
#define __CAP_H__

/*
 *
 *  All capacitance numbers are in SI units
 *
 *
 */
#include "var.h"

#define C_VISITED 0x1
#define C_OUTPUT  0x2
#define C_SUPPLY  0x4

#define C_CUTSET  0x8

struct gnode;

typedef struct gedge {
  int w,l;
  struct gnode *n;
  struct gedge *next;
} gedge_t;
    
typedef struct gnode {
  unsigned int flags;
  double cap;
  struct gedge *e;
} gnode_t;

extern void inc_coupling_errs (void);
extern void inc_chargesharing_errs (void);
extern void compute_derived_params (void);

extern double BSIM2_OXE;
extern double BSIM2_TOX;
extern double BSIM2_N_LDAC;
extern double BSIM2_N_CGD0;
extern double BSIM2_P_LDAC;
extern double BSIM2_P_CGD0;

extern double BSIM2_N_CJ;
extern double BSIM2_N_MJ;
extern double BSIM2_N_PB;
extern double BSIM2_N_CJSW;
extern double BSIM2_N_MJSW;
extern double BSIM2_N_PHP;
extern double BSIM2_P_CJ;
extern double BSIM2_P_MJ;
extern double BSIM2_P_PB;
extern double BSIM2_P_CJSW;
extern double BSIM2_P_MJSW;
extern double BSIM2_P_PHP;

void approx_calculate_p_vext
                     (struct capacitance *output, struct capacitance *exposed,
		      struct capacitance *pchg, 
		      struct capacitance *pchgother, 
		      double *vnorm, double *vother,
		      double adjust_charge,
		      double coupling_ratio);



void approx_calculate_n_vext
                     (struct capacitance *output, struct capacitance *exposed,
		      struct capacitance *pchg,
		      struct capacitance *pchgother,
		      double *vnorm, double *vother,
		      double adjust_charge,
		      double coupling_ratio);


#define NORM_PCHG(flags,type)  (((flags)&VAR_PCHG) || ((type) == P_TYPE && ((flags)&(VAR_PCHG_PDN|VAR_PCHG_NDN))) || ((type) == N_TYPE && ((flags)&(VAR_PCHG_NUP|VAR_PCHG_PUP))))

#define SOME_PCHG(flags) ((flags)&VAR_PCHG_ALL)

#endif /* __CAP_H__ */
