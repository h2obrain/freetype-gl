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

#ifdef FREETYPE_GL_NOGL
#include <ip/dma2d.h>
#else
#define SET_GLYPH_VERTEX(value,x0,y0,z0,s0,t0,r,g,b,a,sh,gm) { \
	glyph_vertex_t *gv=&value;                                 \
	gv->x=x0; gv->y=y0; gv->z=z0;                              \
	gv->u=s0; gv->v=t0;                                        \
	gv->r=r; gv->g=g; gv->b=b; gv->a=a;                        \
	gv->shift=sh; gv->gamma=gm;}
#endif

// ----------------------------------------------------------------------------

#ifdef FREETYPE_GL_NOGL
text_buffer_t *
text_buffer_new(float w, float h, dma2d_pixel_buffer_t *render_surface) {
#else
text_buffer_t *
text_buffer_new(float w, float h) {
#endif
	text_buffer_t *self = (text_buffer_t *) malloc (sizeof(text_buffer_t));
#ifdef FREETYPE_GL_NOGL
	self->render_surface = render_surface;
#else
	self->buffer = vertex_buffer_new( "vertex:3f,tex_coord:2f,color:4f,ashift:1f,agamma:1f" );
#endif
#ifdef FREETYPE_GL_COLOR_ARGB
	self->base_color = (ftgl_color_t){0xff000000};
#else
	self->base_color.r = 0.0;
	self->base_color.g = 0.0;
	self->base_color.b = 0.0;
	self->base_color.a = 1.0;
#endif

	layout_init(&self->layout, w, h);
	return self;
}

// ----------------------------------------------------------------------------
void text_buffer_delete( text_buffer_t * self ) {
//	vector_delete( self->lines );
#ifndef FREETYPE_GL_NOGL
	vertex_buffer_delete( self->buffer );
#endif
	free( self );
}

// ----------------------------------------------------------------------------
void text_buffer_clear( text_buffer_t * self ) {
	assert( self );

#ifndef FREETYPE_GL_NOGL
	vertex_buffer_clear( self->buffer );
#endif
	layout_clear( &self->layout );
}

static inline void layout_to_vertex_buffer(text_buffer_t * self, vec2 *pen) {
	vec2 pen_orig = *pen;
	for (size_t e=0; e<self->layout.element_count; e++) {
		block_element_t *el = &self->layout.elements[e];
		assert(self->layout.elements[e].type==BET_TEXT);
		printf("X po:%5.1f par:%5.1f el:%5.1f\n",pen_orig.x, el->paragraph->x, el->x);
		pen->x = pen_orig.x + el->paragraph->x + el->x;
		pen->y = pen_orig.y + (el->paragraph->y + el->y + self->layout.paragraphs[0].ascender);
		for (uint32_t c=0; c<el->text.glyph_count; c++) {
			text_buffer_add_char(self, pen, el->text.markup, &el->text.glyph_info[c],&el->text.glyph_pos[c]);
		}
	}
}

vec4
text_buffer_get_bounds( text_buffer_t * self, vec2 * pen ) {
	(void)self; (void)pen;
	return (vec4){{10,10,100,100}}; // TODO do :)
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

// ----------------------------------------------------------------------------
void text_buffer_add_char( text_buffer_t *self,
					  vec2 *pen,
					  const markup_t *markup,
					  const hb_glyph_info_t     *glyph_info,
					  const hb_glyph_position_t *glyph_pos
) {
	texture_font_t *font = markup->font;
#ifdef FREETYPE_GL_NOGL
#else
	size_t vcount = 0;
	size_t icount = 0;
	vertex_buffer_t *buffer = self->buffer;
	float gamma = markup->gamma;

	// Maximum number of vertices is 20 (= 5x2 triangles) per glyph:
	//  - 2 triangles for background
	//  - 2 triangles for overline
	//  - 2 triangles for underline
	//  - 2 triangles for strikethrough
	//  - 2 triangles for glyph
	glyph_vertex_t vertices[4*5];
	GLuint indices[6*5];
	texture_glyph_t *black;
	black = texture_font_get_glyph( font, NULL );
#endif
	texture_glyph_t *glyph;

	glyph = texture_font_get_glyph32( font, glyph_info->codepoint );

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

//	int codepoint = glyph_info->codepoint;
	// because of vhinting trick we need the extra 64 (hres)
	float x_advance = glyph_pos->x_advance/(float)(hres*64);
	float x_offset  = glyph_pos->x_offset/(float)(hres*64);
	float y_advance = glyph_pos->y_advance/(float)(64);
	float y_offset  = glyph_pos->y_offset/(float)(64);


#if defined(FREETYPE_GL_NOGL)
#ifndef FREETYPE_GL_COLOR_ARGB
#error "NOGL only works together with ARGB"
#endif

	// Background
	if ( markup->background_color.argb8888.a > 0 ) {
		dma2d_fill(
				self->render_surface,
				markup->background_color.argb8888.c,
				(int16_t)lroundf(pen->x), (int16_t)lroundf(pen->y - font->ascender),
				(int16_t)ceil(x_advance), (int16_t)ceil(font->ascender - font->descender)
			);
	}

	/* Actual glyph */
	{
		static dma2d_pixel_buffer_t atlas_surface = {0};
		if (atlas_surface.buffer != markup->font->atlas->data) {
			atlas_surface = (dma2d_pixel_buffer_t) {
				.buffer = markup->font->atlas->data,
				.width  = markup->font->atlas->width,
				.height = markup->font->atlas->height,
				.in = {
					.pixel = {
							.bitsize = 8,
							.format  = DMA2D_xPFCCR_CM_A8,
							.alpha_mode = {
									.color = markup->foreground_color.argb8888.c,
							},
					},
				},
				.out = {{0}},// out is not supported!
			};
		} else {
			atlas_surface.in.pixel.alpha_mode.color = markup->foreground_color.argb8888.c;
		}
		dma2d_convert_blenddst__no_pxsrc_fix(
//		dma2d_convert_copy__no_pxsrc_fix(
				&atlas_surface, self->render_surface,
				(int16_t)glyph->tex_region.x, (int16_t)glyph->tex_region.y,
				(int16_t)lroundf(pen->x + (x_offset + glyph->offset_x)),
				(int16_t)lroundf(pen->y - (y_offset + glyph->offset_y)),
				(int16_t)glyph->tex_region.width, (int16_t)glyph->tex_region.height
			);

	}

	// Outline
	if ( markup->outline ) {
		int16_t left,right,top,bottom,height,width,underline_thickness;
//		left   = (int16_t)lroundf(pen->x);
//		top    = (int16_t)lroundf(pen->y - font->ascender  - font->underline_thickness);
//		height = (int16_t)ceil(font->ascender - font->descender + 2*font->underline_thickness);
//		width  = (int16_t)ceil(x_advance);
//		underline_thickness = (int16_t)ceil(font->underline_thickness);
//		bottom = top+height-underline_thickness;
//		right  = left+width-underline_thickness;
//		left  -= underline_thickness;
//		top   -= underline_thickness;
		underline_thickness = 1; //(int16_t)lroundf(font->underline_thickness);
		left  = (int16_t)lroundf(pen->x + (x_offset + glyph->offset_x));
		width = (int16_t)glyph->tex_region.width;
		right = left + width;
		left -= underline_thickness;
		width += 2*underline_thickness;
		top   = (int16_t)lroundf(pen->y - (y_offset + glyph->offset_y));
		height = (int16_t)glyph->tex_region.height;
		bottom = top + height;
		top   -= underline_thickness;
		height += 2*underline_thickness;

		// top
		dma2d_fill(
				self->render_surface,
				markup->outline_color.argb8888.c,
				left, top, width, underline_thickness
			);
		// bottom
		dma2d_fill(
				self->render_surface,
				markup->outline_color.argb8888.c,
				left, bottom, width, underline_thickness
			);
		// left
		dma2d_fill(
				self->render_surface,
				markup->outline_color.argb8888.c,
				left, top, underline_thickness, height
			);
		// right
		dma2d_fill(
				self->render_surface,
				markup->outline_color.argb8888.c,
				right, top, underline_thickness, height
			);
	}

	// Underline
	if ( markup->underline ) {
		dma2d_fill(
				self->render_surface,
				markup->underline_color.argb8888.c,
				(int16_t)lroundf(pen->x), (int16_t)lroundf(pen->y - font->underline_position),
				(int16_t)ceil(x_advance), (int16_t)ceil(font->underline_thickness)
			);
	}

	// Overline
	if ( markup->overline ) {
		dma2d_fill(
				self->render_surface,
				markup->overline_color.argb8888.c,
				(int16_t)lroundf(pen->x), (int16_t)lroundf(pen->y - font->ascender - font->underline_thickness),
				(int16_t)ceil(x_advance), (int16_t)ceil(font->underline_thickness)
			);
	}

	/* Strikethrough */
	if ( markup->strikethrough ) {
		dma2d_fill(
				self->render_surface,
				markup->overline_color.argb8888.c,
				(int16_t)lroundf(pen->x), (int16_t)lroundf(pen->y - font->ascender / 3),
				(int16_t)ceil(x_advance), (int16_t)ceil(font->underline_thickness)
			);
	}

//	pen->x += glyph->advance_x * (1.0f + markup->spacing);
	pen->x += x_advance * (1.0f + markup->spacing);
	pen->y += y_advance * (1.0f + markup->spacing);

#else
#ifdef FREETYPE_GL_COLOR_ARGB
#error "ARGB not supported in OpenGL mode"
#endif

//	float kerning = 0.0f;

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
#endif

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
