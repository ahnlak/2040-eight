/*
 * 2040-eight.cpp; a puzzle game for the PicoSystem.
 *
 * This is a version of the ever-popular '2048' game, for the lovely little
 * PicoSystem handheld.
 *
 * Copyright (c) 2021 Pete Favelle <picosystem@ahnlak.com>
 * This file is distributed under the MIT License; see LICENSE for details.
 */

/* System headers. */

#include <cstdlib>
#include <cstring>


/* Local headers. */

#include "picosystem.hpp"
#include "assets/spritesheet.hpp"
#include "assets/logo_ahnlak_1bit.hpp"


/* Local structures and types. */

typedef struct
{
  uint_fast8_t  row;
  uint_fast8_t  col;
  uint16_t      value;
  uint_fast8_t  progress;
} spawn_t;

typedef struct 
{
  uint_fast8_t  start_row;
  uint_fast8_t  start_col;
  uint_fast8_t  end_row;
  uint_fast8_t  end_col;
  uint16_t      start_value;
  uint16_t      end_value;
  uint_fast8_t  pixels_to_end;
} move_t;

typedef struct 
{
  uint32_t      frequency;
  uint32_t      duration;
} note_t;


/* Globals (shhh!) */

#define BOARD_WIDTH   4
#define BOARD_HEIGHT  4
#define MOVE_MAX      12
#define TUNE_LENGTH   16

bool                g_playing = false;
bool                g_moving = false;
uint16_t            g_cells[BOARD_HEIGHT][BOARD_WIDTH];
spawn_t             g_spawn;
move_t              g_moves[MOVE_MAX];
bool                g_splashing = true;
uint_fast8_t        g_splash_tone = 0;
picosystem::voice_t g_voice;
uint16_t            g_max_cell = 0;
note_t              g_tune[TUNE_LENGTH];
uint_fast8_t        g_tune_note = TUNE_LENGTH;
uint_fast8_t        g_tune_note_count = TUNE_LENGTH;
uint32_t            g_last_update_us;


/* Functions. */

/*
 * board_clear - clears all the cells in the board.
 */

void board_clear( void )
{
  /* Work through rows. */
  for( uint_fast8_t l_row = 0; l_row < BOARD_HEIGHT; l_row++ )
  {
    /* And then columns. */
    for( uint_fast8_t l_col = 0; l_col < BOARD_WIDTH; l_col++ )
    {
      g_cells[l_row][l_col] = 0;
    }
  }

  /* Make sure the spawn isn't active. */
  g_spawn.progress = 100;

  /* And make sure all the moves are set to complete. */
  for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
  {
    g_moves[l_index].pixels_to_end = 0;
  }

  /* Reset the max cell record. */
  g_max_cell = 2;

  /* All done. */
  return;
}


/*
 * board_spawn - creates a new value in an empty cell, if possible. This is
 *               usually a 2, but can on random occasions be a 4.
 */

bool board_spawn( void )
{
  uint_fast8_t l_free_cell_count = 0;
  uint_fast8_t l_free_cells[BOARD_HEIGHT*BOARD_WIDTH];

  /* Quickly scan the empty cells. */
  for( uint_fast8_t l_row = 0; l_row < BOARD_HEIGHT; l_row++ )
  {
    /* And then columns. */
    for( uint_fast8_t l_col = 0; l_col < BOARD_WIDTH; l_col++ )
    {
      /* Skip any cells on the board which are filled. */
      if ( g_cells[l_row][l_col] > 0 )
      {
        continue;
      }

      /* We also need to check any active moves. */
      for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
      {
        if ( ( g_moves[l_index].pixels_to_end > 0 ) &&
             ( g_moves[l_index].end_row == l_row ) && ( g_moves[l_index].end_col == l_col ) )
        {
          continue;
        }
      }

      /* Good, then it's free! */
      l_free_cells[l_free_cell_count++] = ( l_row * BOARD_WIDTH ) + l_col;
    }
  }

  /* So, if we found no free cell, it's a fail. */
  if ( l_free_cell_count == 0 )
  {
    return false;
  }

  /* Select a random cell from that array then. */
  uint_fast8_t l_free_cell = l_free_cells[std::rand()%l_free_cell_count];

  /* And fill that in. */
  //g_cells[l_free_cell/BOARD_WIDTH][l_free_cell%BOARD_WIDTH] = 2;
  g_spawn.row = l_free_cell/BOARD_WIDTH;
  g_spawn.col = l_free_cell%BOARD_WIDTH;
  g_spawn.value = 2;
  g_spawn.progress = 0;
  return true;
}


/*
 * board_move - responds to the user choosing a direction; takes a button
 *              direction, and returns true if a move is possible.
 */

bool board_move( uint_fast8_t p_direction )
{
  bool          l_success = false, l_collapsed = false;
  int_fast8_t   l_row, l_col, l_altrow, l_altcol;
  move_t        l_move;
  uint16_t      l_workrow[BOARD_WIDTH], l_workcol[BOARD_HEIGHT];

  /* Work through each direction; this could be genericised into vectors, */
  /* but to be honest it's not worth the effort!                          */
  switch( p_direction )
  {
  case picosystem::UP:
    /* Work through every row. */
    for( l_col = 0; l_col < BOARD_WIDTH; l_col++ )
    {
      /* Only allow one collapse per row. */
      l_collapsed = false;

      /* And we will work on a copy of the column, for ease of fiddling. */
      for ( uint_fast8_t l_index = 0; l_index < BOARD_HEIGHT; l_index++ )
      {
        l_workcol[l_index] = g_cells[l_index][l_col];
      }
      
      /* And each row in that column. */
      for( l_row = 1; l_row < BOARD_HEIGHT; l_row++ )
      {
        /* Set up the potential move. */
        l_move.start_row = l_move.end_row = l_row;
        l_move.start_col = l_move.end_col = l_col;
        l_move.start_value = l_move.end_value = l_workcol[l_row];
        l_move.pixels_to_end = 0;

        /* If this cell isn't empty, run some checks. */
        if ( l_workcol[l_row] > 0 )
        {
          /* Well then, can we move up any? */
          for ( l_altrow = l_row - 1; l_altrow >= 0; l_altrow-- )
          {
            /* Jump out if the cell is occupied. */
            if ( l_workcol[l_altrow] > 0 )
            {
              break;
            }

            /* Then shuffle up and carry on. */
            l_workcol[l_altrow] = l_workcol[l_altrow+1];
            l_workcol[l_altrow+1] = 0;

            /* And add the step to the move. */
            l_move.end_row--;
            l_move.pixels_to_end += 60;
          }

          /* Can we collapse into the cell next to us? */
          if ( !l_collapsed && l_workcol[l_altrow] == l_workcol[l_altrow+1] )
          {
            /* Keep the work column updated. */
            l_workcol[l_altrow] *= 2;
            l_workcol[l_altrow+1] = 0;
            l_collapsed = true;

            /* And the move. */
            l_move.end_row--;
            l_move.end_value *= 2;
            l_move.pixels_to_end += 60;
          }
        }

        /* And lastly, add the move into the queue. */
        if ( l_move.pixels_to_end > 0 )
        {
          /* Find an empty slot. */
          for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
          {
            if ( g_moves[l_index].pixels_to_end == 0 )
            {
              /* So fill it! */
              memcpy( &g_moves[l_index], &l_move, sizeof( move_t ) );

              /* And clear the start slot. */
              g_cells[l_move.start_row][l_move.start_col] = 0;

              /* And we're done. */
              break;
            }
          }
        }
      }
    }
    break;
  case picosystem::DOWN:
    /* Work through every column. */
    for( l_col = 0; l_col < BOARD_WIDTH; l_col++ )
    {
      /* Only allow one collapse per column. */
      l_collapsed = false;

      /* And we will work on a copy of the column, for ease of fiddling. */
      for ( uint_fast8_t l_index = 0; l_index < BOARD_HEIGHT; l_index++ )
      {
        l_workcol[l_index] = g_cells[l_index][l_col];
      }
      
      /* And each row in that column. */
      for( l_row = BOARD_HEIGHT-2; l_row >= 0; l_row-- )
      {
        /* Set up the potential move. */
        l_move.start_row = l_move.end_row = l_row;
        l_move.start_col = l_move.end_col = l_col;
        l_move.start_value = l_move.end_value = l_workcol[l_row];
        l_move.pixels_to_end = 0;

        /* If this cell isn't empty, run some checks. */
        if ( l_workcol[l_row] > 0 )
        {
          /* Well then, can we move down any? */
          for ( l_altrow = l_row + 1; l_altrow < BOARD_HEIGHT; l_altrow++ )
          {
            /* Jump out if the cell is occupied. */
            if ( l_workcol[l_altrow] > 0 )
            {
              break;
            }

            /* Then shuffle down and carry on. */
            l_workcol[l_altrow] = l_workcol[l_altrow-1];
            l_workcol[l_altrow-1] = 0;

            /* And add the step to the move. */
            l_move.end_row++;
            l_move.pixels_to_end += 60;
          }

          /* Can we collapse into the cell next to us? */
          if ( !l_collapsed && l_workcol[l_altrow] == l_workcol[l_altrow-1] )
          {
            /* Keep the work row updated. */
            l_workcol[l_altrow] *= 2;
            l_workcol[l_altrow-1] = 0;
            l_collapsed = true;

            /* And the move. */
            l_move.end_row++;
            l_move.end_value *= 2;
            l_move.pixels_to_end += 60;
          }
        }

        /* And lastly, add the move into the queue. */
        if ( l_move.pixels_to_end > 0 )
        {
          /* Find an empty slot. */
          for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
          {
            if ( g_moves[l_index].pixels_to_end == 0 )
            {
              /* So fill it! */
              memcpy( &g_moves[l_index], &l_move, sizeof( move_t ) );

              /* And clear the start slot. */
              g_cells[l_move.start_row][l_move.start_col] = 0;

              /* And we're done. */
              break;
            }
          }
        }
      }
    }
    break;
  case picosystem::LEFT:
    /* Work through every row. */
    for( l_row = 0; l_row < BOARD_HEIGHT; l_row++ )
    {
      /* Only allow one collapse per row. */
      l_collapsed = false;

      /* And we will work on a copy of the row, for ease of fiddling. */
      for ( uint_fast8_t l_index = 0; l_index < BOARD_WIDTH; l_index++ )
      {
        l_workrow[l_index] = g_cells[l_row][l_index];
      }
      
      /* And each column in that row. */
      for( l_col = 1; l_col < BOARD_WIDTH; l_col++ )
      {
        /* Set up the potential move. */
        l_move.start_row = l_move.end_row = l_row;
        l_move.start_col = l_move.end_col = l_col;
        l_move.start_value = l_move.end_value = l_workrow[l_col];
        l_move.pixels_to_end = 0;

        /* If this cell isn't empty, run some checks. */
        if ( l_workrow[l_col] > 0 )
        {
          /* Well then, can we move to the left any? */
          for ( l_altcol = l_col - 1; l_altcol >= 0; l_altcol-- )
          {
            /* Jump out if the cell is occupied. */
            if ( l_workrow[l_altcol] > 0 )
            {
              break;
            }

            /* Then shuffle to the left and carry on. */
            l_workrow[l_altcol] = l_workrow[l_altcol+1];
            l_workrow[l_altcol+1] = 0;

            /* And add the step to the move. */
            l_move.end_col--;
            l_move.pixels_to_end += 60;
          }

          /* Can we collapse into the cell next to us? */
          if ( !l_collapsed && l_workrow[l_altcol] == l_workrow[l_altcol+1] )
          {
            /* Keep the work row updated. */
            l_workrow[l_altcol] *= 2;
            l_workrow[l_altcol+1] = 0;
            l_collapsed = true;

            /* And the move. */
            l_move.end_col--;
            l_move.end_value *= 2;
            l_move.pixels_to_end += 60;
          }
        }

        /* And lastly, add the move into the queue. */
        if ( l_move.pixels_to_end > 0 )
        {
          /* Find an empty slot. */
          for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
          {
            if ( g_moves[l_index].pixels_to_end == 0 )
            {
              /* So fill it! */
              memcpy( &g_moves[l_index], &l_move, sizeof( move_t ) );

              /* And clear the start slot. */
              g_cells[l_move.start_row][l_move.start_col] = 0;

              /* And we're done. */
              break;
            }
          }
        }
      }
    }
    break;
  case picosystem::RIGHT:
    /* Work through every row. */
    for( l_row = 0; l_row < BOARD_HEIGHT; l_row++ )
    {
      /* Only allow one collapse per row. */
      l_collapsed = false;

      /* And we will work on a copy of the row, for ease of fiddling. */
      for ( uint_fast8_t l_index = 0; l_index < BOARD_WIDTH; l_index++ )
      {
        l_workrow[l_index] = g_cells[l_row][l_index];
      }
      
      /* And each column in that row. */
      for( l_col = BOARD_WIDTH-2; l_col >= 0; l_col-- )
      {
        /* Set up the potential move. */
        l_move.start_row = l_move.end_row = l_row;
        l_move.start_col = l_move.end_col = l_col;
        l_move.start_value = l_move.end_value = l_workrow[l_col];
        l_move.pixels_to_end = 0;

        /* If this cell isn't empty, run some checks. */
        if ( l_workrow[l_col] > 0 )
        {
          /* Well then, can we move to the left any? */
          for ( l_altcol = l_col + 1; l_altcol < BOARD_WIDTH; l_altcol++ )
          {
            /* Jump out if the cell is occupied. */
            if ( l_workrow[l_altcol] > 0 )
            {
              break;
            }

            /* Then shuffle to the left and carry on. */
            l_workrow[l_altcol] = l_workrow[l_altcol-1];
            l_workrow[l_altcol-1] = 0;

            /* And add the step to the move. */
            l_move.end_col++;
            l_move.pixels_to_end += 60;
          }

          /* Can we collapse into the cell next to us? */
          if ( !l_collapsed && l_workrow[l_altcol] == l_workrow[l_altcol-1] )
          {
            /* Keep the work row updated. */
            l_workrow[l_altcol] *= 2;
            l_workrow[l_altcol-1] = 0;
            l_collapsed = true;

            /* And the move. */
            l_move.end_col++;
            l_move.end_value *= 2;
            l_move.pixels_to_end += 60;
          }
        }

        /* And lastly, add the move into the queue. */
        if ( l_move.pixels_to_end > 0 )
        {
          /* Find an empty slot. */
          for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
          {
            if ( g_moves[l_index].pixels_to_end == 0 )
            {
              /* So fill it! */
              memcpy( &g_moves[l_index], &l_move, sizeof( move_t ) );

              /* And clear the start slot. */
              g_cells[l_move.start_row][l_move.start_col] = 0;

              /* And we're done. */
              break;
            }
          }
        }
      }
    }
    break;
  }

  /* Return a flag of whether or not we managed to move. */
  return true; // l_success;
}


/*
 * sprite_row - returns the row co-ordinate of the sprite to use for a given
 *              cell value.
 */

uint_fast8_t sprite_row( uint16_t p_cell_value )
{
  /* There will be faster ways to do this... */
  if ( p_cell_value <= 16 )
  {
    return 0;
  }

  if ( p_cell_value >= 512 )
  {
    return 112;
  }

  return 56;
}


/*
 * sprite_col - returns the column co-ordinates of the sprite to use for a
 *              given cell value.
 */

uint_fast8_t sprite_col( uint16_t p_cell_value )
{
  while( p_cell_value > 16 )
  {
    p_cell_value /= 16;
  }
  if ( p_cell_value == 2 ) return 0;
  if ( p_cell_value == 4 ) return 56;
  if ( p_cell_value == 8 ) return 112;
  return 168;
}


/*
 * init - the PicoSystem SDK entry point; called when the game is launched.
 */

void init( void )
{
  /* Make sure the board is empty. */
  board_clear();

  /* And make sure we're in a known state. */
  g_playing = false;
  g_moving = false;

  /* Make sure we're showing our splash page. */
  g_splashing = true;
  g_splash_tone = 0;

  /* And set the music to be off. */
  g_tune_note = g_tune_note_count = TUNE_LENGTH;

  /* Set up the voice that we'll use for beeps. */
  g_voice = picosystem::voice( 50, 100, 50, 100 );

  /* Remember the time, so we can keep track in updates. */
  g_last_update_us = picosystem::time_us();

  /* All done. */
  return;
}


/*
 * update - called every frame to update the game world. Passed a count
 *          of update frames since the game launched.
 *          This is *probably* around 50hz, but not guaranteed so we need 
 *          to measure time for ourselves...
 */

void update( uint32_t p_tick )
{
  uint_fast8_t l_direction = 0;

  /* We need to keep our own time. */
  uint32_t l_current_us = picosystem::time_us();
  uint32_t l_past_us = l_current_us - g_last_update_us;

  uint8_t l_ticks = l_past_us / 5000;

  /* Don't do anything until the splash screen is finished with. */
  if ( g_splashing )
  {
    g_splash_tone += l_ticks;

    /* First ping at an arbitrary point in the fade up! */
    if ( ( g_splash_tone > 50 ) && ( g_tune_note == TUNE_LENGTH ) )
    {
      g_tune[0].frequency = 800;
      g_tune[0].duration = 200;
      g_tune[1].frequency = 710;
      g_tune[1].duration = 200;
      g_tune[2].frequency = 525;
      g_tune[2].duration = 300;
      g_tune_note = 0;
      g_tune_note_count = 3;
    }

    if ( g_splash_tone >= 150 )
    {
      /* Start fading down. */
      g_splash_tone = 200;
      g_splashing = false;
    }

    /* Save the frame time. */
    g_last_update_us = l_current_us;
    return;
  }
  if ( g_splash_tone > 0 )
  {
    /* Reduce the splash. */
    g_splash_tone -= (g_splash_tone < l_ticks) ? g_splash_tone : l_ticks;

    /* Save the frame time. */
    g_last_update_us = l_current_us;
    return;
  }

  /* If we're not in the game, limited options. */
  if ( !g_playing )
  {
    /* Handle the start button. */
    if ( picosystem::pressed( picosystem::A ) )
    {
      /* Reset the board. */
      board_clear();

      /* Spawn a new cell. */
      board_spawn();

      /* And set the playing flag. */
      g_playing = true;
    }

    /* Save the frame time. */
    g_last_update_us = l_current_us;

    /* Other than that, nothing much to do. */
    return;
  }

  /* Need to process any active spawning. */
  if ( g_spawn.progress < 100 )
  {
    /* Tick through the progress by 5% per tick. */
    g_spawn.progress += l_ticks*4;

    /* Now if we've finished, we need to fill in the actual cell. */
    if ( g_spawn.progress >= 100 )
    {
      /* Set it to exactly 100 to make sure it's finished. */
      g_spawn.progress = 100;

      /* And set the cell. */
      g_cells[g_spawn.row][g_spawn.col] = g_spawn.value;
    }
  }

  /* Figure out if there are any active moves. */
  uint_fast8_t l_moving = 0;
  bool l_was_moving = false;
  for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
  {
    /* If there's still pixels to go, tick them down. */
    if ( g_moves[l_index].pixels_to_end > 0 )
    {
      l_was_moving = true;
      g_moves[l_index].pixels_to_end -= (g_moves[l_index].pixels_to_end<(l_ticks*5)) ? g_moves[l_index].pixels_to_end : l_ticks*5;

      /* If that means we've reached the end, make it permanent. */
      if ( g_moves[l_index].pixels_to_end == 0 )
      {
        /* Record it in the main grid. */
        g_cells[g_moves[l_index].end_row][g_moves[l_index].end_col] = g_moves[l_index].end_value;

        /* Check to see if it's a new record max.*/
        if ( g_moves[l_index].end_value > g_max_cell )
        {
          g_max_cell = g_moves[l_index].end_value;
          picosystem::play( g_voice, 750 + ( g_max_cell * 2 ), 300, 75 );
        }
      }
      else
      {
        /* Otherwise just keep track of what movements are active. */
        l_moving++;
      }
    }
  }

  /* Save the frame time - nothing else time dependent to do. */
  g_last_update_us = l_current_us;

  /* If we were moving and have now stopped, spawn. */
  if ( l_was_moving && l_moving == 0 )
  {
    board_spawn();
  }

  /* So, if we're still moving or spawning, we don't want user input. */
  if ( l_moving > 0 || g_spawn.progress < 100 )
  {
    return;
  }

  /* Well then, only one thing for it - we're waiting on user input. */
  if ( picosystem::pressed( picosystem::B ) )
  {
    /* Quick n dirty reset; probably shouldn't stay here forever... */
    g_playing = false;
  }

  /* Handle movement. */
  if ( picosystem::pressed( picosystem::UP ) )
  {
    l_direction = picosystem::UP;
  }
  if ( picosystem::pressed( picosystem::DOWN ) )
  {
    l_direction = picosystem::DOWN;
  }
  if ( picosystem::pressed( picosystem::LEFT ) )
  {
    l_direction = picosystem::LEFT;
  }
  if ( picosystem::pressed( picosystem::RIGHT ) )
  {
    l_direction = picosystem::RIGHT;
  }

  /* So, if we have a direction try to apply it. */
  if ( l_direction != 0 )
  {
    board_move( l_direction );
  }

  /* All done. */
  return;
}


/*
 * draw - called to draw the screen as and when required. No world updates
 *        should be done here, it's pure presentation.
 */

void draw( uint32_t p_tick )
{
  /* Handle any tune we're playing. */
  if ( g_tune_note < g_tune_note_count )
  {
    if ( !picosystem::audio_playing() )
    {
      play( g_voice, g_tune[g_tune_note].frequency, g_tune[g_tune_note].duration, 50 );
      g_tune_note++;
    }
  }

  /* If we have a splash screen to draw, just do that. */
  if ( g_splashing || g_splash_tone > 0 )
  {
    /* Show our logo at a suitable brightness. */
    picosystem::pen( 0, 0, 0 );
    picosystem::clear();
    picosystem::pen( 15, 15, 15, (g_splash_tone > 150) ? 15 : g_splash_tone / 10 );

    const uint8_t *l_logo = logo_ahnlak_1bit_data;
    for ( uint_fast8_t y = 24; y < 216; y++ )
    {
      for ( uint_fast8_t x = 24; x < 216; x += 8 )
      {
        for ( uint_fast8_t bit = 0; bit < 8; bit++ )
        {
          if ( *l_logo & ( 0b10000000 >> bit ) ) 
          {
            picosystem::pixel( x + bit, y );
          }
        }
        l_logo++;
      }
    }
    return;
  }

  /* Clear the screen back to a fairly neutral grey board. */
  picosystem::pen( 12, 12, 12 );
  picosystem::clear();

  /* Then draw in the individual cell borders. */
  picosystem::pen( 11, 11, 11 );
  for( uint_fast8_t l_index = 0; l_index < 5; l_index ++ )
  {
    picosystem::hline( 0,    l_index*60, picosystem::SCREEN->w );
    picosystem::hline( 0, l_index*60+01, picosystem::SCREEN->w );
    picosystem::hline( 0, l_index*60+58, picosystem::SCREEN->w );
    picosystem::hline( 0, l_index*60+59, picosystem::SCREEN->w );
    picosystem::vline( l_index*60,    0, picosystem::SCREEN->h );
    picosystem::vline( l_index*60+01, 0, picosystem::SCREEN->h );
    picosystem::vline( l_index*60+58, 0, picosystem::SCREEN->h );
    picosystem::vline( l_index*60+59, 0, picosystem::SCREEN->h );
  }

  /* Work through each cell now, drawing a block if one is present. */
  for( uint_fast8_t l_row = 0; l_row < BOARD_HEIGHT; l_row++ )
  {
    for( uint_fast8_t l_col = 0; l_col < BOARD_WIDTH; l_col++ )
    {
      /* So, only have stuff to draw when there's a value. */
      if ( g_cells[l_row][l_col] == 0 )
      {
        continue;
      }

      /* Draw the value into the cell, at least. */
      picosystem::blit( &spritesheet_buffer, 
        sprite_col( g_cells[l_row][l_col] ), sprite_row( g_cells[l_row][l_col] ),
        56, 56, (l_col * 60) + 2, (l_row * 60) + 2 );
    }
  }

  /* If we're spawning draw that in an animated manner. */
  if ( g_spawn.progress < 100 )
  {
    /* Blit it with a suitable offset. */
    uint_fast8_t l_offset = 25 - ( g_spawn.progress / 4 );;

    picosystem::blit( &spritesheet_buffer,
      sprite_col( g_spawn.value ) + l_offset, sprite_row( g_spawn.value ) + l_offset,
      56 - ( l_offset * 2 ), 56 - ( l_offset * 2 ),
      ( g_spawn.col * 60 ) + 2 + l_offset, ( g_spawn.row * 60 ) + 2 + l_offset );
  }

  /* And work through the moving blocks too. */
  for ( uint_fast8_t l_index = 0; l_index < MOVE_MAX; l_index++ )
  {
    /* Only things which are moving, obviously. */
    if ( g_moves[l_index].pixels_to_end == 0 )
    {
      continue;
    }

    /* Work out where to draw, at the end. */
    uint_fast8_t l_move_row = ( g_moves[l_index].end_row * 60 ) + 2;
    uint_fast8_t l_move_col = ( g_moves[l_index].end_col * 60 ) + 2;

    /* Horizontal? */
    if ( g_moves[l_index].start_row == g_moves[l_index].end_row )
    {
      /* Moving right? */
      if ( g_moves[l_index].start_col < g_moves[l_index].end_col )
      {
        l_move_col -= g_moves[l_index].pixels_to_end;
      }
      else
      {
        /* So, moving left then. */
        l_move_col += g_moves[l_index].pixels_to_end;
      }
    }
    else
    {
      /* So, moving vertically - moving up? */
      if ( g_moves[l_index].start_row > g_moves[l_index].end_row )
      {
        l_move_row += g_moves[l_index].pixels_to_end;
      }
      else
      {
        /* Well then, going down... */
        l_move_row -= g_moves[l_index].pixels_to_end;
      }
    }

    /* Good, now we can draw! */
    picosystem::blit( &spritesheet_buffer,
      sprite_col( g_moves[l_index].start_value ), sprite_row( g_moves[l_index].start_value ),
      56, 56, l_move_col, l_move_row );
  }

  /* If we're not playing, add the title and start prompt. */
  if ( !g_playing )
  {
    /* Fade the play area back some. */
    picosystem::pen( 5, 5, 5, 14 );
    picosystem::frect( 0, 0, picosystem::SCREEN->w, picosystem::SCREEN->h );

    /* And then the title stuff. */
    picosystem::blit( &spritesheet_buffer, 0, 168, 112, 112, 
      ( picosystem::SCREEN->w - 112 ) / 2, 48 );
    picosystem::blit( &spritesheet_buffer, 112, 168, 112, 48,
      ( picosystem::SCREEN->w - 112 ) / 2, picosystem::SCREEN->h - ( 32 + 48 ) );
  }

  /* All done. */
  return;
}


/* End of file 2040-eight.cpp */
