#include <config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_VMMETER_H
#if !(defined(bsdi2) || defined(netbsd1))
#include <sys/vmmeter.h>
#endif
#endif
#if HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif
#if HAVE_ASM_PAGE_H
#include <asm/page.h>
#endif
#if HAVE_SYS_SWAP_H
#include <sys/swap.h>
#endif
#if HAVE_SYS_FS_H
#include <sys/fs.h>
#else
#if HAVE_UFS_FS_H
#include <ufs/fs.h>
#else
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_VNODE_H
#include <sys/vnode.h>
#endif
#ifdef HAVE_UFS_UFS_QUOTA_H
#include <ufs/ufs/quota.h>
#endif
#ifdef HAVE_UFS_UFS_INODE_H
#include <ufs/ufs/inode.h>
#endif
#if HAVE_UFS_FFS_FS_H
#include <ufs/ffs/fs.h>
#endif
#endif
#endif
#if HAVE_MTAB_H
#include <mtab.h>
#endif
#include <sys/stat.h>
#include <errno.h>
#if HAVE_FSTAB_H
#include <fstab.h>
#endif
#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif
#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#if (!defined(HAVE_STATVFS)) && defined(HAVE_STATFS)
#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#define statvfs statfs
#endif
#if HAVE_VM_SWAP_PAGER_H
#include <vm/swap_pager.h>
#endif
#if HAVE_SYS_FIXPOINT_H
#include <sys/fixpoint.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if STDC_HEADERS
#include <string.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "mibincl.h"
#include "struct.h"
#include "loadave.h"
#include "util_funcs.h"
#include "../kernel.h"
#include "read_config.h"
#include "agent_read_config.h"
#include "auto_nlist.h"

double maxload[3];

void init_loadave(void)
{

/* define the structure we're going to ask the agent to register our
   information at */
  struct variable2 extensible_loadave_variables[] = {
    {MIBINDEX, ASN_INTEGER, RONLY, var_extensible_loadave, 1, {MIBINDEX}},
    {ERRORNAME, ASN_OCTET_STR, RONLY, var_extensible_loadave, 1, {ERRORNAME}},
    {LOADAVE, ASN_OCTET_STR, RONLY, var_extensible_loadave, 1, {LOADAVE}},
    {LOADMAXVAL, ASN_OCTET_STR, RONLY, var_extensible_loadave, 1, {LOADMAXVAL}},
    {LOADAVEINT, ASN_INTEGER, RONLY, var_extensible_loadave, 1, {LOADAVEINT}},
#ifdef OPAQUE_SPECIAL_TYPES
    {LOADAVEFLOAT, ASN_OPAQUE_FLOAT, RONLY, var_extensible_loadave, 1, {LOADAVEFLOAT}},
#endif
    {ERRORFLAG, ASN_INTEGER, RONLY, var_extensible_loadave, 1, {ERRORFLAG}},
    {ERRORMSG, ASN_OCTET_STR, RONLY, var_extensible_loadave, 1, {ERRORMSG}}
  };

/* Define the OID pointer to the top of the mib tree that we're
   registering underneath */
  oid loadave_variables_oid[] = { EXTENSIBLEMIB,LOADAVEMIBNUM,1 };

  /* register ourselves with the agent to handle our mib tree */
  REGISTER_MIB("ucd_snmp/loadave", extensible_loadave_variables, variable2, \
               loadave_variables_oid);

  snmpd_register_config_handler("load", loadave_parse_config,
                                loadave_free_config, "max1 [max5] [max15]");
}

void loadave_parse_config(char *token, char* cptr)
{
  int i;
  
  for(i=0;i<=2;i++) {
    if (cptr != NULL)
      maxload[i] = atof(cptr);
    else
      maxload[i] = maxload[i-1];
    cptr = skip_not_white(cptr);
    cptr = skip_white(cptr);
  }
}

void loadave_free_config (void)
{
  int i;
  
  for (i=0; i<=2;i++)
    maxload[i] = DEFMAXLOADAVE;
}

  
unsigned char *var_extensible_loadave(struct variable *vp,
				      oid *name,
				      int *length,
				      int exact,
				      int *var_len,
				      WriteMethod **write_method)
{

  static long long_ret;
  static float float_ret;
  static char errmsg[300];
#ifdef HAVE_SYS_FIXPOINT_H
  fix favenrun[3];
#endif
#if defined(ultrix) || defined(sun) || defined(__alpha)
#if defined(sun) || defined(__alpha)
  long favenrun[3];
#define FIX_TO_DBL(_IN) (((double) _IN)/((double) FSCALE))
#endif
  int i;
#endif
  double avenrun[3];
  
  if (!checkmib(vp,name,length,exact,var_len,write_method,3))
    return(NULL);

  switch (vp->magic) {
    case MIBINDEX:
      long_ret = name[*length-1];
      return((u_char *) (&long_ret));
    case ERRORNAME:
      sprintf(errmsg,"Load-%d",((name[*length-1] == 1) ? 1 :
                                ((name[*length-1] == 2) ? 5 : 15)));
      *var_len = strlen(errmsg);
      return((u_char *) (errmsg));
  }
#ifdef HAVE_GETLOADAVG
  if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) == -1)
    return(0);
#elif defined(linux)
  { FILE *in = fopen("/proc/loadavg", "r");
    if (!in) {
      fprintf (stderr, "snmpd: cannot open /proc/loadavg\n");
      return NULL;
    }
    fscanf(in, "%lf %lf %lf", &avenrun[0], &avenrun[1], &avenrun[2]);
    fclose(in);
  }
#elif defined(ultrix) || defined(sun) || defined(__alpha)
  if (auto_nlist(LOADAVE_SYMBOL,(char *) favenrun, sizeof(favenrun)) == 0)
    return(0);
  for(i=0;i<3;i++)
    avenrun[i] = FIX_TO_DBL(favenrun[i]);
#else
  if (auto_nlist(LOADAVE_SYMBOL,(char *) avenrun, sizeof(double)*3) == 0)
    return NULL;
#endif
  switch (vp->magic) {
    case LOADAVE:
      sprintf(errmsg,"%.2f",avenrun[name[*length-1]-1]);
      *var_len = strlen(errmsg);
      return((u_char *) (errmsg));
    case LOADMAXVAL:
      sprintf(errmsg,"%.2f",maxload[name[*length-1]-1]);
      *var_len = strlen(errmsg);
      return((u_char *) (errmsg));
    case LOADAVEINT:
      long_ret = (u_long) (avenrun[name[*length-1]-1]*100);
      return((u_char *) (&long_ret));
#ifdef OPAQUE_SPECIAL_TYPES
    case LOADAVEFLOAT:
      float_ret = avenrun[name[*length-1]-1];
      *var_len = sizeof(float_ret);
      return((u_char *) (&float_ret));
#endif
    case ERRORFLAG:
      long_ret = (maxload[name[*length-1]-1] != 0 &&
                  avenrun[name[*length-1]-1] >= maxload[name[*length-1]-1]) ? 1 : 0;
      return((u_char *) (&long_ret));
    case ERRORMSG:
      if (maxload[name[*length-1]-1] != 0 &&
          avenrun[name[*length-1]-1] >= maxload[name[*length-1]-1]) {
        sprintf(errmsg,"%d min Load Average too high (= %.2f)",
                (name[*length-1] == 1)?1:((name[*length-1] == 2)?5:15),
                avenrun[name[*length-1]-1]);
      } else {
        errmsg[0] = 0;
      }
      *var_len = strlen(errmsg);
      return((u_char *) errmsg);
  }
  return NULL;
}

