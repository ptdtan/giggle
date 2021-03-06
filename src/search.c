#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <sysexits.h>
#include <regex.h>

#include "giggle_index.h"
#include "wah.h"
#include "cache.h"
#include "file_read.h"
#include "util.h"
#include "kfunc.h"
#include "ll.h"

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

int search_help(int exit_code);
int print_giggle_query_result(struct giggle_query_result *gqr,
                              struct giggle_index *gi,
                              regex_t *regexs,
                              char **file_patterns,
                              uint32_t num_file_patterns,
                              uint32_t num_intervals,
                              double mean_interval_size,
                              long long genome_size,
                              uint32_t f_is_set,
                              uint32_t v_is_set,
                              uint32_t c_is_set,
                              uint32_t s_is_set,
                              uint32_t o_is_set);

//{{{ int search_help(int exit_code)
int search_help(int exit_code)
{
    fprintf(stderr,
"%s, v%s\n"
"usage:   %s search -i <index directory> [options]\n"
"         options:\n"
"             -i giggle index directory\n"
"             -r <regions (CSV)>\n"
"             -q <query file>\n"
"             -o give reuslts per record in the query file (omits empty results)\n"
"             -c give counts by indexed file\n"
"             -s give significance by indexed file (requires query file)\n"
"             -v give full record results\n"
"             -f print results for files that match a pattern (regex CSV)\n"
"             -g genome size for significance testing (default 3095677412)\n",
            PROGRAM_NAME, VERSION, PROGRAM_NAME);
    return exit_code;
}
//}}}


int print_giggle_query_result(struct giggle_query_result *gqr,
                              struct giggle_index *gi,
                              regex_t *regexs,
                              char **file_patterns,
                              uint32_t num_file_patterns,
                              uint32_t num_intervals,
                              double mean_interval_size,
                              long long genome_size,
                              uint32_t f_is_set,
                              uint32_t v_is_set,
                              uint32_t c_is_set,
                              uint32_t s_is_set,
                              uint32_t o_is_set)
{
    if (gqr == NULL)
        return EX_OK;

    uint32_t i,j;

    for(i = 0; i < gqr->num_files; i++) {
        struct file_data *fd = 
            (struct file_data *)unordered_list_get(gi->file_index, i); 
        if (test_pattern_match(gi,
                               regexs,
                               file_patterns,
                               num_file_patterns,
                               i,
                               f_is_set)) {
            if ( (v_is_set == 1) && (giggle_get_query_len(gqr, i) > 0 )){
                printf("#%s\n", fd->file_name);
                char *result;
                struct giggle_query_iter *gqi =
                    giggle_get_query_itr(gqr, i);
                while (giggle_query_next(gqi, &result) == 0)
                    printf("%s\n", result);
                giggle_iter_destroy(&gqi);
            } else if (c_is_set == 1) {
                printf("#%s\t"
                       "size:%u\t"
                       "overlaps:%u\n",
                       fd->file_name,
                       fd->num_intervals,
                       giggle_get_query_len(gqr, i));
            } else if (s_is_set == 1) {
                uint32_t file_counts = giggle_get_query_len(gqr, i);
                long long n11 = (long long)(file_counts);
                long long n12 = (long long)(MAX(0,num_intervals-file_counts));
                long long n21 = (long long)
                        (MAX(0,fd->num_intervals-file_counts));
                double comp_mean = 
                        ((fd->mean_interval_size+mean_interval_size));
                long long n22_full = (long long)
                        MAX(n11 + n12 + n21, genome_size/comp_mean);
                long long n22 = MAX(0, n22_full - (n11 + n12 + n21));
                double left, right, two;
                double r = kt_fisher_exact(n11,
                                           n12,
                                           n21,
                                           n22,
                                           &left,
                                           &right,
                                           &two);

                double ratio = 
                        (((double)n11/(double)n12) / ((double)n21/(double)n22));

                printf("#%s\t"
                       "size:%u\t"
                       "overlaps:%u\t"
                       "ratio:%f\t"
                       "sig:%f"
                       "\n",
                       fd->file_name,
                       fd->num_intervals,
                       file_counts,
                       ratio,
                       right);
            }
        }
    }

    return EX_OK;
}

int search_main(int argc, char **argv, char *full_cmd)
{
    if (argc < 2) return search_help(EX_OK);

    uint32_t num_chrms = 100;
    int c;
    char *index_dir_name = NULL,
         *regions = NULL,
         *query_file_name = NULL,
         *file_patterns_to_be_printed = NULL;


    char *i_type = "i";

    int i_is_set = 0,
        r_is_set = 0,
        q_is_set = 0,
        c_is_set = 0,
        s_is_set = 0,
        v_is_set = 0,
        f_is_set = 0,
        o_is_set = 0;

    double genome_size =  3095677412.0;

    //{{{ cmd line param parsing
    //{{{ while((c = getopt (argc, argv, "i:r:q:cvf:h")) != -1) {
    while((c = getopt (argc, argv, "i:r:q:csvof:g:h")) != -1) {
        switch (c) {
            case 'i':
                i_is_set = 1;
                index_dir_name = optarg;
                break;
            case 'r':
                r_is_set = 1;
                regions = optarg;
                break;
            case 'q':
                q_is_set = 1;
                query_file_name = optarg;
                break;
            case 'c':
                c_is_set = 1;
                break;
            case 's':
                s_is_set = 1;
                break;
            case 'v':
                v_is_set = 1;
                break;
            case 'o':
                o_is_set = 1;
                break;
            case 'f':
                f_is_set = 1;
                file_patterns_to_be_printed = optarg;
                break;
            case 'g':
                genome_size =  atof(optarg);
                break;
            case 'h':
                return search_help(EX_OK);
            case '?':
                 if ( (optopt == 'i') ||
                      (optopt == 'r') ||
                      (optopt == 'q') ||
                      (optopt == 'f') )
                        fprintf (stderr, "Option -%c requires an argument.\n",
                                optopt);
                    else if (isprint (optopt))
                        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                    else
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                return search_help(EX_USAGE);
            default:
                return search_help(EX_OK);
        }
    }
    //}}}
    
    if (i_is_set == 0) {
        fprintf(stderr, "Index directory is not set\n");
        return search_help(EX_USAGE);
    } 

    if ( (s_is_set == 1) && (q_is_set ==0)) {
        fprintf(stderr, "Significance testing requires a query file input\n");
        return search_help(EX_USAGE);
    }

    // need either a regions or a query file, but not both
    if ((r_is_set == 0) && (q_is_set == 0)) {
        fprintf(stderr, "Neither regions nor query file is set\n");
        return search_help(EX_USAGE);
    } if ((r_is_set == 1) && (q_is_set == 1)) {
        fprintf(stderr, "Both regions and query file is set\n");
        return search_help(EX_USAGE);
    }

    if ((v_is_set == 0) && (s_is_set == 0))
        c_is_set = 1;
    //}}}

    uint32_t num_file_patterns = 0;
    regex_t *regexs = NULL;
    char **file_patterns = NULL;

    //{{{ comiple file name regexs
    if (f_is_set == 1) {
        int s = 0, e = 0;
        while (scan_s(file_patterns_to_be_printed,
                      strlen(file_patterns_to_be_printed),
                      &s,
                      &e,
                      ',') >= 0) {
            num_file_patterns += 1;
            s = e + 1;
        }

        if (num_file_patterns == 0) {
            fprintf(stderr, "No file patterns detected.\n");
            return search_help(EX_USAGE);
        }

        regexs = (regex_t *)
                malloc(num_file_patterns * sizeof(regex_t));
        file_patterns = (char **)
                malloc(num_file_patterns * sizeof(char *));
        uint32_t i = 0;
        s = 0;
        e = 0;
        while (scan_s(file_patterns_to_be_printed,
                      strlen(file_patterns_to_be_printed),
                      &s,
                      &e,
                      ',') >= 0) {
            file_patterns[i] = strndup(file_patterns_to_be_printed + s, e-s);
            int r = regcomp(&(regexs[i]), file_patterns[i], 0);
            if (r != 0) {
                errx(EX_USAGE,
                     "Could not compile regex '%s'",
                     file_patterns[i]);
            }
            i += 1;
            s = e + 1;
        }
    }
    //}}}

    struct giggle_index *gi =
                giggle_load(index_dir_name,
                            uint32_t_ll_giggle_set_data_handler);

#if BLOCK_STORE
    giggle_data_handler.giggle_collect_intersection =
            giggle_collect_intersection_data_in_block;

    giggle_data_handler.map_intersection_to_offset_list =
            leaf_data_map_intersection_to_offset_list;
#endif

    struct giggle_query_result *gqr = NULL;

    uint32_t num_intervals = 0;
    double mean_interval_size = 0.0;

    if (r_is_set == 1) {
        // search the list of regions
        uint32_t i, last = 0, len = strlen(regions);
        char *chrm;
        uint32_t start, end;
        for (i = 0; i <= len; ++i) {
            if ((regions[i] == ',') || (regions[i] == '\0') ) {
                regions[i] = '\0';
                char *region;
                asprintf(&region, "%s", regions + last);
                if (parse_region(region, &chrm, &start, &end) == 0) {
                    gqr = giggle_query(gi, chrm, start, end, gqr);
                    free(region);
                } else {
                    errx(EX_USAGE,
                         "Error parsing region '%s'\n",
                         regions + last);
                }
                last = i + 1;
            }
        }
    } else if (q_is_set == 1) {
        // search a file
        int chrm_len = 50;
        char *chrm = (char *)malloc(chrm_len*sizeof(char));
        uint32_t start, end;
        long offset;

        struct input_file *q_f = input_file_init(query_file_name);

        while ( q_f->input_file_get_next_interval(q_f, 
                                                  &chrm,
                                                  &chrm_len,
                                                  &start,
                                                  &end,
                                                  &offset) >= 0 ) {
            gqr = giggle_query(gi, chrm, start, end, gqr);
            if ( (o_is_set == 1) && (gqr->num_hits > 0) ) {
                char *str;
                input_file_get_curr_line_bgzf(q_f, &str);
                printf("##%s",str);
                // Ugh
                if (q_f->type == BED)
                    printf("\n");
                int r = print_giggle_query_result(gqr,
                                                  gi,
                                                  regexs,
                                                  file_patterns,
                                                  num_file_patterns,
                                                  num_intervals,
                                                  mean_interval_size,
                                                  genome_size,
                                                  f_is_set,
                                                  v_is_set,
                                                  c_is_set,
                                                  s_is_set,
                                                  o_is_set);
                giggle_query_result_destroy(&gqr);
            }
            num_intervals += 1;
            mean_interval_size += end - start;
        }

        mean_interval_size = mean_interval_size/num_intervals;
    }


    int r = print_giggle_query_result(gqr,
                                      gi,
                                      regexs,
                                      file_patterns,
                                      num_file_patterns,
                                      num_intervals,
                                      mean_interval_size,
                                      genome_size,
                                      f_is_set,
                                      v_is_set,
                                      c_is_set,
                                      s_is_set,
                                      o_is_set);

#if 0
    if (gqr == NULL)
        return EX_OK;

    uint32_t i,j;

    for(i = 0; i < gqr->num_files; i++) {
        struct file_data *fd = 
            (struct file_data *)unordered_list_get(gi->file_index, i); 
        if (test_pattern_match(gi,
                               regexs,
                               file_patterns,
                               num_file_patterns,
                               i,
                               f_is_set)) {
            if (v_is_set == 1) {
                printf("#%s\n", fd->file_name);
                char *result;
                struct giggle_query_iter *gqi =
                    giggle_get_query_itr(gqr, i);
                while (giggle_query_next(gqi, &result) == 0)
                    printf("%s\n", result);
                giggle_iter_destroy(&gqi);
            } else if (c_is_set == 1) {
                printf("#%s\t"
                       "size:%u\t"
                       "overlaps:%u\n",
                       fd->file_name,
                       fd->num_intervals,
                       giggle_get_query_len(gqr, i));
            } else if (s_is_set == 1) {
                uint32_t file_counts = giggle_get_query_len(gqr, i);
                long long n11 = (long long)(file_counts);
                long long n12 = (long long)(MAX(0,num_intervals-file_counts));
                long long n21 = (long long)
                        (MAX(0,fd->num_intervals-file_counts));
                double comp_mean = 
                        ((fd->mean_interval_size+mean_interval_size));
                long long n22_full = (long long)
                        MAX(n11 + n12 + n21, genome_size/comp_mean);
                long long n22 = MAX(0, n22_full - (n11 + n12 + n21));
                double left, right, two;
                double r = kt_fisher_exact(n11,
                                           n12,
                                           n21,
                                           n22,
                                           &left,
                                           &right,
                                           &two);

                double ratio = 
                        (((double)n11/(double)n12) / ((double)n21/(double)n22));

                printf("#%s\t"
                       "size:%u\t"
                       "overlaps:%u\t"
                       "ratio:%f\t"
                       "sig:%f"
                       "\n",
                       fd->file_name,
                       fd->num_intervals,
                       file_counts,
                       ratio,
                       right);
            }
        }
    }
#endif
    giggle_query_result_destroy(&gqr);
    giggle_index_destroy(&gi);
    cache.destroy();
    free(full_cmd);
    return r;
}
