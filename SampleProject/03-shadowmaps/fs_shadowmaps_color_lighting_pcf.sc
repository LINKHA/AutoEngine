$input v_normal, v_view, v_viewEnv, v_shadowcoord

#define SM_PCF 1

#include "fs_shadowmaps_color_lighting.sh"

void main()
{
#include "fs_shadowmaps_color_lighting_main.sh"
}
