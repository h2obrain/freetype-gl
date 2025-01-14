/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#include <stdio.h>
#include <string.h>

#include "freetype-gl.h"
#include "mat4.h"
#include "shader.h"
#include "screenshot-util.h"
#include "vertex-buffer.h"

#include <GLFW/glfw3.h>


// ------------------------------------------------------- typedef & struct ---
typedef struct {
	float x, y, z;    // position
	float s, t;       // texture
	float r, g, b, a; // color
} vertex_t;


// ------------------------------------------------------- global variables ---
GLuint shader;
texture_atlas_t *atlas;
vertex_buffer_t *buffer;
mat4   model, view, projection;


// --------------------------------------------------------------- add_text ---
void add_text( vertex_buffer_t * buffer, texture_font_t * font,
			   char * text, vec4 * color, vec2 * pen ) {
	size_t i;
	float r = color->red, g = color->green, b = color->blue, a = color->alpha;
	for ( i = 0; i < strlen(text); ++i ) {
		texture_glyph_t *glyph = texture_font_get_glyph( font, text + i );
		if ( glyph != NULL ) {
			float kerning =  0.0f;
			if ( i > 0) {
				kerning = texture_glyph_get_kerning( glyph, text + i - 1 );
			}
			pen->x += kerning;
			int x0  = (int)( pen->x + glyph->offset_x );
			int y0  = (int)( pen->y + glyph->offset_y );
			int x1  = (int)( x0 + glyph->width );
			int y1  = (int)( y0 - glyph->height );
			float s0 = glyph->s0;
			float t0 = glyph->t0;
			float s1 = glyph->s1;
			float t1 = glyph->t1;
			GLuint indices[6] = {0,1,2, 0,2,3};
			vertex_t vertices[4] = { { x0,y0,0,  s0,t0,  r,g,b,a },
									 { x0,y1,0,  s0,t1,  r,g,b,a },
									 { x1,y1,0,  s1,t1,  r,g,b,a },
									 { x1,y0,0,  s1,t0,  r,g,b,a } };
			vertex_buffer_push_back( buffer, vertices, 4, indices, 6 );
			pen->x += glyph->advance_x;
		}
	}
}


// ------------------------------------------------------------------- init ---
void init( void ) {
	size_t i;
	texture_font_t *font = 0;
	atlas = texture_atlas_new( 512, 512, 1 );
	const char * filename = "fonts/Vera.ttf";
	char * text = "A Quick Brown Fox Jumps Over The Lazy Dog 0123456789";
	buffer = vertex_buffer_new( "vertex:3f,tex_coord:2f,color:4f" );
	vec2 pen = {{5,400}};
	vec4 black = {{0,0,0,1}};
	for ( i=7; i < 27; ++i) {
		font = texture_font_new_from_file( atlas, i, filename );
		pen.x = 5;
		pen.y -= font->height;
		texture_font_load_glyphs( font, text );
		add_text( buffer, font, text, &black, &pen );
		texture_font_delete( font );
	}

	glGenTextures( 1, &atlas->id );
	glBindTexture( GL_TEXTURE_2D, atlas->id );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RED, atlas->width, atlas->height,
				  0, GL_RED, GL_UNSIGNED_BYTE, atlas->data );

	shader = shader_load("shaders/v3f-t2f-c4f.vert",
						 "shaders/v3f-t2f-c4f.frag");
	mat4_set_identity( &projection );
	mat4_set_identity( &model );
	mat4_set_identity( &view );
}


// ---------------------------------------------------------------- display ---
void display( GLFWwindow* window ) {
	glClearColor( 1, 1, 1, 1 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glUseProgram( shader );
	{
		glUniform1i( glGetUniformLocation( shader, "texture" ),
					 0 );
		glUniformMatrix4fv( glGetUniformLocation( shader, "model" ),
							1, 0, model.data);
		glUniformMatrix4fv( glGetUniformLocation( shader, "view" ),
							1, 0, view.data);
		glUniformMatrix4fv( glGetUniformLocation( shader, "projection" ),
							1, 0, projection.data);
		vertex_buffer_render( buffer, GL_TRIANGLES );
	}

	glfwSwapBuffers( window );
}


// ---------------------------------------------------------------- reshape ---
void reshape( GLFWwindow* window, int width, int height ) {
	glViewport(0, 0, width, height);
	mat4_set_orthographic( &projection, 0, width, 0, height, -1, 1);
}


// --------------------------------------------------------------- keyboard ---
void keyboard( GLFWwindow* window, int key, int scancode, int action, int mods ) {
	if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS ) {
		glfwSetWindowShouldClose( window, GL_TRUE );
	}
}


// --------------------------------------------------------- error-callback ---
void error_callback( int error, const char* description ) {
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

	glfwWindowHint( GLFW_VISIBLE, GL_FALSE );
	glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );

	window = glfwCreateWindow( 800, 500, argv[0], NULL, NULL );

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
	reshape( window, 800, 500 );

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
