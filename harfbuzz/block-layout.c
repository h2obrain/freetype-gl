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
void
add_paragraph(block_layout_t *self, const markup_t *markup) {
	assert(self->paragraph_count<BL_MAX_PARAGRAPH_COUNT);
	if (self->paragraph_count && (self->paragraphs[self->paragraph_count-1].w == self->w)) return;
	self->paragraphs[self->paragraph_count++] = (block_paragraph_t) {
			.y = 0,
			.ascender  = markup->font->ascender,
			.descender = markup->font->descender,
			.w = self->w, .h = markup->font->height
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
	pos_t height = 0;
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
		pos_t width = 0;
		pos_t hres = markup->font->hres;
		uint32_t breakable = UINT32_MAX;
		pos_t breakable_width = 0;
		for (i = 0; i < glyph_count; ++i) {
			int codepoint = glyph_info[i].codepoint;
			pos_t x_advance = glyph_pos[i].x_advance/(pos_t)(hres*64);
			pos_t x_offset = glyph_pos[i].x_offset/(pos_t)(hres*64);
			texture_glyph_t *glyph = texture_font_get_glyph32(markup->font, codepoint);
			if (glyph_info[i].codepoint == ' ') { // breakable spot.. real implementations are MUCH more complex than this!
				breakable = i;
				breakable_width = width;
			}
			if ( i < (glyph_count-1) ) {
				width += x_advance + x_offset;
			} else {
				width += glyph->offset_x + glyph->width;
			}
			if (width > available_width) {
				if (breakable != UINT32_MAX) {
					glyph_count = i = breakable;
					width = breakable_width;
					end = text + glyph_info[i].cluster;
					break;
				} else
				if (self->element_count > 0) {
					uint32_t e = self->element_count - 1;
					while (self->elements[self->element_count].paragraph_y == self->elements[e].paragraph_y) {
						pos_t element_height = self->elements[e].y_off;
						switch (self->elements[e].type) {
							case BET_TEXT:
								element_height += self->elements[e].text.markup->font->height;
								break;
							case BET_BITMAP:
								element_height += self->elements[e].bitmap.margin_top
								                + self->elements[e].bitmap.margin_bottom
												+ self->elements[e].bitmap.h;
								break;
						}
						while (height < element_height) {
							height += markup->font->height;
						}
						available_width += self->elements[self->element_count].w; // TODO + advance_x_e;
						if (width<available_width) break;
						if (e==0) {
							assert("Failed to fit element!" && 0);
							return -1;
						}
						e--;
					}
				}
			}
		}

		self->paragraphs[self->paragraph_count-1].w += width;

		/* add element */
		hb_glyph_info_t *gi = malloc(glyph_count * sizeof(hb_glyph_info_t));
		memcpy(gi,glyph_info,glyph_count * sizeof(hb_glyph_info_t));
		hb_glyph_position_t *gp = malloc(glyph_count * sizeof(hb_glyph_position_t));
		memcpy(gp,glyph_pos,glyph_count * sizeof(hb_glyph_position_t));
		self->elements[self->element_count++] = (block_element_t) {
				.paragraph_y = self->paragraphs[self->paragraph_count-1].y, // identifies line
				.y_off = height,
				.w = width,      // width
				.type = BET_TEXT,
				// descender and margin_bottom could be removed to save space
				.text = {
//						.line_nr = line_nr,
						.last_line = !end,
//						.ascender  = markup->font->ascender,
//						.descender = markup->font->descender,
						.text = text, // utf8 text
						.text_size = end-text, // size of text in bytes
						.markup = markup,
						.glyph_count = glyph_count,
						.glyph_info  = gi, //glyph_info,
						.glyph_pos   = gp  //glyph_pos
					}
			};
		/* height */
		height += markup->font->height;

		if (!end) break;
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

	if (self->paragraphs[self->paragraph_count-1].h < height) {
		self->paragraphs[self->paragraph_count-1].h = height;
	}


	return 0;
}




