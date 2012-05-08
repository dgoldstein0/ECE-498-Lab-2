/*                                                                        tab:8
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
 * Author:            Steve Lumetta
 * Version:            1
 * Creation Date:   Mon Apr 13 20:20:34 2009
 * Filename:            lab3-base.c
 * History:
 *        SL        1        Mon Apr 13 20:20:34 2009
 *                First written.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <iostream>
#include <math.h>
#include <vector>

using namespace std;

struct params_t {
    int32_t x_start;
    int32_t x_end;
    int32_t y_start;
    int32_t y_end;
    int32_t thread_num;

    params_t(int32_t x_s, int32_t x_e,
             int32_t y_s, int32_t y_e,
             int32_t t_num) : x_start(x_s), x_end(x_e), y_start(y_s),
                              y_end(y_e), thread_num(t_num)
    {
    }
};

//Global edge-detection map
static int32_t* edges;

//width and height of our image
static int32_t width;
static int32_t height;

//The input image
static JSAMPLE* input_image;

//The square of the threshold
static int32_t thresh;

static pthread_barrier_t barrier;

//datastore for disjoint sets structure
//First layer  = thread number
//Second layer = color
static vector<vector<int32_t> > dsets;
static pthread_mutex_t dsets_lock = PTHREAD_MUTEX_INITIALIZER;


/*
 * load_jpeg_file -- load a JPEG from a file into an RGB pixel format
 * INPUTS: fname -- name of JPEG file
 * OUTPUTS: *w_ptr -- image width in pixels
 *          *h_ptr -- image height in pixels
 * RETURN VALUE: dynamically allocated buffer (with new[]) containing 2D array of
 *               3-byte RGB data per pixel; linearized as top to bottom,
 *               left to right, so first byte is R of upper left, then
 *               G, then B, followed by pixel to right of upper left
 *               corner, etc.; returns NULL on failure handled internally
 * SIDE EFFECTS: prints error messages to stderr; terminates on fatal
 *               error, etc. (libjpeg defines standard error handling)
 */
JSAMPLE*
load_jpeg_file (const char* fname, int32_t* w_ptr, int32_t* h_ptr)
{
    FILE* f;
    static struct jpeg_error_mgr jem;
    struct jpeg_decompress_struct decompress;
    JSAMPLE* buf;
    int32_t n_read;
    int32_t one_read;
    JSAMPARRAY rows;
    uint32_t i;
    uint32_t height;
    uint32_t width;

    if (NULL == (f = fopen (fname, "rb")))
    {
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
    if (NULL == (buf = new JSAMPLE[width * height * 3]) ||
        NULL == (rows = (JSAMPLE**) malloc (height * sizeof (rows[0])))) {
        if (NULL != buf) {delete [] buf;}
        perror ("malloc");
        fclose (f);
        jpeg_destroy_decompress (&decompress);
        return NULL;
    }
    for (i = 0; height > i; i++) {
        rows[i] = (JSAMPLE*) buf + i * width * 3;
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
    if (NULL == (rows = (JSAMPARRAY) malloc (height * sizeof (rows[0])))) {
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
 * find_edges -- find and return an edge image based on an RGB image
 * INPUTS: x_start -- start x location (inclusive)
 *         x_end   -- ending x location (exclusive)
 *         y_start -- start y location (inclusive)
 *         y_end   -- ending y location (exclusive)
 *         buf -- image data (array of rows, interleaved RGB)
 *         thresh -- threshold for edge identification
 * OUTPUTS: none
 * RETURN VALUE: dynamically allocated buffer containing 2D array of
 *               0/1-valued pixel data (array of rows, using one 32-bit
 *               integer per pixel in original image)
 * SIDE EFFECTS: dynamically allocates memory
 */
void find_edges (int32_t* edge, int32_t x_start, int32_t x_end, int32_t y_start,
                 int32_t y_end, int32_t width, int32_t height, JSAMPLE* buf,
                 int32_t thresh)
{
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


    x_off = 3;
    y_off = 3 * width;
    for (y = y_start, mid_img = mid_edge = 0; y < y_end; y++)
    {
        for (x = x_start; x < x_end; x++, mid_edge++)
        {
            up    = (0 < y ? -y_off : 0);
            down  = (height - 1 > y ? y_off : 0);
            left  = (0 < x ? -x_off : 0);
            right = (width - 1 > x ? x_off : 0);
            for (color = 0, g_sum = 0; color < 3; color++, mid_img++)
            {
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


/*
 * color_one_component -- flood fill a color into an edge image
 *                        (helper routine for color_components)
 * INPUTS: width -- image width in pixels
 *         height -- image height in pixels
 *         edge -- edge/color data (array of rows)
 *         color -- fill color
 * OUTPUTS: none
 * RETURN VALUE: number of pixels colored
 */
static int32_t
color_one_component (comp_queue_t* cq, int32_t x_start, int32_t x_end,
                     int32_t y_start, int32_t y_end,
                     int32_t width, int32_t height, int32_t* edge, 
                     int32_t color)
{
    int32_t x;
    int32_t y;

    int32_t cq_head = 0;
    int32_t cq_tail = 1;

    while (cq_head != cq_tail) {
        x = cq[cq_head].x;
        y = cq[cq_head].y;
        cq_head++;

        if (y_start < y && 0 == edge[(y - 1) * width + x]) {
            edge[(y - 1) * width + x] = color;
            cq[cq_tail].x = x;
            cq[cq_tail].y = y - 1;
            cq_tail++;
        }
        if (y_end > y + 1 && 0 == edge[(y + 1) * width + x]) {
            edge[(y + 1) * width + x] = color;
            cq[cq_tail].x = x;
            cq[cq_tail].y = y + 1;
            cq_tail++;
        }
        if (x_start < x && 0 == edge[y * width + x - 1]) {
            edge[y * width + x - 1] = color;
            cq[cq_tail].x = x - 1;
            cq[cq_tail].y = y;
            cq_tail++;
        }
        if (x_end > x + 1 && 0 == edge[y * width + x + 1]) {
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
color_components (int32_t x_start, int32_t x_end, int32_t y_start,
                  int32_t y_end, int32_t width, int32_t height, int32_t* edge,
                  vector<int32_t>** pix_count_ptr, int32_t thread_num)
{
    int32_t cur_col;
    int32_t x;
    int32_t y;
    vector<int32_t> *color_pixels = new vector<int32_t>();
    comp_queue_t* cq = new comp_queue_t[(x_end - x_start) * (y_end - y_start)];

    if (color_pixels == NULL || cq == NULL)
    {
        delete color_pixels; //It is safe to delete null
        delete [] cq;
        return -1;
    }

    //insert entries for 0 and 1, since they aren't used as colors
    color_pixels->push_back(0);
    color_pixels->push_back(0);

    cur_col = 2;
    for (y = y_start; y < y_end; y++) {
        for (x = x_start; x < x_end; x++) {
            if (0 == edge[y * width + x]) {
                edge[y * width + x] = cur_col;
                cq[0].x = x;
                cq[0].y = y;
                color_pixels->push_back(
                    color_one_component(
                        cq, x_start, x_end, y_start, y_end,
                        width, height, edge, cur_col | (thread_num << 16)
                    )
                );

                cur_col++;
            }
        }
    }

    delete [] cq;
    *pix_count_ptr = color_pixels;

    return cur_col;
}

pair<int32_t, int32_t> find(int32_t num)
{
	int32_t value = dsets[num >> 16][num & 0xFFFF];
	if(value < 0)
		return pair<int32_t, int32_t>(num, -value);
	else
		return find(value);
}

int32_t find_and_compress(int32_t num)
{
	int32_t value = dsets[num >> 16][num & 0xFFFF];
	if(value < 0)
		return num;
	else
		return dsets[num >> 16][num & 0xFFFF] = find_and_compress(value);
}

void set_union(int32_t a, int32_t b)
{
    pthread_mutex_lock(&dsets_lock);
	int32_t seta = find_and_compress(a);
	int32_t setb = find_and_compress(b);
	
	/*do nothing if a and b are already in the same set.  This is
	important because we don't want to set the head to point to itself
	or mess up the size count - which this code would do.*/

	if(seta==setb)
    {
        pthread_mutex_unlock(&dsets_lock);
        return;
    }
	
    int32_t value1 = dsets[seta >> 16][seta & 0xFFFF];
    int32_t value2 = dsets[setb >> 16][setb & 0xFFFF];
	int32_t newsize = value1 + value2;

	if(value1 > value2)
	{
		dsets[seta >> 16][seta & 0xFFFF] = setb;
		dsets[setb >> 16][setb & 0xFFFF] = newsize;
	}
	else
	{
		dsets[seta >> 16][seta & 0xFFFF] = newsize;
		dsets[setb >> 16][setb & 0xFFFF] = seta;
	}

    pthread_mutex_unlock(&dsets_lock);
}

void union_at_boundaries(int32_t x_start, int32_t x_end, int32_t y_start, int32_t y_end, int32_t thread_num)
{
    //Only look at bottom and right edges of rectangle; other threads will handle rest

    //bottom edge
    if (y_end != height)
    {
        for (int x = x_start; x < x_end; x++)
        {
            int32_t color1 = edges[x + y_end*width];
            int32_t color2 = edges[x+(y_end+1)*width];
            if (color1 >= 2  && color2 >= 2)
            {
                //union these!
                set_union(color1, color2);
            }
        }
    }

    if (x_end != width)
    {
        for (int y = y_start; y < y_end; y++)
        {
            int32_t color1 = edges[x_end     + y*width];
            int32_t color2 = edges[x_end + 1 + y*width];
            if (color1 >= 2  && color2 >= 2)
            {
                //union these!
                set_union(color1, color2);
            }
        }
    }
}

void* thread_func (void* param)
{
    params_t *p = (params_t*) param;
    vector<int32_t> *pix_count_ptr;

    //Phase 1: find edges and local components

    find_edges (edges, p->x_start, p->x_end, p->y_start,
                p->y_end, width, height, input_image, thresh);

    int32_t num_colors = color_components (
                            p->x_start, p->x_end, p->y_start, p->y_end,
                            width, height, edges, &pix_count_ptr, p->thread_num
                         ) - 2;

    //And set up dsets
    for (int i=0; i < num_colors; i++)
        dsets[p->thread_num].push_back(-(*pix_count_ptr)[i]);

    pthread_barrier_wait(&barrier);
    //Phase 2: scan boundaries and union if necessary

    union_at_boundaries(p->x_start, p->x_end, p->y_start, p->y_end, p->thread_num);

    pthread_barrier_wait(&barrier);
    //Phase 3: write out images.  This will take a bit of load balancing.



    // save some components...
    //TODO: make this work

    if (-1 == save_jpeg_file ("new.jpg", width, height, input_image)) {
        cout << "save_jpeg_file returned -1 "<< endl;
        return NULL;
    }

    delete p;

    return NULL;
}

/*
 * does everything
 */
void
operate (int num_cores)
{
    pthread_t *threads;

    //initialization
    threads = new pthread_t[num_cores];
    pthread_barrier_init(&barrier, NULL, num_cores);

    for(int i = 0; i < num_cores; i++)
    {
        dsets.push_back(vector<int>());
        params_t *param = new params_t(0, width, (i*height)/num_cores, ((i+1)*height)/num_cores, i);

        int rc = pthread_create(threads+i, NULL, thread_func, param);
        if (rc)
        {
            cout << "Failed to allocate thread #" << i << endl;
            delete [] threads;
            return;
        }
    }

    for (int i = 0; i < num_cores; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barrier);
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
    char* after; 
    double threshd;
    int32_t seg_size;

    if (4 != argc) {
        return usage (argv[0]);
    }
    threshd = strtod (argv[2], &after);
    if (argv[2] == after  || '\0' != *after) {
        return usage (argv[0]);
    }
    thresh = ceil(threshd*threshd);

    seg_size = strtol (argv[3], &after, 10);
    if (argv[3] == after  || '\0' != *after) {
        return usage (argv[0]);
    }

    input_image = load_jpeg_file (argv[1], &width, &height);
    if (input_image == NULL) {
        return 2;
    }

    edges = (int32_t*) calloc (width * height, sizeof (edges[0]));
    if (edges == NULL) {
        delete [] input_image;
        return 2;
    }

    int num_cores = sysconf( _SC_NPROCESSORS_ONLN );

    cout << num_cores << " cores" << endl;

    operate (num_cores);

    free (edges);
    delete [] input_image;

    return 0;
}


