/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 *
 * ============================================================================
 *
 * All credits go to https://github.com/lxnt/ex-sdl-freetype-harfbuzz
 *
 * ============================================================================
 */
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <hb.h>
#include <hb-ft.h>

#include "freetype-gl.h"
#include "mat4.h"
#include "shader.h"
#include "vertex-buffer.h"
#include "texture-font.h"
#include "screenshot-util.h"

#include <GLFW/glfw3.h>


/* google this */
#ifndef unlikely
#define unlikely
#endif


#define WIDTH  800
#define HEIGHT 480

// ------------------------------------------------------- global variables ---
GLuint shader;
texture_atlas_t *atlas;
vertex_buffer_t * vbuffer;
mat4 model, view, projection;

const char *text_ar = "صِف خَلقَ خَودِ كَمِثلِ الشَمسِ إِذ بَزَغَت — يَحظى الضَجيعُ بِها نَجلاءَ مِعطارِ";
const char *font_filename      = "fonts/amiri-regular.ttf";
const hb_direction_t direction_ar = HB_DIRECTION_RTL;
const hb_script_t script_ar       = HB_SCRIPT_ARABIC;
const char *language           = "ar";

// ------------------------------------------------------------------- init ---
#define NUM_FONTS (sizeof(fonts)/sizeof(texture_font_t *))
void init( void ) {
	size_t i, j;

	atlas = texture_atlas_new( 300, 330, 3 );
	texture_font_t *fonts[15];
	printf("Creating font size: %d\n", 8);
	fonts[0] = texture_font_new_from_file(atlas, 8,72,100, font_filename, language);
	texture_font_load_glyphs(fonts[0], text_ar );
	for ( i=1; i<NUM_FONTS; ++i ) {
		printf("Creating font size: %zu\n", 8+i);
//		fonts[i] = texture_font_new_from_file(atlas, 12+i, font_filename, language);
		fonts[i] = texture_font_clone(fonts[0], 8+i);
		texture_font_load_glyphs(fonts[i], text_ar );
	}


	typedef struct { float x,y,z, u,v, r,g,b,a, shift, gamma; } vertex_t;
	vbuffer = vertex_buffer_new( "vertex:3f,tex_coord:2f,"
								"color:4f,ashift:1f,agamma:1f" );

	/* Create a buffer for harfbuzz to use */
	hb_buffer_t *buffer = hb_buffer_create();

	/* display atlas */
	float gamma = 1.0;
	float shift = 0.0;
	float r = 0.0;
	float g = 0.0;
	float b = 0.0;
	float a = 1.0;
	float x0 = 0;
	float x1 = atlas->width;
	float y0 = HEIGHT;
	float y1 = HEIGHT-atlas->height;
	float s0 = 0;
	float t0 = 0;
	float s1 = 1;
	float t1 = 1;
	vertex_t vertices[4] =  {
		{x0,y0,0, s0,t0, r,g,b,a, shift, gamma},
		{x0,y1,0, s0,t1, r,g,b,a, shift, gamma},
		{x1,y1,0, s1,t1, r,g,b,a, shift, gamma},
		{x1,y0,0, s1,t0, r,g,b,a, shift, gamma} };
	GLuint indices[6] = { 0,1,2, 0,2,3 };
	vertex_buffer_push_back( vbuffer, vertices, 4, indices, 6 );

#if !defined(TEXTURE_FONT_ENABLE_NORMALIZED_TEXTURE_COORDINATES)
	float atlas_width,atlas_height;
	atlas_width  = (float)atlas->width;
	atlas_height = (float)atlas->height;
#endif

	float x,y;
	y = HEIGHT;
	for (i=0; i < NUM_FONTS; ++i) {
		hb_buffer_add_utf8( buffer, text_ar, strlen(text_ar), 0, strlen(text_ar) );
		hb_buffer_set_language( buffer, fonts[i]->language );
//		hb_buffer_guess_segment_properties( buffer );
		hb_buffer_set_script( buffer, script_ar );
		hb_buffer_set_direction( buffer, direction_ar );
		hb_shape( fonts[i]->hb_ft_font, buffer, NULL, 0 );

		unsigned int         glyph_count;
		hb_glyph_info_t     *glyph_info;
		hb_glyph_position_t *glyph_pos;
		glyph_info = hb_buffer_get_glyph_infos(buffer, &glyph_count);
		glyph_pos  = hb_buffer_get_glyph_positions(buffer, &glyph_count);

		texture_font_load_glyphs( fonts[i], text_ar );

		float gamma = 1.0;
		float shift = 0.0;
		float width = 0.0;
		float hres = fonts[i]->hres;
		for (j = 0; j < glyph_count; ++j) {
			int codepoint = glyph_info[j].codepoint;
			float x_advance = glyph_pos[j].x_advance/(float)(hres*64);
			float x_offset = glyph_pos[j].x_offset/(float)(hres*64);
			texture_glyph_t *glyph = texture_font_get_glyph32(fonts[i], codepoint);
			if ( i < (glyph_count-1) )
				width += x_advance + x_offset;
			else
				width += glyph->offset_x + glyph->width;
		}
		x = WIDTH - width - 10 ;
//		y = HEIGHT - i * (10+i) - 15;
		printf("asc=%.3f desc=%.3f\n",fonts[i]->ascender,fonts[i]->descender);
		y -= fonts[i]->ascender;

		for (j = 0; j < glyph_count; ++j) {
			int codepoint = glyph_info[j].codepoint;
			// because of vhinting trick we need the extra 64 (hres)
			float x_advance = glyph_pos[j].x_advance/(float)(hres*64);
			float x_offset = glyph_pos[j].x_offset/(float)(hres*64);
			float y_advance = glyph_pos[j].y_advance/(float)(64);
			float y_offset = glyph_pos[j].y_offset/(float)(64);
			texture_glyph_t *glyph = texture_font_get_glyph32(fonts[i], codepoint);
			if (glyph == NULL) continue;

			float r = 0.0;
			float g = 0.0;
			float b = 0.0;
			float a = 1.0;
			float x0 = x + x_offset + glyph->offset_x;
			float x1 = x0 + glyph->width;
			float y0 = floor(y + y_offset + glyph->offset_y);
			float y1 = floor(y0 - glyph->height);
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
			vertex_t vertices[4] =  {
				{x0,y0,0, s0,t0, r,g,b,a, shift, gamma},
				{x0,y1,0, s0,t1, r,g,b,a, shift, gamma},
				{x1,y1,0, s1,t1, r,g,b,a, shift, gamma},
				{x1,y0,0, s1,t0, r,g,b,a, shift, gamma} };
			GLuint indices[6] = { 0,1,2, 0,2,3 };
			vertex_buffer_push_back( vbuffer, vertices, 4, indices, 6 );

			x += x_advance;
			y -= y_advance;
		}

		y += fonts[i]->descender;

		/* clean up the buffer, but don't kill it just yet */
		hb_buffer_reset(buffer);
	}

	glClearColor(1,1,1,1);
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glGenTextures( 1, &atlas->id );
	glBindTexture( GL_TEXTURE_2D, atlas->id );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, atlas->width, atlas->height,
				  0, GL_RGB, GL_UNSIGNED_BYTE, atlas->data );
	vertex_buffer_upload( vbuffer );
	shader = shader_load("shaders/text.vert", "shaders/text.frag");
	mat4_set_identity( &projection );
	mat4_set_identity( &model );
	mat4_set_identity( &view );
}


// ---------------------------------------------------------------- display ---
void display( GLFWwindow* window ) {
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glUseProgram( shader );
	{
		glUniform1i( glGetUniformLocation( shader, "texture" ),
					 0 );
		glUniform3f( glGetUniformLocation( shader, "pixel" ), 1/512., 1/512., 1.0f );

		glUniformMatrix4fv( glGetUniformLocation( shader, "model" ),
							1, 0, model.data);
		glUniformMatrix4fv( glGetUniformLocation( shader, "view" ),
							1, 0, view.data);
		glUniformMatrix4fv( glGetUniformLocation( shader, "projection" ),
							1, 0, projection.data);
		vertex_buffer_render( vbuffer, GL_TRIANGLES );
	}

	glfwSwapBuffers( window );
}


// ---------------------------------------------------------------- reshape ---
void reshape( GLFWwindow* window, int width, int height ) {
	(void)window;
	glViewport(0, 0, width, height);
	mat4_set_orthographic( &projection, 0, width, 0, height, -1, 1);
}


// --------------------------------------------------------------- keyboard ---
void keyboard( GLFWwindow* window, int key, int scancode, int action, int mods ) {
	(void)window; (void)scancode; (void)mods;
	if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS ) {
		glfwSetWindowShouldClose( window, GL_TRUE );
	}
}


// --------------------------------------------------------- error-callback ---
void error_callback( int error, const char* description ) {
	(void)error;
	fputs( description, stderr );
}


// ------------------------------------------------------------------- main ---
int main( int argc, char **argv ) {
	GLFWwindow* window;
	char* screenshot_path = NULL;

	if (argc > 1) {
		if (argc == 3 && 0 == strcmp( "--screenshot", argv[1] ))
			screenshot_path = argv[2];
		else
		{
			fprintf( stderr, "Unknown or incomplete parameters given\n" );
			exit( EXIT_FAILURE );
		}
	}

	glfwSetErrorCallback( error_callback );

	if (!glfwInit( )) {
		exit( EXIT_FAILURE );
	}

	glfwWindowHint( GLFW_VISIBLE, GL_TRUE );
	glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );

	window = glfwCreateWindow( WIDTH, HEIGHT, argv[0], NULL, NULL );

	if (!window) {
		glfwTerminate( );
		exit( EXIT_FAILURE );
	}

	glfwMakeContextCurrent( window );
	glfwSwapInterval( 1 );

	glfwSetFramebufferSizeCallback( window, reshape );
	glfwSetWindowRefreshCallback( window, display );
	glfwSetKeyCallback( window, keyboard );

#ifndef __APPLE__
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err) {
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf( stderr, "Error: %s\n", glewGetErrorString(err) );
		exit( EXIT_FAILURE );
	}
	fprintf( stderr, "Using GLEW %s\n", glewGetString(GLEW_VERSION) );
#endif

	init();

	glfwShowWindow( window );
	reshape( window, WIDTH, HEIGHT );

	while (!glfwWindowShouldClose( window )) {
		display( window );
		glfwPollEvents( );

		if (screenshot_path) {
			screenshot( window, screenshot_path );
			glfwSetWindowShouldClose( window, 1 );
		}
	}

	glDeleteTextures( 1, &atlas->id );
	atlas->id = 0;
	texture_atlas_delete( atlas );

	glfwDestroyWindow( window );
	glfwTerminate( );

	return EXIT_SUCCESS;
}
