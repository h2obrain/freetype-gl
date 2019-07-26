/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#if 0
#  if !defined(_WIN32) && !defined(_WIN64)
#    include <fontconfig/fontconfig.h>
#  endif
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font-manager.h"
#include "freetype-gl-err.h"

// ------------------------------------------------------------ file_exists ---
#if !defined(FREETYPE_GL_USE_MEMORY_FONTS) // more like no stream or so
static int
file_exists( const char * filename ) {
	FILE * file = fopen( filename, "r" );
	if ( file ) {
		fclose(file);
		return 1;
	}
	return 0;
}
#endif

// ------------------------------------------------------- font_manager_new ---
font_manager_t *
font_manager_new( size_t width, size_t height, size_t depth, const float dpi, const float hres ) {
	font_manager_t *self;
	texture_atlas_t *atlas = texture_atlas_new( width, height, depth );
	self = (font_manager_t *) malloc( sizeof(font_manager_t) );
	if ( !self ) {
		freetype_gl_error( Out_Of_Memory,
			   "line %d: No more memory for allocating data\n", __LINE__ );
	return NULL;
		/* exit( EXIT_FAILURE ); */ /* Never ever exit from a library */
	}
	self->atlas = atlas;
	self->fonts = vector_new( sizeof(texture_font_t *) );
	self->cache = strdup( " " );
	self->dpi   = dpi;
	self->hres  = hres;
	return self;
}


// ---------------------------------------------------- font_manager_delete ---
void
font_manager_delete( font_manager_t * self ) {
	size_t i;
	texture_font_t *font;
	assert( self );

	for ( i=0; i<vector_size( self->fonts ); ++i) {
		font = *(texture_font_t **) vector_get( self->fonts, i );
		texture_font_delete( font );
	}
	vector_delete( self->fonts );
	texture_atlas_delete( self->atlas );
	if ( self->cache ) {
		free( self->cache );
	}
	free( self );
}



// ----------------------------------------------- font_manager_delete_font ---
void
font_manager_delete_font( font_manager_t * self,
						  texture_font_t * font) {
	size_t i;
	texture_font_t *other;

	assert( self );
	assert( font );

	for ( i=0; i<self->fonts->size;++i ) {
		other = (texture_font_t *) vector_get( self->fonts, i );
		if ( (strcmp(font->filename, other->filename) == 0)
			   && ( font->pt_size == other->pt_size) ) {
			vector_erase( self->fonts, i);
			break;
		}
	}
	texture_font_delete( font );
}



// ----------------------------------------- font_manager_get_from_filename ---
texture_font_t *
font_manager_get_from_memory( font_manager_t *self,
								const font_family_t font_data,
								const float size,
								const char *language
) {
	size_t i;
	texture_font_t *font;

	assert( self );
	for ( i=0; i<vector_size(self->fonts); ++i ) {
		font = * (texture_font_t **) vector_get( self->fonts, i );
		if ((font->location==TEXTURE_FONT_MEMORY)
		 && (font->memory.base == font_data.font_base)
		 && (font->pt_size == size) ) {
			return font;
		}
	}
	font = texture_font_new_from_memory( self->atlas, size,self->dpi,self->hres, font_data.font_base,font_data.font_size, language );
	if ( font ) {
		vector_push_back( self->fonts, &font );
		texture_font_load_glyphs( font, self->cache );
		printf( "Font MEMORY_FONT\n"
				"  - size:      %8.1f\n"
				"  - ascender:  %8.1f\n"
				"  - descender: %8.1f\n"
				"  - height:    %8.1f\n",
				size,
				font->ascender,font->descender,
				font->height);
		return font;
	}
	freetype_gl_error( Cannot_Load_File,
		   	"Unable to load MEMORY_FONT (size=%.1f)\n", size );
	return 0;
}

#ifndef FT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT
texture_font_t *
font_manager_get_from_filename( font_manager_t *self,
								const char * filename,
								const float size,
								const char *language) {
	size_t i;
	texture_font_t *font_base;

	assert( self );
	for ( i=0; i<vector_size(self->fonts); ++i ) {
		font_base = * (texture_font_t **) vector_get( self->fonts, i );
		if ((font_base->location==TEXTURE_FONT_FILE)
		 && (strcmp(font_base->filename, filename) == 0)
		 && (font_base->pt_size == size)) {
			return font_base;
		}
	}
	font_base = texture_font_new_from_file( self->atlas, size,72,100, filename, language );
	if ( font_base ) {
		vector_push_back( self->fonts, &font_base );
		texture_font_load_glyphs( font_base, self->cache );
		printf( "Font %s\n"
				"  - size:      %8.1f\n"
				"  - ascender:  %8.1f\n"
				"  - descender: %8.1f\n"
				"  - height:    %8.1f\n",
				filename,
				size,
				font_base->ascender,font_base->descender,
				font_base->height);
		return font_base;
	}
	freetype_gl_error( Cannot_Load_File,
		   	"Unable to load \"%s\" (size=%.1f)\n", filename, size );
	return 0;
}
#endif

// ----------------------------------------- font_manager_get_from_description ---
texture_font_t *
font_manager_get_from_description( font_manager_t *self,
								   const font_family_t family,
								   const float size,
								   const int bold,
								   const int italic,
								   const char *language
) {
#if defined(FREETYPE_GL_USE_MEMORY_FONTS)
	(void)bold;(void)italic;
	return font_manager_get_from_memory(self, family, size, language);
#else
	texture_font_t *font_base;
	char *filename = 0;

	assert( self );

	if ( file_exists( family ) ) {
		filename = strdup( family );
	} else {
#if defined(_WIN32) || defined(_WIN64)
		freetype_gl_error( Unimplemented_Function,
			   "\"font_manager_get_from_description\" not implemented yet.\n" );
		return 0;
#endif
		filename = font_manager_match_description( self, family, size, bold, italic );
		if ( !filename ) {
			freetype_gl_error( Font_Unavailable,
			   	"No \"%s (size=%.1f, bold=%d, italic=%d)\" font available.\n",
					 family, size, bold, italic );
			return 0;
		}
	}
	font_base = font_manager_get_from_filename( self, filename, size, language );

	free( filename );
	return font_base;
#endif
}

// ------------------------------------------- font_manager_get_from_markup ---
texture_font_t *
font_manager_get_from_markup( font_manager_t *self,
							  const markup_t *markup ) {
	assert( self );
	assert( markup );

	return font_manager_get_from_description(
				self,
				markup->family, markup->size,
				markup->bold, markup->italic,
				markup->language
			);
}

// ----------------------------------------- font_manager_match_description ---
char *
font_manager_match_description( font_manager_t * self,
								const char * family,
								const float size,
								const int bold,
								const int italic
) {
// Use of fontconfig is disabled by default.
#if 1
	(void)self;
	(void)family;
	(void)size;
	(void)bold;
	(void)italic;
	return 0;
#else
#  if defined _WIN32 || defined _WIN64
	  freetype_gl_error( Unimplemented_Function,
			 "\"font_manager_match_description\" not implemented for windows.\n" );
	  return 0;
#  endif
	char *filename = 0;
	int weight = FC_WEIGHT_REGULAR;
	int slant = FC_SLANT_ROMAN;
	if ( bold ) {
		weight = FC_WEIGHT_BOLD;
	}
	if ( italic ) {
		slant = FC_SLANT_ITALIC;
	}
	FcInit();
	FcPattern *pattern = FcPatternCreate();
	FcPatternAddDouble( pattern, FC_SIZE, pt_size );
	FcPatternAddInteger( pattern, FC_WEIGHT, weight );
	FcPatternAddInteger( pattern, FC_SLANT, slant );
	FcPatternAddString( pattern, FC_FAMILY, (FcChar8*) family );
	FcConfigSubstitute( 0, pattern, FcMatchPattern );
	FcDefaultSubstitute( pattern );
	FcResult result;
	FcPattern *match = FcFontMatch( 0, pattern, &result );
	FcPatternDestroy( pattern );

	if ( !match ) {
		freetype_gl_error( Cant_Match_Family,
			   "fontconfig error: could not match family '%s'", family );
		return 0;
	} else {
		FcValue value;
		FcResult result = FcPatternGet( match, FC_FILE, 0, &value );
		if ( result ) {
			freetype_gl_error( Cant_Match_Family,
			   	"fontconfig error: could not match family '%s'", family );
		} else {
			filename = strdup( (char *)(value.u.s) );
		}
	}
	FcPatternDestroy( match );
	return filename;
#endif
}
