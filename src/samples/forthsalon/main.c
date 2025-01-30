/*
 * lwan - web server
 * Copyright (c) 2025 L. A. F. Pereira <l@tia.mat.br>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <math.h>

#include "lwan.h"
#include "forth.h"
#include "gif.h"

/* Twister by boomlinde
 * https://forthsalon.appspot.com/haiku-view/ag5mb3J0aHNhbG9uLWhyZHISCxIFSGFpa3UYgICAvJXxgwsM
 */
static const char twister[] = ": t' t pi * 2 / ;\n"
": l * + sin ;\n"
": r t' 1 y t' + 4 l + 1.57 ;\n"
": x' x 4 * 2 - t' y 3 l + ;\n"
": v 2dup x' >= swap x' < * -rot swap - l ;\n"
": a r 4 l ; : b r 1 l ;\n"
": c r 2 l ; : d r 3 l ;\n"
"0 d a v a b v b c v c d v 0.1 0.2";

static void destroy_forth_ctx(void *p) { forth_free(p); }
static void destroy_gif_writer(void *p) { GifEnd(p); }

LWAN_HANDLER_ROUTE(twister, "/")
{
    struct forth_ctx *f = forth_new();
    double current_time = (int32_t)time(NULL);

    coro_defer(request->conn->coro, destroy_forth_ctx, f);

    if (!forth_parse_string(f, twister))
        return HTTP_INTERNAL_ERROR;

    uint8_t *frame_buffer = coro_malloc(request->conn->coro, 64 * 64 * 4);
    if (!frame_buffer)
        return HTTP_INTERNAL_ERROR;

    response->mime_type = "image/gif";

    if (!lwan_response_set_chunked(request, HTTP_OK))
        return HTTP_INTERNAL_ERROR;

    GifWriter writer = {};
    coro_defer(request->conn->coro, destroy_gif_writer, &writer);

    GifBegin(&writer, response->buffer, 64, 64, 2, 8, true);

    for (int frame = 0; frame < 1000; frame++) {
        for (int x = 0; x < 64; x++) {
            for (int y = 0; y < 64; y++) {
                uint8_t *pixel = &frame_buffer[4 * (y * 64 + x)];

                struct forth_vars vars = {
                    .x = x / 64.,
                    .y = y / 64.,
                    .t = current_time,
                };
                if (!forth_run(f, &vars))
                    return HTTP_INTERNAL_ERROR;
                switch (forth_d_stack_len(f)) {
                case 3:
                    pixel[3] = 0;
                    pixel[2] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    pixel[1] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    pixel[0] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    break;
                case 4:
                    pixel[3] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    pixel[2] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    pixel[1] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    pixel[0] = (uint8_t)(round(forth_d_stack_pop(f) * 255.));
                    break;
                default:
                    return HTTP_INTERNAL_ERROR;
                }
            }
        }

        GifWriteFrame(&writer, frame_buffer, 64, 64, 2, 8, true);
        lwan_response_send_chunk(request);
        lwan_request_sleep(request, 16);
        current_time += .016;
    }

    return HTTP_OK;
}

int main(void) { return lwan_main(); }
