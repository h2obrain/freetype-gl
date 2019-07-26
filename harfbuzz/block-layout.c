/*
 * block_layout.c
 *
 *  Created on: Jul 10, 2019
 *      Author: h2obrain
 *      License: LGPL
 */

#include "block-layout.h"
#include <assert.h>

int layout_init(block_layout_t *self, pos_t w, pos_t h) {
	self->w = w;
	self->h = h;
	self->paragraph_count = 0;
	self->element_count = 0;
	return 0;
}

int layout_clear(block_layout_t *self) {
	self->element_count = self->paragraph_count = 0;
	return 0;
}

static inline
char *utf8_walk_forward(char *str, size_t dist, const char *limit) {
	while (dist--) {
//		str += utf8_surrogate_len(str);
		str++;
		if (str>=limit) return limit;
		while ((*str & 0xC0) == 0x80) {
			str++;
			if (str>=limit) return limit;
		}
	}
	return str;
}
static inline
char *utf8_walk_backward(char *str, size_t dist, const char *limit) {
	while (dist--) {
		str--;
		if (str<=limit) return limit;
		while ((*str & 0xC0) == 0x80) {
			str--;
			if (str<=limit) return limit;
		}
	}
	return str;
}

static inline
size_t print_broken_text_sample(
		char *buf, size_t buf_len,
		const char *center_ptr, size_t len,
		const char *start_ptr,const char *end_ptr
) {
	assert(start_ptr<=center_ptr);
	assert(center_ptr<=end_ptr);
	const char *s = utf8_walk_backward(center_ptr, len, start_ptr);
	const char *e = utf8_walk_forward(center_ptr, len, end_ptr);
	return snprintf(buf,buf_len, "'%.*s'|'%.*s'", center_ptr-s,s, e-center_ptr,center_ptr);
}

static inline
void
add_paragraph(block_layout_t *self, const markup_t *markup) {
	assert(self->paragraph_count<BL_MAX_PARAGRAPH_COUNT);
	if (self->paragraph_count && (self->paragraphs[self->paragraph_count-1].w == self->w)) return;
	printf("adding new paragraph\n");
	self->paragraphs[self->paragraph_count++] = (block_paragraph_t) {
			.x = 0, .y = 0,
			.w = 0, .h = 0,
			.pen = {0,0},
			.ascender  = markup->font->ascender,
			.descender = markup->font->descender
		};
}

int layout_add_text(
		block_layout_t *self,
		const markup_t *markup,
		const char *text, size_t text_size
) {
	assert(self && text && markup);
	if ( text_size == 0 ) {
		text_size = strlen(text);
		if ( text_size == 0 ) return 0;
	}

	pos_t available_width = self->w;
	if (self->paragraph_count == 0) {
		add_paragraph(self, markup);
	} else {
		available_width -= self->paragraphs[self->paragraph_count].w; // TODO + x_advance;

		if (self->paragraphs[self->paragraph_count-1].ascender < markup->font->ascender) {
			self->paragraphs[self->paragraph_count-1].ascender = markup->font->ascender;
		}
		if (self->paragraphs[self->paragraph_count-1].descender > markup->font->descender) {
			self->paragraphs[self->paragraph_count-1].descender = markup->font->descender;
		}
	}
	block_paragraph_t *paragraph = &self->paragraphs[self->paragraph_count-1];

	bool first = true;
	/* Create a buffer for harfbuzz to use (TODO use markup->font->buffer) */
	hb_buffer_t *buffer = hb_buffer_create();
	while (text_size) {
		const char *end = memchr(text,'\n',text_size);

		if (text == end) {
			if (!first) self->elements[self->element_count-1].text.last_line = true;
			while (*text++ == '\n');
			add_paragraph(self, markup);
		}
		first = false;

        hb_buffer_set_direction(buffer, markup->direction);
        hb_buffer_set_script(buffer, markup->script); /* see hb-unicode.h */
		hb_buffer_set_language(buffer, markup->font->language);

		size_t len;
		if (end) {
			len = end-text;
			printf("add_text(line:%2d): '%.*s'\n",len,len,text);
		} else {
			len = text_size;
			printf("add_text(rest:%2d): '%.*s'\n",len,len,text);
		}
		hb_buffer_add_utf8( buffer, text, len, 0, len );
		hb_buffer_guess_segment_properties( buffer );
		hb_shape( markup->font->hb_ft_font, buffer, NULL, 0 );

		unsigned int         glyph_count;
		hb_glyph_info_t     *glyph_info;
		hb_glyph_position_t *glyph_pos;
		glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
		glyph_pos  = hb_buffer_get_glyph_positions(buffer, &glyph_count);

		/* load not existing glyphs */
		texture_font_load_glyphs( markup->font, text );

		size_t i;
//		uint32_t line_nr = 0;

		/* layout width.. */
		pos_t width,height;
		width = height = 0;
		pos_t hres = markup->font->hres;
		uint32_t breakable = UINT32_MAX;
		pos_t breakable_width, breakable_height;
		breakable_width = breakable_height = 0;
		for (i = 0; i < glyph_count; ++i) {
			int codepoint = glyph_info[i].codepoint;
			pos_t x_advance = glyph_pos[i].x_advance/(pos_t)(hres*64);
			pos_t y_advance = glyph_pos[i].y_advance/(pos_t)(hres*64);
			pos_t x_offset = glyph_pos[i].x_offset/(pos_t)(hres*64);
			pos_t y_offset = glyph_pos[i].y_offset/(pos_t)(hres*64);
			texture_glyph_t *glyph = texture_font_get_glyph32(markup->font, codepoint);
			if (glyph_info[i].codepoint == ' ') { // breakable spot.. real implementations are MUCH more complex than this!
				breakable = i;
				breakable_width  = width;
				breakable_height = height;
			}
			if ( i < (glyph_count-1) ) {
				width  += x_advance + x_offset;
				height += y_advance + y_offset;
			} else {
				width  += glyph->offset_x + glyph->width;
				height += glyph->offset_y + glyph->height;
			}
			if (width > available_width) {
				if (breakable != UINT32_MAX) {
					i = breakable;
					width  = breakable_width;
					height = breakable_height;
					end = text + glyph_info[i].cluster;
					break;
				} else {
					i=0;
					width = height = 0;
					end = text;
					break;
					// >> old!
////				if (self->element_count > 0) {
////					uint32_t e = self->element_count - 1;
////					while (self->elements[self->element_count].paragraph_y == self->elements[e].paragraph_y) {
////						pos_t element_height = self->elements[e].y_off;
////						switch (self->elements[e].type) {
////							case BET_TEXT:
////								element_height += self->elements[e].text.markup->font->height;
////								break;
////							case BET_BITMAP:
////								element_height += self->elements[e].bitmap.margin_top
////								                + self->elements[e].bitmap.margin_bottom
////												+ self->elements[e].bitmap.h;
////								break;
////						}
////						while (paragraph->pen.y < element_height) {
////							paragraph->pen.y += markup->font->height;
////						}
////						available_width += self->elements[self->element_count].w; // TODO + advance_x_e;
////						if (width<available_width) break;
////						if (e==0) {
////							assert("Failed to fit element!" && 0);
////							return -1;
////						}
////						e--;
////					}
				}
			}
		}
		if (i>0) {
			/* add element */
			hb_glyph_info_t *gi = malloc(i * sizeof(hb_glyph_info_t));
			memcpy(gi,glyph_info,i * sizeof(hb_glyph_info_t));
			hb_glyph_position_t *gp = malloc(i * sizeof(hb_glyph_position_t));
			memcpy(gp,glyph_pos,i * sizeof(hb_glyph_position_t));
			self->elements[self->element_count++] = (block_element_t) {
					.paragraph = paragraph,
					.x = paragraph->pen.x,
					.y = paragraph->pen.y,
					.w = width,      // width
					.h = height,     // height
					.type = BET_TEXT,
					// descender and margin_bottom could be removed to save space
					.text = {
	//						.line_nr     = line_nr,
							.last_line   = !end,
	//						.ascender    = markup->font->ascender,
	//						.descender   = markup->font->descender,
							.text        = text, // utf8 text
							.text_size   = end-text, // size of text in bytes
							.markup      = markup,
							.glyph_count = i,
							.glyph_info  = gi, //glyph_info,
							.glyph_pos   = gp  //glyph_pos
						}
				};

			/* update paragraph width */
//			switch (markup->font->language)
			paragraph->pen.x += width;
//			paragraph->pen.y += height;
			if (paragraph->w < paragraph->pen.x) paragraph->w = paragraph->pen.x;
		}

//		pos_t height = paragraph->pen.y;
//		/* height */
//		height += markup->font->height;
		/* update paragraph height */
		if (paragraph->h < height) paragraph->h = height;
		/* end line */
		if (!end) break;
		/* update pen */
		paragraph->pen.x  = 0;
		paragraph->pen.y += paragraph->h - paragraph->descender;
//		paragraph->ascender = paragraph->descender = 0;

		char buf[1024]; char *bp=buf; buf[0]='\0';
		bp += strlen(strncpy(bp,"Breaking line ",sizeof(buf)-(bp-buf)));
		bp += print_broken_text_sample(
				bp,sizeof(buf)-(bp-buf),
				(i<glyph_count ? text + glyph_info[i].cluster : text+text_size), 10,
				text,text+text_size
			);
		*bp++ = '\n'; *bp='\0';
		puts(buf);

		/* update pointer to the start of the new line */
		text_size -= end+1-text;
		text = end+1;

		/* break-line */
//		multiline_element = true;
//		recalculate available width
//		if (line_nr == 0) {
//			self->elements[self->element_count-1].text.line_nr = 1;
//			line_nr = 2;
//		} else {
//			line_nr++;
//		}

		hb_buffer_reset(buffer);
	}

	return 0;
}




