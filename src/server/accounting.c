/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/*
 * accounting.c - contains functions to record accounting information
 *
 * Functions included are:
 * acct_open()
 * acct_record()
 * acct_close()
 */


#include <pbs_config.h>   /* the master config generated by configure */
#include "portability.h"
#include <sys/param.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "pbs_job.h"
#include "queue.h"
#include "log.h"
#include "../lib/Liblog/pbs_log.h"
#include "acct.h"
#ifdef USESAVEDRESOURCES
#include "resource.h"
#endif
#include "svrfunc.h"
#include "server.h"
#include "utils.h"

/* Local Data */

static FILE         *acctfile;  /* open stream for log file */
static volatile int  acct_opened = 0;
static int           acct_opened_day;
static int           acct_auto_switch = 0;
pthread_mutex_t     *acctfile_mutex;

/* Global Data */

extern attribute_def job_attr_def[];
extern char     *path_acct;
extern char     *acct_file;
extern int       LOGLEVEL;



#define EXTRA_PAD 1000 /* Used to bad the account buffer string */



/*
 * acct_job - build common data for start/end job accounting record
 * Used by account_jobstr() and account_jobend()
 */

int acct_job(

  job            *pjob, /* I */
  std::string&    ds)   /* O */

  {
  long        cray_enabled = FALSE;
  int         resc_access_perm = READ_ONLY;
  char        local_buf[MAXLINE*4];
  pbs_queue  *pque;

  tlist_head  attrlist;
  svrattrl   *pal;

  if (pjob == NULL)
    {
    return(PBSE_NONE);
    }

  CLEAR_HEAD(attrlist);

  if (LOGLEVEL >= 10)
    log_event(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB, __func__, pjob->ji_qs.ji_jobid);

  /* user */

	/* acct_job is only called from account_jobstr and account_jobend. BufSize should be
	 	 PBS_ACCT_MAX_RCD + 1 in size. */
  sprintf(local_buf, "user=%s ",
    pjob->ji_wattr[JOB_ATR_euser].at_val.at_str);
  ds += local_buf;

  /* group */
  sprintf(local_buf, "group=%s ",
    pjob->ji_wattr[JOB_ATR_egroup].at_val.at_str);
  ds += local_buf;

  /* account */
  if (pjob->ji_wattr[JOB_ATR_account].at_flags & ATR_VFLAG_SET)
    {
    sprintf(local_buf, "account=%s ",
      pjob->ji_wattr[JOB_ATR_account].at_val.at_str);
    ds += local_buf;
    }

  /* job name */
  sprintf(local_buf, "jobname=%s ",
    pjob->ji_wattr[JOB_ATR_jobname].at_val.at_str);
  ds += local_buf;

  if ((pque = get_jobs_queue(&pjob)) != NULL)
    {
    /* queue name */
    sprintf(local_buf, "queue=%s ", pque->qu_qs.qu_name);
    unlock_queue(pque, __func__, NULL, LOGLEVEL);

    ds += local_buf;
    }
  else if (pjob == NULL)
    {
    log_err(PBSE_JOBNOTFOUND, __func__, "Job lost while acquiring queue 1");
    return(PBSE_JOBNOTFOUND);
    }

  /* create time */
  sprintf(local_buf, "ctime=%ld ",
    pjob->ji_wattr[JOB_ATR_ctime].at_val.at_long);
  ds += local_buf;

  /* queued time */
  sprintf(local_buf, "qtime=%ld ",
    pjob->ji_wattr[JOB_ATR_qtime].at_val.at_long);
  ds += local_buf;

  /* eligible time, how long ready to run */
  sprintf(local_buf, "etime=%ld ",
    pjob->ji_wattr[JOB_ATR_etime].at_val.at_long);
  ds += local_buf;

  /* execution start time */
  sprintf(local_buf, "start=%ld ",
    (long)pjob->ji_qs.ji_stime);
  ds += local_buf;

  /* user */
  sprintf(local_buf, "owner=%s ",
    pjob->ji_wattr[JOB_ATR_job_owner].at_val.at_str);
  ds += local_buf;
 
  /* For large clusters strings can get pretty long. We need to see if there
     is a need to allocate a bigger buffer */
  /* execution host name */
  if (pjob->ji_wattr[JOB_ATR_exec_host].at_val.at_str != NULL)
    {
    ds += "exec_host=";
    ds += pjob->ji_wattr[JOB_ATR_exec_host].at_val.at_str;
    ds += " ";
    }

  get_svr_attr_l(SRV_ATR_CrayEnabled, &cray_enabled);
  if ((cray_enabled == TRUE) &&
      (pjob->ji_wattr[JOB_ATR_login_node_id].at_flags & ATR_VFLAG_SET))
    {
    ds += "login_node=";
    ds += pjob->ji_wattr[JOB_ATR_login_node_id].at_val.at_str;
    ds += " ";
    }

  /* now encode the job's resource_list pbs_attribute */
  job_attr_def[JOB_ATR_resource].at_encode(
    &pjob->ji_wattr[JOB_ATR_resource],
    &attrlist,
    job_attr_def[JOB_ATR_resource].at_name,
    NULL,
    ATR_ENCODE_CLIENT,
    resc_access_perm);

  while ((pal = (svrattrl *)GET_NEXT(attrlist)) != NULL)
    {
		/* exec_host can use a lot of buffer space. Use a dynamic string */
    ds += pal->al_name;

    if (pal->al_resc != NULL)
      {
      ds += ".";
      ds += pal->al_resc;
      }

    ds += "=";
    ds += pal->al_value;
    ds += " ";

    delete_link(&pal->al_link);
    free(pal);
    }  /* END while (pal != NULL) */

#ifdef ATTR_X_ACCT

  /* x attributes */
  if (pjob->ji_wattr[JOB_SITE_ATR_x].at_flags & ATR_VFLAG_SET)
    {
    sprintf(local_buf, "x=%s ",
            pjob->ji_wattr[JOB_SITE_ATR_x].at_val.at_str);
    ds += local_buf;
    }

#endif

  /* SUCCESS */

  return(PBSE_NONE);
  }  /* END acct_job() */







/*
 * acct_open() - open the acct file for append.
 *
 * Opens a (new) acct file.
 * If a acct file is already open, and the new file is successfully opened,
 * the old file is closed.  Otherwise the old file is left open.
 */

int acct_open(

  char *filename,  /* abs pathname or NULL */
  bool  acct_mutex_locked)

  {
  char  filen[_POSIX_PATH_MAX];
  char  logmsg[_POSIX_PATH_MAX + 80];
  FILE *newacct;
  time_t now;

  struct tm *ptm;
  struct tm tmpPtm;

  if (filename == NULL)
    {
    /* go with default */

    now = time(0);

    ptm = localtime_r(&now,&tmpPtm);

    sprintf(filen, "%s%04d%02d%02d",
            path_acct,
            ptm->tm_year + 1900,
            ptm->tm_mon + 1,
            ptm->tm_mday);

    filename = filen;

    acct_auto_switch = 1;

    acct_opened_day = ptm->tm_yday;
    }
  else if (*filename == '\0')
    {
    /* a null name is not an error */

    return(0);  /* turns off account logging.  */
    }
  else if (*filename != '/')
    {
    /* not absolute */

    return(-1);
    }

  if ((newacct = fopen(filename, "a")) == NULL)
    {
    log_err(errno, __func__, filename);

    return(-1);
    }

  setbuf(newacct, NULL);        /* set no buffering */

  if (acct_mutex_locked == false)
    pthread_mutex_lock(acctfile_mutex);

  if (acct_opened > 0)          /* if acct was open, close it */
    fclose(acctfile);

  acctfile = newacct;
  
  acct_opened = 1;  /* note that file is open */

  if (acct_mutex_locked == false)
    pthread_mutex_unlock(acctfile_mutex);

  sprintf(logmsg, "Account file %s opened",
          filename);

  log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, "Act", logmsg);

  return(0);
  }  /* END acct_open() */





/*
 * acct_close - close the current open log file
 */

void acct_close(
    
  bool acct_mutex_locked)

  {
  if (acct_mutex_locked == false)
    pthread_mutex_lock(acctfile_mutex);

  if (acct_opened == 1)
    {
    fclose(acctfile);

    acct_opened = 0;
    }

  if (acct_mutex_locked == false)
    pthread_mutex_unlock(acctfile_mutex);

  return;
  }  /* END acct_close() */





/*
 * account_record - write basic accounting record
 */

void account_record(

  int         acctype, /* accounting record type */
  job        *pjob,
  const char *text)  /* text to log, may be null */

  {
  time_t     time_now = time(NULL);
  struct tm *ptm;
  struct tm  tmpPtm;

  pthread_mutex_lock(acctfile_mutex);
  if (acct_opened == 0)
    {
    acct_open(acct_file, true);
    }

  ptm = localtime_r(&time_now,&tmpPtm);

  /* Do we need to switch files */

  if ((acct_auto_switch != 0) &&
      (acct_opened_day != ptm->tm_yday))
    {
    acct_close(true);

    acct_open(NULL, true);
    }

  if (text == NULL)
    text = (char *)"";

  fprintf(acctfile, "%02d/%02d/%04d %02d:%02d:%02d;%c;%s;%s\n",
          ptm->tm_mon + 1,
          ptm->tm_mday,
          ptm->tm_year + 1900,
          ptm->tm_hour,
          ptm->tm_min,
          ptm->tm_sec,
          (char)acctype,
          pjob->ji_qs.ji_jobid,
          text);
  pthread_mutex_unlock(acctfile_mutex);

  return;
  }  /* END account_record() */






/*
 * account_jobstr - write a job start record
 */

void account_jobstr(

  job *pjob)

  {
  std::string ds = "";

  acct_job(pjob, ds);

  account_record(PBS_ACCT_RUN, pjob, ds.c_str());

  return;
  }  /* END account_jobstr() */
 
 
 
/*
 * add_procs_and_nodes_used
 *
 * Adds a string specifying how many procs and nodes the job used
 * To count these values we parse the exec host list, which is in
 * the format of host/<index>[+host2/index2[+host3/index3[...]]]
 *
 * @param pjob (I) - the job whose procs we're measuring
 * @param acct_data (O) - the string we're adding to
 */

void add_procs_and_nodes_used(

  job         &pjob,
  std::string &acct_data)

  {
  if (pjob.ji_wattr[JOB_ATR_exec_host].at_val.at_str != NULL)
    {
    char        resc_buf[1024];
    std::string nodelist(pjob.ji_wattr[JOB_ATR_exec_host].at_val.at_str);
    std::size_t pos = 0;
    std::string last_host;
    int         hosts = 0;
    int         total_execution_slots = 0;

    while (pos < nodelist.size())
      {
      std::size_t      plus = nodelist.find("+", pos);
      std::string      host(nodelist.substr(pos, plus - pos));
      std::size_t      slash = host.find("/");
      std::string      range;
      std::vector<int> indices;

      // remove the /<index>
      if (slash != std::string::npos)
        {
        range = host.substr(slash + 1);
        host.erase(slash);
        }

      translate_range_string_to_vector(range.c_str(), indices);
      total_execution_slots += indices.size();

      if (last_host != host)
        {
        last_host = host;
        hosts++;
        }

      if (plus != std::string::npos)
        pos = plus + 1;
      else
        break;
      }

    snprintf(resc_buf, sizeof(resc_buf), "total_execution_slots=%d unique_node_count=%d ",
      total_execution_slots, hosts);

    acct_data += resc_buf;
    }

  } // END add_procs_and_nodes_used()



/*
 * account_jobend - write a job termination/resource usage record
 */

void account_jobend(

  job         *pjob,
  std::string &used) /* job usage information, see req_jobobit() */

  {
  std::string ds = "";
  char                local_buf[MAXLINE * 4];
#ifdef USESAVEDRESOURCES
  pbs_attribute      *pattr;
  long                walltime_val = 0;
#endif

  if ((acct_job(pjob, ds)) != PBSE_NONE)
    {
    return;
    }

  /* session */
  sprintf(local_buf, "session=%ld ",
    pjob->ji_wattr[JOB_ATR_session_id].at_val.at_long);

  ds += local_buf;

  /* Alternate id if present */
  if (pjob->ji_wattr[JOB_ATR_altid].at_flags & ATR_VFLAG_SET)
    {
    sprintf(local_buf, "alt_id=%s ",
      pjob->ji_wattr[JOB_ATR_altid].at_val.at_str);

    ds += local_buf;
    }

  add_procs_and_nodes_used(*pjob, ds);

  /* add the execution end time */
#ifdef USESAVEDRESOURCES
  pattr = &pjob->ji_wattr[JOB_ATR_resc_used];

  if (pattr->at_flags & ATR_VFLAG_SET)
    {
    resource *pres;
    const char     *pname;

    pres = (resource *)GET_NEXT(pattr->at_val.at_list);
    
    /* find the walltime resource */
    for (;pres != NULL;pres = (resource *)GET_NEXT(pres->rs_link))
      {
      pname = pres->rs_defin->rs_name;
      
      if (strcmp(pname, "walltime") == 0)
        {
        /* found walltime */
        walltime_val = pres->rs_value.at_val.at_long;
        break;
        }
      }
    }
  sprintf(local_buf, "end=%ld ", (long)pjob->ji_qs.ji_stime + walltime_val);
#else
  sprintf(local_buf, "end=%ld ", (long)pjob->ji_wattr[JOB_ATR_comp_time].at_val.at_long);
#endif /* USESAVEDRESOURCES */

  ds += local_buf;

  /* finally add on resources used from req_jobobit() */
  ds += used;

  account_record(PBS_ACCT_END, pjob, ds.c_str());

  return;
  }  /* END account_jobend() */

/*
 * acct_cleanup - remove the old accounting files
 */

void acct_cleanup(

  long	days_to_keep)	/* Number of days to keep accounting files */

  {

  if (log_remove_old(path_acct,(days_to_keep * SECS_PER_DAY)) != 0)
    {
    log_err(-1, __func__, "failure occurred when checking for old accounting logs");
    }

  return;
  }  /* END acct_cleanup() */




/* END accounting.c */

