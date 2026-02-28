#ifndef RENDER_H
#define RENDER_H

#include "redefines.h"
#include "spectrum.h"

void render_draw(const spectrum_state_t *s, i32 cursor_lock_enabled, i32 cursor_locked_index, i32 cursor_hover_index);

#endif
