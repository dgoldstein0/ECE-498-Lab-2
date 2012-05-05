/*									tab:8
 *
 * lab3-seq.c - sequential implementation of ECE498SL Lab 3, Spring 2009
 *
 * "Copyright (c) 2009 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 * 
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO 
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL 
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, 
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE 
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE, 
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:	    Steve Lumetta
 * Version:	    1
 * Creation Date:   Mon Apr 13 20:20:34 2009
 * Filename:	    lab3-base.c
 * History:
 *	SL	1	Mon Apr 13 20:20:34 2009
 *		First written.
 *	SL	2	Wed Apr 22 08:33:23 2009
 *		Built from lab3-base.c
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>


/*
 * load_jpeg_file -- load a JPEG from a file into an RGB pixel format
 * INPUTS: fname -- name of JPEG file
 * OUTPUTS: *w_ptr -- image width in pixels
 *          *h_ptr -- image height in pixels
 * RETURN VALUE: dynamically allocated buffer containing 2D array of
 *               3-byte RGB data per pixel; linearized as top to bottom,
 *               left to right, so first byte is R of upper left, then
 *               G, then B, followed by pixel to right of upper left
 *               corner, etc.; returns NULL on failure handled internally
 * SIDE EFFECTS: prints error messages to stderr; terminates on fatal
 *               error, etc. (libjpeg defines standard error handling)
 */
static JSAMPLE*
load_jpeg_file (const char* fname, int32_t* w_ptr, int32_t* h_ptr)
{
    FILE* f;
    static struct jpeg_error_mgr jem;
    struct jpeg_decompress_struct decompress;
    JSAMPLE* buf;
    int32_t n_read;
    int32_t one_read;
    JSAMPARRAY rows;
    int32_t i;
    int32_t height;
    int32_t width;

    if (NULL == (f = fopen (fname, "rb"))) {
        perror ("fopen");
	return NULL;
    }

    /* 
     * error management with libjpeg is through callbacks defined
     * in the error manager structure; the following call uses
     * the "standard" set, which basically just dump errors to the
     * terminal, terminate on fatal errors, etc.  override them
     * if you want to do so...
     */
    decompress.err = jpeg_std_error (&jem);

    jpeg_create_decompress (&decompress);
    jpeg_stdio_src (&decompress, f);
    if (JPEG_HEADER_OK != jpeg_read_header (&decompress, TRUE)) {
        fputs ("bad header!  (not a JPEG file?)\n", stderr);
	fclose (f);
	jpeg_destroy_decompress (&decompress);
	return NULL;
    }
    /*
     * returns a boolean...what does it mean?  
     * hard to say...no documentation 
     */
    jpeg_start_decompress (&decompress);

    width = decompress.output_width;
    height = decompress.output_height;
    if (3 != decompress.output_components) {
        fputs ("not an RGB JPEG file\n", stderr);
	fclose (f);
	jpeg_destroy_decompress (&decompress);
	return NULL;
    }

    // assume RGB
    if (NULL == (buf = malloc (width * height * 3)) ||
	NULL == (rows = malloc (height * sizeof (rows[0])))) {
	if (NULL != buf) {free (buf);}
        perror ("malloc");
	fclose (f);
	jpeg_destroy_decompress (&decompress);
	return NULL;
    }
    for (i = 0; height > i; i++) {
        rows[i] = buf + i * width * 3;
    }

    n_read = 0;
    while (decompress.output_scanline < height) {
        one_read = jpeg_read_scanlines (&decompress, rows + n_read, 
					height - n_read);
	n_read += one_read;
    }

    jpeg_finish_decompress (&decompress);
    fclose (f);
    jpeg_destroy_decompress (&decompress);

    *w_ptr = width;
    *h_ptr = height;
    free (rows);

    return buf;
}


/*
 * find_edges -- find and return an edge image based on an RGB image
 * INPUTS: width -- image width in pixels
 *         height -- image height in pixels
 *         buf -- image data (array of rows, interleaved RGB)
 *         thresh -- threshold for edge identification
 * OUTPUTS: none
 * RETURN VALUE: dynamically allocated buffer containing 2D array of
 *               0/1-valued pixel data (array of rows, using one 32-bit
 *               integer per pixel in original image)
 * SIDE EFFECTS: dynamically allocates memory
 */
static int32_t*
find_edges (int32_t width, int32_t height, JSAMPLE* buf, int32_t thresh)
{
    int32_t* edge;
    int32_t x;
    int32_t y;
    int32_t color;
    int32_t mid_img;
    int32_t mid_edge;
    int32_t x_off;
    int32_t y_off;
    int32_t up;
    int32_t down;
    int32_t left;
    int32_t right;
    int32_t g_x;
    int32_t g_y;
    int32_t g_sum;

    if (NULL == (edge = calloc (width * height, sizeof (edge[0])))) {
        return NULL;
    }

    x_off = 3;
    y_off = 3 * width;
    for (y = 0, mid_img = mid_edge = 0; height > y; y++) {
	for (x = 0; width > x; x++, mid_edge++) {
	    up    = (0 < y ? -y_off : 0);
	    down  = (height - 1 > y ? y_off : 0);
	    left  = (0 < x ? -x_off : 0);
	    right = (width - 1 > x ? x_off : 0);
	    for (color = 0, g_sum = 0; 3 > color; color++, mid_img++) {
	        g_x = GETJOCTET (buf[mid_img + up + right]) + 
		      2 * GETJOCTET (buf[mid_img + right]) + 
		      GETJOCTET (buf[mid_img + down + right]) - 
		      GETJOCTET (buf[mid_img + up + left]) - 
		      2 * GETJOCTET (buf[mid_img + left]) - 
		      GETJOCTET (buf[mid_img + down + left]);
	        g_y = GETJOCTET (buf[mid_img + down + left]) + 
		      2 * GETJOCTET (buf[mid_img + down]) + 
		      GETJOCTET (buf[mid_img + down + right]) - 
		      GETJOCTET (buf[mid_img + up + left]) - 
		      2 * GETJOCTET (buf[mid_img + up]) - 
		      GETJOCTET (buf[mid_img + up + right]);
		g_sum += g_x * g_x + g_y * g_y;
	    }
	    edge[mid_edge] = (thresh <= g_sum);
	}
    }

    return edge;
}


/*
 * We use a queue (BFS) for connected components to avoid long, snake-like 
 * paths and long queues (as opposed to rings in the image and short queues).
 * DFS may still be faster on large images because of cache effects, but
 * wasn't tested.
 */
typedef struct comp_queue_t comp_queue_t;
struct comp_queue_t {
    int32_t x;
    int32_t y;
};
static comp_queue_t* cq;
static int32_t cq_head;
static int32_t cq_tail;


/*
 * color_one_component -- flood fill a color into an edge image
 *                        (helper routine for color_components)
 * INPUTS: width -- image width in pixels
 *         height -- image height in pixels
 *         edge -- edge/color data (array of rows)
 *         color -- fill color
 * OUTPUTS: none
 * RETURN VALUE: number of pixels colored
 * SIDE EFFECTS: the queue cq MUST be initialized before calling this routine
 */
static int32_t
color_one_component (int32_t width, int32_t height, int32_t* edge, 
                     int32_t color)
{
    int32_t x;
    int32_t y;

    while (cq_head != cq_tail) {
	x = cq[cq_head].x;
	y = cq[cq_head].y;
        cq_head++;

	if (0 < y && 0 == edge[(y - 1) * width + x]) {
	    edge[(y - 1) * width + x] = color;
	    cq[cq_tail].x = x;
	    cq[cq_tail].y = y - 1;
	    cq_tail++;
	}
	if (height - 1 > y && 0 == edge[(y + 1) * width + x]) {
	    edge[(y + 1) * width + x] = color;
	    cq[cq_tail].x = x;
	    cq[cq_tail].y = y + 1;
	    cq_tail++;
	}
	if (0 < x && 0 == edge[y * width + x - 1]) {
	    edge[y * width + x - 1] = color;
	    cq[cq_tail].x = x - 1;
	    cq[cq_tail].y = y;
	    cq_tail++;
	}
	if (width - 1 > x && 0 == edge[y * width + x + 1]) {
	    edge[y * width + x + 1] = color;
	    cq[cq_tail].x = x + 1;
	    cq[cq_tail].y = y;
	    cq_tail++;
	}
    }
    return cq_tail;
}

/*
 * color_components -- identify connected components in an edge image
 * INPUTS: width -- image width in pixels
 *         height -- image height in pixels
 *         edge -- edge data with one 32-bit integer per pixel (0 or 1)
 * OUTPUTS: edge -- colored image using distinct integer values (2+)
 *                  for each image component (separated by edges w/value 1)
 *          pix_count_ptr -- a dynamically-allocated array of pixel counts
 *                           by color (coo
 * RETURN VALUE: -1 on failure, (# colors needed + 2) on success (the value 1
 *               represents edges, so color values start at 2)
 * SIDE EFFECTS: dynamically allocates memory
 */
static int32_t
color_components (int32_t width, int32_t height, int32_t* edge, 
		  int32_t** pix_count_ptr)
{
    int32_t cur_col;
    int32_t x;
    int32_t y;
    int32_t* color_pixels;

    if (NULL == (cq = malloc (width * height * sizeof (cq[0])))) {
        return -1;
    }
    if (NULL == (color_pixels = malloc (width * height * 
    					sizeof (color_pixels[0])))) {
	free (cq);
        return -1;
    }

    cur_col = 2;
    for (y = 0; height > y; y++) {
	for (x = 0; width > x; x++) {
	    if (0 == edge[y * width + x]) {
		edge[y * width + x] = cur_col;
		cq[0].x = x;
		cq[0].y = y;
		cq_head = 0;
		cq_tail = 1;
	        color_pixels[cur_col] = color_one_component 
			(width, height, edge, cur_col);
	        cur_col++;
	    }
	}
    }

    free (cq);
    *pix_count_ptr = color_pixels;

    return cur_col;
}

/*
 * save_jpeg_file -- save an image in RGB pixel format as a JPEG
 * INPUTS: fname -- name of JPEG file to write
 *         width -- image width in pixels
 *         height -- image height in pixels
 *         buf -- image data in 3-byte RGB form, top to bottom, left to right
 *                (English reading order)
 * OUTPUTS: none
 * RETURN VALUE: 0 on success, -1 on failure
 * SIDE EFFECTS: prints error messages to stderr; terminates on fatal
 *               error, etc. (libjpeg defines standard error handling);
 *               note also that dynamically-allocated image data is NOT
 *               deallocated inside this routine
 */
static int32_t
save_jpeg_file (const char* fname, int32_t width, int32_t height, 
		JSAMPLE* buf)
{
    FILE* g;
    static struct jpeg_error_mgr jem;
    struct jpeg_compress_struct compress;
    JSAMPARRAY rows;
    int32_t i;

    if (NULL == (g = fopen (fname, "wb"))) {
        perror ("fopen new");
	return -1;
    }
    if (NULL == (rows = malloc (height * sizeof (rows[0])))) {
        perror ("malloc");
	fclose (g);
	jpeg_destroy_compress (&compress);
	return -1;
    }
    for (i = 0; height > i; i++) {
        rows[i] = buf + i * width * 3;
    }

    /* 
     * error management with libjpeg is through callbacks defined
     * in the error manager structure; the following call uses
     * the "standard" set, which basically just dump errors to the
     * terminal, terminate on fatal errors, etc.  override them
     * if you want to do so...
     */
    compress.err = jpeg_std_error (&jem);
    compress.err = &jem;
    jpeg_create_compress (&compress);
    jpeg_stdio_dest (&compress, g);
    compress.image_width  = width;
    compress.image_height = height;
    compress.input_components = 3;
    compress.in_color_space = JCS_RGB;
    jpeg_set_defaults (&compress);
    jpeg_start_compress (&compress, TRUE);
    jpeg_write_scanlines (&compress, rows, height);
    jpeg_finish_compress (&compress);
    fclose (g);
    jpeg_destroy_compress (&compress);
    free (rows);

    return 0;
}


/*
 * write_new_image -- write one output image based on component + image data;
 *                    new image contains portion of original image 
 *                    corresponding to a given color; other pixels are black
 * INPUTS: width -- image width in pixels
 *         height -- image height in pixels
 *         buf -- image data in 3-byte RGB form, top to bottom, left to right
 *                (English reading order)
 *         edge -- edge/color data (array of rows)
 *         color -- component color for image extraction
 *         img_num -- image id for output file name
 * OUTPUTS: none
 * RETURN VALUE: 0 on success, -1 on failure
 * SIDE EFFECTS: writes a file (see also save_jpeg_file for other side
 *               effects)
 */
static int32_t
write_new_image (int32_t width, int32_t height, JSAMPLE* buf, int32_t* edge, 
		 int32_t color, int32_t img_num)
{
    JSAMPLE* new_buf;
    char fname[30];
    int32_t x;
    int32_t y;
    int32_t mid;
    int32_t ret_val;

    if (NULL == (new_buf = malloc (width * height * sizeof (new_buf[0]) * 3))) {
        return -1;
    }

    for (y = 0, mid = 0; height > y; y++) {
	for (x = 0; width > x; x++) {
	    if (color == edge[y * width + x]) {
	        new_buf[mid] = buf[mid];
	        new_buf[mid + 1] = buf[mid + 1];
	        new_buf[mid + 2] = buf[mid + 2];
	    } else {
	        new_buf[mid] = new_buf[mid + 1] = new_buf[mid + 2] = 0;
	    }
	    mid += 3;
	}
    }
    sprintf (fname, "output%d.jpg", img_num);
    ret_val = save_jpeg_file (fname, width, height, new_buf);
    free (new_buf);
    return (-1 == ret_val ? -1 : 0);
}


static int32_t
usage (const char* exec_name)
{
    fprintf (stderr, "syntax: %s <jpg file> <threshold> <segment size>\n", 
	     exec_name);
    return 2;
}

int
main (int argc, char* argv[])
{
    JSAMPLE* buf;
    int32_t height;
    int32_t width;
    double thresh;
    int32_t* edge;
    char* after; 
    int32_t tot_col;
    int32_t* pix_count;
    int32_t color;
    int32_t seg_size;
    int32_t img_num;

    if (4 != argc) {
	return usage (argv[0]);
    }
    thresh = strtod (argv[2], &after);
    if (argv[2] == after  || '\0' != *after) {
	return usage (argv[0]);
    }
    seg_size = strtol (argv[3], &after, 10);
    if (argv[3] == after  || '\0' != *after) {
	return usage (argv[0]);
    }
    if (NULL == (buf = load_jpeg_file (argv[1], &width, &height))) {
        return 2;
    }
    if (NULL == (edge = find_edges (width, height, buf, 
    				    ceil (thresh * thresh)))) {
	fputs ("edge finding failed\n", stderr);
	free (buf);
        return 3;
    }
    if (-1 == (tot_col = color_components (width, height, edge, &pix_count))) {
	fputs ("component coloring failed\n", stderr);
	free (edge);
	free (buf);
        return 3;
    }
    // printf ("total regions = %d\n", tot_col - 2);
    for (color = 2, img_num = 0; tot_col > color; color++) {
	if (seg_size <= pix_count[color]) {
	    // printf ("region %d has %d pixels\n", color, pix_count[color]);
	    if (-1 == write_new_image (width, height, buf, edge, color, 
				       img_num++)) {
		free (pix_count);
		free (edge);
		free (buf);
		return 3;
	    }
	}
    }

    free (pix_count);
    free (edge);
    free (buf);

    return 0;
}


