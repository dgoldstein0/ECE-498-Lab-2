/*									tab:8
 *
 * lab3-base.c - sample libjpeg code for ECE498SL Lab 3, Spring 2009
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
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>


/*
 * sample operation signature
 */
void
operate (int32_t width, int32_t height, int8_t* buf)
{
}

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
int8_t*
load_jpeg_file (const char* fname, int32_t* w_ptr, int32_t* h_ptr)
{
    FILE* f;
    static struct jpeg_error_mgr jem;
    struct jpeg_decompress_struct decompress;
    int8_t* buf;
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
int32_t
save_jpeg_file (const char* fname, int32_t width, int32_t height, 
		int8_t* buf)
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


int
main (int argc, char* argv[])
{
    int8_t* buf;
    int32_t height;
    int32_t width;

    if (2 != argc) {
        fprintf (stderr, "syntax: %s <jpg file>\n", argv[0]);
	return 2;
    }
    if (NULL == (buf = load_jpeg_file (argv[1], &width, &height))) {
        return 2;
    }
    operate (width, height, buf);
    if (-1 == save_jpeg_file ("new.jpg", width, height, buf)) {
        return 2;
    }
    free (buf);

    return 0;
}


