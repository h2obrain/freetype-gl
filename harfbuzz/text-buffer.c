/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

//#include "opengl.h"
#include "text-buffer.h"
#include "utf8-utils.h"
#include "freetype-gl-err.h"


//static void text_buffer_move_last_line( text_buffer_t * self, float dx, float dy );
//static void text_buffer_finish_line( text_buffer_t * self, vec2 * pen, bool advancePen );
//static void text_buffer_set_line_direction( text_buffer_t * self, hb_direction_t direction, vec2 *pen);

#define SET_GLYPH_VERTEX(value,x0,y0,z0,s0,t0,r,g,b,a,sh,gm) { \
	glyph_vertex_t *gv=&value;                                 \
	gv->x=x0; gv->y=y0; gv->z=z0;                              \
	gv->u=s0; gv->v=t0;                                        \
	gv->r=r; gv->g=g; gv->b=b; gv->a=a;                        \
	gv->shift=sh; gv->gamma=gm;}

// ----------------------------------------------------------------------------

text_buffer_t *
text_buffer_new(float w, float h) {
	text_buffer_t *self = (text_buffer_t *) malloc (sizeof(text_buffer_t));
	self->buffer = vertex_buffer_new( "vertex:3f,tex_coord:2f,color:4f,ashift:1f,agamma:1f" );
	self->base_color.r = 0.0;
	self->base_color.g = 0.0;
	self->base_color.b = 0.0;
	self->base_color.a = 1.0;
//	self->line_start_index = 0;
//	self->line_ascender = 0;
//	self->line_descender = 0;
//	self->lines = vector_new( sizeof(line_info_t) );
//	self->bounds.left   = 0.0;
//	self->bounds.top    = 0.0;
//	self->bounds.width  = 0.0;
//	self->bounds.height = 0.0;
//	self->direction = HB_DIRECTION_INVALID;
	layout_init(&self->layout, w, h);
	return self;
}

// ----------------------------------------------------------------------------
void text_buffer_delete( text_buffer_t * self ) {
//	vector_delete( self->lines );
	vertex_buffer_delete( self->buffer );
	free( self );
}

// ----------------------------------------------------------------------------
void text_buffer_clear( text_buffer_t * self ) {
	assert( self );

	vertex_buffer_clear( self->buffer );
//	self->line_start_index = 0;
//	self->line_ascender = 0;
//	self->line_descender = 0;
//	vector_clear( self->lines );
//	self->bounds.left   = 0.0;
//	self->bounds.top    = 0.0;
//	self->bounds.width  = 0.0;
//	self->bounds.height = 0.0;
	layout_clear( &self->layout );
}

static inline void layout_to_vertex_buffer(text_buffer_t * self, vec2 *pen) {
	vec2 pen_orig = *pen;
	for (size_t e=0; e<self->layout.element_count; e++) {
		block_element_t *el = &self->layout.elements[e];
		assert(self->layout.elements.type==BET_TEXT);
		printf("X po:%5.1f par:%5.1f el:%5.1f\n",pen_orig.x, el->paragraph->x, el->x);
		pen->x = pen_orig.x + el->paragraph->x + el->x;
		pen->y = pen_orig.y - (el->paragraph->y + el->y + self->layout.paragraphs[0].ascender);
		for (int32_t c=0; c<el->text.glyph_count; c++) {
			text_buffer_add_char(self, pen, el->text.markup, &el->text.glyph_info[c],&el->text.glyph_pos[c]);
		}
	}
}

vec4
text_buffer_get_bounds( text_buffer_t * self, vec2 * pen ) {
	return (vec4){10,10,100,100};
//
//	for (size_t p=0; p<self->layout.paragraph_count; p++) {
//		block_element_t *par = &self->layout.paragraphs[p];
//		par->w
//	}
//	for (size_t e=0; e<self->layout.element_count; e++) {
//		block_element_t *el = &self->layout.elements[e];
//
//	}
}


// ----------------------------------------------------------------------------
void text_buffer_printf( text_buffer_t * self, vec2 *pen, ... ) {
	markup_t *markup;
	char *text;
	va_list args;

//	if ( vertex_buffer_size( self->buffer ) == 0 ) {
//		self->origin = *pen;
//	}

	va_start ( args, pen );
	do {
		markup = va_arg( args, markup_t * );
		if ( markup == NULL ) {
			goto text_buffer_printf_done;
		}
		text = va_arg( args, char * );
//		text_buffer_add_text( self, pen, markup, text, 0 );
		layout_add_text(&self->layout, markup, text,0);
	} while ( markup != 0 );
text_buffer_printf_done:
	va_end ( args );

	layout_to_vertex_buffer(self, pen);
}

//// ----------------------------------------------------------------------------
//void text_buffer_move_last_line( text_buffer_t * self, float dx, float dy ) {
//	size_t i;
//	int j;
//	self->line_start.x += dx;
//	self->line_start.y -= dy;
//	for ( i=self->line_start_index; i < vector_size( self->buffer->items ); ++i ) {
//		ivec4 *item = (ivec4 *) vector_get( self->buffer->items, i);
//		for ( j=item->vstart; j<item->vstart+item->vcount; ++j) {
//			glyph_vertex_t * vertex =
//				(glyph_vertex_t *)  vector_get( self->buffer->vertices, j );
//			vertex->x += dx;
//			vertex->y -= dy;
//		}
//	}
//}
//
//// ----------------------------------------------------------------------------
//// text_buffer_finish_line (internal use only)
////
//// Performs calculations needed at the end of each line of text
//// and prepares for the next line if necessary
////
//// advancePen: if true, advance the pen to the next line
////
//void text_buffer_finish_line( text_buffer_t * self, vec2 * pen, bool advancePen ) {
//	float line_right,line_bottom;
////	float line_left = self->line_start.x;
////	float line_right = pen->x;
////	float line_width  = line_right - line_left;
////	float line_top = self->line_start.y + self->line_ascender;
////	float line_bottom = pen->y + self->line_descender;
////	float line_height  = line_top - line_bottom;
//////	float line_top = pen->y + self->line_ascender;
//////	float line_height = self->line_ascender - self->line_descender;
//////	float line_bottom = line_top - line_height;
//
//	line_info_t line_info;
//	line_info.line_start_index = self->line_start_index;
//	line_info.bounds.left = self->line_start.x;
//	line_right = pen->x;
//	switch (self->direction) {
//		case HB_DIRECTION_LTR:
//		case HB_DIRECTION_RTL:
//			line_info.bounds.top = self->line_start.y + self->line_ascender;
//			line_bottom = pen->y + self->line_descender;
//			break;
//		case HB_DIRECTION_TTB:
//		case HB_DIRECTION_BTT:
//			line_info.bounds.top = self->line_start.y;
//			line_bottom = pen->y;
//			break;
//	}
//	line_info.bounds.width = line_right - line_info.bounds.left;
//	line_info.bounds.height = line_info.bounds.top - line_bottom;
//	line_info.direction = self->direction;
//
//	vector_push_back( self->lines,  &line_info);
//
//	printf("line (start: %5.1f,ascender: %5.1f,height: %5.1f)\n",self->line_start.y,self->line_ascender,line_info.bounds.height);
//
//	if (line_info.bounds.left < self->bounds.left) {
//		printf("bounds.left(%4.1f,%4.1f)\n",self->bounds.left, line_info.bounds.left);
//		self->bounds.left = line_info.bounds.left;
//	}
//	if (line_info.bounds.top > self->bounds.top) {
//		printf("bounds.top(%4.1f,%4.1f)\n",self->bounds.top, line_info.bounds.top);
//		self->bounds.top = line_info.bounds.top;
//	}
//
//	float self_right = self->bounds.left + self->bounds.width;
//	float self_bottom = self->bounds.top - self->bounds.height;
//
//	if (line_right > self_right) {
//		printf("bounds.width(%4.1f,%4.1f)\n",self->bounds.width, line_info.bounds.width);
//		self->bounds.width = line_right - self->bounds.left;
//	}
//	if (line_bottom < self_bottom) {
//		printf("bounds.height(%4.1f,%4.1f)\n",self->bounds.height, line_info.bounds.height);
//		self->bounds.height = self->bounds.top - line_bottom;
//	}
//
//	if ( advancePen ) {
//		switch (self->direction) {
//			case HB_DIRECTION_LTR:
//			case HB_DIRECTION_RTL:
////				pen->x  = self->origin.x;
//				pen->x  = self->line_start.x;
//				pen->y += self->line_descender;
//				break;
//			case HB_DIRECTION_TTB:
//			case HB_DIRECTION_BTT:
//				text_buffer_move_last_line(self, (self->line_ascender - self->line_descender)/2, 0);
//				pen->x += self->line_ascender - self->line_descender;
//				pen->y  = self->line_start.y;
//				break;
//		}
//	}
//
//	self->line_descender = 0;
//	self->line_ascender = 0;
//	self->line_start_index = vector_size( self->buffer->items );
////	self->line_start.x = pen->x;
//	self->line_start = *pen;
//}
//
//// ----------------------------------------------------------------------------
//void text_buffer_set_line_direction( text_buffer_t * self, hb_direction_t direction, vec2 *pen) {
//	if (vertex_buffer_size(self->buffer) == 0) {
//		self->direction = HB_DIRECTION_INVALID;
//	}
//	switch (self->direction) {
//		case HB_DIRECTION_LTR:
//		case HB_DIRECTION_RTL:
//			switch (direction) {
//				case HB_DIRECTION_LTR:
//				case HB_DIRECTION_RTL:
//					break;
//				case HB_DIRECTION_TTB:
//				case HB_DIRECTION_BTT:
//					text_buffer_finish_line(self, pen, false);
////					text_buffer_finish_line(self, &self->last_pen, false);
////						self->last_pen = self->line_start = (vec2){ .x=self->origin.x, .y=self->bounds.top-self->bounds.height };
////					self->origin.y = self->last_pen.y;
////					self->line_start.x = pen->x;
//					break;
//			}
//			break;
//		case HB_DIRECTION_TTB:
//		case HB_DIRECTION_BTT:
//			switch (direction) {
//				case HB_DIRECTION_LTR:
//				case HB_DIRECTION_RTL:
//					text_buffer_finish_line(self, pen, false);
////					text_buffer_finish_line(self, &self->last_pen, false);
//					printf("bounds top:%5.1f height:%5.1f\n",self->bounds.top,self->bounds.height);
//					//*pen =
////					self->line_start = (vec2){ .x=self->origin.x, .y=self->bounds.top-self->bounds.height };
//					break;
//				case HB_DIRECTION_TTB:
//				case HB_DIRECTION_BTT:
//					break;
//			}
//			break;
//		case HB_DIRECTION_INVALID:
//			// first
//			printf("first direction\n");
//			break;
//	}
//	self->direction = direction;
//	printf("pendc(%4.1f,%4.1f)\n",pen->x,pen->y);
//}
//
//// ----------------------------------------------------------------------------
//void
//text_buffer_add_text( text_buffer_t *self,
//					  vec2 *pen, markup_t *markup,
//					  const char *text, size_t length ) {
//	size_t i;
//	const char * prev_character = NULL;
//
//	if ( markup == NULL ) {
//		return;
//	}
//
//	if ( !markup->font ) {
//		freetype_gl_error( No_Font_In_Markup,
//			   "Houston, we've got a problem !\n" );
//		return;
//	}
//
//	if ( length == 0 ) {
//		length = strlen(text);
////		length = utf8_end(text)-text;
////		char_count = utf8_strlen(text);
//		if ( length == 0 ) {
//			return;
//		}
//	}
//	if ( vertex_buffer_size( self->buffer ) == 0 ) {
//		self->origin = *pen;
//		self->line_start = *pen;
//		self->bounds.left = pen->x;
//		self->bounds.top = pen->y;
//	} else {
//		if (pen->x < self->origin.x) {
//			self->origin.x = pen->x;
//		}
//		if (pen->y != self->last_pen.y) {
//			text_buffer_finish_line(self, pen, false);
//		}
//	}
//
//	text_buffer_set_line_direction(self, markup->direction, pen);
//
//	/* Create a buffer for harfbuzz to use (TODO use markup->font->buffer) */
//	hb_buffer_t *buffer = hb_buffer_create();
//	while (length) {
//		const char *end = memchr(text,'\n',length);
//
//        hb_buffer_set_direction(buffer, markup->direction);
//        hb_buffer_set_script(buffer, markup->script); /* see hb-unicode.h */
//		hb_buffer_set_language(buffer, markup->font->language);
//
//		size_t len;
//		if (end) {
//			len = end-text;
//			printf("add_text(line:%2d): '%.*s'\n",len,len,text);
//		} else {
//			len = length;
//			printf("add_text(rest:%2d): '%.*s'\n",len,len,text);
//		}
//		hb_buffer_add_utf8( buffer, text, len, 0, len );
//		hb_buffer_guess_segment_properties( buffer );
//		hb_shape( markup->font->hb_ft_font, buffer, NULL, 0 );
//
//		unsigned int         glyph_count;
//		hb_glyph_info_t     *glyph_info;
//		hb_glyph_position_t *glyph_pos;
//		glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
//		glyph_pos  = hb_buffer_get_glyph_positions(buffer, &glyph_count);
//
//		/* load not existing glyphs */
//		texture_font_load_glyphs( markup->font, text );
//
//		/* layout width.. */
////		float width = 0.0;
////		float hres = markup->font->hres;
////		for (i = 0; i < glyph_count; ++i) {
////			int codepoint = glyph_info[i].codepoint;
////			float x_advance = glyph_pos[i].x_advance/(float)(hres*64);
////			float x_offset = glyph_pos[i].x_offset/(float)(hres*64);
////			texture_glyph_t *glyph = texture_font_get_glyph32(fonts[i], codepoint);
////			if ( i < (glyph_count-1) )
////				width += x_advance + x_offset;
////			else
////				width += glyph->offset_x + glyph->width;
////		}
//
//		printf("self->bounds.top: %5.1f\n",self->bounds.top);
//		for (i = 0; i < glyph_count; ++i) {
//			text_buffer_add_char( self, pen, markup, &glyph_info[i], &glyph_pos[i] );
//		}
//		printf("self->bounds.top: %5.1f\n",self->bounds.top);
//
//		if (!end) break;
//		length -= end+1-text;
//		text = end+1;
//
//		/* newline */
//		text_buffer_finish_line(self, pen, true);
//
//		hb_buffer_reset(buffer);
//	}
//
//
//	self->last_pen = *pen;
//}

// ----------------------------------------------------------------------------
void
text_buffer_add_char( text_buffer_t * self,
					  vec2 * pen, markup_t * markup,
					  hb_glyph_info_t     *glyph_info,
					  hb_glyph_position_t *glyph_pos
) {
	size_t vcount = 0;
	size_t icount = 0;
	vertex_buffer_t * buffer = self->buffer;
	texture_font_t * font = markup->font;
	float gamma = markup->gamma;

	// Maximum number of vertices is 20 (= 5x2 triangles) per glyph:
	//  - 2 triangles for background
	//  - 2 triangles for overline
	//  - 2 triangles for underline
	//  - 2 triangles for strikethrough
	//  - 2 triangles for glyph
	glyph_vertex_t vertices[4*5];
	GLuint indices[6*5];
	texture_glyph_t *glyph;
	texture_glyph_t *black;
	float kerning = 0.0f;

//	if ( markup->font->ascender > self->line_ascender ) {
//		switch (markup->direction) {
//			case HB_DIRECTION_LTR:
//			case HB_DIRECTION_RTL: {
//				printf("ascender (%5.1f > %5.1f)\n",markup->font->ascender, self->line_ascender);
//				float y = pen->y;
//				pen->y -= (markup->font->ascender - self->line_ascender);
//				text_buffer_move_last_line( self, 0,y-pen->y );
//				self->line_ascender = markup->font->ascender;
//			}	break;
//			case HB_DIRECTION_TTB:
//			case HB_DIRECTION_BTT:
//				printf("desc: %5.1f\n",markup->font->descender);
//				/* aaah! haaaack :) */
//				/* first char on line */
//				if (self->line_ascender==0) {
//					pen->y += markup->font->ascender + markup->font->descender;
//				}
//				self->line_ascender = markup->font->ascender;
//				break;
//		}
//	}
//	if ( markup->font->descender < self->line_descender ) {
//		self->line_descender = markup->font->descender;
//	}

//	if ( glyph_info->codepoint == '\n' ) {
//		text_buffer_finish_line(self, pen, true);
//		return;
//	}

	glyph = texture_font_get_glyph32( font, glyph_info->codepoint );
	black = texture_font_get_glyph( font, NULL );

	if ( glyph == NULL ) {
		return;
	}

//	if ( previous && markup->font->kerning ) {
//		kerning = texture_glyph_get_kerning( glyph, previous );
//	}
//	pen->x += kerning;

	printf("pen(%4.1f,%4.1f)\n",pen->x,pen->y);

	float hres = markup->font->hres;

	// ..

	int codepoint = glyph_info->codepoint;
	// because of vhinting trick we need the extra 64 (hres)
	float x_advance = glyph_pos->x_advance/(float)(hres*64);
	float x_offset  = glyph_pos->x_offset/(float)(hres*64);
	float y_advance = glyph_pos->y_advance/(float)(64);
	float y_offset  = glyph_pos->y_offset/(float)(64);

#if !defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
	float atlas_width,atlas_height;
	atlas_width  = (float)markup->font->atlas->width;
	atlas_height = (float)markup->font->atlas->height;
#endif

	// Background
	if ( markup->background_color.alpha > 0 ) {
		float r = markup->background_color.r;
		float g = markup->background_color.g;
		float b = markup->background_color.b;
		float a = markup->background_color.a;
		float x0 = pen->x;
		float y0 = (float)(int)( pen->y + font->descender );
		float x1 = x0 + x_advance;
		float y1 = (float)(int)( pen->y + font->ascender );
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
		float s0 = black->s0;
		float t0 = black->t0;
		float s1 = black->s1;
		float t1 = black->t1;
#else
		float s0 = black->tex_region.x/atlas_width;
		float t0 = black->tex_region.y/atlas_height;
		float s1 = (black->tex_region.x+black->tex_region.width)/atlas_width;
		float t1 = (black->tex_region.y+black->tex_region.height)/atlas_height;
#endif

		SET_GLYPH_VERTEX(vertices[vcount+0], (float)(int)x0,y0,0,  s0,t0,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+1], (float)(int)x0,y1,0,  s0,t1,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+2], (float)(int)x1,y1,0,  s1,t1,  r,g,b,a,  x1-((int)x1), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+3], (float)(int)x1,y0,0,  s1,t0,  r,g,b,a,  x1-((int)x1), gamma );
		indices[icount + 0] = vcount+0;
		indices[icount + 1] = vcount+1;
		indices[icount + 2] = vcount+2;
		indices[icount + 3] = vcount+0;
		indices[icount + 4] = vcount+2;
		indices[icount + 5] = vcount+3;
		vcount += 4;
		icount += 6;
	}

	// Underline
	if ( markup->underline ) {
		float r = markup->underline_color.r;
		float g = markup->underline_color.g;
		float b = markup->underline_color.b;
		float a = markup->underline_color.a;
		float x0 = pen->x;
		float y0 = (float)(int)( pen->y + font->underline_position );
		float x1 = x0 + x_advance;
		float y1 = (float)(int)( y0 + font->underline_thickness );
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
		float s0 = black->s0;
		float t0 = black->t0;
		float s1 = black->s1;
		float t1 = black->t1;
#else
		float s0 = black->tex_region.x/atlas_width;
		float t0 = black->tex_region.y/atlas_height;
		float s1 = (black->tex_region.x+black->tex_region.width)/atlas_width;
		float t1 = (black->tex_region.y+black->tex_region.height)/atlas_height;
#endif

		SET_GLYPH_VERTEX(vertices[vcount+0], (float)(int)x0,y0,0,  s0,t0,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+1], (float)(int)x0,y1,0,  s0,t1,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+2], (float)(int)x1,y1,0,  s1,t1,  r,g,b,a,  x1-((int)x1), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+3], (float)(int)x1,y0,0,  s1,t0,  r,g,b,a,  x1-((int)x1), gamma );
		indices[icount + 0] = vcount+0;
		indices[icount + 1] = vcount+1;
		indices[icount + 2] = vcount+2;
		indices[icount + 3] = vcount+0;
		indices[icount + 4] = vcount+2;
		indices[icount + 5] = vcount+3;
		vcount += 4;
		icount += 6;
	}

	// Overline
	if ( markup->overline ) {
		float r = markup->overline_color.r;
		float g = markup->overline_color.g;
		float b = markup->overline_color.b;
		float a = markup->overline_color.a;
		float x0 = pen->x;
		float y0 = (float)(int)( pen->y + (int)font->ascender );
		float x1 = x0 + x_advance;
		float y1 = (float)(int)( y0 + (int)font->underline_thickness );
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
		float s0 = black->s0;
		float t0 = black->t0;
		float s1 = black->s1;
		float t1 = black->t1;
#else
		float s0 = black->tex_region.x/atlas_width;
		float t0 = black->tex_region.y/atlas_height;
		float s1 = (black->tex_region.x+black->tex_region.width)/atlas_width;
		float t1 = (black->tex_region.y+black->tex_region.height)/atlas_height;
#endif
		SET_GLYPH_VERTEX(vertices[vcount+0], (float)(int)x0,y0,0,  s0,t0,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+1], (float)(int)x0,y1,0,  s0,t1,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+2], (float)(int)x1,y1,0,  s1,t1,  r,g,b,a,  x1-((int)x1), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+3], (float)(int)x1,y0,0,  s1,t0,  r,g,b,a,  x1-((int)x1), gamma );
		indices[icount + 0] = vcount+0;
		indices[icount + 1] = vcount+1;
		indices[icount + 2] = vcount+2;
		indices[icount + 3] = vcount+0;
		indices[icount + 4] = vcount+2;
		indices[icount + 5] = vcount+3;
		vcount += 4;
		icount += 6;
	}

	/* Strikethrough */
	if ( markup->strikethrough ) {
		float r = markup->strikethrough_color.r;
		float g = markup->strikethrough_color.g;
		float b = markup->strikethrough_color.b;
		float a = markup->strikethrough_color.a;
		float x0  = pen->x;
		float y0  = (float)(int)( pen->y + (float)font->ascender/3);
		float x1  = x0 + x_advance;
		float y1  = (float)(int)( y0 + (int)font->underline_thickness );
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
		float s0 = black->s0;
		float t0 = black->t0;
		float s1 = black->s1;
		float t1 = black->t1;
#else
		float s0 = black->tex_region.x/atlas_width;
		float t0 = black->tex_region.y/atlas_height;
		float s1 = (black->tex_region.x+black->tex_region.width)/atlas_width;
		float t1 = (black->tex_region.y+black->tex_region.height)/atlas_height;
#endif
		SET_GLYPH_VERTEX(vertices[vcount+0], (float)(int)x0,y0,0,  s0,t0,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+1], (float)(int)x0,y1,0,  s0,t1,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+2], (float)(int)x1,y1,0,  s1,t1,  r,g,b,a,  x1-((int)x1), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+3], (float)(int)x1,y0,0,  s1,t0,  r,g,b,a,  x1-((int)x1), gamma );
		indices[icount + 0] = vcount+0;
		indices[icount + 1] = vcount+1;
		indices[icount + 2] = vcount+2;
		indices[icount + 3] = vcount+0;
		indices[icount + 4] = vcount+2;
		indices[icount + 5] = vcount+3;
		vcount += 4;
		icount += 6;
	}
	{
		// Actual glyph
		float r = markup->foreground_color.red;
		float g = markup->foreground_color.green;
		float b = markup->foreground_color.blue;
		float a = markup->foreground_color.alpha;
		float x0 = pen->x + x_offset + glyph->offset_x;
		float x1 = x0 + glyph->width;
		float y0 = floor(pen->y + y_offset + glyph->offset_y);
		float y1 = floor(y0 - glyph->height);
//		float x0 = ( pen->x + glyph->offset_x );
//		float y0 = (float)(int)( pen->y + glyph->offset_y );
//		float x1 = ( x0 + glyph->width );
//		float y1 = (float)(int)( y0 - glyph->height );
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
		float s0 = glyph->s0;
		float t0 = glyph->t0;
		float s1 = glyph->s1;
		float t1 = glyph->t1;
#else
		float s0 = glyph->tex_region.x/atlas_width;
		float t0 = glyph->tex_region.y/atlas_height;
		float s1 = (glyph->tex_region.x+glyph->tex_region.width)/atlas_width;
		float t1 = (glyph->tex_region.y+glyph->tex_region.height)/atlas_height;
#endif

		SET_GLYPH_VERTEX(vertices[vcount+0], (float)(int)x0,y0,0,  s0,t0,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+1], (float)(int)x0,y1,0,  s0,t1,  r,g,b,a,  x0-((int)x0), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+2], (float)(int)x1,y1,0,  s1,t1,  r,g,b,a,  x1-((int)x1), gamma );
		SET_GLYPH_VERTEX(vertices[vcount+3], (float)(int)x1,y0,0,  s1,t0,  r,g,b,a,  x1-((int)x1), gamma );
		indices[icount + 0] = vcount+0;
		indices[icount + 1] = vcount+1;
		indices[icount + 2] = vcount+2;
		indices[icount + 3] = vcount+0;
		indices[icount + 4] = vcount+2;
		indices[icount + 5] = vcount+3;
		vcount += 4;
		icount += 6;

		vertex_buffer_push_back( buffer, vertices, vcount, indices, icount );
//		pen->x += glyph->advance_x * (1.0f + markup->spacing);
		pen->x += x_advance * (1.0f + markup->spacing);
		pen->y += y_advance * (1.0f + markup->spacing);
	}
}

//// ----------------------------------------------------------------------------
//void
//text_buffer_align( text_buffer_t * self, vec2 * pen,
//				   enum Align alignment ) {
//	/* TODO handle direction */
//
//	if (ALIGN_LEFT == alignment) {
//		return;
//	}
//
//	size_t total_items = vector_size( self->buffer->items );
//	if ( self->line_start_index != total_items ) {
//		text_buffer_finish_line( self, pen, false );
//	}
//
//
//	size_t i, j;
//	int k;
//	float self_left, self_right, self_center;
//	float line_left, line_right, line_center;
//	float dx;
//
//	self_left = self->bounds.left;
//	self_right = self->bounds.left + self->bounds.width;
//	self_center = (self_left + self_right) / 2;
//
//	line_info_t* line_info;
//	size_t lines_count, line_end;
//
//	lines_count = vector_size( self->lines );
//	for ( i = 0; i < lines_count; ++i ) {
//		line_info = (line_info_t*)vector_get( self->lines, i );
//
//		if ( i + 1 < lines_count ) {
//			line_end = ((line_info_t*)vector_get( self->lines, i + 1 ))->line_start_index;
//		} else {
//			line_end = vector_size( self->buffer->items );
//		}
//
//		line_right = line_info->bounds.left + line_info->bounds.width;
//
//		if ( ALIGN_RIGHT == alignment ) {
//			dx = self_right - line_right;
//		} else // ALIGN_CENTER
//		{
//			line_left = line_info->bounds.left;
//			line_center = (line_left + line_right) / 2;
//			dx = self_center - line_center;
//		}
//
//		dx = roundf( dx );
//
//		for ( j=line_info->line_start_index; j < line_end; ++j ) {
//			ivec4 *item = (ivec4 *) vector_get( self->buffer->items, j);
//			for ( k=item->vstart; k<item->vstart+item->vcount; ++k) {
//				glyph_vertex_t * vertex = (glyph_vertex_t *)vector_get( self->buffer->vertices, k );
//				vertex->x += dx;
//			}
//		}
//	}
//}
//
//vec4
//text_buffer_get_bounds( text_buffer_t * self, vec2 * pen ) {
//	size_t total_items = vector_size( self->buffer->items );
//	if ( self->line_start_index != total_items ) {
//		text_buffer_finish_line( self, pen, false );
//	}
//
//	return self->bounds;
//}
