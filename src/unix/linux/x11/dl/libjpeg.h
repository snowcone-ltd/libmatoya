// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "sym.h"


// Interface

#define DCTSIZE2            64
#define NUM_QUANT_TBLS      4
#define NUM_HUFF_TBLS       4
#define NUM_ARITH_TBLS      16
#define MAX_COMPS_IN_SCAN   4
#define D_MAX_BLOCKS_IN_MCU 10
#define JMSG_STR_PARM_MAX   80
#define JPEG_LIB_VERSION    80
#define JPEG_HEADER_OK      1

#define TRUE  1
#define FALSE 0

#define jpeg_common_fields \
	struct jpeg_error_mgr *err; \
	struct jpeg_memory_mgr *mem; \
	struct jpeg_progress_mgr *progress; \
	void *client_data; \
	boolean is_decompressor; \
	int global_state

#define JMETHOD(type, methodname, arglist) type (*methodname) arglist

typedef int boolean;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int JDIMENSION;
typedef unsigned char JSAMPLE;

typedef JSAMPLE *JSAMPROW;
typedef JSAMPROW *JSAMPARRAY;
typedef struct jpeg_marker_struct *jpeg_saved_marker_ptr;
typedef struct jpeg_common_struct *j_common_ptr;
typedef struct jpeg_decompress_struct *j_decompress_ptr;

typedef enum {
	JCS_UNKNOWN,
	JCS_GRAYSCALE,
	JCS_RGB,
	JCS_YCbCr,
	JCS_CMYK,
	JCS_YCCK
} J_COLOR_SPACE;

typedef enum {
	JDCT_ISLOW,
	JDCT_IFAST,
	JDCT_FLOAT
} J_DCT_METHOD;

typedef enum {
	JDITHER_NONE,
	JDITHER_ORDERED,
	JDITHER_FS
} J_DITHER_MODE;

typedef struct {
	UINT8 bits[17];
	UINT8 huffval[256];
	boolean sent_table;
} JHUFF_TBL;

typedef struct {
	UINT16 quantval[DCTSIZE2];
	boolean sent_table;
} JQUANT_TBL;

struct jpeg_common_struct {
	jpeg_common_fields;
};

struct jpeg_error_mgr {
	JMETHOD(void, error_exit, (j_common_ptr cinfo));
	JMETHOD(void, emit_message, (j_common_ptr cinfo, int msg_level));
	JMETHOD(void, output_message, (j_common_ptr cinfo));
	JMETHOD(void, format_message, (j_common_ptr cinfo, char *buffer));
	JMETHOD(void, reset_error_mgr, (j_common_ptr cinfo));
	int msg_code;
	union {
		int i[8];
		char s[JMSG_STR_PARM_MAX];
	} msg_parm;
	int trace_level;
	long num_warnings;
	const char * const *jpeg_message_table;
	int last_jpeg_message;
	const char * const *addon_message_table;
	int first_addon_message;
	int last_addon_message;
};

typedef struct {
	int component_id;
	int component_index;
	int h_samp_factor;
	int v_samp_factor;
	int quant_tbl_no;
	int dc_tbl_no;
	int ac_tbl_no;
	JDIMENSION width_in_blocks;
	JDIMENSION height_in_blocks;
	int DCT_h_scaled_size;
	int DCT_v_scaled_size;
	JDIMENSION downsampled_width;
	JDIMENSION downsampled_height;
	boolean component_needed;
	int MCU_width;
	int MCU_height;
	int MCU_blocks;
	int MCU_sample_width;
	int last_col_width;
	int last_row_height;
	JQUANT_TBL *quant_table;
	void *dct_table;
} jpeg_component_info;

struct jpeg_decompress_struct {
	jpeg_common_fields;
	struct jpeg_source_mgr *src;
	JDIMENSION image_width;
	JDIMENSION image_height;
	int num_components;
	J_COLOR_SPACE jpeg_color_space;
	J_COLOR_SPACE out_color_space;
	unsigned int scale_num, scale_denom;
	double output_gamma;
	boolean buffered_image;
	boolean raw_data_out;
	J_DCT_METHOD dct_method;
	boolean do_fancy_upsampling;
	boolean do_block_smoothing;
	boolean quantize_colors;
	J_DITHER_MODE dither_mode;
	boolean two_pass_quantize;
	int desired_number_of_colors;
	boolean enable_1pass_quant;
	boolean enable_external_quant;
	boolean enable_2pass_quant;
	JDIMENSION output_width;
	JDIMENSION output_height;
	int out_color_components;
	int output_components;
	int rec_outbuf_height;
	int actual_number_of_colors;
	JSAMPARRAY colormap;
	JDIMENSION output_scanline;
	int input_scan_number;
	JDIMENSION input_iMCU_row;
	int output_scan_number;
	JDIMENSION output_iMCU_row;
	int (*coef_bits)[DCTSIZE2];
	JQUANT_TBL *quant_tbl_ptrs[NUM_QUANT_TBLS];
	JHUFF_TBL *dc_huff_tbl_ptrs[NUM_HUFF_TBLS];
	JHUFF_TBL *ac_huff_tbl_ptrs[NUM_HUFF_TBLS];
	int data_precision;
	jpeg_component_info *comp_info;
	boolean is_baseline;
	boolean progressive_mode;
	boolean arith_code;
	UINT8 arith_dc_L[NUM_ARITH_TBLS];
	UINT8 arith_dc_U[NUM_ARITH_TBLS];
	UINT8 arith_ac_K[NUM_ARITH_TBLS];
	unsigned int restart_interval;
	boolean saw_JFIF_marker;
	UINT8 JFIF_major_version;
	UINT8 JFIF_minor_version;
	UINT8 density_unit;
	UINT16 X_density;
	UINT16 Y_density;
	boolean saw_Adobe_marker;
	UINT8 Adobe_transform;
	boolean CCIR601_sampling;
	jpeg_saved_marker_ptr marker_list;
	int max_h_samp_factor;
	int max_v_samp_factor;
	int min_DCT_h_scaled_size;
	int min_DCT_v_scaled_size;
	JDIMENSION total_iMCU_rows;
	JSAMPLE *sample_range_limit;
	int comps_in_scan;
	jpeg_component_info *cur_comp_info[MAX_COMPS_IN_SCAN];
	JDIMENSION MCUs_per_row;
	JDIMENSION MCU_rows_in_scan;
	int blocks_in_MCU;
	int MCU_membership[D_MAX_BLOCKS_IN_MCU];
	int Ss, Se, Ah, Al;
	int block_size;
	const int *natural_order;
	int lim_Se;
	int unread_marker;
	struct jpeg_decomp_master *master;
	struct jpeg_d_main_controller *main;
	struct jpeg_d_coef_controller *coef;
	struct jpeg_d_post_controller *post;
	struct jpeg_input_controller *inputctl;
	struct jpeg_marker_reader *marker;
	struct jpeg_entropy_decoder *entropy;
	struct jpeg_inverse_dct *idct;
	struct jpeg_upsampler *upsample;
	struct jpeg_color_deconverter *cconvert;
	struct jpeg_color_quantizer *cquantize;
};

static struct jpeg_error_mgr *(*jpeg_std_error)(struct jpeg_error_mgr *err);
static void (*jpeg_CreateDecompress)(j_decompress_ptr cinfo, int version, size_t structsize);
static void (*jpeg_mem_src)(j_decompress_ptr cinfo, void *buffer, long nbytes);
static int (*jpeg_read_header)(j_decompress_ptr cinfo, boolean require_image);
static boolean (*jpeg_start_decompress)(j_decompress_ptr cinfo);
static JDIMENSION (*jpeg_read_scanlines)(j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines);
static boolean (*jpeg_finish_decompress)(j_decompress_ptr cinfo);
static void (*jpeg_destroy_decompress)(j_decompress_ptr cinfo);


// Runtime open

static MTY_Atomic32 LIBJPEG_LOCK;
static MTY_SO *LIBJPEG_SO;
static bool LIBJPEG_INIT;

static void __attribute__((destructor)) libjpeg_global_destroy(void)
{
	MTY_GlobalLock(&LIBJPEG_LOCK);

	MTY_SOUnload(&LIBJPEG_SO);
	LIBJPEG_INIT = false;

	MTY_GlobalUnlock(&LIBJPEG_LOCK);
}

static bool libjpeg_global_init(void)
{
	MTY_GlobalLock(&LIBJPEG_LOCK);

	if (!LIBJPEG_INIT) {
		bool r = true;

		LIBJPEG_SO = MTY_SOLoad("libjpeg.so.8");
		if (!LIBJPEG_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBJPEG_SO, jpeg_std_error);
		LOAD_SYM(LIBJPEG_SO, jpeg_CreateDecompress);
		LOAD_SYM(LIBJPEG_SO, jpeg_mem_src);
		LOAD_SYM(LIBJPEG_SO, jpeg_read_header);
		LOAD_SYM(LIBJPEG_SO, jpeg_start_decompress);
		LOAD_SYM(LIBJPEG_SO, jpeg_read_scanlines);
		LOAD_SYM(LIBJPEG_SO, jpeg_finish_decompress);
		LOAD_SYM(LIBJPEG_SO, jpeg_destroy_decompress);

		except:

		if (!r)
			libjpeg_global_destroy();

		LIBJPEG_INIT = r;
	}

	MTY_GlobalUnlock(&LIBJPEG_LOCK);

	return LIBJPEG_INIT;
}
