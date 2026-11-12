//
//
// Wrappers for looking at the mouse input buffer
//--------------------------------------------------------------------------------

#include <utilitieslib/utils/wininclude.h>
#include "UI/uiInput.h"
#include "UI/uiGame.h"
#include "UI/sprite/sprite_base.h"
#include "cmdparse/cmdcommon.h"
#include "entity/entDebug.h"
#include "graphics/font.h"
#include "UI/uiCursor.h"
#include "UI/uiWindows.h"
#include "UI/uiScrollBar.h"
#include "win/input.h"
#include "UI/uiBaseInput.h"
#include "UI/uiOptions.h"
#include "win/gamepadNav.h"
#include <utilitieslib/utils/Cbox.h>

mouse_input gMouseInpBuf[MOUSE_INPUT_SIZE] = { 0 };
mouse_input gMouseInpCur  = {0};
mouse_input gMouseInpLast = {0};

int gInpBufferSize;

int gLastDownX = 0;
int gLastDownY = 0;

// ramp up when you hold the mouse down
static float cursor_repeat_times_fast[] =
{
    8, 15, 21, 26, 30, 34, 38, 42, 46, 50, 54, 57, 60 
};

static float cursor_repeat_times_slow[] =
{
    20, 35, 45, 52, 57, 60
};

// Tells D-pad navigation about a box the UI is hit testing. Boxes are in UI
// space on the front end menus and in client pixels in the game, the same split
// scaled_point_cbox_clsn makes. Where a box's middle is in client pixels, as
// the pad's navigation sees it.
void boxScreenCenter(const CBox *box, int *x, int *y)
{
	F32 cx = (box->left + box->right) / 2;
	F32 cy = (box->top + box->bottom) / 2;

	if (shell_menu()) {
		F32 xsc, ysc, xOffset, yOffset;

		calc_scrn_scaling_with_offset(&xsc, &ysc, &xOffset, &yOffset);
		cx = cx * xsc + xOffset;
		cy = cy * ysc + yOffset;
	}
	*x = (int)cx;
	*y = (int)cy;
}

static void reportBoxToNavigation(CBox *box, int blocked)
{
	NavRect ui, screen;

	ui.left = box->left;
	ui.top = box->top;
	ui.right = box->right;
	ui.bottom = box->bottom;
	screen = ui;

	if (outside_scissor((ui.left + ui.right) / 2, (ui.top + ui.bottom) / 2))
		return;

	if (shell_menu()) {
		F32 xsc, ysc, xOffset, yOffset;

		calc_scrn_scaling_with_offset(&xsc, &ysc, &xOffset, &yOffset);
		screen.left = ui.left * xsc + xOffset;
		screen.right = ui.right * xsc + xOffset;
		screen.top = ui.top * ysc + yOffset;
		screen.bottom = ui.bottom * ysc + yOffset;
	}

	gamepadNavAddBox(&ui, &screen, blocked);
}

int mouseDragging()
{
    if( cursor.dragging || gWindowDragging || gScrollBarDragging ||
        baseEdit_state.surface_dragging == kDragType_Drag ||
        baseEdit_state.detail_dragging == kDragType_Drag ||
        baseEdit_state.room_dragging == kDragType_Drag ||
        baseEdit_state.door_dragging == kDragType_Drag ||
        baseEdit_state.plot_dragging == kDragType_Drag )
    {
        return true;
    }

    return false;
}

// returns true if the mouse was pressed down
int mouseDown( int button )
{
    int i;

    if( control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_DOWN ) ||
             ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_DOWN ) )
        {
            return TRUE;
        }
    }

    return FALSE;

}

// returns true if the mouse was released 
int mouseUp( int button )
{
    int i;

    if( control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_UP ) ||
             ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_UP ) )
        {
            return TRUE;
        }
    }
    return FALSE;
}

// returns true if the mouse button is currently down
int isDown( int button )
{
    if( control_state.canlook )
        return FALSE;

    if( ( button == MS_LEFT && gMouseInpCur.left == MS_DOWN ) ||
        ( button == MS_RIGHT && gMouseInpCur.right == MS_DOWN ) )
    {
        return TRUE;
    }

    return FALSE;
}

// returns true if the right mouse button was clicked
int mouseRightClick()
{
    int i;
 
     if( entDebugMenuVisible() || control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

     for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( gMouseInpBuf[i].right == MS_CLICK )
        {
            return TRUE;    
        }
    }

    return FALSE;
}

int rightClickCoords( int *x, int *y )
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( gMouseInpBuf[i].right == MS_CLICK )
        {
            *x = gMouseInpBuf[i].x;
            *y = gMouseInpBuf[i].y;
            reversePointToScreenScalingFactori(x, y);
            return TRUE;    
        }
    }

    return FALSE;
}

// returns true if the left mouse button was clicked
int mouseLeftClick()
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( gMouseInpBuf[i].left == MS_CLICK )
        {
            return TRUE;
        }
    }

    return FALSE;
}

int leftClickCoords( int *x, int *y )
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( gMouseInpBuf[i].left == MS_CLICK )
        {
            *x = gMouseInpBuf[i].x;
            *y = gMouseInpBuf[i].y;
            reversePointToScreenScalingFactori(x, y);
            return TRUE;    
        }
    }

    return FALSE;
}

// returns true if the left mouse button is dragging
int mouseLeftDrag( CBox *box )
{
    int i;

     if( entDebugMenuVisible() || control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for( i = 0; i < gInpBufferSize; i++ )
    {
        if ( gMouseInpBuf[i].left == MS_DRAG )
        {
            int xp = gMouseInpBuf[i].x;
            int yp = gMouseInpBuf[i].y;

            if( scaled_point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ) )
                return TRUE;
        }
    }

    return FALSE;
}

// returns true if the mouse button was clicked while over a given box
int mouseDownHit( CBox *box, int button )
{
     int i;

    if( entDebugMenuVisible() || control_state.canlook || mouseDragging() )
        return FALSE;

    // Collisions turned off for the rest of the frame still leave the box
    // a gamepad focus target; see reportBoxToNavigation().
    reportBoxToNavigation( box, collisions_off_for_rest_of_frame );
    if( collisions_off_for_rest_of_frame )
        return FALSE;

     for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_DOWN ) ||
             ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_DOWN ) )
        {
            int xp = gMouseInpBuf[i].x;
            int yp = gMouseInpBuf[i].y;

             if ( scaled_point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ) )
                return TRUE;
        }
    }

    return FALSE;
}

// returns true if the mouse button was released while over a given box
int mouseUpHit( CBox *box, int button )
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_UP ) ||
            ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_UP ) )
        {
            float xp = gMouseInpBuf[i].x;
            float yp = gMouseInpBuf[i].y;

            if ( scaled_point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ) )
                return TRUE;
        }
    }
    return FALSE;
}

// returns true if the mouse button was clicked while over a given box
int mouseClickHit( CBox *box, int button )
{
    int i;

    if( entDebugMenuVisible() || mouseDragging() )
        return FALSE;

    // Collisions turned off for the rest of the frame still leave the box
    // a gamepad focus target; see reportBoxToNavigation().
    reportBoxToNavigation( box, collisions_off_for_rest_of_frame );
    if( collisions_off_for_rest_of_frame )
        return FALSE;

    for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_CLICK ) ||
            ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_CLICK) )
        {
            float xp = gMouseInpBuf[i].x;
            float yp = gMouseInpBuf[i].y;

            if ( scaled_point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ) )
                return TRUE;
        }
    }

    return FALSE;
}

int mouseDoubleClickHit( CBox *box, int button )
{
    int i;

    if( entDebugMenuVisible() || collisions_off_for_rest_of_frame || mouseDragging() )
        return FALSE;

    for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_DBLCLICK ) ||
            ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_DBLCLICK) )
        {
            float xp = gMouseInpBuf[i].x;
            float yp = gMouseInpBuf[i].y;

            if ( scaled_point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ) )
                return TRUE;
        }
    }

    return FALSE;
}

// returns true if the mouse did collide
//
static int mouseCollisionEx(CBox *box, int forNavigation)
{
    float xp, yp;

      if( entDebugMenuVisible() || control_state.canlook ||
            gWindowDragging || gScrollBarDragging )
        return FALSE;

    // A control under the mouse, such as a help button, can turn collisions
    // off for the rest of the frame. The mouse cannot hit what follows, but
    // the gamepad's D-pad must still be able to reach it.
    if( forNavigation )
        reportBoxToNavigation( box, collisions_off_for_rest_of_frame );
    if( collisions_off_for_rest_of_frame )
        return FALSE;

       xp = gMouseInpCur.x;
     yp = gMouseInpCur.y;

     if ( scaled_point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ) )
        return TRUE;

    //if ( point_cbox_clsn( xp, yp, box ) && !outside_scissor( xp, yp ))
    //    return TRUE;

    return FALSE;
}

int mouseCollision(CBox *box)
{
	return mouseCollisionEx(box, 1);
}

// The same, for a box the gamepad should not stop on, such as each word of a
// block of text.
int mouseCollisionQuiet(CBox *box)
{
	return mouseCollisionEx(box, 0);
}

// returns true if the mouse button was pressed
int mouseDownHitNoCol( int button )
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || mouseDragging() )
        return FALSE;

    for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_DOWN ) ||
            ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_DOWN ) )
            return true;
    }
    return FALSE;
}

// returns true if the mouse button was released
int mouseUpHitNoCol( int button )
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || mouseDragging() )
        return FALSE;

    for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_UP ) ||
            ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_UP ) )
            return true;
    }
    return FALSE;
}

// returns true if the mouse button was clicked
int mouseClickHitNoCol( int button )
{
    int i;

    if( entDebugMenuVisible() || control_state.canlook || mouseDragging() )
        return FALSE;

    for ( i = 0; i < gInpBufferSize; i++ )
    {
        if ( ( button == MS_LEFT  && gMouseInpBuf[i].left  == MS_CLICK ) ||
            ( button == MS_RIGHT && gMouseInpBuf[i].right == MS_CLICK ) )
            return true;
    }
    return FALSE;
}


// Returns true if mouse is doing something like dragging or looking
int mouseActive()
{
    if( entDebugMenuVisible() || control_state.canlook || 
        collisions_off_for_rest_of_frame || gWindowDragging || gScrollBarDragging )
        return TRUE;
    return FALSE;
}

void mouseFlush()
{
    memset( &gMouseInpBuf, 0, sizeof(mouse_input) * MOUSE_INPUT_SIZE );
    gInpBufferSize = 0;
    MouseClearButtonState();
}

void mouseClear()
{
     int i;
    int found=0;
     for( i = 0; i < gInpBufferSize; i++ )
    {
        if( gMouseInpBuf[i].right == MS_UP ) {
            memcpy( &gMouseInpBuf, &gMouseInpBuf[i+1], sizeof(mouse_input)*(gInpBufferSize-(i+1)) );
            gInpBufferSize -= i;
            found=1;
        }
    }
    if (!found) {
        mouseFlush();
    }
}

F32 mouseScrollingScaled()
{
    if( !optionGet(kUO_DisableMouseScroll) )
    {
        int scroll_wheel = gMouseInpCur.z - gMouseInpLast.z;
        if(scroll_wheel < 0)
        {
            return optionGetf(kUO_MouseScrollSpeed);
        }
        else if (scroll_wheel > 0)
        {
            return -optionGetf(kUO_MouseScrollSpeed);
        }
    }
    return 0;
}
