#!/usr/bin/env python3
"""
Draw the control sprites shown in the side panel's Controls list (assets/Textures/controls/*.png).

Pixel art at the size they are drawn (20 px tall, the panel's text line is 25 px), in the palette of the other sprites:
grey keycaps with a darker front lip, a light mouse with the pressed button in cyan. Key letters (V, S, Esc, ...) are
not baked in: the app writes them on the blank keycaps in its own font, so one sprite serves every key.

Run from the repository folder:  python3 tools/make_control_sprites.py
"""

import os

from PIL import Image


OUTLINE   = ( 30,  30,  40, 255)
FACE      = (205, 205, 215, 255)
HIGHLIGHT = (240, 240, 245, 255)
LIP       = (130, 130, 145, 255)
GLYPH     = ( 45,  45,  60, 255)
PRESSED   = ( 80, 200, 235, 255)
CLEAR     = (  0,   0,   0,   0)


def render_keycap( width : int, height : int = 20 ) -> Image.Image:
    """
    Render a blank keycap: outline with cut corners, a light top face, a highlight line and a darker front lip.

    Parameters
    ----------
    width : int
        Width in pixels.
    height : int
        Height in pixels.

    Returns
    -------
    Image.Image
        RGBA image of size (width, height).
    """


    # pseudo-code:
    # outline on the border except the four corner pixels; face inside; top inner row highlight; bottom 4 inner rows lip


    image  = Image.new( "RGBA", ( width, height ), CLEAR )
    pixels = image.load()


    for y in range( height ):
        for x in range( width ):
            corner = ( x in ( 0, width - 1 ) ) and ( y in ( 0, height - 1 ) )
            border = x in ( 0, width - 1 ) or y in ( 0, height - 1 )
            if corner:
                continue
            if border:
                pixels[ x, y ] = OUTLINE
            elif y == 1:
                pixels[ x, y ] = HIGHLIGHT
            elif y >= height - 5:
                pixels[ x, y ] = LIP
            else:
                pixels[ x, y ] = FACE


    return image


def render_arrow_key( direction : str ) -> Image.Image:
    """
    Render a 20 x 20 keycap with an arrow on its face.

    Parameters
    ----------
    direction : str
        "up", "down", "left" or "right".

    Returns
    -------
    Image.Image
        RGBA image of size (20, 20).
    """


    # pseudo-code:
    # a triangle pointing up, 9 px wide and 5 px tall, centred on the face (x 5..13, y 4..8), then rotated to direction


    image  = render_keycap( 20 )
    arrow  = Image.new( "RGBA", ( 20, 20 ), CLEAR )
    pixels = arrow.load()
    for row in range( 5 ):
        for x in range( 9 - row, 10 + row ):
            pixels[ x, 4 + row ] = GLYPH
    for y in range( 9, 13 ):          # the arrow's stem
        for x in range( 8, 11 ):
            pixels[ x, y ] = GLYPH
    rotation = { "up" : 0, "left" : 90, "down" : 180, "right" : 270 }[ direction ]
    arrow    = arrow.rotate( rotation, center = ( 9.5, 8.5 ) )


    image.alpha_composite( arrow )


    return image


def render_mouse( pressed : str ) -> Image.Image:
    """
    Render a 20 x 20 mouse seen from above with one button pressed (cyan).

    Parameters
    ----------
    pressed : str
        "left" or "right".

    Returns
    -------
    Image.Image
        RGBA image of size (20, 20).
    """


    # pseudo-code:
    # body: a 13 x 19 rounded rectangle (x 3..15, y 0..18); buttons: the top 8 rows, split by the middle column x = 9
    # the pressed button is cyan, the other light; the lower body is the face colour with a lip at the bottom


    image  = Image.new( "RGBA", ( 20, 20 ), CLEAR )
    pixels = image.load()
    left, right, top, bottom, split, buttons_bottom = 3, 15, 0, 18, 9, 8


    for y in range( top, bottom + 1 ):
        # round the four corners by two pixels
        inset = 2 if y in ( top, bottom ) else ( 1 if y in ( top + 1, bottom - 1 ) else 0 )
        for x in range( left + inset, right - inset + 1 ):
            border = x in ( left + inset, right - inset ) or y in ( top, bottom ) or y == buttons_bottom
            if border or ( x == split and y < buttons_bottom ):
                pixels[ x, y ] = OUTLINE
            elif y < buttons_bottom:
                is_pressed     = ( x < split ) == ( pressed == "left" )
                pixels[ x, y ] = PRESSED if is_pressed else HIGHLIGHT
            elif y >= bottom - 3:
                pixels[ x, y ] = LIP
            else:
                pixels[ x, y ] = FACE


    return image


def save_control_sprites( folder : str ) -> None:
    """
    Render every control sprite and save it as a PNG.

    Parameters
    ----------
    folder : str
        Output folder (created if missing).
    """


    # pseudo-code:
    # key.png 20 x 20, key_wide.png 40 x 20, key_up / down / left / right.png, mouse_left.png, mouse_right.png


    os.makedirs( folder, exist_ok = True )
    sprites = {
        "key"         : render_keycap( 20 ),
        "key_wide"    : render_keycap( 40 ),
        "key_up"      : render_arrow_key( "up" ),
        "key_down"    : render_arrow_key( "down" ),
        "key_left"    : render_arrow_key( "left" ),
        "key_right"   : render_arrow_key( "right" ),
        "mouse_left"  : render_mouse( "left" ),
        "mouse_right" : render_mouse( "right" ),
    }


    for name, image in sprites.items():
        image.save( os.path.join( folder, f"{name}.png" ) )
        print( f"Saved {folder}/{name}.png ({image.width} x {image.height} px)" )


if __name__ == "__main__":
    save_control_sprites( "assets/Textures/controls" )
