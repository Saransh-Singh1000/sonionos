#ifndef SONION_WALLPAPER_H
#define SONION_WALLPAPER_H

int wallpaper_load(const char *path);
void wallpaper_draw(int screen_width, int screen_height);
void wallpaper_unload(void);

#endif
