#include "license_pbs.h" /* See here for the software license */
/*
 *
 * qalter - (PBS) alter batch job
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

  int    argc,  /* I */
  char **argv)  /* I */

  {
  int c;
  int errflg = 0;
  int any_failed = 0;
  char *pc;
  int i;
  int local_errno;
  int u_cnt, o_cnt, s_cnt, n_cnt;
  int asynch = FALSE;
  int rc = 0;

  struct attrl *attrib = NULL;
  char *keyword;
  char *pdepend;
  char *valuewd;
  time_t after;
  char a_value[80];

  char job_id[PBS_MAXCLTJOBID] = "";

  char job_id_out[PBS_MAXCLTJOBID] = "";
  char server_out[MAXSERVERNAME] = "";
  char rmt_server[MAXSERVERNAME] = "";
  char path_out[MAXPATHLEN + 1] = "";
  char extend[MAXPATHLEN] = "";
  char *extend_ptr = NULL; /* only give value if extend has value */

#define GETOPT_ARGS "a:A:c:e:h:j:k:l:m:M:n:N:o:p:qr:S:t:u:v:W:x:"

  while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    {
    switch (c)
      {

      case 'a':

        if ((after = cvtdate(optarg)) < 0)
          {
          fprintf(stderr, "qalter: illegal -a value\n");
          errflg++;

          break;
          }

        sprintf(a_value, "%ld",

                (long)after);

        set_attr(&attrib, ATTR_a, a_value);

        break;

      case 'A':

        set_attr(&attrib, ATTR_A, optarg);

        break;

      case 'c':

        while (isspace((int)*optarg))
          optarg++;

        pc = optarg;

        if (strlen(pc) == 1)
          {
          if ((*pc != 'n') && (*pc != 's') && (*pc != 'c'))
            {
            fprintf(stderr, "qalter: illegal -c value\n");

            errflg++;

            break;
            }
          }
        else
          {
          if (strncmp(pc, "c=", 2) != 0)
            {
            fprintf(stderr, "qalter: illegal -c value\n");

            errflg++;

            break;
            }

          pc += 2;

          if (*pc == '\0')
            {
            fprintf(stderr, "qalter: illegal -c value\n");

            errflg++;

            break;
            }

          while (isdigit(*pc))
            pc++;

          if (*pc != '\0')
            {
            fprintf(stderr, "qalter: illegal -c value\n");

            errflg++;

            break;
            }
          }

        set_attr(&attrib, ATTR_c, optarg);

        break;

      case 'e':
 
        rc = prepare_path(optarg, path_out,NULL);
        if ((rc == 0) || (rc == 3))
          {
          set_attr(&attrib, ATTR_e, path_out);
          }
        else
          {
          fprintf(stderr, "qalter: illegal -e value\n");

          errflg++;
          }

        break;

      case 'h':

        /* hold */

        while (isspace((int)*optarg))
          optarg++;

        if (strlen(optarg) == 0)
          {
          fprintf(stderr, "qalter: illegal -h value\n");
          errflg++;

          break;
          }

        pc = optarg;

        u_cnt = o_cnt = s_cnt = n_cnt = 0;

        while (*pc)
          {
          if (*pc == 'u')
            u_cnt++;
          else if (*pc == 'o')
            o_cnt++;
          else if (*pc == 's')
            s_cnt++;
          else if (*pc == 'n')
            n_cnt++;
          else
            {
            fprintf(stderr, "qalter: illegal -h value\n");

            errflg++;

            break;
            }

          pc++;
          }

        if (n_cnt && (u_cnt + o_cnt + s_cnt))
          {
          fprintf(stderr, "qalter: illegal -h value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_h, optarg);

        break;

      case 'j':

        if ((strcmp(optarg, "oe") != 0) &&
            (strcmp(optarg, "eo") != 0) &&
            (strcmp(optarg, "n")  != 0))
          {
          fprintf(stderr, "qalter: illegal -j value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_j, optarg);

        break;

      case 'k':

        /* keep */

        if ((strcmp(optarg, "o")  != 0) &&
            (strcmp(optarg, "e")  != 0) &&
            (strcmp(optarg, "oe") != 0) &&
            (strcmp(optarg, "eo") != 0) &&
            (strcmp(optarg, "n")  != 0))
          {
          fprintf(stderr, "qalter: illegal -k value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_k, optarg);

        break;

      case 'l':

        /* resource requirements */

        if (set_resources(&attrib, optarg, TRUE))
          {
          fprintf(stderr, "qalter: illegal -l value\n");

          errflg++;
          }

        break;

      case 'm':

        /* mail event */

        while (isspace((int)*optarg))
          optarg++;

        if (strlen(optarg) == 0)
          {
          fprintf(stderr, "qalter: illegal -m value\n");
          errflg++;

          break;
          }

        if (strcmp(optarg, "n") != 0)
          {
          pc = optarg;

          while (*pc)
            {
            if ((*pc != 'a') && (*pc != 'b') && (*pc != 'e'))
              {
              fprintf(stderr, "qalter: illegal -m value\n");

              errflg++;

              break;
              }

            pc++;
            }
          }

        set_attr(&attrib, ATTR_m, optarg);

        break;

      case 'M':

        /* mail destination */

        if (parse_at_list(optarg, FALSE, FALSE))
          {
          fprintf(stderr, "qalter: illegal -M value\n");
          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_M, optarg);

        break;
      
      case 'n':

        if (strlen(optarg) != 1)
          {
          fprintf(stderr, "qalter: illegal -n value\n");

          errflg++;

          break;
          }

        if ((*optarg != 'y') && (*optarg != 'n'))
          {
          fprintf(stderr, "qalter: illegal -n value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_node_exclusive, optarg);

        break;

      case 'N':

        /* name */

        if (check_job_name(optarg, 1) == 0)
          {
          set_attr(&attrib, ATTR_N, optarg);
          }
        else
          {
          fprintf(stderr, "qalter: illegal -N value\n");

          errflg++;
          }

        break;

      case 'o':

        /* output */
 
        rc = prepare_path(optarg, path_out,NULL);
        if ((rc == 0) || (rc == 3))
          {
          set_attr(&attrib, ATTR_o, path_out);
          }
        else
          {
          fprintf(stderr, "qalter: illegal -o value\n");

          errflg++;
          }

        break;

      case 'p':

        /* priority */

        while (isspace((int)*optarg))
          optarg++;

        pc = optarg;

        if ((*pc == '-') || (*pc == '+'))
          pc++;

        if (strlen(pc) == 0)
          {
          fprintf(stderr, "qalter: illegal -p value\n");

          errflg++;

          break;
          }

        while (*pc != '\0')
          {
          if (!isdigit(*pc))
            {
            fprintf(stderr, "qalter: illegal -p value\n");

            errflg++;

            break;
            }

          pc++;
          }

        i = atoi(optarg);

        if ((i < -1024) || (i > 1023))
          {
          fprintf(stderr, "qalter: illegal -p value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_p, optarg);

        break;

      case 'q':

        /* quick - asynchronous */

        asynch = TRUE;

        break;

      case 'r':

        /* rerun */

        if (strlen(optarg) != 1)
          {
          fprintf(stderr, "qalter: illegal -r value\n");

          errflg++;

          break;
          }

        if ((*optarg != 'y') && (*optarg != 'n'))
          {
          fprintf(stderr, "qalter: illegal -r value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_r, optarg);

        break;

      case 'S':

        if (parse_at_list(optarg, TRUE, TRUE))
          {
          fprintf(stderr, "qalter: illegal -S value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_S, optarg);

        break;

      case 't':

        if ((optarg != NULL) && 
            (strlen(optarg) > 0))
          {
          snprintf(extend,sizeof(extend),"%s%s",
            ARRAY_RANGE,
            optarg);
          extend_ptr = extend;
          }

        break;

      case 'u':

        if (parse_at_list(optarg, TRUE, FALSE))
          {
          fprintf(stderr, "qalter: illegal -u value\n");

          errflg++;

          break;
          }

        set_attr(&attrib, ATTR_u, optarg);

        break;

      case 'v':

        set_attr(&attrib, ATTR_v, optarg);

        break;

      case 'W':

        while (isspace((int)*optarg))
          optarg++;

        if (strlen(optarg) == 0)
          {
          fprintf(stderr, "qalter: illegal -W value\n");

          errflg++;

          break;
          }

        i = parse_equal_string(optarg, &keyword, &valuewd);

        while (i == 1)
          {
          if (strcmp(keyword, ATTR_depend) == 0)
            {
            int rtn = 0;
            pdepend = (char *)calloc(1, PBS_DEPEND_LEN);

            if ((pdepend == NULL) ||
                 (rtn = parse_depend_list(valuewd,pdepend,PBS_DEPEND_LEN)))
              {
              if (rtn == 2)
                {
                fprintf(stderr,"qalter: -W value exceeded max length (%d)\n",
                  PBS_DEPEND_LEN);
                }
              else
                {
                fprintf(stderr,"qalter: illegal -W value\n");
                }

              errflg++;

              break;
              }

            valuewd = pdepend;
            }
          else if (strcmp(keyword, ATTR_stagein) == 0)
            {
            if (parse_stage_list(valuewd))
              {
              fprintf(stderr, "qalter: illegal -W value\n");

              errflg++;

              break;
              }
            }
          else if (strcmp(keyword, ATTR_stageout) == 0)
            {
            if (parse_stage_list(valuewd))
              {
              fprintf(stderr, "qalter: illegal -W value\n");

              errflg++;

              break;
              }
            }
          else if (strcmp(keyword, ATTR_g) == 0)
            {
            if (parse_at_list(valuewd, TRUE, FALSE))
              {
              fprintf(stderr, "qalter: illegal -W value\n");

              errflg++;

              break;
              }
            }

          set_attr(&attrib, keyword, valuewd);

          i = parse_equal_string(NULL, &keyword, &valuewd);
          }

        if (i == -1)
          {
          fprintf(stderr, "qalter: illegal -W value\n");

          errflg++;
          }

        break;

      case 'x':

        /* exec_hosts */

        set_attr(&attrib, ATTR_exechost, optarg);

        break;

      case '?':

      default:

        errflg++;

        break;
      }  /* END switch(c) */
    }    /* END while(c = getopt()) */

  if (errflg || (optind == argc))
    {
    static char usage[] = "usage: qalter \
                          [-a date_time] [-A account_string] [-c interval] [-e path] \n\
                          [-h hold_list] [-j y|n] [-k keep] [-l resource_list] [-m mail_options] \n\
                          [-M user_list] [-N jobname] [-o path] [-p priority] [-q] [-r y|n] [-S path] \n\
                          [t [array range]%slot limit] [-u user_list] [-v variable_list] \n\
                          [-W additional_attributes] [-x exec_host] job_identifier...\n";

    fprintf(stderr, "%s", usage);

    exit(2);
    }

  for (;optind < argc;optind++)
    {
    int connect;
    int stat = 0;
    int located = FALSE;

    snprintf(job_id, sizeof(job_id), "%s", argv[optind]);

    if (get_server(job_id, job_id_out, sizeof(job_id_out), server_out, sizeof(server_out)))
      {
      fprintf(stderr, "qalter: illegally formed job identifier: %s\n",
              job_id);

      any_failed = 1;

      continue;
      }

cnt:

    connect = cnt2server(server_out);

    if (connect <= 0)
      {
      local_errno = -1 * connect;

      if (server_out[0] != 0)
        fprintf(stderr, "qalter: cannot connect to server %s (errno=%d) %s\n",
              server_out,
              local_errno,
              pbs_strerror(local_errno));
      else
        fprintf(stderr, "qalter: cannot connect to server %s (errno=%d) %s\n",
              pbs_server,
              local_errno,
              pbs_strerror(local_errno));

      any_failed = local_errno;

      continue;
      }

    if (asynch)
      {
      stat = pbs_alterjob_async_err(connect, job_id_out, attrib, extend_ptr, &any_failed);
      }
    else
      {
      stat = pbs_alterjob_err(connect, job_id_out, attrib, extend_ptr, &any_failed);
      }

    if ((stat != 0) &&
        (any_failed != PBSE_UNKJOBID))
      {
      prt_job_err((char *)"qalter", connect, job_id_out);
      }
    else if ((stat != 0) && 
             (any_failed != PBSE_UNKJOBID) &&
             !located)
      {
      located = TRUE;

      if (locate_job(job_id_out, server_out, rmt_server))
        {
        pbs_disconnect(connect);

        strcpy(server_out, rmt_server);

        goto cnt;
        }

      prt_job_err((char *)"qalter", connect, job_id_out);
      }  /* END else if (stat && ...) */

    pbs_disconnect(connect);
    }  /* END for (optind) */

  exit(any_failed);
  }  /* END main() */

/* END qalter.c */
