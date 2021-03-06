/************************************************************\
 * Copyright 2014 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <jansson.h>
#include <inttypes.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/xzmalloc.h"


/******************************************************************************
 *                                                                            *
 *                Internal types, macros and static variables                 *
 *                                                                            *
 ******************************************************************************/
typedef struct {
    flux_t *h;
    FILE *op;
} jstatctx_t;

/* Note: Should only be used for signal handling */
static flux_t *sig_flux_h;

#define OPTIONS "o:h"
static const struct option longopts[] = {
    {"help",       no_argument,        0, 'h'},
    {"testout",    required_argument,  0, 'o'},
    { 0, 0, 0, 0 },
};


/******************************************************************************
 *                                                                            *
 *                              Utilities                                     *
 *                                                                            *
 ******************************************************************************/
static void usage (int code)
{
    fprintf (stderr,
"Usage: flux-jstat notify\n"
"       flux-jstat query jobid <top-level JCB attribute>\n"
"       flux-jstat update jobid <top-level JCB attribute> <JCB JSON>\n"
);
    exit (code);
}

static void freectx (void *arg)
{
    jstatctx_t *ctx = arg;
    if (ctx->op)
        fclose (ctx->op);
}

static jstatctx_t *getctx (flux_t *h)
{
    jstatctx_t *ctx = (jstatctx_t *)flux_aux_get (h, "jstat");
    if (!ctx) {
        ctx = xzmalloc (sizeof (*ctx));
        ctx->h = h;
        ctx->op = NULL;
        flux_aux_set (h, "jstat", ctx, freectx);
    }
    return ctx;
}

static void sig_handler (int s)
{
    if (s == SIGINT) {
        fprintf (stdout, "Exit on INT");
        exit (0);
    }
}

static FILE *open_test_outfile (const char *fn)
{
    FILE *fp;
    if (!fn)
        fp = NULL;
    else if ( !(fp = fopen (fn, "w")))
        fprintf (stderr, "Failed to open %s\n", fn);
    return fp;
}

/******************************************************************************
 *                                                                            *
 *                      Async notification callback                           *
 *                                                                            *
 ******************************************************************************/

static int job_status_cb (const char *jcbstr, void *arg, int errnum)
{
    int os;
    int ns;
    jstatctx_t *ctx = NULL;
    flux_t *h = (flux_t *)arg;
    json_t *jcb = NULL;

    ctx = getctx (h);
    if (errnum > 0) {
        flux_log (ctx->h, LOG_ERR, "job_status_cb: errnum passed in");
        return -1;
    }

    if (!(jcb = json_loads (jcbstr, 0, NULL))
        || json_unpack (jcb, "{s:{s:i s:i}}", JSC_STATE_PAIR,
                                              JSC_STATE_PAIR_OSTATE, &os,
                                              JSC_STATE_PAIR_NSTATE, &ns) < 0) {
        flux_log (ctx->h, LOG_ERR, "job_status_cb: error parsing JSON string");
        json_decref (jcb);
        return -1;
    }

    fprintf (ctx->op, "%s->%s\n", jsc_job_num2state (os),
                                  jsc_job_num2state (ns));
    fflush (ctx->op);
    json_decref (jcb);
    return 0;
}


/******************************************************************************
 *                                                                            *
 *      Top-level Handlers for each of notify, query and update commands      *
 *                                                                            *
 ******************************************************************************/

static int handle_notify_req (flux_t *h, const char *ofn)
{
    jstatctx_t *ctx = NULL;

    sig_flux_h = h;
    if (signal (SIGINT, sig_handler) == SIG_ERR)
        return -1;

    ctx = getctx (h);
    ctx->op = (ofn)?  open_test_outfile (ofn) : stdout;

    if (jsc_notify_status (h, job_status_cb, (void *)h) != 0) {
        flux_log (h, LOG_ERR, "failed to reg a job status change CB");
        return -1;
    }
    if (flux_reactor_run (flux_get_reactor (h), 0) < 0)
        flux_log (h, LOG_ERR, "error in flux_reactor_run");

    return 0;
}

static int handle_query_req (flux_t *h, int64_t j, const char *k, const char *n)
{
    jstatctx_t *ctx = getctx (h);
    json_t *jcb = NULL;
    char *jcbstr = NULL;
    char *jcbstr_pretty = NULL;

    ctx->op = n ? open_test_outfile (n) : stdout;
    if (jsc_query_jcb (h, j, k, &jcbstr) != 0) {
        flux_log (h, LOG_ERR, "jsc_query_jcb reported an error");
        return -1;
    }
    if (!(jcb = json_loads (jcbstr, 0, NULL))) {
        flux_log (h, LOG_ERR, "error loading jcbstr");
        goto done;
    }
    if (!(jcbstr_pretty = json_dumps (jcb, JSON_INDENT(2)))) {
        flux_log (h, LOG_ERR, "error dumping jcbstr");
        goto done;
    }

    fprintf (ctx->op, "Job Control Block: attribute %s for job %"PRIi64"\n", k, j);
    fprintf (ctx->op, "%s\n", jcbstr_pretty);
done:
    free (jcbstr_pretty);
    json_decref (jcb);
    free (jcbstr);
    return 0;
}

static int handle_update_req (flux_t *h, int64_t j, const char *k,
                              const char *jcbstr, const char *n)
{
    jstatctx_t *ctx = NULL;
    ctx = getctx (h);
    ctx->op = n?  open_test_outfile (n) : stdout;

    return (jsc_update_jcb (ctx->h, j, k, jcbstr));
}


/******************************************************************************
 *                                                                            *
 *                            Main entry point                                *
 *                                                                            *
 ******************************************************************************/

int main (int argc, char *argv[])
{
    flux_t *h;
    int ch = 0;
    int rc = 0;
    char *cmd = NULL;
    const char *j = NULL;
    const char *ofn = NULL;
    const char *attr = NULL;
    const char *jcbstr = NULL;

    log_init ("flux-jstat");
    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage (0);
                break;
            case 'o': /* --testout */
                ofn = xasprintf ("%s", optarg);
                break;
            default:
                usage (1);
                break;
        }
    }
    if (optind == argc)
        usage (1);

    if (!(h = flux_open  (NULL, 0)))
        log_err_exit ("flux_open");

    flux_log_set_appname (h, "jstat");
    cmd = argv[optind++];

    if (!strcmp ("notify", cmd))
        rc = handle_notify_req (h, (const char *)ofn);
    else if (!strcmp ("query", cmd) && optind == argc - 2) {
        j = (const char *)(*(argv+optind));
        attr = (const char *)(*(argv+optind+1));
        rc = handle_query_req (h, strtol (j, NULL, 10), attr, ofn);
    }
    else if (!strcmp ("update", cmd) && optind == argc - 3) {
        j = (const char *)(*(argv+optind));
        attr = (const char *)(*(argv+optind+1));
        jcbstr = (const char *)(*(argv+optind+2));
        rc = handle_update_req (h, strtol (j, NULL, 10), attr, jcbstr, ofn);
    }
    else
        usage (1);

    flux_close (h);
    log_fini ();

    return (!rc)? 0: 42;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
