#include "license_pbs.h" /* See here for the software license */
/*
 *
 * qmsg - (PBS) send message to batch job
 *
 * Authors:
 *      Terry Heidelberg
 *      Livermore Computing
 *
 *      Bruce Kelly
 *      National Energy Research Supercomputer Center
 *
 *      Lawrence Livermore National Laboratory
 *      University of California
 */

#include "cmds.h"
#include "net_cache.h"
#include <pbs_config.h>   /* the master config generated by configure */
#include "../lib/Libifl/lib_ifl.h"


int main(
    
  int    argc,
  char **argv) /* qmsg */

  {
  int c;
  int to_file;
  int errflg = 0;
  int any_failed = 0;

  char job_id[PBS_MAXCLTJOBID];       /* from the command line */

  char job_id_out[PBS_MAXCLTJOBID];
  char server_out[MAXSERVERNAME] = "";
  char rmt_server[MAXSERVERNAME];

#define MAX_MSG_STRING_LEN 256
  char msg_string[MAX_MSG_STRING_LEN+1];

#define GETOPT_ARGS "EO"

  msg_string[0] = '\0';
  to_file = 0;

  while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    switch (c)
      {

      case 'E':
        to_file |= MSG_ERR;
        break;

      case 'O':
        to_file |= MSG_OUT;
        break;

      default :
        errflg++;
      }

  if (to_file == 0) to_file = MSG_ERR;   /* default */

  if (errflg || ((optind + 1) >= argc))
    {
    static char usage[] =
      "usage: qmsg [-O] [-E] msg_string job_identifier...\n";
    fprintf(stderr,"%s", usage);
    exit(2);
    }

  snprintf(msg_string, sizeof(msg_string), "%s", argv[optind]);

  for (optind++; optind < argc; optind++)
    {
    int connect;
    int stat = 0;
    int located = FALSE;

    snprintf(job_id, sizeof(job_id), "%s", argv[optind]);

    if (get_server(job_id, job_id_out, sizeof(job_id_out), server_out, sizeof(server_out)))
      {
      fprintf(stderr, "qmsg: illegally formed job identifier: %s\n", job_id);
      any_failed = 1;
      continue;
      }

cnt:

    connect = cnt2server(server_out);

    if (connect <= 0)
      {
      any_failed = -1 * connect;

      if (server_out[0] != 0)
        fprintf(stderr, "qmsg: cannot connect to server %s (errno=%d) %s\n",
              server_out, any_failed, pbs_strerror(any_failed));
      else
        fprintf(stderr, "qmsg: cannot connect to server %s (errno=%d) %s\n",
              pbs_server, any_failed, pbs_strerror(any_failed));
      continue;
      }

    stat = pbs_msgjob_err(connect, job_id_out, to_file, msg_string, NULL, &any_failed);

    if (stat && (any_failed != PBSE_UNKJOBID))
      {
      prt_job_err("qmsg", connect, job_id_out);
      }
    else if (stat && (any_failed == PBSE_UNKJOBID) && !located)
      {
      located = TRUE;

      if (locate_job(job_id_out, server_out, rmt_server))
        {
        pbs_disconnect(connect);
        strcpy(server_out, rmt_server);
        goto cnt;
        }

      prt_job_err("qmsg", connect, job_id_out);
      }

    pbs_disconnect(connect);
    }

  exit(any_failed);
  }
