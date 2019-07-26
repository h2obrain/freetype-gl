/* Freetype GL - A C OpenGL Freetype engine
 *
 * Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
 * file `LICENSE` for more details.
 */
#ifndef __MARKUP_H__
#define __MARKUP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "texture-font.h"

#include <stdbool.h>

#if defined(FREETYPE_GL_COLOR_ARGB)
#include <gfx/gfx.h>
typedef gfx_color_t ftgl_color_t;
#else
#include "vec234.h"
typedef gfx_color_t vec4;
#endif

#if defined(FREETYPE_GL_USE_MEMORY_FONTS)
typedef struct {
	void  *font_base;
	size_t font_size;
} memory_font_t;
typedef memory_font_t font_family_t;
#else
typedef char* font_family_t;
#endif

#include <hb.h>

#ifdef __cplusplus
namespace ftgl {
#endif

/**
 * @file   markup.h
 * @author Nicolas Rougier (Nicolas.Rougier@inria.fr)
 *
 * @defgroup markup Markup
 *
 * Simple structure that describes text properties.
 *
 * <b>Example Usage</b>:
 * @code
 * #include "markup.h"
 *
 * ...
 *
 * gfx_color_t black  = {{0.0, 0.0, 0.0, 1.0}};
 * gfx_color_t white  = {{1.0, 1.0, 1.0, 1.0}};
 * gfx_color_t none   = {{1.0, 1.0, 1.0, 0.0}};
 *
 * markup_t normal = {
 *     .family  = "Droid Serif",
 *     .size = 24.0,
 *     .bold = 0,
 *     .italic = 0,
 *     .spacing = 1.0,
 *     .gamma = 1.0,
 *     .foreground_color = black, .background_color    = none,
 *     .underline        = 0,     .underline_color     = black,
 *     .overline         = 0,     .overline_color      = black,
 *     .strikethrough    = 0,     .strikethrough_color = black,
 *     .font = 0,
 * };
 *
 * ...
 *
 * @endcode
 *
 * @{
 */


/**
 * Simple structure that describes text properties.
 */
typedef struct markup_t
{
	char *language;
	hb_direction_t direction;
	hb_script_t    script;

	/**
	 * A font family name such as "normal", "sans", "serif" or "monospace".
	 */
	font_family_t family;

	/**
	 * Font size.
	 */
	float size;

	/**
	 * Whether text is bold.
	 */
	bool bold;

	/**
	 * Whether text is italic.
	 */
	bool italic;

	/**
	 * Spacing between letters.
	 */
	float spacing;

#ifndef FREETYPE_GL_NOGL
	/**
	 * Gamma correction.
	 */
	float gamma;
#endif

	/**
	 * Text color.
	 */
	gfx_color_t foreground_color;

	/**
	 * Background color.
	 */
	gfx_color_t background_color;

	/**
	 * Whether outline is active.
	 */
	bool outline;

	/**
	 * Outline color.
	 */
	gfx_color_t outline_color;

	/**
	 * Whether underline is active.
	 */
	bool underline;

	/**
	 * Underline color.
	 */
	gfx_color_t underline_color;

	/**
	 * Whether overline is active.
	 */
	bool overline;

	/**
	 * Overline color.
	 */
	gfx_color_t overline_color;

	/**
	 * Whether strikethrough is active.
	 */
	bool strikethrough;

	/**
	 * Strikethrough color.
	 */
	gfx_color_t strikethrough_color;

	/**
	 * Pointer on the corresponding font (family/size/bold/italic)
	 */
	texture_font_t *font;

} markup_t;

/**
  * Default markup
  */
extern markup_t default_markup;


/** @} */

#ifdef __cplusplus
}
}
#endif

#endif /* __MARKUP_H__ */
