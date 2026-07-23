#ifndef _APP_EGUI_RESOURCE_GENERATE_H_
#define _APP_EGUI_RESOURCE_GENERATE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This port creates its PSRAM-backed image descriptors at runtime, so there
 * are no generated merge.bin resources. Keep the conventional base entry to
 * satisfy EmbeddedGUI's resource-manager contract. */
enum
{
    EGUI_EXT_RES_ID_BASE = 0x00,
    EGUI_EXT_RES_ID_MAX,
};

extern const uint32_t egui_ext_res_id_map[EGUI_EXT_RES_ID_MAX];

#ifdef __cplusplus
}
#endif

#endif /* _APP_EGUI_RESOURCE_GENERATE_H_ */
