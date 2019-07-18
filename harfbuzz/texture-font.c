/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H
#include FT_STROKER_H
// #include FT_ADVANCES_H
#include FT_LCD_FILTER_H
#include FT_TRUETYPE_TABLES_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include "distance-field.h"
#include "texture-font.h"
#include "platform.h"
#include "utf8-utils.h"
#include "freetype-gl-err.h"

#define HRES  100
#define DPI   72

#undef __FTERRORS_H__
#define FT_ERRORDEF( e, v, s )  { e, s },
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       { 0, 0 } };
const struct {
	int          code;
	const char*  message;
} FT_Errors[] =
#include FT_ERRORS_H

// per-thread library

__THREAD texture_font_library_t * freetype_gl_library = NULL;
__THREAD font_mode_t mode_default=MODE_AUTO_CLOSE;

// ------------------------------------------------------ texture_glyph_new ---
texture_glyph_t *
texture_glyph_new(void) {
	texture_glyph_t *self = (texture_glyph_t *) malloc( sizeof(texture_glyph_t) );
	if (self == NULL) {
		freetype_gl_error( Out_Of_Memory,
			   "%s:%d: No more memory for allocating data\n", __FILENAME__, __LINE__);
		return NULL;
	}

	self->codepoint = -1;
	self->width     = 0;
	self->height    = 0;
	/* Attributes that can have different images for the same codepoint */
	self->rendermode = RENDER_NORMAL;
	self->outline_thickness = 0.0;
	self->glyphmode = GLYPH_END;
	/* End of attribute part */
	self->offset_x  = 0;
	self->offset_y  = 0;
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
	self->s0        = 0.0;
	self->t0        = 0.0;
	self->s1        = 0.0;
	self->t1        = 0.0;
#else
	self->tex_region = (ivec4) {0};
#endif
	return self;
}

// ---------------------------------------------- texture_font_default_mode ---
void
texture_font_default_mode(font_mode_t mode) {
	mode_default=mode;
}

// --------------------------------------------------- texture_glyph_delete ---
void
texture_glyph_delete( texture_glyph_t *self ) {
	assert( self );
	free( self );
}

// -------------------------------------------------- texture_is_color_font ---

int
texture_is_color_font( texture_font_t *self) {
	static const uint32_t tag = FT_MAKE_TAG('C', 'B', 'D', 'T');
	unsigned long length = 0;
	FT_Load_Sfnt_Table(self->face, tag, 0, NULL, &length);
	return length != 0;
}

// -------------------------------------------------- texture_font_set_size ---

static
int
texture_font_set_size ( texture_font_t *self, float size ) {
	FT_Error error=0;
	FT_Matrix matrix = {
		(int)((1.0/self->hres) * 0x10000L),
		(int)((0.0)      * 0x10000L),
		(int)((0.0)      * 0x10000L),
		(int)((1.0)      * 0x10000L)};

	if ( texture_is_color_font( self ) ) {
		/* Select best size */
		if (self->face->num_fixed_sizes == 0) {
			freetype_error( error, "FT_Error (%s:%d) : no fixed size in color font\n",
					__FILENAME__, __LINE__);
			return 0;
		}

		int best_match = 0;
		int diff = abs((int)size - self->face->available_sizes[0].width);
		int i;
	
		for (i = 1; i < self->face->num_fixed_sizes; ++i) {
			int ndiff = abs((int)size - self->face->available_sizes[i].width);
			if (ndiff < diff) {
				best_match = i;
				diff = ndiff;
			}
		}
		error = FT_Select_Size(self->face, best_match);
		if (error) {
			freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
					__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
			return 0;
		}
#if defined(TEXTURE_FONT_ENABLE_SCALING)
		self->scale = pt_size / self->face->available_sizes[best_match].width;
#else
		size = self->face->available_sizes[best_match].width;
		self->pt_size = size / self->hres;
#endif
	} else {
		/* Set char size */
		self->pt_size = size / self->hres;
//		self->size = size;
		error = FT_Set_Char_Size(self->face, (int)(size * 64), 0, DPI * self->hres, DPI);

		if (error) {
			freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
					__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
			return 0;
		}
	}
	/* Set transform matrix */
	FT_Set_Transform(self->face, &matrix, NULL);

	return 1;
}

// --------------------------------------------------

void
texture_font_init_size( texture_font_t * self) {
	FT_Size_Metrics metrics;
	
	self->underline_position = self->face->underline_position / (float)(self->hres*self->hres) * self->pt_size;
	self->underline_position = roundf( self->underline_position );
	if ( self->underline_position > -2 ) {
		self->underline_position = -2.0;
	}

	self->underline_thickness = self->face->underline_thickness / (float)(self->hres*self->hres) * self->pt_size;
	self->underline_thickness = roundf( self->underline_thickness );
	if ( self->underline_thickness < 1 ) {
		self->underline_thickness = 1.0;
	}

	metrics = self->face->size->metrics;
	self->ascender = (float)(metrics.ascender >> 6) / self->hres; //100.f;
	self->descender = (float)(metrics.descender >> 6) / self->hres; //100.f;
	self->height = (float)(metrics.height >> 6) / self->hres; //100.f;
	self->linegap = self->height - self->ascender + self->descender;
}

// ------------------------------------------------------ texture_font_init ---
static int
texture_font_init(texture_font_t *self) {
	assert(self->atlas);
	assert(self->pt_size > 0);
	assert((self->location == TEXTURE_FONT_FILE && self->filename)
		|| (self->location == TEXTURE_FONT_MEMORY
			&& self->memory.base && self->memory.size));

	self->glyphs = vector_new(sizeof(texture_glyph_t *));
	self->height = 0;
	self->ascender = 0;
	self->descender = 0;
	self->rendermode = RENDER_NORMAL;
	self->outline_thickness = 0.0;
	self->hres = HRES;
	self->hinting = 1;
	self->filtering = 1;
#if defined(TEXTURE_FONT_ENABLE_SCALING)
	self->scaletex = 1;
#endif
#if defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
	self->scale = 1.0;
#endif
	self->face = NULL;
	self->hb_ft_font = 0;

	// FT_LCD_FILTER_LIGHT   is (0x00, 0x55, 0x56, 0x55, 0x00)
	// FT_LCD_FILTER_DEFAULT is (0x10, 0x40, 0x70, 0x40, 0x10)
	self->lcd_weights[0] = 0x10;
	self->lcd_weights[1] = 0x40;
	self->lcd_weights[2] = 0x70;
	self->lcd_weights[3] = 0x40;
	self->lcd_weights[4] = 0x10;

	if (!texture_font_load_face(self, self->pt_size * self->hres)) {
		return -1;
	}

	texture_font_init_size( self );

	if (!texture_font_set_size(self, self->pt_size)) {
		return -1;
	}
	
	/* Set harfbuzz font */
	self->hb_ft_font = hb_ft_font_create( self->face, NULL );

	/* NULL is a special glyph */
	texture_font_get_glyph( self, NULL );

	return 0;
}

// ---------------------------------------------------- texture_library_new ---
texture_font_library_t *
texture_library_new() {
	texture_font_library_t *self = calloc(1, sizeof(*self));
	
	self->mode = MODE_ALWAYS_OPEN;
	
	return self;
}

// --------------------------------------------- texture_font_new_from_file ---
texture_font_t *
texture_font_new_from_file(texture_atlas_t *atlas, const float pt_size,
		const char *filename, const char *language) {
	texture_font_t *self;

	assert(filename);

	self = calloc(1, sizeof(*self));
	if (!self) {
		freetype_gl_error( Out_Of_Memory,
			   "%s:%d: No more memory for allocating data\n", __FILENAME__, __LINE__);
		return NULL;
	}

	self->atlas = atlas;
	self->pt_size  = pt_size;

	self->location = TEXTURE_FONT_FILE;
	self->filename = strdup(filename);
	self->mode = mode_default;

	self->language = hb_language_from_string(language,strnlen(language,64));

	if (texture_font_init(self)) {
		texture_font_delete(self);
		return NULL;
	}

	return self;
}

// ------------------------------------------- texture_font_new_from_memory ---
texture_font_t *
texture_font_new_from_memory(texture_atlas_t *atlas, float pt_size,
		const void *memory_base, size_t memory_size, const char *language) {
	texture_font_t *self;

	assert(memory_base);
	assert(memory_size);

	self = calloc(1, sizeof(*self));
	if (!self) {
		freetype_gl_error( Out_Of_Memory,
			   "line %d: No more memory for allocating data\n", __LINE__);
		return NULL;
	}

	self->atlas = atlas;
	self->pt_size = pt_size;

	self->location = TEXTURE_FONT_MEMORY;
	self->memory.base = memory_base;
	self->memory.size = memory_size;
	self->mode = mode_default;

	self->language = hb_language_from_string(language,strnlen(language,15));

	if (texture_font_init(self)) {
		texture_font_delete(self);
		return NULL;
	}

	return self;
}

// ----------------------------------------------------- texture_font_clone ---
texture_font_t *
texture_font_clone( texture_font_t *old, float pt_size) {
	texture_font_t *self;
//	FT_Error error = 0;
	
	self = calloc(1, sizeof(*self));
	if (!self) {
		freetype_gl_error( Out_Of_Memory,
			   "line %d: No more memory for allocating data\n", __LINE__);
		return NULL;
	}

	memcpy(self, old, sizeof(*self));

	self->glyphs = NULL;
	self->pt_size = pt_size;
	texture_font_init(self);


//	self->glyphs = vector_new(sizeof(texture_glyph_t *));
//
////	error = FT_New_Size( self->face, &self->ft_size );
////	if (error) {
////		freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
////				__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
////		return NULL;
////	}
////
////	error = FT_Activate_Size( self->ft_size );
////	if (error) {
////		freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
////				__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
////		return NULL;
////	}
//
//	self->pt_size = pt_size;
//
//	if (!texture_font_load_face(self, self->pt_size * self->hres)) {
//		return -1;
//	}
//
//	texture_font_init_size( self );
//
//	if (!texture_font_set_size ( self, self->pt_size )) {
//		return NULL;
//	}

	return self;
}
// ----------------------------------------------------- texture_font_close ---

void
texture_font_close( texture_font_t *self, font_mode_t face_mode, font_mode_t library_mode ) {
	(void)library_mode;
	if (self->buffer && self->mode <= face_mode ) {
		hb_buffer_destroy(self->buffer);
		self->buffer = NULL;
	}

//	if ( self->face && self->mode <= face_mode ) {
//		FT_Done_Face( self->face );
//		self->face = NULL;
//	} else {
		return; // never close the library when the face stays open
//	}
//
//	if ( self->library && self->library->library && self->library->mode <= library_mode ) {
//		FT_Done_FreeType( self->library->library );
//		self->library->library = NULL;
//	}
}

// ------------------------------------------------- texture_font_load_face ---

int
texture_font_load_face( texture_font_t *self, float size ) {
	FT_Error error;

	if ( !self->library ) {
		if ( !freetype_gl_library ) {
			freetype_gl_library = texture_library_new();
		}
		self->library = freetype_gl_library;
	}
	
	if ( !self->library->library ) {
		error = FT_Init_FreeType( &self->library->library );
		if (error) {
			freetype_error(error, "FT_Error (0x%02x) : %s\n",
				   FT_Errors[error].code, FT_Errors[error].message);
			goto cleanup;
		}
	}
	
	if ( !self->buffer ) {
		self->buffer = hb_buffer_create();
	} else {
		/* clean up the buffer, but don't kill it just yet */
		hb_buffer_reset(self->buffer);
	}

	if ( !self->face ) {
	switch (self->location) {
		case TEXTURE_FONT_FILE:
			error = FT_New_Face(self->library->library, self->filename, 0, &self->face);
			if (error) {
				freetype_error( error, "FT_Error, file %s (%s:%d, code 0x%02x) : %s\n",
						self->filename, __FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
				goto cleanup_library;
			}
			break;

		case TEXTURE_FONT_MEMORY:
			error = FT_New_Memory_Face(self->library->library,
						self->memory.base, self->memory.size, 0, &self->face);
			if (error) {
				freetype_error( error, "FT_Error memory %p:%x (%s:%d, code 0x%02x) : %s\n",
						self->memory.base, self->memory.size,
						__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
				goto cleanup_library;
			}
			break;
	}

	/* Select charmap */
	error = FT_Select_Charmap(self->face, FT_ENCODING_UNICODE);
	if (error) {
		freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
				__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
		goto cleanup_face;
	}

	error = FT_New_Size( self->face, &self->ft_size );
	if (error) {
		freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
				__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
		goto cleanup_face;
	}

	error = FT_Activate_Size( self->ft_size );
	if (error) {
		freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
				__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
		goto cleanup_face;
	}
	
	if (!texture_font_set_size ( self, size ))
		goto cleanup_face;
	}
	
	return 1;
	
cleanup_face:
	texture_font_close( self, MODE_ALWAYS_OPEN, MODE_FREE_CLOSE );
	return 0;
cleanup_library:
	texture_font_close( self, MODE_ALWAYS_OPEN, MODE_ALWAYS_OPEN );
cleanup:
	return 0;
}

// ---------------------------------------------------- texture_font_delete ---
void
texture_font_delete( texture_font_t *self ) {
	size_t i;
	texture_glyph_t *glyph;
	FT_Error error=0;

	assert( self );

	error = FT_Done_Size( self->ft_size );
	if (error) {
		freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
			__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
	}

	texture_font_close( self, MODE_ALWAYS_OPEN, MODE_FREE_CLOSE );

	if (self->location == TEXTURE_FONT_FILE && self->filename) free( self->filename );

	GLYPHS_ITERATOR(i, glyph, self->glyphs) {
		//if ((__i | i << 8) == glyph->codepoint) {
			texture_glyph_delete( glyph );
		//}
	} GLYPHS_ITERATOR_END1
	free( __glyphs );
	GLYPHS_ITERATOR_END2

	vector_delete( self->glyphs );

	FT_Done_Face( self->face );
	hb_font_destroy( self->hb_ft_font );
	
	/* Cleanup */
	hb_buffer_destroy( self->buffer );

	free( self );
}

texture_glyph_t *
texture_font_find_glyph( texture_font_t * self, const char * codepoint ) {
	return texture_font_find_glyph32(self, codepoint ? utf8_to_utf32( codepoint ) : UINT32_MAX);
}
texture_glyph_t *
texture_font_find_glyph32( texture_font_t * self,
						 uint32_t codepoint ) {
	uint32_t i = codepoint >> 8;
	uint32_t j = codepoint & 0xFF;
	texture_glyph_t **glyph_index1, *glyph;

	if (codepoint == UINT32_MAX) {
		return (texture_glyph_t *)self->atlas->special;
	}

	if (self->glyphs->size <= i) {
		return NULL;
	}

	glyph_index1 = *(texture_glyph_t ***) vector_get( self->glyphs, i );

	if (!glyph_index1) {
		return NULL;
	} else {
		glyph = glyph_index1[j];
	}

	while ( glyph && // if no glyph is there, we are done here
			(glyph->rendermode != self->rendermode ||
			glyph->outline_thickness != self->outline_thickness) ) {
		if ( glyph->glyphmode != GLYPH_CONT) return NULL;
		glyph++;
	}
	return glyph;
}

int
texture_font_index_glyph32( texture_font_t * self,
			   	texture_glyph_t *glyph,
			   	uint32_t codepoint) {
	uint32_t i = codepoint >> 8;
	uint32_t j = codepoint & 0xFF;
	texture_glyph_t ***glyph_index1, *glyph_insert;

	if (self->glyphs->size <= i) {
	vector_resize( self->glyphs, i+1);
	}

	glyph_index1 = (texture_glyph_t ***) vector_get( self->glyphs, i );

	if (!*glyph_index1) {
	*glyph_index1 = calloc( 0x100, sizeof(texture_glyph_t*) );
	}

	if (( glyph_insert = (*glyph_index1)[j] )) {
		int c = 0;
		// fprintf(stderr, "glyph already there\n");
		while (glyph_insert[c].glyphmode != GLYPH_END) c++;
		// fprintf(stderr, "Insert a glyph after position %d\n", i);
		glyph_insert[c].glyphmode = GLYPH_CONT;
		(*glyph_index1)[j] = glyph_insert = realloc( glyph_insert, sizeof(texture_glyph_t)*(c+2) );
		memcpy( glyph_insert+(c+1), glyph, sizeof(texture_glyph_t) );
		return 1;
	} else {
	(*glyph_index1)[j] = glyph;
		return 0;
	}
}

// ------------------------------------------------ texture_font_load_glyph ---
int
texture_font_load_glyph( texture_font_t * self,
						 const char * codepoint
) {
	return !texture_font_load_glyphs(self, codepoint);
}

// ----------------------------------------------- texture_font_load_glyphs ---

size_t
texture_font_load_glyphs( texture_font_t * self,
						  const char * codepoints) {
	//size_t i, x, y, w, h; //width, height, depth,
	size_t i, x, y, h;

	FT_Error error;
	FT_Glyph ft_glyph;
	FT_GlyphSlot slot;
	FT_Bitmap ft_bitmap;

	unsigned int glyph_count;
	//FT_UInt glyph_index;
	texture_glyph_t *glyph;
	FT_Int32 flags = 0;
	int ft_glyph_top = 0;
	int ft_glyph_left = 0;

//	hb_buffer_t *buffer;
	hb_glyph_info_t *glyph_info;

	ivec4 region;

	assert( self );
	//assert( codepoints );

	/* codepoint NULL is special : it is used for line drawing (overline,
	 * underline, strikethrough) and background.
	 */
	if ( !codepoints ) {
		return 0;
	}

	if (!texture_font_load_face(self, self->pt_size)) {
		return utf8_strlen( codepoints );
	}

//	width  = self->atlas->width;
//	height = self->atlas->height;
//	depth  = self->atlas->depth;

	hb_buffer_set_language( self->buffer, self->language );
//	hb_buffer_set_script( self->buffer, self->script );
//	hb_buffer_set_direction( self->buffer, self->direction );

	/* Layout the text */
	hb_buffer_add_utf8( self->buffer, codepoints, strlen(codepoints), 0, strlen(codepoints) );
	/* Guess text script and direction */
	hb_buffer_guess_segment_properties( self->buffer ); /* change me! */
	hb_shape( self->hb_ft_font, self->buffer, NULL, 0 );

	glyph_info = hb_buffer_get_glyph_infos(self->buffer, &glyph_count);

	for ( i = 0; i < glyph_count; ++i ) {
		/* Check if codepoint has been already loaded */
		if ( texture_font_find_glyph32( self, glyph_info[i].codepoint ) ) {
			continue;
		}

		flags = 0;
		ft_glyph_top = 0;
		ft_glyph_left = 0;
		// WARNING: We use texture-atlas depth to guess if user wants
		//          LCD subpixel rendering

		if ( self->rendermode != RENDER_NORMAL && self->rendermode != RENDER_SIGNED_DISTANCE_FIELD ) {
			flags |= FT_LOAD_NO_BITMAP;
		} else {
			flags |= FT_LOAD_RENDER;
		}

		if ( !self->hinting ) {
			flags |= FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;
		} else {
			flags |= FT_LOAD_FORCE_AUTOHINT;
		}

		if ( self->atlas->depth == 3 ) {
			FT_Library_SetLcdFilter( self->library->library, FT_LCD_FILTER_LIGHT );
			flags |= FT_LOAD_TARGET_LCD;

			if ( self->filtering ) {
				FT_Library_SetLcdFilterWeights( self->library->library, self->lcd_weights );
			}
		}

		if ( self->atlas->depth == 4 ) {
#ifdef FT_LOAD_COLOR
		flags |= FT_LOAD_COLOR;
#else
		freetype_error( 0, "FT_Error (%s:%d, code 0x%02x) : %s\n",
					__FILENAME__, __LINE__, 0, "FT_LOAD_COLOR not available");
#endif
		}

		error = FT_Activate_Size( self->ft_size );
		if (error) {
			freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
					__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
			return glyph_count - i;
		}

		error = FT_Load_Glyph( self->face, glyph_info[i].codepoint, flags );
		if ( error ) {
			freetype_error( error, "FT_Error (%s:%d, code 0x%02x) : %s\n",
				__FILENAME__, __LINE__, FT_Errors[error].code, FT_Errors[error].message);
			texture_font_close( self, MODE_AUTO_CLOSE, MODE_AUTO_CLOSE );
			//return 0;
			return glyph_count - i;
		}

		if ( self->rendermode == RENDER_NORMAL || self->rendermode == RENDER_SIGNED_DISTANCE_FIELD ) {
			slot            = self->face->glyph;
			ft_bitmap       = slot->bitmap;
			ft_glyph_top    = slot->bitmap_top;
			ft_glyph_left   = slot->bitmap_left;
		} else {
			FT_Stroker stroker;
			FT_BitmapGlyph ft_bitmap_glyph;

			error = FT_Stroker_New( self->library->library, &stroker );

			if ( error ) {
			freetype_error( error, "FT_Error (0x%02x) : %s\n",
				FT_Errors[error].code, FT_Errors[error].message);
			goto cleanup_stroker;
			}

			FT_Stroker_Set(stroker,
							(int)(self->outline_thickness * self->hres),
							FT_STROKER_LINECAP_ROUND,
							FT_STROKER_LINEJOIN_ROUND,
							0);

			error = FT_Get_Glyph( self->face->glyph, &ft_glyph);

			if ( error ) {
				freetype_error( error, "FT_Error (0x%02x) : %s\n",
					FT_Errors[error].code, FT_Errors[error].message);
				goto cleanup_stroker;
			}

			if ( self->rendermode == RENDER_OUTLINE_EDGE )
				error = FT_Glyph_Stroke( &ft_glyph, stroker, 1 );
			else
			if ( self->rendermode == RENDER_OUTLINE_POSITIVE )
				error = FT_Glyph_StrokeBorder( &ft_glyph, stroker, 0, 1 );
			else
			if ( self->rendermode == RENDER_OUTLINE_NEGATIVE )
				error = FT_Glyph_StrokeBorder( &ft_glyph, stroker, 1, 1 );

			if ( error ) {
				freetype_error( error, "FT_Error (0x%02x) : %s\n",
					FT_Errors[error].code, FT_Errors[error].message);
				goto cleanup_stroker;
			}

			switch( self->atlas->depth ) {
				case 1:
					error = FT_Glyph_To_Bitmap( &ft_glyph, FT_RENDER_MODE_NORMAL, 0, 1);
					break;
				case 3:
					error = FT_Glyph_To_Bitmap( &ft_glyph, FT_RENDER_MODE_LCD, 0, 1);
					break;
				case 4:
					error = FT_Glyph_To_Bitmap( &ft_glyph, FT_RENDER_MODE_NORMAL, 0, 1);
					break;
			}

			if ( error ) {
				freetype_error( error, "FT_Error (0x%02x) : %s\n",
						FT_Errors[error].code, FT_Errors[error].message);
				goto cleanup_stroker;
			}

			ft_bitmap_glyph = (FT_BitmapGlyph) ft_glyph;
			ft_bitmap       = ft_bitmap_glyph->bitmap;
			ft_glyph_top    = ft_bitmap_glyph->top;
			ft_glyph_left   = ft_bitmap_glyph->left;

cleanup_stroker:
			FT_Stroker_Done(stroker);

			if ( error ) {
				texture_font_close( self, MODE_AUTO_CLOSE, MODE_AUTO_CLOSE );
				//return 0;
				return glyph_count - i;
			}
		}

		struct {
			int left;
			int top;
			int right;
			int bottom;
		} padding = { 0, 0, 1, 1 };

		if ( self->rendermode == RENDER_SIGNED_DISTANCE_FIELD ) {
			padding.top = 1;
			padding.left = 1;
		}

		size_t src_w = self->atlas->depth == 3 ? ft_bitmap.width/3 : ft_bitmap.width;
		size_t src_h = ft_bitmap.rows;

		size_t tgt_w = src_w + padding.left + padding.right;
		size_t tgt_h = src_h + padding.top + padding.bottom;

		region = texture_atlas_get_region( self->atlas, tgt_w, tgt_h );
//		printf("Region { %d,%d,%zu,%zu,%zu,%zu,%d }\n",region.x,region.y,tgt_w,tgt_h,src_w,src_h,ft_bitmap.width);

		if ( region.x < 0 ) {
			freetype_gl_error( Texture_Atlas_Full,
							   "Texture atlas is full, asked for %i*%i (%s:%d)\n",
							   tgt_w, tgt_h,
							   __FILENAME__, __LINE__ );
			texture_font_close( self, MODE_AUTO_CLOSE, MODE_AUTO_CLOSE );
			return glyph_count - i; //return 0;
		}

		x = region.x;
		y = region.y;

		unsigned char *buffer = calloc( tgt_w * tgt_h * self->atlas->depth, sizeof(unsigned char) );

		unsigned char *dst_ptr = buffer + (padding.top * tgt_w + padding.left) * self->atlas->depth;
		unsigned char *src_ptr = ft_bitmap.buffer;
		for ( h = 0; h < src_h; h++ ) {
			//difference between width and pitch: https://www.freetype.org/freetype2/docs/reference/ft2-basic_types.html#FT_Bitmap
			memcpy( dst_ptr, src_ptr, ft_bitmap.width << (self->atlas->depth == 4 ? 2 : 0));
			dst_ptr += tgt_w * self->atlas->depth;
			src_ptr += ft_bitmap.pitch;
		}

		if ( self->rendermode == RENDER_SIGNED_DISTANCE_FIELD ) {
			unsigned char *sdf = make_distance_mapb( buffer, tgt_w, tgt_h );
			free( buffer );
			buffer = sdf;
		}

		texture_atlas_set_region( self->atlas, x, y, tgt_w, tgt_h, buffer, tgt_w * self->atlas->depth);

		free( buffer );

		glyph = texture_glyph_new( );
		glyph->codepoint = glyph_info[i].codepoint;

#ifdef TEXTURE_FONT_ENABLE_SCALING
		glyph->width    = tgt_w * self->scale;
		glyph->height   = tgt_h * self->scale;
		glyph->offset_x = ft_glyph_left * self->scale;
		glyph->offset_y = ft_glyph_top * self->scale;
#else
		glyph->width    = tgt_w;
		glyph->height   = tgt_h;
		glyph->offset_x = ft_glyph_left;
		glyph->offset_y = ft_glyph_top;
#endif
		glyph->rendermode = self->rendermode;
		glyph->outline_thickness = self->outline_thickness;
#ifdef TEXTURE_FONT_ENABLE_TEXTURE_COORDINATE_SCALING
		if (self->scaletex) {
			glyph->s0       = x/(float)self->atlas->width;
			glyph->t0       = y/(float)self->atlas->height;
			glyph->s1       = (x + glyph->width)/(float)self->atlas->width;
			glyph->t1       = (y + glyph->height)/(float)self->atlas->height;
		} else {
			// fix up unscaled coordinates by subtracting 0.5
			// this avoids drawing pixels from neighboring characters
			// note that you also have to paint these glyphs with an offset of
			// half a pixel each to get crisp rendering
			glyph->s0       = x - 0.5;
			glyph->t0       = y - 0.5;
			glyph->s1       = x + tgt_w - 0.5;
			glyph->t1       = y + tgt_h - 0.5;
		}
#else
		glyph->tex_region = region;
#endif
		/*
		slot = self->face->glyph;
		if ( self->atlas->depth == 4 ) {
			// color fonts use actual pixels, not subpixels
			glyph->advance_x = slot->advance.x * self->scale;
			glyph->advance_y = slot->advance.y * self->scale;
		} else {
			glyph->advance_x = slot->advance.x * self->scale / self->hres;
			glyph->advance_y = slot->advance.y * self->scale / self->hres;
		}
*/
		int free_glyph = texture_font_index_glyph32(self, glyph, glyph_info[i].codepoint);
		if (!glyph_info[i].codepoint) { //if (!glyph_index) {
			if (!free_glyph) {
				texture_glyph_t *new_glyph = malloc(sizeof(texture_glyph_t));
				memcpy(new_glyph, glyph, sizeof(texture_glyph_t));
				glyph=new_glyph;
			}
			free_glyph = texture_font_index_glyph32(self, glyph, 0);
		}
		if (free_glyph) {
			// fprintf(stderr, "Free glyph\n");
			free(glyph);
		}

		if ( self->rendermode != RENDER_NORMAL && self->rendermode != RENDER_SIGNED_DISTANCE_FIELD ) {
			FT_Done_Glyph( ft_glyph );
		}
	}

	texture_font_close( self, MODE_AUTO_CLOSE, MODE_AUTO_CLOSE );

	return 0;
}

// ------------------------------------------------- texture_font_get_glyph ---
texture_glyph_t *
texture_font_get_glyph( texture_font_t * self, const char * codepoint ) {
	return texture_font_get_glyph32(self, codepoint ? utf8_to_utf32( codepoint ) : UINT32_MAX );
}
texture_glyph_t *
texture_font_get_glyph32( texture_font_t * self, uint32_t codepoint ) {
	texture_glyph_t *glyph;

	assert( self );
	assert( self->filename );
	assert( self->atlas );

	/* Check if codepoint has been already loaded */
	if ( (glyph = texture_font_find_glyph32( self, codepoint )) ) {
		return glyph;
	}

	/* Glyph has not been already loaded */
	char utf8_buf[6];
	if ( texture_font_load_glyph( self, utf32_to_utf8(codepoint,utf8_buf) ) ) {
		return texture_font_find_glyph32(self, codepoint );
	}

	return NULL;
}

// ------------------------------------------  texture_font_enlarge_texture ---
void
texture_font_enlarge_texture( texture_font_t * self, size_t width_new,
			  	size_t height_new) {
	assert(self);
	assert(self->atlas);
	//ensure size increased
	assert(width_new >= self->atlas->width);
	assert(height_new >= self->atlas->height);
	assert(width_new + height_new > self->atlas->width + self->atlas->height);    
	texture_atlas_t* ta = self->atlas;
	size_t width_old = ta->width;
	size_t height_old = ta->height;    
	//allocate new buffer
	unsigned char* data_old = ta->data;
	ta->data = calloc(1,width_new*height_new * sizeof(char)*ta->depth);    
	//update atlas size
	ta->width = width_new;
	ta->height = height_new;
	//add node reflecting the gained space on the right
	if (width_new>width_old){
		ivec3 node;
		node.x = width_old - 1;
		node.y = 1;
		node.z = width_new - width_old;
		vector_push_back(ta->nodes, &node);    
	}
	//copy over data from the old buffer, skipping first row and column because of the margin
	size_t pixel_size = sizeof(char) * ta->depth;
	size_t old_row_size = width_old * pixel_size;
	texture_atlas_set_region(ta, 1, 1, width_old - 2, height_old - 2, data_old + old_row_size + pixel_size, old_row_size);
	free(data_old);    
}
#if defined(TEXTURE_FONT_ENABLE_SCALING) && defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
// -------------------------------------------- texture_font_enlarge_atlas ---
void
texture_font_enlarge_glyphs( texture_font_t * self, float mulw, float mulh) {
	size_t i;
	texture_glyph_t* g;
	GLYPHS_ITERATOR(i, g, self->glyphs) {
		g->s0 *= mulw;
		g->s1 *= mulw;
		g->t0 *= mulh;
		g->t1 *= mulh;
	} GLYPHS_ITERATOR_END
}

// -------------------------------------------  texture_font_enlarge_atlas ---
void
texture_font_enlarge_atlas( texture_font_t * self, size_t width_new,
				size_t height_new) {
	texture_atlas_t* ta = self->atlas;
	size_t width_old = ta->width;
	size_t height_old = ta->height;    

	texture_font_enlarge_texture( self, width_new, height_new);
	if ( self->scaletex ) {
		float mulw = (float)width_old / width_new;
		float mulh = (float)height_old / height_new;
		texture_font_enlarge_glyphs( self, mulw, mulh );
	}
}
#endif
