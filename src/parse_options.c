/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Parse commandline functions.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif

#include "utilities.h"
#include "ior.h"
#include "aiori.h"
#include "parse_options.h"
#include "option.h"
#include "aiori.h"

#define ISPOWEROFTWO(x) ((x != 0) && !(x & (x - 1)))

IOR_param_t initialTestParams;

option_help * createGlobalOptions(IOR_param_t * params);


static IOR_param_t * parameters;
static options_all_t * global_options;


/*
 * Check and correct all settings of each test in queue for correctness.
 */
static void CheckRunSettings(IOR_test_t *tests)
{
        IOR_test_t *ptr;
        IOR_param_t *params;

        for (ptr = tests; ptr != NULL; ptr = ptr->next) {
                params = &ptr->params;

                /* If no write/read/check action requested, set both write and read */
                if (params->writeFile == FALSE
                    && params->readFile == FALSE
                    && params->checkWrite == FALSE
                    && params->checkRead == FALSE) {
                        params->readFile = TRUE;
                        params->writeFile = TRUE;
                }

                /* If only read or write is requested, then fix the default
                 * openFlags to not be open RDWR.  It matters in the case
                 * of HDFS, which doesn't support opening RDWR.
                 * (We assume int-valued params are exclusively 0 or 1.)
                 */
                if ((params->openFlags & IOR_RDWR)
                    && ((params->readFile | params->checkRead | params->checkWrite)
                        ^ params->writeFile)) {

                        params->openFlags &= ~(IOR_RDWR);
                        if (params->readFile | params->checkRead) {
                                params->openFlags |= IOR_RDONLY;
                                params->openFlags &= ~(IOR_CREAT|IOR_EXCL);
                        }
                        else
                                params->openFlags |= IOR_WRONLY;
                }
        }
}

/*
 * Set flags from commandline string/value pairs.
 */
void DecodeDirective(char *line, IOR_param_t *params, options_all_t * module_options)
{
        char option[MAX_STR];
        char value[MAX_STR];
        int rc;
        int initialized;

        rc = sscanf(line, " %[^=# \t\r\n] = %[^# \t\r\n] ", option, value);
        if (rc != 2 && rank == 0) {
                fprintf(out_logfile, "Syntax error in configuration options: %s\n",
                        line);
                MPI_CHECK(MPI_Initialized(&initialized), "MPI_Initialized() error");
                if (initialized)
                    MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
                else
                    exit(-1);
        }
        if (strcasecmp(option, "api") == 0) {
          params->api = strdup(value);

          params->backend = aiori_select(params->api);
          if (params->backend == NULL){
            fprintf(out_logfile, "Could not load backend API %s\n", params->api);
            exit(-1);
          }
        } else if (strcasecmp(option, "summaryFile") == 0) {
          if (rank == 0){
            out_resultfile = fopen(value, "w");
            if (out_resultfile == NULL){
              FAIL("Cannot open output file for writes!");
            }
            printf("Writing output to %s\n", value);
          }
        } else if (strcasecmp(option, "summaryFormat") == 0) {
                if(strcasecmp(value, "default") == 0){
                  outputFormat = OUTPUT_DEFAULT;
                }else if(strcasecmp(value, "JSON") == 0){
                  outputFormat = OUTPUT_JSON;
                }else if(strcasecmp(value, "CSV") == 0){
                  outputFormat = OUTPUT_CSV;
                }else{
                  FAIL("Unknown summaryFormat");
                }
        } else if (strcasecmp(option, "refnum") == 0) {
                params->referenceNumber = atoi(value);
        } else if (strcasecmp(option, "debug") == 0) {
                params->debug = strdup(value);
        } else if (strcasecmp(option, "platform") == 0) {
                params->platform  = strdup(value);
        } else if (strcasecmp(option, "testfile") == 0) {
                params->testFileName  = strdup(value);
        } else if (strcasecmp(option, "hintsfilename") == 0) {
                params->hintsFileName  = strdup(value);
        } else if (strcasecmp(option, "deadlineforstonewalling") == 0) {
                params->deadlineForStonewalling = atoi(value);
        } else if (strcasecmp(option, "stoneWallingWearOut") == 0) {
                params->stoneWallingWearOut = atoi(value);
        } else if (strcasecmp(option, "stoneWallingWearOutIterations") == 0) {
                params->stoneWallingWearOutIterations = atoll(value);
        } else if (strcasecmp(option, "stoneWallingStatusFile") == 0) {
                params->stoneWallingStatusFile  = strdup(value);
        } else if (strcasecmp(option, "maxtimeduration") == 0) {
                params->maxTimeDuration = atoi(value);
        } else if (strcasecmp(option, "outlierthreshold") == 0) {
                params->outlierThreshold = atoi(value);
        } else if (strcasecmp(option, "nodes") == 0) {
                params->nodes = atoi(value);
        } else if (strcasecmp(option, "repetitions") == 0) {
                params->repetitions = atoi(value);
        } else if (strcasecmp(option, "intertestdelay") == 0) {
                params->interTestDelay = atoi(value);
        } else if (strcasecmp(option, "interiodelay") == 0) {
                params->interIODelay = atoi(value);
        } else if (strcasecmp(option, "readfile") == 0) {
                params->readFile = atoi(value);
        } else if (strcasecmp(option, "writefile") == 0) {
                params->writeFile = atoi(value);
        } else if (strcasecmp(option, "fileperproc") == 0) {
                params->filePerProc = atoi(value);
        } else if (strcasecmp(option, "taskpernodeoffset") == 0) {
                params->taskPerNodeOffset = atoi(value);
        } else if (strcasecmp(option, "reordertasksconstant") == 0) {
                params->reorderTasks = atoi(value);
        } else if (strcasecmp(option, "reordertasksrandom") == 0) {
                params->reorderTasksRandom = atoi(value);
        } else if (strcasecmp(option, "reordertasksrandomSeed") == 0) {
                params->reorderTasksRandomSeed = atoi(value);
        } else if (strcasecmp(option, "reordertasks") == 0) {
                /* Backwards compatibility for the "reorderTasks" option.
                   MUST follow the other longer reordertasks checks. */
                params->reorderTasks = atoi(value);
        } else if (strcasecmp(option, "checkwrite") == 0) {
                params->checkWrite = atoi(value);
        } else if (strcasecmp(option, "checkread") == 0) {
                params->checkRead = atoi(value);
        } else if (strcasecmp(option, "keepfile") == 0) {
                params->keepFile = atoi(value);
        } else if (strcasecmp(option, "keepfilewitherror") == 0) {
                params->keepFileWithError = atoi(value);
        } else if (strcasecmp(option, "multiFile") == 0) {
                params->multiFile = atoi(value);
        } else if (strcasecmp(option, "quitonerror") == 0) {
                params->quitOnError = atoi(value);
        } else if (strcasecmp(option, "segmentcount") == 0) {
                params->segmentCount = string_to_bytes(value);
        } else if (strcasecmp(option, "blocksize") == 0) {
                params->blockSize = string_to_bytes(value);
        } else if (strcasecmp(option, "transfersize") == 0) {
                params->transferSize = string_to_bytes(value);
        } else if (strcasecmp(option, "setalignment") == 0) {
                params->setAlignment = string_to_bytes(value);
        } else if (strcasecmp(option, "singlexferattempt") == 0) {
                params->singleXferAttempt = atoi(value);
        } else if (strcasecmp(option, "individualdatasets") == 0) {
                params->individualDataSets = atoi(value);
        } else if (strcasecmp(option, "intraTestBarriers") == 0) {
                params->intraTestBarriers = atoi(value);
        } else if (strcasecmp(option, "nofill") == 0) {
                params->noFill = atoi(value);
        } else if (strcasecmp(option, "verbose") == 0) {
                params->verbose = atoi(value);
        } else if (strcasecmp(option, "settimestampsignature") == 0) {
                params->setTimeStampSignature = atoi(value);
        } else if (strcasecmp(option, "collective") == 0) {
                params->collective = atoi(value);
        } else if (strcasecmp(option, "preallocate") == 0) {
                params->preallocate = atoi(value);
        } else if (strcasecmp(option, "storefileoffset") == 0) {
                params->storeFileOffset = atoi(value);
        } else if (strcasecmp(option, "usefileview") == 0) {
                params->useFileView = atoi(value);
        } else if (strcasecmp(option, "usesharedfilepointer") == 0) {
                params->useSharedFilePointer = atoi(value);
        } else if (strcasecmp(option, "usestrideddatatype") == 0) {
                params->useStridedDatatype = atoi(value);
        } else if (strcasecmp(option, "showhints") == 0) {
                params->showHints = atoi(value);
        } else if (strcasecmp(option, "uniqueDir") == 0) {
                params->uniqueDir = atoi(value);
        } else if (strcasecmp(option, "useexistingtestfile") == 0) {
                params->useExistingTestFile = atoi(value);
        } else if (strcasecmp(option, "fsyncperwrite") == 0) {
                params->fsyncPerWrite = atoi(value);
        } else if (strcasecmp(option, "fsync") == 0) {
                params->fsync = atoi(value);
        } else if (strcasecmp(option, "randomoffset") == 0) {
                params->randomOffset = atoi(value);
        } else if (strcasecmp(option, "memoryPerTask") == 0) {
                params->memoryPerTask = string_to_bytes(value);
                params->memoryPerNode = 0;
        } else if (strcasecmp(option, "memoryPerNode") == 0) {
                params->memoryPerNode = NodeMemoryStringToBytes(value);
                params->memoryPerTask = 0;
        } else if (strcasecmp(option, "lustrestripecount") == 0) {
#ifndef HAVE_LUSTRE_LUSTRE_USER_H
                ERR("ior was not compiled with Lustre support");
#endif
                params->lustre_stripe_count = atoi(value);
                params->lustre_set_striping = 1;
        } else if (strcasecmp(option, "lustrestripesize") == 0) {
#ifndef HAVE_LUSTRE_LUSTRE_USER_H
                ERR("ior was not compiled with Lustre support");
#endif
                params->lustre_stripe_size = string_to_bytes(value);
                params->lustre_set_striping = 1;
        } else if (strcasecmp(option, "lustrestartost") == 0) {
#ifndef HAVE_LUSTRE_LUSTRE_USER_H
                ERR("ior was not compiled with Lustre support");
#endif
                params->lustre_start_ost = atoi(value);
                params->lustre_set_striping = 1;
        } else if (strcasecmp(option, "lustreignorelocks") == 0) {
#ifndef HAVE_LUSTRE_LUSTRE_USER_H
                ERR("ior was not compiled with Lustre support");
#endif
                params->lustre_ignore_locks = atoi(value);
        } else if (strcasecmp(option, "gpfshintaccess") == 0) {
#ifndef HAVE_GPFS_FCNTL_H
                ERR("ior was not compiled with GPFS hint support");
#endif
                params->gpfs_hint_access = atoi(value);
        } else if (strcasecmp(option, "gpfsreleasetoken") == 0) {
#ifndef HAVE_GPFS_FCNTL_H
                ERR("ior was not compiled with GPFS hint support");
#endif
                params->gpfs_release_token = atoi(value);
       } else if (strcasecmp(option, "beegfsNumTargets") == 0) {
#ifndef HAVE_BEEGFS_BEEGFS_H
                ERR("ior was not compiled with BeeGFS support");
#endif
                params->beegfs_numTargets = atoi(value);
                if (params->beegfs_numTargets < 1)
                        ERR("beegfsNumTargets must be >= 1");
        } else if (strcasecmp(option, "beegfsChunkSize") == 0) {
 #ifndef HAVE_BEEGFS_BEEGFS_H
                 ERR("ior was not compiled with BeeGFS support");
 #endif
                 params->beegfs_chunkSize = string_to_bytes(value);
                 if (!ISPOWEROFTWO(params->beegfs_chunkSize) || params->beegfs_chunkSize < (1<<16))
                         ERR("beegfsChunkSize must be a power of two and >64k");
        } else if (strcasecmp(option, "numtasks") == 0) {
                params->numTasks = atoi(value);
        } else if (strcasecmp(option, "summaryalways") == 0) {
                params->summary_every_test = atoi(value);
        } else {
                // backward compatibility for now
                if (strcasecmp(option, "useo_direct") == 0) {
                  strcpy(option, "--posix.odirect");
                }
                int parsing_error = option_parse_key_value(option, value, module_options);
                if(parsing_error){
                  if (rank == 0)
                          fprintf(out_logfile, "Unrecognized parameter \"%s\"\n",
                                  option);
                  MPI_CHECK(MPI_Initialized(&initialized), "MPI_Initialized() error");
                  if (initialized)
                      MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1), "MPI_Abort() error");
                  else
                      exit(-1);
                }
        }
}

static void decodeDirectiveWrapper(char *line){
  DecodeDirective(line, parameters, global_options);
}

/*
 * Parse a single line, which may contain multiple comma-seperated directives
 */
void ParseLine(char *line, IOR_param_t * test, options_all_t * module_options)
{
        char *start, *end;

        start = line;
        do {
                end = strchr(start, ',');
                if (end != NULL)
                        *end = '\0';
                DecodeDirective(start, test, module_options);
                start = end + 1;
        } while (end != NULL);
}

/*
 * Determines if the string "haystack" contains only the string "needle", and
 * possibly whitespace before and after needle.  Function is case insensitive.
 */
int contains_only(char *haystack, char *needle)
{
        char *ptr, *end;

        end = haystack + strlen(haystack);
        /* skip over leading shitspace */
        for (ptr = haystack; ptr < end; ptr++) {
                if (!isspace(*ptr))
                        break;
        }
        /* check for "needle" */
        if (strncasecmp(ptr, needle, strlen(needle)) != 0)
                return 0;
        /* make sure the rest of the line is only whitspace as well */
        for (ptr += strlen(needle); ptr < end; ptr++) {
                if (!isspace(*ptr))
                        return 0;
        }

        return 1;
}

/*
 * Read the configuration script, allocating and filling in the structure of
 * global parameters.
 */
IOR_test_t *ReadConfigScript(char *scriptName)
{
        int test_num = 0;
        int runflag = 0;
        char linebuf[MAX_STR];
        char empty[MAX_STR];
        char *ptr;
        FILE *file;
        IOR_test_t *head = NULL;
        IOR_test_t *tail = NULL;

        option_help ** option_p = & global_options->modules[0].options;

        /* Initialize the first test */
        head = CreateTest(& initialTestParams, test_num++);
        tail = head;
        *option_p = createGlobalOptions(& ((IOR_test_t*) head)->params); /* The current options */

        /* open the script */
        file = fopen(scriptName, "r");
        if (file == NULL)
                ERR("fopen() failed");

        /* search for the "IOR START" line */
        while (fgets(linebuf, MAX_STR, file) != NULL) {
                if (contains_only(linebuf, "ior start")) {
                        break;
                }
        }

        /* Iterate over a block of IOR commands */
        while (fgets(linebuf, MAX_STR, file) != NULL) {
                /* skip over leading whitespace */
                ptr = linebuf;
                while (isspace(*ptr))
                    ptr++;

                /* skip empty lines */
                if (sscanf(ptr, "%s", empty) == -1)
                        continue;

                /* skip lines containing only comments */
                if (sscanf(ptr, " #%s", empty) == 1)
                        continue;

                if (contains_only(ptr, "ior stop")) {
                        break;
                } else if (contains_only(ptr, "run")) {
                        if (runflag) {
                                /* previous line was a "run" as well
                                   create duplicate test */
                                tail->next = CreateTest(&tail->params, test_num++);
                                AllocResults(tail);
                                ((IOR_test_t*) tail)->params.backend_options = airoi_update_module_options(((IOR_test_t*) tail)->params.backend, global_options);

                                tail = tail->next;
                                *option_p = createGlobalOptions(& ((IOR_test_t*) tail->next)->params);
                        }
                        runflag = 1;
                } else if (runflag) {
                        /* If this directive was preceded by a "run" line, then
                           create and initialize a new test structure */
                        runflag = 0;
                        tail->next = CreateTest(&tail->params, test_num++);
                        *option_p = createGlobalOptions(& ((IOR_test_t*) tail->next)->params);
                        AllocResults(tail);
                        ((IOR_test_t*) tail)->params.backend_options = airoi_update_module_options(((IOR_test_t*) tail)->params.backend, global_options);

                        tail = tail->next;
                        ParseLine(ptr, &tail->params, global_options);
                } else {
                        ParseLine(ptr, &tail->params, global_options);
                }
        }

        /* close the script */
        if (fclose(file) != 0)
                ERR("fclose() of script file failed");
        AllocResults(tail);          /* copy the actual module options into the test */
        ((IOR_test_t*) tail)->params.backend_options = airoi_update_module_options(((IOR_test_t*) tail)->params.backend, global_options);

        return head;
}


option_help * createGlobalOptions(IOR_param_t * params){
  char APIs[1024];
  char APIs_legacy[1024];
  aiori_supported_apis(APIs, APIs_legacy, IOR);
  char apiStr[1024];
  sprintf(apiStr, "API for I/O [%s]", APIs);

  option_help o [] = {
    {'a', NULL,        apiStr, OPTION_OPTIONAL_ARGUMENT, 's', & params->api},
    {'A', NULL,        "refNum -- user supplied reference number to include in the summary", OPTION_OPTIONAL_ARGUMENT, 'd', & params->referenceNumber},
    {'b', NULL,        "blockSize -- contiguous bytes to write per task  (e.g.: 8, 4k, 2m, 1g)", OPTION_OPTIONAL_ARGUMENT, 'l', & params->blockSize},
    {'c', NULL,        "collective -- collective I/O", OPTION_FLAG, 'd', & params->collective},
    {'C', NULL,        "reorderTasks -- changes task ordering to n+1 ordering for readback", OPTION_FLAG, 'd', & params->reorderTasks},
    {'d', NULL,        "interTestDelay -- delay between reps in seconds", OPTION_OPTIONAL_ARGUMENT, 'd', & params->interTestDelay},
    {'D', NULL,        "deadlineForStonewalling -- seconds before stopping write or read phase", OPTION_OPTIONAL_ARGUMENT, 'd', & params->deadlineForStonewalling},
    {.help="  -O stoneWallingWearOut=1           -- once the stonewalling timout is over, all process finish to access the amount of data", .arg = OPTION_OPTIONAL_ARGUMENT},
    {.help="  -O stoneWallingWearOutIterations=N -- stop after processing this number of iterations, needed for reading data back written with stoneWallingWearOut", .arg = OPTION_OPTIONAL_ARGUMENT},
    {.help="  -O stoneWallingStatusFile=FILE     -- this file keeps the number of iterations from stonewalling during write and allows to use them for read", .arg = OPTION_OPTIONAL_ARGUMENT},
    {'e', NULL,        "fsync -- perform sync operation after each block write", OPTION_FLAG, 'd', & params->fsync},
    {'E', NULL,        "useExistingTestFile -- do not remove test file before write access", OPTION_FLAG, 'd', & params->useExistingTestFile},
    {'f', NULL,        "scriptFile -- test script name", OPTION_OPTIONAL_ARGUMENT, 's', & params->testscripts},
    {'F', NULL,        "filePerProc -- file-per-process", OPTION_FLAG, 'd', & params->filePerProc},
    {'g', NULL,        "intraTestBarriers -- use barriers between open, write/read, and close", OPTION_FLAG, 'd', & params->intraTestBarriers},
    /* This option toggles between Incompressible Seed and Time stamp sig based on -l,
     * so we'll toss the value in both for now, and sort it out in initialization
     * after all the arguments are in and we know which it keep.
     */
    {'G', NULL,        "setTimeStampSignature -- set value for time stamp signature/random seed", OPTION_OPTIONAL_ARGUMENT, 'd', & params->setTimeStampSignature},
    {'H', NULL,        "showHints -- show hints", OPTION_FLAG, 'd', & params->showHints},
    {'i', NULL,        "repetitions -- number of repetitions of test", OPTION_OPTIONAL_ARGUMENT, 'd', & params->repetitions},
    {'I', NULL,        "individualDataSets -- datasets not shared by all procs [not working]", OPTION_FLAG, 'd', & params->individualDataSets},
    {'j', NULL,        "outlierThreshold -- warn on outlier N seconds from mean", OPTION_OPTIONAL_ARGUMENT, 'd', & params->outlierThreshold},
    {'J', NULL,        "setAlignment -- HDF5 alignment in bytes (e.g.: 8, 4k, 2m, 1g)", OPTION_OPTIONAL_ARGUMENT, 'd', & params->setAlignment},
    {'k', NULL,        "keepFile -- don't remove the test file(s) on program exit", OPTION_FLAG, 'd', & params->keepFile},
    {'K', NULL,        "keepFileWithError  -- keep error-filled file(s) after data-checking", OPTION_FLAG, 'd', & params->keepFileWithError},
    {'l', NULL,        "datapacket type-- type of packet that will be created [offset|incompressible|timestamp|o|i|t]", OPTION_OPTIONAL_ARGUMENT, 's', &  params->buffer_type},
    {'m', NULL,        "multiFile -- use number of reps (-i) for multiple file count", OPTION_FLAG, 'd', & params->multiFile},
    {'M', NULL,        "memoryPerNode -- hog memory on the node  (e.g.: 2g, 75%)", OPTION_OPTIONAL_ARGUMENT, 's', & params->memoryPerNodeStr},
    {'n', NULL,        "noFill -- no fill in HDF5 file creation", OPTION_FLAG, 'd', & params->noFill},
    {'N', NULL,        "numTasks -- number of tasks that should participate in the test", OPTION_OPTIONAL_ARGUMENT, 'd', & params->numTasks},
    {'o', NULL,        "testFile -- full name for test", OPTION_OPTIONAL_ARGUMENT, 's', & params->testFileName},
    {'O', NULL,        "string of IOR directives (e.g. -O checkRead=1,lustreStripeCount=32)", OPTION_OPTIONAL_ARGUMENT, 'p', & decodeDirectiveWrapper},
    {'p', NULL,        "preallocate -- preallocate file size", OPTION_FLAG, 'd', & params->preallocate},
    {'P', NULL,        "useSharedFilePointer -- use shared file pointer [not working]", OPTION_FLAG, 'd', & params->useSharedFilePointer},
    {'q', NULL,        "quitOnError -- during file error-checking, abort on error", OPTION_FLAG, 'd', & params->quitOnError},
    {'Q', NULL,        "taskPerNodeOffset for read tests use with -C & -Z options (-C constant N, -Z at least N)", OPTION_OPTIONAL_ARGUMENT, 'd', & params->taskPerNodeOffset},
    {'r', NULL,        "readFile -- read existing file", OPTION_FLAG, 'd', & params->readFile},
    {'R', NULL,        "checkRead -- verify that the output of read matches the expected signature (used with -G)", OPTION_FLAG, 'd', & params->checkRead},
    {'s', NULL,        "segmentCount -- number of segments", OPTION_OPTIONAL_ARGUMENT, 'd', & params->segmentCount},
    {'S', NULL,        "useStridedDatatype -- put strided access into datatype [not working]", OPTION_FLAG, 'd', & params->useStridedDatatype},
    {'t', NULL,        "transferSize -- size of transfer in bytes (e.g.: 8, 4k, 2m, 1g)", OPTION_OPTIONAL_ARGUMENT, 'l', & params->transferSize},
    {'T', NULL,        "maxTimeDuration -- max time in minutes executing repeated test; it aborts only between iterations and not within a test!", OPTION_OPTIONAL_ARGUMENT, 'd', & params->maxTimeDuration},
    {'u', NULL,        "uniqueDir -- use unique directory name for each file-per-process", OPTION_FLAG, 'd', & params->uniqueDir},
    {'U', NULL,        "hintsFileName -- full name for hints file", OPTION_OPTIONAL_ARGUMENT, 's', & params->hintsFileName},
    {'v', NULL,        "verbose -- output information (repeating flag increases level)", OPTION_FLAG, 'd', & params->verbose},
    {'V', NULL,        "useFileView -- use MPI_File_set_view", OPTION_FLAG, 'd', & params->useFileView},
    {'w', NULL,        "writeFile -- write file", OPTION_FLAG, 'd', & params->writeFile},
    {'W', NULL,        "checkWrite -- check read after write", OPTION_FLAG, 'd', & params->checkWrite},
    {'x', NULL,        "singleXferAttempt -- do not retry transfer if incomplete", OPTION_FLAG, 'd', & params->singleXferAttempt},
    {'X', NULL,        "reorderTasksRandomSeed -- random seed for -Z option", OPTION_OPTIONAL_ARGUMENT, 'd', & params->reorderTasksRandomSeed},
    {'Y', NULL,        "fsyncPerWrite -- perform sync operation after every write operation", OPTION_FLAG, 'd', & params->fsyncPerWrite},
    {'z', NULL,        "randomOffset -- access is to random, not sequential, offsets within a file", OPTION_FLAG, 'd', & params->randomOffset},
    {'Z', NULL,        "reorderTasksRandom -- changes task ordering to random ordering for readback", OPTION_FLAG, 'd', & params->reorderTasksRandom},
    {.help="  -O summaryFile=FILE                 -- store result data into this file", .arg = OPTION_OPTIONAL_ARGUMENT},
    {.help="  -O summaryFormat=[default,JSON,CSV] -- use the format for outputing the summary", .arg = OPTION_OPTIONAL_ARGUMENT},
    {0, "dryRun",      "do not perform any I/Os just run evtl. inputs print dummy output", OPTION_FLAG, 'd', & params->dryRun},
    LAST_OPTION,
  };
  option_help * options = malloc(sizeof(o));
  memcpy(options, & o, sizeof(o));
  return options;
}


/*
 * Parse Commandline.
 */
IOR_test_t *ParseCommandLine(int argc, char **argv)
{
    init_IOR_Param_t(& initialTestParams);

    IOR_test_t *tests = NULL;

    GetPlatformName(initialTestParams.platform);

    option_help * options = createGlobalOptions( & initialTestParams);
    parameters = & initialTestParams;
    global_options = airoi_create_all_module_options(options);
    option_parse(argc, argv, global_options);
    updateParsedOptions(& initialTestParams, global_options);

    if (initialTestParams.testscripts){
      tests = ReadConfigScript(initialTestParams.testscripts);
    }else{
      tests = CreateTest(&initialTestParams, 0);
      AllocResults(tests);
    }

    CheckRunSettings(tests);

    return (tests);
}
