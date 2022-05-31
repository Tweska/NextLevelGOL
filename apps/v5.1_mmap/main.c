#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "gif.h"
#include "gol.h"
#include "opts.h"
#include "util.h"

int main(int argc, char *argv[]) {
    FILE *input_fp = NULL;
    int output_fd = -1;

    world worlds[2];
    world *cur_world, *next_world;

    options opts;
    parse_opts(argc, argv, &opts);

    size_t img_size = GIF_SIZE(opts.width, opts.height, opts.steps);
    size_t offset = 0;
    uint8_t *img = NULL;

    /* Open the input file. */
    if (opts.use_input) {
        input_fp = fopen(opts.input, "r");
        if (input_fp == NULL) {
            fprintf(stderr, "Failed to open input file: '%s'!", opts.input);
            exit(1);
        }
    }

/* Open the output file. */
    if (opts.use_output) {
        output_fd = open(opts.output, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (output_fd < 0) {
            fprintf(stderr, "Failed to open output file: '%s'!\n", opts.output);
            exit(1);
        }

        if (ftruncate(output_fd, img_size) < 0) {
            fprintf(stderr, "Failed to stretch output file!\n");
            exit(1);
        }

        img = (uint8_t *) mmap(NULL, img_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
    }

    /* Initialize worlds. */
    worlds[0].width = opts.width;
    worlds[0].height = opts.height;
    worlds[0].cells = alloc_2d_int_array(opts.height, opts.width);

    worlds[1].width = opts.width;
    worlds[1].height = opts.height;
    worlds[1].cells = alloc_2d_int_array(opts.height, opts.width);

    cur_world = &worlds[0];
    next_world = &worlds[1];

    /* Initialize world */
    if (opts.use_input != 0) {
        world_load(cur_world, input_fp);
        fclose(input_fp);
    } else {
        world_init_random(cur_world, opts.seed);
    }

    /* Print the initial world state */
    if (opts.verbose != 0) {
        printf("\ninitial world:\n\n");
        world_print(cur_world);
    }

    /* Write the header part of the gif file */
    if (opts.use_output != 0) {
        offset = write_gif_header(cur_world->width, cur_world->height, img, offset);
    }

    /* Branch for timing code. */
    if (opts.time_code != 0) {
        struct timeval tv;
        double time_start, time_end, total;
        double wrap = 0.0;
        double step = 0.0;
        double swap = 0.0;
        double gif  = 0.0;

        /* Time steps... */
        for (int n = 0; n < opts.steps; n++) {
            world *tmp_world;

            time_start = time_secs(tv);
            world_border_timestep(cur_world, next_world);
            time_end = time_secs(tv);
            wrap += time_end - time_start;

            time_start = time_secs(tv);
            world_timestep(cur_world, next_world);
            time_end = time_secs(tv);
            step += time_end - time_start;

            /* Swap old and new worlds. */
            time_start = time_secs(tv);
            tmp_world = cur_world;
            cur_world = next_world;
            next_world = tmp_world;
            time_end = time_secs(tv);
            swap += time_end - time_start;

            time_start = time_secs(tv);

            if (opts.verbose != 0) {
                printf("World contains %d live cells after time step %d:\n\n", world_count(cur_world), n);
                world_print(cur_world);
            }

            if (opts.use_output != 0) {
                offset = write_gif_frame(cur_world->width, cur_world->height, cur_world->cells[0], img, offset);
            }

            time_end = time_secs(tv);
            gif += time_end - time_start;
        }

        /* Print timing data */
        total = wrap + step + swap + gif;
        fprintf(stderr, "Total time spent in each part:\n");
        fprintf(stderr, "  wrap : %7.3f seconds (%6.2f%%)\n", wrap, wrap/total*100);
        fprintf(stderr, "  step : %7.3f seconds (%6.2f%%)\n", step, step/total*100);
        fprintf(stderr, "  swap : %7.3f seconds (%6.2f%%)\n", swap, swap/total*100);
        fprintf(stderr, "  gif  : %7.3f seconds (%6.2f%%)\n", gif, gif/total*100);
        fprintf(stderr, "  -----------------------------------\n");
        fprintf(stderr, "  total: %7.3f seconds (100.00%%)\n\n", total);

        fprintf(stderr, "Throughput: %.0f pixels/second\n", opts.width * opts.height / total);
    } else {
        /* Time steps... */
        for (int n = 0; n < opts.steps; n++) {
            world *tmp_world;

            world_border_timestep(cur_world, next_world);
            world_timestep(cur_world, next_world);

            /* Swap old and new worlds. */
            tmp_world = cur_world;
            cur_world = next_world;
            next_world = tmp_world;

            if (opts.verbose != 0) {
                printf("World contains %d live cells after time step %d:\n\n", world_count(cur_world), n);
                world_print(cur_world);
            }

            if (opts.use_output != 0) {
                offset = write_gif_frame(cur_world->width, cur_world->height, cur_world->cells[0], img, offset);
            }
        }
    }

    /* Close the gif file */
    if (opts.use_output != 0) {
        write_gif_trailer(img, offset);

        msync(img, img_size, MS_SYNC);
        munmap(img, img_size);
        close(output_fd);
    }

    return 0;
}