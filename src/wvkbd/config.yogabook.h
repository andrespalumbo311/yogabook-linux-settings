#ifndef CONFIG_YOGABOOK_H
#define CONFIG_YOGABOOK_H

#define DEFAULT_FONT "Sans 14"
#define DEFAULT_ROUNDING 6
#define SHIFT_SPACE_IS_TAB
static const int transparency = 255;

struct clr_scheme schemes[] = {
{
  /* default keys (scheme 0) */
  .bg = {.bgra = {15, 15, 15, transparency}},
  .fg = {.bgra = {45, 45, 45, transparency}},
  .high = {.bgra = {100, 100, 100, transparency}},
  .swipe = {.bgra = {100, 255, 100, 64}},
  .text = {.color = UINT32_MAX},
  .text_press = {.color = UINT32_MAX},
  .text_swipe = {.color = UINT32_MAX},
  .font = DEFAULT_FONT,
  .rounding = DEFAULT_ROUNDING,
},
{
  /* modifier & action keys (scheme 1) */
  .bg = {.bgra = {15, 15, 15, transparency}},
  .fg = {.bgra = {32, 32, 32, transparency}},
  .high = {.bgra = {100, 100, 100, transparency}},
  .swipe = {.bgra = {100, 255, 100, 64}},
  .text_press = {.color = UINT32_MAX},
  .text_swipe = {.color = UINT32_MAX},
  .text = {.color = UINT32_MAX},
  .font = DEFAULT_FONT,
  .rounding = DEFAULT_ROUNDING,
}
};

/* layers is an ordered list of layouts, used to cycle through */
static enum layout_id layers[] = {
  Full,
  Special,
  Special2,
  Accents,
  Emoji,
  NumLayouts
};

/* landscape_layers is used in landscape orientation */
static enum layout_id landscape_layers[] = {
  Full,
  Special,
  Special2,
  Accents,
  Emoji,
  NumLayouts
};

#endif /* CONFIG_YOGABOOK_H */
