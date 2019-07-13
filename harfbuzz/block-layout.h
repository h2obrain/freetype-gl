/*
 * block_layout.h
 *
 *  Created on: Jul 10, 2019
 *      Author: h2obrain
 *      License: LGPL
 */

#include <inttypes.h>
#include <stdbool.h>
//#include "vector.h"
#include "markup.h"

typedef float pos_t; // int16_t, double

typedef enum {
	BET_TEXT,
	BET_BITMAP,
} block_element_type_t;

//typedef block_element_t;
typedef struct {
//	pos_t w,h;
	pos_t paragraph_y; // identifies paragraph
//	pos_t off_x; // identifies block_element_groups
	pos_t y_off; // for multiline elements
	pos_t w;      // width
//	pos_t x_off;  // position in the line
	block_element_type_t type;
	union {
		// descender and margin_bottom could be removed to save space
		struct {
			uint32_t line_nr; // if >0 the element was split into multiple lines (elements)
			bool last_line;
//			pos_t ascender, descender;
			const char *text; // utf8 text
			size_t text_size; // size of text in bytes
			const markup_t *markup;
			uint32_t glyph_count;
			hb_glyph_info_t glyph_info[];
			hb_glyph_position_t glyph_pos[];
		} text;
		struct {
			pos_t margin_top, margin_bottom;
			// tbd
			pos_t h;
			void *data;
		} bitmap;
	};
//	int (*cut)(block_element_t *self, pos_t w); // break
//	int (*crop)(block_element_t *self, pos_t w, pos_t h);
} block_element_t;

typedef struct {
	pos_t y;
	pos_t ascender,descender; // max of all elements
	/* infos */
	pos_t w,h;
} block_paragraph_t;

#define BL_MAX_ELEMENT_COUNT 100
#define BL_MAX_PARAGRAPH_COUNT 100
typedef struct {
	pos_t w,h;
//	/* could be replaced by "current paragraph" */
	size_t paragraph_count;
	block_paragraph_t paragraphs[BL_MAX_PARAGRAPH_COUNT]; // aligned
//	block_line_t current_paragraph;
	size_t element_count;
	block_element_t elements[BL_MAX_ELEMENT_COUNT]; // aligned!!
} block_layout_t;

int layout_init(block_layout_t *self, pos_t w, pos_t h);
int layout_clear(block_layout_t *self);
int layout_add_text(
		block_layout_t *self,
		const markup_t *markup,
		const char *text, size_t text_size
	);
