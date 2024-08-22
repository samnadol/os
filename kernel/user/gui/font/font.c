#include "font.h"

#include "../../../lib/string.h"

#include "font_small.h"
#include "font_big.h"
#include "font_num.h"
#include "font_misc.h"

uint8_t *chars[] = {
    font_null, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, font_space, 0, 0, 0, 0, 0, 0, 0,
    font_parenthesis_open, font_parenthesis_close, 0, font_plus, font_comma, font_dash, font_period, font_slash, font_0, font_1, 
    font_2, font_3, font_4, font_5, font_6, font_7, font_8, font_9, font_colon, 0,
    0, 0, 0, font_question, font_at, font_A, font_B, font_C, font_D, font_E, font_F,
    font_G, font_H, font_I, font_J, font_K, font_L, font_M, font_N, font_O, font_P,
    font_Q, font_R, font_S, font_T, font_U, font_V, font_W, font_X, font_Y, font_Z,
    0, 0, 0, 0, 0, 0, font_a, font_b, font_c, font_d,
    font_e, font_f, font_g, font_h, font_i, font_j, font_k, font_l, font_m, font_n, 
    font_o, font_p, font_q, font_r, font_s, font_t, font_u, font_v, font_w, font_x, 
    font_y, font_z, 0, 0, 0, 0, 0, 0, 0, 0,
};

uint8_t *font_get_char(char c)
{
    if (!chars[(size_t)c])
        return font_unknown;
    return chars[(size_t)c];
}