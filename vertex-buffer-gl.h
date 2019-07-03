/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */

#ifndef __VERTEX_BUFFER_H__
#error "This file must be included from vertex-buffer.h!"
#endif

#ifndef __VERTEX_BUFFER_GL_H__
#define __VERTEX_BUFFER_GL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "opengl.h"
#include "vector.h"
#include "vertex-attribute-gl.h"

#ifdef __cplusplus
namespace ftgl {
#endif

/**
 * @file   vertex-buffer.h
 * @author Nicolas Rougier (Nicolas.Rougier@inria.fr)
 * @date   April, 2012
 *
 * @defgroup vertex-buffer Vertex buffer
 *
 * @{
 */


/**
 * Generic vertex buffer.
 */
typedef struct vertex_buffer_t
{
	/** Format of the vertex buffer. */
	char * format;

	/** Vector of vertices. */
	vector_t * vertices;

#ifdef FREETYPE_GL_USE_VAO
	/** GL identity of the Vertex Array Object */
	GLuint VAO_id;
#endif

	/** GL identity of the vertices buffer. */
	GLuint vertices_id;

	/** Vector of indices. */
	vector_t * indices;

	/** GL identity of the indices buffer. */
	GLuint indices_id;

	/** Current size of the vertices buffer in GPU */
	size_t GPU_vsize;

	/** Current size of the indices buffer in GPU*/
	size_t GPU_isize;

	/** GL primitives to render. */
	GLenum mode;

	/** Whether the vertex buffer needs to be uploaded to GPU memory. */
	char state;

	/** Individual items */
	vector_t * items;

	/** Array of attributes. */
	vertex_attribute_t *attributes[MAX_VERTEX_ATTRIBUTE];
} vertex_buffer_t;


/** @} */

#ifdef __cplusplus
}
}
#endif

#endif /* __VERTEX_BUFFER_H__ */
