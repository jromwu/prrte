/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2013 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2012-2017 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2012-2020 Cisco Systems, Inc.  All rights reserved
 * Copyright (c) 2015-2017 Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * Copyright (c) 2016-2020 Intel, Inc.  All rights reserved.
 * Copyright (c) 2017      IBM Corporation. All rights reserved.
 * Copyright (c) 2021-2022 Nanook Consulting.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "prte_config.h"

#include "src/class/prte_list.h"
#include "src/class/prte_object.h"
#include "src/runtime/prte_globals.h"
#include "src/util/pmix_argv.h"
#include "src/util/cmd_line.h"
#include "src/util/output.h"
#include "src/util/show_help.h"

// Local functions
static int endswith(const char *str, const char *suffix)
{
    size_t lenstr, lensuffix;

    if (NULL == str || NULL == suffix) {
        return PRTE_ERR_BAD_PARAM;
    }

    lenstr = strlen(str);
    lensuffix = strlen(suffix);
    if (lensuffix > lenstr) {
        return PRTE_ERR_BAD_PARAM;
    }
    if (0 == strncmp(str + lenstr - lensuffix, suffix, lensuffix)) {
        return PRTE_SUCCESS;
    }
    return PRTE_ERR_BAD_PARAM;
}

static void check_store(const char *name, const char *option,
                        prte_cli_result_t *results)
{
    prte_cli_item_t *opt;

    PRTE_LIST_FOREACH(opt, &results->instances, prte_cli_item_t) {
        if (0 == strcmp(opt->key, name)) {
            /* if the name is NULL, then this is just setting
             * a boolean value - the presence of the option in
             * the results is considered "true" */
            if (NULL != option) {
                pmix_argv_append_nosize(&opt->values, option);
            }
            return;
        }
    }

    // get here if this is new option
    opt = PRTE_NEW(prte_cli_item_t);
    opt->key = strdup(name);
    prte_list_append(&results->instances, &opt->super);
    /* if the name is NULL, then this is just setting
     * a boolean value - the presence of the option in
     * the results is considered "true" */
    if (NULL != option) {
        pmix_argv_append_nosize(&opt->values, option);
    }
    return;
}

int prte_cmd_line_parse(char **pargv, char *shorts,
                        struct option myoptions[],
                        prte_cmd_line_store_fn_t storefn,
                        prte_cli_result_t *results,
                        char *helpfile)
{
    int option_index = 0;   /* getopt_long stores the option index here. */
    int n, opt, argc, rc, argind;
    bool found;
    char *ptr, *str, c, **argv;
    prte_cmd_line_store_fn_t mystore;

    /* the getopt_long parser reorders the input argv array, so
     * we have to protect it here - remove all leading/trailing
     * quotes to ensure we are looking at simple options/values */
    argv = pmix_argv_copy_strip(pargv);
    argc = pmix_argv_count(argv);
    // assign a default store_fn if one isn't provided
    if (NULL == storefn) {
        mystore = check_store;
    } else {
        mystore = storefn;
    }

    /* reset the parser - must be done each time we use it
     * to avoid hysteresis */
    optind = 0;
    opterr = 0;
    optopt = 0;
    optarg = NULL;

    // run the parser
    while (1) {
        argind = optind;
        opt = getopt_long(argc, argv, shorts, myoptions, &option_index);
        if (-1 == opt) {
            break;
        }
        switch (opt) {
            case 0:
                /* we allow someone to specify an option followed by
                 * the "help" directive - thus requesting detailed help
                 * on that specific option */
                if (NULL != optarg) {
                    if (0 == strcmp(optarg, "--help") || 0 == strcmp(optarg, "-help") ||
                        0 == strcmp(optarg, "help") || 0 == strcmp(optarg, "h") ||
                        0 == strcmp(optarg, "-h")) {
                        str = prte_show_help_string(helpfile, myoptions[option_index].name, false);
                        if (NULL != str) {
                            printf("%s", str);
                            free(str);
                        }
                        pmix_argv_free(argv);
                        return PRTE_ERR_SILENT;
                    }
                }
                /* if this is an MCA param of some type, store it */
                if (0 == endswith(myoptions[option_index].name, "mca")) {
                    /* format mca params as param:value - the optind value
                     * will have been incremented since the MCA param options
                     * require an argument */
                    pmix_asprintf(&str, "%s=%s", argv[optind-1], argv[optind]);
                    mystore(myoptions[option_index].name, str, results);
                    free(str);
                    ++optind;
                    break;
                }
                /* store actual option */
                mystore(myoptions[option_index].name, optarg, results);
                break;
            case 'h':
                /* the "help" option can optionally take an argument. Since
                 * the argument _is_ optional, getopt will _NOT_ increment
                 * optind, so argv[optind] is the potential argument */
                if (NULL == optarg &&
                    NULL != argv[optind]) {
                    /* strip any leading dashes */
                    ptr = argv[optind];
                    while ('-' == *ptr) {
                        ++ptr;
                    }
                    // check for standard options
                    if (0 == strcmp(ptr, "version") || 0 == strcmp(ptr, "V")) {
                        str = prte_show_help_string("help-cli.txt", "version", false);
                        if (NULL != str) {
                            printf("%s", str);
                            free(str);
                        }
                        pmix_argv_free(argv);
                        return PRTE_ERR_SILENT;
                    }
                    if (0 == strcmp(ptr, "verbose") || 0 == strcmp(ptr, "v")) {
                        str = prte_show_help_string("help-cli.txt", "verbose", false);
                        if (NULL != str) {
                            printf("%s", str);
                            free(str);
                        }
                        pmix_argv_free(argv);
                        return PRTE_ERR_SILENT;
                    }
                    if (0 == strcmp(ptr, "help") || 0 == strcmp(ptr, "h")) {
                        // they requested help on the "help" option itself
                        str = prte_show_help_string("help-cli.txt", "help", false,
                                                    prte_tool_basename, prte_tool_basename,
                                                    prte_tool_basename, prte_tool_basename,
                                                    prte_tool_basename, prte_tool_basename,
                                                    prte_tool_basename, prte_tool_basename);
                        if (NULL != str) {
                            printf("%s", str);
                            free(str);
                        }
                        pmix_argv_free(argv);
                        return PRTE_ERR_SILENT;
                    }
                    /* see if the argument is one of our options */
                    found = false;
                    if (NULL != ptr) {
                        for (n=0; NULL != myoptions[n].name; n++) {
                            if (0 == strcmp(ptr, myoptions[n].name)) {
                                // it is, so they requested help on this option
                                str = prte_show_help_string(helpfile, ptr, false);
                                if (NULL != str) {
                                    printf("%s", str);
                                    free(str);
                                }
                                pmix_argv_free(argv);
                                return PRTE_ERR_SILENT;
                            }
                        }
                    }
                    if (!found) {
                        // let the user know we don't recognize that option
                        str = prte_show_help_string("help-cli.txt", "unknown-option", true,
                                                    ptr, prte_tool_basename);
                        if (NULL != str) {
                            printf("%s", str);
                            free(str);
                        }
                        pmix_argv_free(argv);
                        return PRTE_ERR_SILENT;
                    }
                } else if (NULL == optarg) {
                    // high-level help request
                    str = prte_show_help_string(helpfile, "usage", false,
                                                prte_tool_basename, "PRRTE",
                                                PRTE_PROXY_VERSION_STRING,
                                                prte_tool_basename,
                                                PRTE_PROXY_BUGREPORT);
                    if (NULL != str) {
                        printf("%s", str);
                        free(str);
                    }
                    pmix_argv_free(argv);
                    return PRTE_ERR_SILENT;
                } else {  // unrecognized option
                    str = prte_show_help_string("help-cli.txt", "unrecognized-option", true,
                                                prte_tool_basename, optarg);
                    if (NULL != str) {
                        printf("%s", str);
                        free(str);
                    }
                    pmix_argv_free(argv);
                    return PRTE_ERR_SILENT;
                }
                break;
            case 'V':
                str = prte_show_help_string(helpfile, "version", false,
                                            prte_tool_basename, "PRRTE",
                                            PRTE_PROXY_VERSION_STRING,
                                            PRTE_PROXY_BUGREPORT);
                if (NULL != str) {
                    printf("%s", str);
                    free(str);
                }
                // if they ask for the version, that is all we do
                pmix_argv_free(argv);
                return PRTE_ERR_SILENT;
            default:
                /* this could be one of the short options other than 'h' or 'V', so
                 * we have to check */
                if (0 != argind && '-' != argv[argind][0]) {
                    // this was not an option
                    break;
                }
                found = false;
                for (n=0; '\0' != shorts[n]; n++) {
                    int ascii = shorts[n];
                    bool optional=false, reqd=false, noarg=false;
                    if (opt == ascii) {
                        /* found it - now search for matching option. The
                         * getopt fn will have already incremented optind
                         * to point at the next argument.
                         * If this short option required an argument, then
                         * it will be indicated by a ':' in the next shorts
                         * spot and the argument will be in optarg.
                         *
                         * If the short option takes an optional argument, then
                         * it will be indicated by two ':' after the option - in
                         * this case optarg will contain the argument if given.
                         * Note that the proper form of the optional argument
                         * option is "-zfoo", where 'z' is the option and "foo"
                         * is the argument. Putting a space between the option
                         * and the argument is forbidden and results in reporting
                         * of 'z' without an argument - usually followed by
                         * incorrectly marking "foo" as the beginning of the
                         * command "tail" */
                        if (':' == shorts[n+1]) {
                            // could be an optional arg
                            if (':' == shorts[n+2]) {
                                /* in this case, the argument (if given) must be immediately
                                 * attached to the option */
                                ptr = argv[optind-1];
                                ptr += 2;  // step over the '-' and option
                            } else {
                                ptr = optarg;
                            }
                        } else {
                            ptr = NULL;
                        }
                        for (n=0; NULL != myoptions[n].name; n++) {
                            if (ascii == myoptions[n].val) {
                                if (NULL != ptr) {
                                    /* we allow someone to specify an option followed by
                                     * the "help" directive - thus requesting detailed help
                                     * on that specific option */
                                    if (0 == strcmp(ptr, "--help") || 0 == strcmp(ptr, "-h") ||
                                        0 == strcmp(ptr, "help") || 0 == strcmp(ptr, "h")) {
                                        str = prte_show_help_string(helpfile, myoptions[n].name, true);
                                        if (NULL != str) {
                                            printf("%s", str);
                                            free(str);
                                        }
                                        pmix_argv_free(argv);
                                        return PRTE_ERR_SILENT;
                                    }
                                }
                                /* store actual option */
                                if (PRTE_ARG_NONE == myoptions[n].has_arg) {
                                    ptr = NULL;
                                }
                                mystore(myoptions[n].name, ptr, results);
                                found = true;
                                break;
                            }
                        }
                        if (found) {
                            break;
                        }
                        str = prte_show_help_string("help-cli.txt", "short-no-long", true,
                                                    prte_tool_basename, argv[optind-1]);
                        if (NULL != str) {
                            printf("%s", str);
                            free(str);
                        }
                        pmix_argv_free(argv);
                        return PRTE_ERR_SILENT;
                    }
                }
                if (found) {
                    break;
                }
                str = prte_show_help_string("help-cli.txt", "unregistered-option", true,
                                            prte_tool_basename, argv[optind-1], prte_tool_basename);
                if (NULL != str) {
                    printf("%s", str);
                    free(str);
                }
                pmix_argv_free(argv);
                return PRTE_ERR_SILENT;
        }
    }
    if (optind < argc) {
        results->tail = pmix_argv_copy(&argv[optind]);
    }
    pmix_argv_free(argv);
    return PRTE_SUCCESS;
}

static void icon(prte_cli_item_t *p)
{
    p->key = NULL;
    p->values = NULL;
}
static void ides(prte_cli_item_t *p)
{
    if (NULL != p->key) {
        free(p->key);
    }
    if (NULL != p->values) {
        pmix_argv_free(p->values);
    }
}
PRTE_CLASS_INSTANCE(prte_cli_item_t,
                    prte_list_item_t,
                    icon, ides);

static void ocon(prte_cli_result_t *p)
{
    PRTE_CONSTRUCT(&p->instances, prte_list_t);
    p->tail = NULL;
}
static void odes(prte_cli_result_t *p)
{
    PRTE_LIST_DESTRUCT(&p->instances);
    if (NULL != p->tail) {
        pmix_argv_free(p->tail);
    }
}
PRTE_CLASS_INSTANCE(prte_cli_result_t,
                    prte_object_t,
                    ocon, odes);
