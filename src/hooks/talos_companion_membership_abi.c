#include "hooks/talos_companion_membership_abi.h"

#include <string.h>

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))
#define SYMBOL_BIT(symbol_value) (UINT32_C(1) << (uint32_t)(symbol_value))

/* Full exact bodies for the two synchronous presentation closures that the
 * public RemovePlayer ID-10 path reaches after roster mutation. */
static const uint8_t stat_display_camera_sync_bytes[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x64,0x80,0x3d,0x08,
    0x9e,0x80,0x00,0x00,0x8b,0x51,0x58,0xa1,0x1c,0x8d,0x80,0x00,
    0x53,0x56,0x57,0x89,0x54,0x24,0x20,0x75,0x6f,0x85,0xc0,0x0f,
    0x84,0x2e,0x02,0x00,0x00,0xd9,0x80,0x4c,0x01,0x00,0x00,0xc6,
    0x05,0x08,0x9e,0x80,0x00,0x01,0xd9,0x1d,0xcc,0xcd,0x80,0x00,
    0xd9,0x80,0x50,0x01,0x00,0x00,0xd9,0x1d,0xc8,0xcd,0x80,0x00,
    0xd9,0x80,0x54,0x01,0x00,0x00,0xd9,0x1d,0xc4,0xcd,0x80,0x00,
    0xd9,0x80,0x58,0x01,0x00,0x00,0xd9,0x1d,0xc0,0xcd,0x80,0x00,
    0xd9,0x05,0xcc,0xcd,0x80,0x00,0xd9,0x1d,0x3c,0x30,0x7c,0x00,
    0xd9,0x05,0xc4,0xcd,0x80,0x00,0xd9,0x1d,0x44,0x30,0x7c,0x00,
    0xd9,0x05,0xc8,0xcd,0x80,0x00,0xd9,0x1d,0x40,0x30,0x7c,0x00,
    0xd9,0x05,0xc0,0xcd,0x80,0x00,0xd9,0x1d,0x48,0x30,0x7c,0x00,
    0xd9,0x05,0x3c,0x30,0x7c,0x00,0xd9,0xc0,0xd9,0xee,0xd9,0xc0,
    0xdd,0xea,0xdf,0xe0,0xdd,0xd9,0xf6,0xc4,0x44,0x7b,0x0a,0xd9,
    0xc9,0xd9,0x1d,0xcc,0xcd,0x80,0x00,0xeb,0x02,0xdd,0xd9,0xd9,
    0x05,0x44,0x30,0x7c,0x00,0xd9,0xc0,0xd9,0xc2,0xda,0xe9,0xdf,
    0xe0,0xf6,0xc4,0x44,0x7b,0x08,0xd9,0x1d,0xc4,0xcd,0x80,0x00,
    0xeb,0x02,0xdd,0xd8,0xd9,0x05,0x40,0x30,0x7c,0x00,0xd9,0xc0,
    0xd9,0xc2,0xda,0xe9,0xdf,0xe0,0xf6,0xc4,0x44,0x7b,0x08,0xd9,
    0x1d,0xc8,0xcd,0x80,0x00,0xeb,0x02,0xdd,0xd8,0xd9,0x05,0x48,
    0x30,0x7c,0x00,0xd9,0xc0,0xdd,0xea,0xdf,0xe0,0xdd,0xd9,0xf6,
    0xc4,0x44,0x7b,0x08,0xd9,0x1d,0xc0,0xcd,0x80,0x00,0xeb,0x02,
    0xdd,0xd8,0x8b,0x81,0xcc,0x00,0x00,0x00,0x8b,0x58,0x08,0x8d,
    0xb3,0x90,0x00,0x00,0x00,0xb9,0x10,0x00,0x00,0x00,0x8d,0x7c,
    0x24,0x30,0xf3,0xa5,0x8b,0x0d,0x30,0x2f,0x7c,0x00,0x8b,0x41,
    0x20,0x85,0xc0,0x74,0x0a,0x8b,0x40,0x34,0x05,0xc0,0x00,0x00,
    0x00,0xeb,0x02,0x33,0xc0,0xd9,0x00,0xd8,0x64,0x24,0x60,0xd9,
    0x5c,0x24,0x24,0xd9,0x40,0x04,0xd8,0x64,0x24,0x64,0xd9,0x5c,
    0x24,0x28,0xd9,0x40,0x08,0xd8,0x64,0x24,0x68,0xd9,0x5c,0x24,
    0x2c,0xd9,0x44,0x24,0x24,0xd9,0x44,0x24,0x28,0xd9,0x44,0x24,
    0x2c,0xd9,0xc1,0xde,0xca,0xd9,0xc2,0xde,0xcb,0xd9,0xc9,0xde,
    0xc2,0xdc,0xc8,0xde,0xc1,0xd9,0x5c,0x24,0x18,0xd9,0x44,0x24,
    0x18,0xe8,0x7a,0x57,0x16,0x00,0xd9,0x5c,0x24,0x18,0xd9,0x44,
    0x24,0x18,0x32,0xc9,0xd9,0x5c,0x24,0x1c,0xd9,0xe8,0xd9,0x54,
    0x24,0x18,0xd9,0x44,0x24,0x1c,0xd9,0x05,0xc4,0xcd,0x80,0x00,
    0xd8,0xd9,0xdf,0xe0,0xf6,0xc4,0x41,0x7a,0x08,0xd9,0x05,0xc0,
    0xcd,0x80,0x00,0xeb,0x15,0xd9,0x05,0xcc,0xcd,0x80,0x00,0xd8,
    0xd9,0xdf,0xe0,0xf6,0xc4,0x01,0x75,0x0c,0xd9,0x05,0xc8,0xcd,
    0x80,0x00,0xd9,0x5c,0x24,0x18,0xb1,0x01,0xd9,0xee,0xd9,0x54,
    0x24,0x5c,0xd9,0x54,0x24,0x54,0xd9,0x54,0x24,0x50,0xd9,0x54,
    0x24,0x4c,0xd9,0x54,0x24,0x48,0xd9,0x54,0x24,0x40,0xd9,0x54,
    0x24,0x3c,0xd9,0x54,0x24,0x38,0xd9,0x5c,0x24,0x34,0xd9,0xc9,
    0xd9,0x54,0x24,0x6c,0xd9,0x54,0x24,0x58,0xd9,0x54,0x24,0x44,
    0xd9,0x5c,0x24,0x30,0x84,0xc9,0x74,0x16,0xd8,0x4c,0x24,0x18,
    0xd9,0x5c,0x24,0x58,0xd9,0x44,0x24,0x58,0xd9,0x54,0x24,0x44,
    0xd9,0x5c,0x24,0x30,0xeb,0x02,0xdd,0xd8,0x8b,0x44,0x24,0x20,
    0x8d,0xbb,0x90,0x00,0x00,0x00,0xb9,0x10,0x00,0x00,0x00,0x8d,
    0x74,0x24,0x30,0xf3,0xa5,0xba,0x01,0x00,0x00,0x00,0x66,0x01,
    0x53,0x2c,0x66,0x01,0x50,0x2c,0x8d,0xb8,0x90,0x00,0x00,0x00,
    0xb9,0x10,0x00,0x00,0x00,0x8d,0x74,0x24,0x30,0xf3,0xa5,0x5f,
    0x5e,0x5b,0x8b,0xe5,0x5d,0xc3
};
static const uint8_t hud_resource_selector_bytes[] = {
    0x8b,0x44,0x24,0x04,0x8b,0x90,0x94,0x00,0x00,0x00,0x8b,0x42,
    0x3c,0x0f,0xb6,0x50,0x09,0x3b,0xca,0x73,0x10,0x8b,0x40,0x04,
    0x8b,0x04,0x88,0x85,0xc0,0x74,0x06,0x0f,0xb6,0x40,0x19,0xeb,
    0x02,0x33,0xc0,0x48,0x83,0xf8,0x0a,0x0f,0x87,0xb7,0x02,0x00,
    0x00,0xff,0x24,0x85,0x50,0xbe,0x52,0x00,0x80,0x3d,0x09,0x9e,
    0x80,0x00,0x00,0x74,0x06,0xb8,0x58,0x22,0x6d,0x00,0xc3,0xa1,
    0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,0x36,0x01,0x00,0x00,0x76,
    0x19,0x8b,0x48,0x10,0x8b,0x81,0xd8,0x04,0x00,0x00,0xf7,0x00,
    0x00,0x00,0x00,0x80,0x74,0x04,0x83,0xc0,0x04,0xc3,0x8b,0x40,
    0x04,0xc3,0x68,0x36,0x01,0x00,0x00,0x68,0x90,0x22,0x6d,0x00,
    0xba,0x50,0x16,0x76,0x00,0xe8,0x8e,0x73,0xee,0xff,0x83,0xc4,
    0x08,0xb8,0x50,0x16,0x76,0x00,0xc3,0x80,0x3d,0x09,0x9e,0x80,
    0x00,0x00,0x75,0xad,0xa1,0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,
    0x37,0x01,0x00,0x00,0x76,0x15,0x8b,0x50,0x10,0x8b,0x82,0xdc,
    0x04,0x00,0x00,0xf7,0x00,0x00,0x00,0x00,0x80,0x74,0xb7,0x83,
    0xc0,0x04,0xc3,0x68,0x37,0x01,0x00,0x00,0xeb,0xb5,0x80,0x3d,
    0x09,0x9e,0x80,0x00,0x00,0x0f,0x85,0x76,0xff,0xff,0xff,0xa1,
    0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,0x38,0x01,0x00,0x00,0x76,
    0x15,0x8b,0x40,0x10,0x8b,0x80,0xe0,0x04,0x00,0x00,0xf7,0x00,
    0x00,0x00,0x00,0x80,0x74,0x80,0x83,0xc0,0x04,0xc3,0x68,0x38,
    0x01,0x00,0x00,0xe9,0x7b,0xff,0xff,0xff,0x80,0x3d,0x09,0x9e,
    0x80,0x00,0x00,0x0f,0x85,0x3c,0xff,0xff,0xff,0xa1,0x5c,0x30,
    0x7c,0x00,0x81,0x78,0x08,0x39,0x01,0x00,0x00,0x76,0x19,0x8b,
    0x48,0x10,0x8b,0x81,0xe4,0x04,0x00,0x00,0xf7,0x00,0x00,0x00,
    0x00,0x80,0x0f,0x84,0x42,0xff,0xff,0xff,0x83,0xc0,0x04,0xc3,
    0x68,0x39,0x01,0x00,0x00,0xe9,0x3d,0xff,0xff,0xff,0x80,0x3d,
    0x09,0x9e,0x80,0x00,0x00,0x0f,0x85,0xfe,0xfe,0xff,0xff,0xa1,
    0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,0x3a,0x01,0x00,0x00,0x76,
    0x19,0x8b,0x50,0x10,0x8b,0x82,0xe8,0x04,0x00,0x00,0xf7,0x00,
    0x00,0x00,0x00,0x80,0x0f,0x84,0x04,0xff,0xff,0xff,0x83,0xc0,
    0x04,0xc3,0x68,0x3a,0x01,0x00,0x00,0xe9,0xff,0xfe,0xff,0xff,
    0x80,0x3d,0x09,0x9e,0x80,0x00,0x00,0x0f,0x85,0xc0,0xfe,0xff,
    0xff,0xa1,0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,0x3b,0x01,0x00,
    0x00,0x76,0x19,0x8b,0x40,0x10,0x8b,0x80,0xec,0x04,0x00,0x00,
    0xf7,0x00,0x00,0x00,0x00,0x80,0x0f,0x84,0xc6,0xfe,0xff,0xff,
    0x83,0xc0,0x04,0xc3,0x68,0x3b,0x01,0x00,0x00,0xe9,0xc1,0xfe,
    0xff,0xff,0x80,0x3d,0x09,0x9e,0x80,0x00,0x00,0x0f,0x85,0x82,
    0xfe,0xff,0xff,0xa1,0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,0x3c,
    0x01,0x00,0x00,0x76,0x19,0x8b,0x48,0x10,0x8b,0x81,0xf0,0x04,
    0x00,0x00,0xf7,0x00,0x00,0x00,0x00,0x80,0x0f,0x84,0x88,0xfe,
    0xff,0xff,0x83,0xc0,0x04,0xc3,0x68,0x3c,0x01,0x00,0x00,0xe9,
    0x83,0xfe,0xff,0xff,0x80,0x3d,0x09,0x9e,0x80,0x00,0x00,0x0f,
    0x85,0x44,0xfe,0xff,0xff,0xa1,0x5c,0x30,0x7c,0x00,0x81,0x78,
    0x08,0x3d,0x01,0x00,0x00,0x76,0x19,0x8b,0x50,0x10,0x8b,0x82,
    0xf4,0x04,0x00,0x00,0xf7,0x00,0x00,0x00,0x00,0x80,0x0f,0x84,
    0x4a,0xfe,0xff,0xff,0x83,0xc0,0x04,0xc3,0x68,0x3d,0x01,0x00,
    0x00,0xe9,0x45,0xfe,0xff,0xff,0x80,0x3d,0x09,0x9e,0x80,0x00,
    0x00,0x0f,0x85,0x06,0xfe,0xff,0xff,0xa1,0x5c,0x30,0x7c,0x00,
    0x81,0x78,0x08,0x3e,0x01,0x00,0x00,0x76,0x19,0x8b,0x40,0x10,
    0x8b,0x80,0xf8,0x04,0x00,0x00,0xf7,0x00,0x00,0x00,0x00,0x80,
    0x0f,0x84,0x0c,0xfe,0xff,0xff,0x83,0xc0,0x04,0xc3,0x68,0x3e,
    0x01,0x00,0x00,0xe9,0x07,0xfe,0xff,0xff,0x80,0x3d,0x09,0x9e,
    0x80,0x00,0x00,0x0f,0x85,0xc8,0xfd,0xff,0xff,0xa1,0x5c,0x30,
    0x7c,0x00,0x81,0x78,0x08,0x3f,0x01,0x00,0x00,0x76,0x19,0x8b,
    0x48,0x10,0x8b,0x81,0xfc,0x04,0x00,0x00,0xf7,0x00,0x00,0x00,
    0x00,0x80,0x0f,0x84,0xce,0xfd,0xff,0xff,0x83,0xc0,0x04,0xc3,
    0x68,0x3f,0x01,0x00,0x00,0xe9,0xc9,0xfd,0xff,0xff,0x80,0x3d,
    0x09,0x9e,0x80,0x00,0x00,0x0f,0x85,0x8a,0xfd,0xff,0xff,0xa1,
    0x5c,0x30,0x7c,0x00,0x81,0x78,0x08,0x40,0x01,0x00,0x00,0x76,
    0x19,0x8b,0x50,0x10,0x8b,0x82,0x00,0x05,0x00,0x00,0xf7,0x00,
    0x00,0x00,0x00,0x80,0x0f,0x84,0x90,0xfd,0xff,0xff,0x83,0xc0,
    0x04,0xc3,0x68,0x40,0x01,0x00,0x00,0xe9,0x8b,0xfd,0xff,0xff,
    0xb8,0xd0,0x34,0x6d,0x00,0xc3
};
static const uint8_t hud_string_assign_bytes[] = {
    0x55,0x8b,0x6c,0x24,0x08,0x85,0xed,0x0f,0x84,0xbf,0x00,0x00,
    0x00,0x66,0x83,0x7d,0x00,0x00,0x0f,0x84,0xb4,0x00,0x00,0x00,
    0x56,0x8b,0xf5,0x8d,0x4e,0x02,0x8b,0xff,0x66,0x8b,0x06,0x83,
    0xc6,0x02,0x66,0x85,0xc0,0x75,0xf5,0x2b,0xf1,0xd1,0xfe,0x53,
    0x8d,0x5e,0x01,0x83,0xfb,0x1c,0x76,0x52,0xf7,0x07,0x00,0x00,
    0x00,0x80,0x75,0x0c,0x8b,0x47,0x04,0x50,0xe8,0x0d,0xe9,0x08,
    0x00,0x83,0xc4,0x04,0x33,0xc9,0x8b,0xc3,0xba,0x02,0x00,0x00,
    0x00,0xf7,0xe2,0x0f,0x90,0xc1,0xf7,0xd9,0x0b,0xc8,0x51,0xe8,
    0x2a,0xe8,0x08,0x00,0x8d,0x4c,0x36,0x02,0x51,0x55,0x50,0x89,
    0x47,0x04,0xe8,0xcd,0x45,0x09,0x00,0x8b,0x57,0x04,0x83,0xc4,
    0x10,0x33,0xc0,0x66,0x89,0x04,0x72,0x8b,0x47,0x04,0x5b,0x89,
    0x37,0x5e,0x5d,0xc2,0x04,0x00,0xf7,0x07,0x00,0x00,0x00,0x80,
    0x75,0x0c,0x8b,0x4f,0x04,0x51,0xe8,0xbb,0xe8,0x08,0x00,0x83,
    0xc4,0x04,0x8d,0x54,0x36,0x02,0x52,0x8d,0x5f,0x04,0x55,0x53,
    0xc7,0x03,0x00,0x00,0x00,0x00,0xc7,0x07,0x00,0x00,0x00,0x80,
    0xe8,0x87,0x45,0x09,0x00,0x83,0xc4,0x0c,0x81,0xce,0x00,0x00,
    0x00,0x80,0x8b,0xc3,0x5b,0x89,0x37,0x5e,0x5d,0xc2,0x04,0x00,
    0xf7,0x07,0x00,0x00,0x00,0x80,0x75,0x0c,0x8b,0x47,0x04,0x50,
    0xe8,0x79,0xe8,0x08,0x00,0x83,0xc4,0x04,0xc7,0x47,0x04,0x00,
    0x00,0x00,0x00,0xc7,0x07,0x00,0x00,0x00,0x80,0x33,0xc0,0x5d,
    0xc2,0x04,0x00
};
enum {
    RVA_GET_GROUP_PLAYERS = 0x00025100u,
    RVA_GET_PC = 0x00104480u,
    RVA_ADD_PLAYER = 0x00023230u,
    RVA_REMOVE_PLAYER = 0x00023390u,
    RVA_IS_PLAYER = 0x00023a00u,
    RVA_GET_INDEX = 0x00023ac0u,
    RVA_ALL_PENDING_LOADED = 0x00004fb0u,
    RVA_IN_COMBAT = 0x00004fa0u,
    RVA_ASYNC_ACTIVE = 0x000fdd80u,
    RVA_GEL_RESOLVER = 0x001bf4e0u,
    RVA_GEL_CLEANUP = 0x000015e0u,
    RVA_GEL_VTABLE = 0x002c0098u,
    RVA_GEL_DELETING_DESTRUCTOR = 0x00001b30u,
    RVA_GEL_GET_RAW_ENTITY = 0x000017b0u,
    RVA_GEL_GET_TYPE_NAME = 0x00001820u,
    RVA_GEL_WRAPPER_FACTORY = 0x000019c0u,
    RVA_GEL_CORE_DESTRUCTOR = 0x00001b50u,
    RVA_PTR_REGISTRATION_CONSTRUCTOR = 0x000020d0u,
    RVA_PTR_REGISTRY_DELETE_ALL = 0x00002190u,
    RVA_PTR_REGISTRY_ERASE = 0x000021c0u,
    RVA_PTR_REGISTRY_FIND = 0x00002410u,
    RVA_AI_LISTENER_VTABLE = 0x002ca244u,
    RVA_AI_LISTENER_ADD = 0x000f2b00u,
    RVA_AI_LISTENER_REMOVE = 0x000f2b30u,
    RVA_FORMATION_ADD = 0x000b2cb0u,
    RVA_FORMATION_REMOVE = 0x000b2d50u,
    RVA_FORMATION_CANONICALIZER = 0x000b3dd0u,
    RVA_GEL_POINTER_CONSTRUCTOR = 0x00012e90u,
    RVA_GROUP_ADD_CORE = 0x00023280u,
    RVA_GROUP_REMOVE_CORE = 0x000233e0u,
    RVA_GEL_POINTER_COPY = 0x000015b0u,
    RVA_GEL_POINTER_ASSIGN = 0x00001750u,
    RVA_GET_PC_LOOKUP = 0x00103fb0u,
    RVA_GET_PC_WRAPPER_CONSTRUCTOR = 0x00001c20u,
    RVA_GEL_RESOLVER_SEH = 0x002947f8u,
    RVA_GEL_FACTORY_SEH = 0x002947a3u,
    RVA_GEL_DESTRUCTOR_SEH = 0x00294783u,
    RVA_GEL_TYPE_NAME_TEXT = 0x002c0018u,
    RVA_PTR_OBJECT_VTABLE = 0x002c018cu,
    RVA_PTR_ALLOCATION_FAILURE_VTABLE = 0x002c017cu,
    RVA_PTR_REGISTRY_ERROR_TEXT = 0x002c0138u,
    RVA_PTR_REGISTRY_ROOT = 0x003c3904u,
    RVA_PTR_REGISTRY_COUNT = 0x003c3908u,
    RVA_NATIVE_HEAP_ALLOCATE = 0x002484fau,
    RVA_NATIVE_HEAP_FREE = 0x0024844fu,
    RVA_PTR_REGISTRY_INSERT = 0x00002750u,
    RVA_PTR_ALLOCATION_FAILURE = 0x00002070u,
    RVA_GEL_OBSERVER_LINK = 0x00001e00u,
    RVA_GEL_OBSERVER_COPY = 0x00002000u,
    RVA_PTR_TREE_ERASE_ERROR = 0x00258df0u,
    RVA_PTR_TREE_ERASE_PREPARE = 0x00002660u,
    RVA_PTR_TREE_MAXIMUM = 0x00002590u,
    RVA_PTR_TREE_ROTATE_LEFT = 0x000024f0u,
    RVA_PTR_TREE_ROTATE_RIGHT = 0x00002540u,
    RVA_GROUP_PRIMARY_CHANGED = 0x000237b0u,
    RVA_PLAYER_SET_ARMED = 0x000db5f0u,
    RVA_STAT_DISPLAY_REFRESH = 0x00129780u,
    RVA_GROUP_PREPARE_PRIMARY_REMOVAL = 0x00023f60u,
    RVA_GROUP_REMOVE_EPILOGUE = 0x0009dba0u,
    RVA_UI_CONTROLLER_GLOBAL = 0x003c2f88u,
    RVA_UI_CONTROLLER_VTABLE = 0x002caf9cu,
    RVA_UI_CONTROLLER_DISPATCH = 0x0009d9b0u,
    RVA_UI_DISPATCH_JUMP_TABLE = 0x0009db70u,
    RVA_UI_DISPATCH_SELECTOR_TABLE = 0x0009db84u,
    RVA_HUD_CHILD_VTABLE = 0x002cb3e4u,
    RVA_HUD_CHILD_DISPATCH = 0x000a5930u,
    RVA_HUD_CHILD_JUMP_TABLE = 0x000a5aa8u,
    RVA_HUD_CHILD_SELECTOR_TABLE = 0x000a5ac4u,
    RVA_HUD_PORTRAIT_STATE_HANDLER = 0x000a5985u,
    RVA_HUD_PORTRAIT_STATE_SETTER = 0x000aa910u,
    RVA_STAT_DISPLAY_CONSTRUCTOR = 0x00127ce0u,
    RVA_STAT_DISPLAY_PRIMARY_VTABLE = 0x002d21e4u,
    RVA_STAT_DISPLAY_SECONDARY_VTABLE = 0x002d2224u,
    RVA_STAT_DISPLAY_PRIMARY_DESTRUCTOR = 0x0012a6c0u,
    RVA_STAT_DISPLAY_SECONDARY_DESTRUCTOR = 0x0012a800u,
    RVA_STAT_BAR_FILL = 0x00182230u,
    RVA_STAT_BAR_FILL_EARLY_RETURN = 0x00182340u,
    RVA_STAT_BAR_FILL_RETURN = 0x00182344u,
    RVA_STAT_DISPLAY_CAMERA_SYNC = 0x0012a460u,
    RVA_STAT_DISPLAY_CAMERA_INIT = 0x00409e08u,
    RVA_STAT_DISPLAY_SAVED_BOUNDS = 0x003c303cu,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS = 0x0040cdc0u,
    RVA_CAMERA_MANAGER_GLOBAL = 0x003c2f30u,
    RVA_SQUARE_ROOT = 0x0028fd60u,
    RVA_HUD_RESOURCE_SELECTOR = 0x0012bb60u,
    RVA_HUD_RESOURCE_SELECTOR_JUMP_TABLE = 0x0012be50u,
    RVA_HUD_RESOURCE_INITIALIZED = 0x00409e09u,
    RVA_HUD_RESOURCE_TABLE_GLOBAL = 0x003c305cu,
    RVA_HUD_RESOURCE_INLINE_TEXT = 0x002d2258u,
    RVA_HUD_RESOURCE_ERROR_TEXT = 0x002d2290u,
    RVA_HUD_RESOURCE_DEFAULT_TEXT = 0x002d34d0u,
    RVA_HUD_RESOURCE_FALLBACK_TEXT = 0x00361650u,
    RVA_HUD_RESOURCE_MISSING_REPORT = 0x00012f70u,
    RVA_HUD_STRING_ASSIGN = 0x001b9fc0u,
    RVA_HUD_STRING_FREE = 0x00248916u,
    RVA_HUD_STRING_ALLOCATE = 0x0024884eu,
    RVA_HUD_STRING_COPY = 0x0024e600u,
    RVA_FLOAT_TO_INT = 0x0028f010u,
    RVA_SET_ARMED_RELEASE_ATTACHMENT = 0x000c77b0u,
    RVA_SET_ARMED_RESET_STATE = 0x000dad60u,
    RVA_SET_ARMED_BLEND = 0x000c3830u,
    RVA_STAT_FILL_SCALE_A = 0x002e3620u,
    RVA_STAT_FILL_SCALE_B = 0x002e3690u,
    RVA_STAT_FILL_SCALE_C = 0x002e37c0u,
    RVA_UI_SCENE_GLOBAL = 0x00408d1cu,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_ASYNC_PENDING_GLOBAL = 0x003c30d8u,
    RVA_ASYNC_STREAM_GLOBAL = 0x003c30d4u,
    RVA_FORMATION_DEFAULT_DISTANCE = 0x002c3f54u,
    RVA_AI_MANAGER_GLOBAL = 0x00409de4u,
    RVA_MAX_RELOCATED_TARGET = RVA_HUD_RESOURCE_INITIALIZED
};

static const uint8_t get_pc_bytes[] = {
    0x51u,0x8bu,0x44u,0x24u,0x08u,0x50u,0x8du,0x4cu,0x24u,0x04u,
    0x51u,0xe8u,0x20u,0xfbu,0xffu,0xffu,0x8bu,0x00u,0x83u,0xc4u,
    0x08u,0x50u,0xe8u,0x85u,0xd7u,0xefu,0xffu,0x59u,0xc3u
};

static const uint8_t add_player_bytes[] = {
    0x83u,0xecu,0x0cu,0x56u,0x8bu,0xf1u,0x8du,0x4cu,0x24u,0x04u,
    0xe8u,0x51u,0xfcu,0xfeu,0xffu,0x8bu,0x4cu,0x24u,0x14u,0x8du,
    0x44u,0x24u,0x04u,0x50u,0x51u,0xe8u,0x92u,0xc2u,0x19u,0x00u,
    0x83u,0xc4u,0x08u,0x84u,0xc0u,0x74u,0x10u,0x8bu,0x54u,0x24u,
    0x04u,0x51u,0x8bu,0xc4u,0x89u,0x10u,0x8bu,0xc6u,0xe8u,0x1bu,
    0x00u,0x00u,0x00u,0x8du,0x4cu,0x24u,0x04u,0xe8u,0x72u,0xe3u,
    0xfdu,0xffu,0x5eu,0x83u,0xc4u,0x0cu,0xc2u,0x04u,0x00u
};

static const uint8_t remove_player_bytes[] = {
    0x83u,0xecu,0x0cu,0x56u,0x8bu,0xf1u,0x8du,0x4cu,0x24u,0x04u,
    0xe8u,0xf1u,0xfau,0xfeu,0xffu,0x8bu,0x4cu,0x24u,0x14u,0x8du,
    0x44u,0x24u,0x04u,0x50u,0x51u,0xe8u,0x32u,0xc1u,0x19u,0x00u,
    0x83u,0xc4u,0x08u,0x84u,0xc0u,0x74u,0x11u,0x8bu,0x54u,0x24u,
    0x04u,0x6au,0x01u,0x51u,0x8bu,0xc4u,0x56u,0x89u,0x10u,0xe8u,
    0x1au,0x00u,0x00u,0x00u,0x8du,0x4cu,0x24u,0x04u,0xe8u,0x11u,
    0xe2u,0xfdu,0xffu,0x5eu,0x83u,0xc4u,0x0cu,0xc2u,0x04u,0x00u
};

/* These raw group-core windows gate the fixed four-entry scan, count update,
 * listener vslot, and callee cleanup that the public wrappers depend on. */
static const uint8_t group_add_core_front[] = {
    0x55u,0x8bu,0xecu,0x83u,0xe4u,0xf8u,0x8bu,0x55u,0x08u,0x83u,
    0xecu,0x14u,0x53u,0x56u,0x57u,0x8bu,0xf0u,0x85u,0xd2u,0x74u,
    0x21u,0x33u,0xc9u,0x8du,0x86u,0x90u,0x00u,0x00u,0x00u,0x8du,
    0x49u,0x00u,0x83u,0x38u,0x00u,0x74u,0x08u,0x39u,0x10u,0x0fu,
    0x84u,0xd5u,0x00u,0x00u,0x00u,0x41u,0x83u,0xc0u,0x0cu,0x83u,
    0xf9u,0x04u,0x72u,0xeau,0x8bu,0x8eu,0xccu,0x00u,0x00u,0x00u,
    0x8du,0x44u,0x49u,0x24u,0x41u,0x8du,0x04u,0x86u,0x89u,0x8eu,
    0xccu,0x00u,0x00u,0x00u,0xe8u,0x81u,0xe4u,0xfdu,0xffu
};

static const uint8_t group_add_core_listener[] = {
    0x33u,0xdbu,0x8bu,0x46u,0x38u,0x8bu,0xc8u,0x3bu,0xd9u,0x72u,
    0x04u,0x3bu,0xc1u,0x73u,0x36u,0x3bu,0xd8u,0x0fu,0x94u,0xc0u,
    0x84u,0xc0u,0x75u,0x2du,0x8bu,0x56u,0x40u,0x8bu,0x3cu,0x9au,
    0x85u,0xffu,0x74u,0x20u,0x8bu,0x07u,0x83u,0xc0u,0x18u,0x83u,
    0xecu,0x0cu,0x89u,0x44u,0x24u,0x1cu,0x8du,0x4du,0x08u,0x8bu,
    0xc4u,0xe8u,0xc6u,0xecu,0xfdu,0xffu,0x8bu,0x44u,0x24u,0x1cu,
    0x8bu,0x10u,0x8bu,0xcfu,0xffu,0xd2u
};

static const uint8_t group_add_core_return[] = {
    0x5fu,0x5eu,0x5bu,0x8bu,0xe5u,0x5du,0xc2u,0x04u,0x00u
};

static const uint8_t group_remove_core_front[] = {
    0x55u,0x8bu,0xecu,0x83u,0xe4u,0xf8u,0x51u,0x53u,0x8bu,0x5du,
    0x08u,0x56u,0x8bu,0x75u,0x0cu,0x83u,0xcau,0xffu,0x57u,0x85u,
    0xf6u,0x74u,0x1fu,0x33u,0xc0u,0x8du,0x8bu,0x90u,0x00u,0x00u,
    0x00u,0x90u,0x83u,0x39u,0x00u,0x74u,0x04u,0x39u,0x31u,0x74u,
    0x0bu,0x40u,0x83u,0xc1u,0x0cu,0x83u,0xf8u,0x04u,0x72u,0xeeu,
    0xebu,0x02u,0x8bu,0xd0u,0x8bu,0xfau,0x85u,0xd2u
};

static const uint8_t group_remove_core_compaction[] = {
    0x8du,0x44u,0x7fu,0x24u,0x8bu,0x0cu,0x83u,0x8du,0x04u,0x83u,
    0x33u,0xd2u,0x3bu,0xcau,0x74u,0x2bu,0x39u,0x41u,0x04u,0x75u,
    0x06u,0x8bu,0x70u,0x08u,0x89u,0x71u,0x04u,0x8bu,0x48u,0x04u,
    0x3bu,0xcau,0x74u,0x06u,0x8bu,0x70u,0x08u,0x89u,0x71u,0x08u,
    0x8bu,0x48u,0x08u,0x3bu,0xcau,0x74u,0x06u,0x8bu,0x70u,0x04u,
    0x89u,0x71u,0x04u,0x89u,0x50u,0x08u,0x89u,0x50u,0x04u,0xb9u,
    0x01u,0x00u,0x00u,0x00u,0x89u,0x10u,0xffu,0x8bu,0xccu,0x00u,
    0x00u,0x00u,0x89u,0x4cu,0x24u,0x0cu,0x8du,0xbbu,0x98u,0x00u,
    0x00u,0x00u
};

static const uint8_t group_remove_core_listener[] = {
    0x33u,0xf6u,0x8bu,0x43u,0x38u,0x8bu,0xc8u,0x3bu,0xf1u,0x72u,
    0x04u,0x3bu,0xc1u,0x73u,0x2fu,0x3bu,0xf0u,0x0fu,0x94u,0xc0u,
    0x84u,0xc0u,0x75u,0x26u,0x8bu,0x43u,0x40u,0x8bu,0x3cu,0xb0u,
    0x85u,0xffu,0x74u,0x19u,0x8bu,0x1fu,0x83u,0xecu,0x0cu,0x8du,
    0x4du,0x0cu,0x8bu,0xc4u,0xe8u,0x49u,0xeau,0xfdu,0xffu,0x8bu,
    0x53u,0x1cu,0x8bu,0xcfu,0xffu,0xd2u,0x8bu,0x5du,0x08u
};

static const uint8_t group_remove_core_return[] = {
    0x8bu,0x3du,0x88u,0x2fu,0x7cu,0x00u,0xe8u,0xd1u,0xa5u,0x07u,
    0x00u,0x5fu,0x5eu,0x5bu,0x8bu,0xe5u,0x5du,0xc2u,0x0cu,0x00u
};

/* Synchronous RemovePlayer presentation epilogue. The single absolute
 * operand is the active-group singleton and is checked independently so the
 * same pure gate remains valid for a loader-rebased image. */
static const uint8_t remove_epilogue_bytes[] = {
    0xa1,0x94,0x8d,0x80,0x00,0x83,0xec,0x14,0x53,0x55,0x56,0x85,
    0xc0,0x0f,0x84,0x7a,0x01,0x00,0x00,0x8d,0x88,0x90,0x00,0x00,
    0x00,0x33,0xdb,0x89,0x4c,0x24,0x0c,0x8d,0x44,0x24,0x14,0xe8,
    0xe8,0x39,0xf6,0xff,0x8b,0x6c,0x24,0x14,0x8d,0x4c,0x24,0x14,
    0xe8,0x0b,0x3a,0xf6,0xff,0x85,0xed,0x0f,0x84,0x3b,0x01,0x00,
    0x00,0xd9,0xee,0x8b,0x75,0x4c,0x8b,0x07,0x8b,0x50,0x20,0x83,
    0xec,0x08,0xd9,0x5c,0x24,0x04,0x8b,0xcf,0xd9,0x46,0x2c,0xd9,
    0x1c,0x24,0x6a,0x05,0x53,0xff,0xd2,0xd9,0xee,0x8b,0x07,0x8b,
    0x50,0x20,0x83,0xec,0x08,0xd9,0x5c,0x24,0x04,0x8b,0xcf,0xd9,
    0x46,0x34,0xd9,0x1c,0x24,0x6a,0x07,0x53,0xff,0xd2,0xd9,0xee,
    0x8b,0x07,0x8b,0x50,0x20,0x83,0xec,0x08,0xd9,0x5c,0x24,0x04,
    0x8b,0xcf,0xd9,0x46,0x40,0xd9,0x1c,0x24,0x6a,0x0a,0x53,0xff,
    0xd2,0xd9,0xee,0x8b,0xb5,0xa8,0x00,0x00,0x00,0x8b,0x4e,0x40,
    0x0f,0xb6,0x51,0x4c,0x8b,0x07,0x8b,0x40,0x20,0x83,0xec,0x08,
    0x89,0x54,0x24,0x18,0x8b,0xcf,0xd9,0x5c,0x24,0x04,0xdb,0x44,
    0x24,0x18,0xd9,0x1c,0x24,0x6a,0x1a,0x53,0xff,0xd0,0xd9,0xee,
    0x8b,0x46,0x44,0x0f,0xb6,0x48,0x4c,0x8b,0x17,0x8b,0x52,0x20,
    0x83,0xec,0x08,0x89,0x4c,0x24,0x18,0x8b,0xcf,0xd9,0x5c,0x24,
    0x04,0xdb,0x44,0x24,0x18,0xd9,0x1c,0x24,0x6a,0x1b,0x53,0xff,
    0xd2,0xd9,0xee,0x8b,0x4e,0x50,0x0f,0xb6,0x51,0x4c,0x8b,0x07,
    0x8b,0x40,0x20,0x83,0xec,0x08,0x89,0x54,0x24,0x18,0x8b,0xcf,
    0xd9,0x5c,0x24,0x04,0xdb,0x44,0x24,0x18,0xd9,0x1c,0x24,0x6a,
    0x1e,0x53,0xff,0xd0,0xd9,0xee,0x8b,0x17,0x83,0xec,0x08,0xd9,
    0x5c,0x24,0x04,0x8b,0x46,0x54,0x0f,0xb6,0x48,0x4c,0x8b,0x52,
    0x20,0x89,0x4c,0x24,0x18,0x8b,0xcf,0xdb,0x44,0x24,0x18,0xd9,
    0x1c,0x24,0x6a,0x20,0x53,0xff,0xd2,0xd9,0xee,0x8b,0x4e,0x58,
    0x0f,0xb6,0x51,0x4c,0x8b,0x07,0x8b,0x40,0x20,0x83,0xec,0x08,
    0x89,0x54,0x24,0x18,0x8b,0xcf,0xd9,0x5c,0x24,0x04,0xdb,0x44,
    0x24,0x18,0xd9,0x1c,0x24,0x6a,0x1f,0x53,0xff,0xd0,0x8b,0x45,
    0x4c,0xd9,0x40,0x30,0x8b,0xb5,0xb0,0x00,0x00,0x00,0x83,0xec,
    0x08,0xd9,0x5c,0x24,0x04,0xd9,0x40,0x2c,0xd9,0x1c,0x24,0xe8,
    0x68,0xba,0x08,0x00,0x8b,0x4c,0x24,0x0c,0x43,0x83,0xc1,0x0c,
    0x89,0x4c,0x24,0x0c,0x83,0xfb,0x04,0x0f,0x8c,0x92,0xfe,0xff,
    0xff,0x5e,0x5d,0x5b,0x83,0xc4,0x14,0xc3
};

static const uint16_t remove_epilogue_relocations[] = {0x01u};

static const uint8_t player_set_armed_bytes[] = {
    0x51,0x8a,0x44,0x24,0x08,0x55,0x02,0xc0,0x56,0x8b,0xf1,0x32,
    0x46,0x60,0x8b,0x56,0x58,0x24,0x02,0x30,0x46,0x60,0x8b,0x46,
    0x10,0x8a,0x4e,0x60,0x8b,0xa8,0x80,0x00,0x00,0x00,0x80,0xe2,
    0x0f,0x57,0x80,0xfa,0x02,0x75,0x3c,0x84,0xca,0x75,0x38,0x8b,
    0x80,0xbc,0x00,0x00,0x00,0x85,0xc0,0x74,0x07,0x8b,0xf8,0xe8,
    0x80,0xc1,0xfe,0xff,0x8b,0xc6,0xe8,0x29,0xf7,0xff,0xff,0x81,
    0x66,0x50,0xeb,0xed,0xfd,0xff,0x85,0xed,0x74,0x15,0xd9,0xe8,
    0x83,0xec,0x08,0xd9,0x5c,0x24,0x04,0x8b,0xcd,0xd9,0xee,0xd9,
    0x1c,0x24,0xe8,0xd9,0x81,0xfe,0xff,0x8b,0x46,0x58,0x24,0x0f,
    0x3c,0x04,0x75,0x1f,0xf6,0x46,0x60,0x02,0x74,0x19,0x85,0xed,
    0x74,0x15,0xd9,0xe8,0x83,0xec,0x08,0xd9,0x5c,0x24,0x04,0x8b,
    0xcd,0xd9,0xee,0xd9,0x1c,0x24,0xe8,0xb1,0x81,0xfe,0xff,0x5f,
    0x5e,0x5d,0x59,0xc2,0x04,0x00
};

static const uint8_t stat_display_refresh_bytes[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf8,0x51,0xd9,0x45,0x08,0x57,0xd9,
    0x96,0x6c,0x01,0x00,0x00,0xd8,0x75,0x0c,0xd9,0x5c,0x24,0x04,
    0xd9,0xee,0xd9,0x44,0x24,0x04,0xd8,0xd1,0xdf,0xe0,0xf6,0xc4,
    0x05,0x7a,0x08,0xdd,0xd8,0xd9,0x5c,0x24,0x04,0xeb,0x11,0xdd,
    0xd9,0xd9,0xe8,0xd8,0xd1,0xdf,0xe0,0xdd,0xd9,0xf6,0xc4,0x05,
    0x7b,0xeb,0xdd,0xd8,0xd9,0x44,0x24,0x04,0x51,0x8d,0xbe,0xd0,
    0x00,0x00,0x00,0xd9,0x1c,0x24,0x33,0xc0,0xe8,0x5b,0x8a,0x05,
    0x00,0xd9,0x44,0x24,0x04,0x51,0xb8,0x01,0x00,0x00,0x00,0xd9,
    0x1c,0x24,0xe8,0x49,0x8a,0x05,0x00,0x8b,0xce,0xe8,0x72,0x0c,
    0x00,0x00,0x5f,0x8b,0xe5,0x5d,0xc2,0x08,0x00
};

static const uint8_t stat_display_constructor_vtables[] = {
    0xc7u,0x06u,0xe4u,0x21u,0x6du,0x00u,0xc7u,0x46u,0x04u,0x24u,
    0x22u,0x6du,0x00u
};

static const uint16_t stat_display_constructor_vtable_relocations[] = {
    0x02u, 0x09u
};

static const uint8_t stat_bar_fill_bytes[] = {
    0x55,0x8b,0xec,0x83,0xe4,0xf0,0x83,0xec,0x4c,0x56,0x8b,0xf0,
    0x85,0xf6,0x0f,0x88,0xfc,0x00,0x00,0x00,0x3b,0x77,0x50,0x0f,
    0x8d,0xf3,0x00,0x00,0x00,0xd9,0x45,0x08,0xdc,0x0d,0x20,0x36,
    0x6e,0x00,0xd9,0x5c,0x24,0x0c,0xd9,0x44,0x24,0x0c,0xdc,0x0d,
    0x90,0x36,0x6e,0x00,0xd9,0x5c,0x24,0x0c,0xd9,0x44,0x24,0x0c,
    0xe8,0x9f,0xcd,0x10,0x00,0x89,0x44,0x24,0x0c,0xdb,0x44,0x24,
    0x0c,0x8b,0x47,0x58,0x8d,0x0c,0xb0,0xdc,0x0d,0xc0,0x37,0x6e,
    0x00,0xd9,0x5d,0x08,0xd9,0x01,0xd9,0x45,0x08,0xd9,0xc0,0xdd,
    0xea,0xdf,0xe0,0xdd,0xd9,0xf6,0xc4,0x44,0x0f,0x8b,0xa0,0x00,
    0x00,0x00,0xd9,0x11,0x8b,0x4f,0x4c,0x83,0x3c,0xb1,0xff,0x0f,
    0x84,0x91,0x00,0x00,0x00,0xd9,0xee,0x8b,0x57,0x34,0x8b,0x34,
    0xb2,0xd9,0x54,0x24,0x48,0xd9,0x54,0x24,0x44,0x6a,0xff,0xd9,
    0x54,0x24,0x44,0x8b,0xce,0xd9,0x54,0x24,0x40,0xd9,0x54,0x24,
    0x38,0xd9,0x54,0x24,0x34,0xd9,0x54,0x24,0x30,0xd9,0x54,0x24,
    0x24,0xd9,0x54,0x24,0x20,0xd9,0x54,0x24,0x18,0xd9,0xe8,0xd9,
    0x54,0x24,0x50,0xd9,0x54,0x24,0x3c,0xd9,0x54,0x24,0x28,0xd9,
    0x5c,0x24,0x14,0xd9,0xc9,0xd9,0x5c,0x24,0x1c,0xd9,0x5c,0x24,
    0x2c,0x8b,0x06,0x8b,0x50,0x18,0xff,0xd2,0x84,0xc0,0x74,0x10,
    0x8b,0x06,0x8b,0x50,0x38,0x8d,0x4c,0x24,0x10,0x51,0x6a,0xff,
    0x8b,0xce,0xff,0xd2,0x8b,0x06,0x8b,0x50,0x18,0x6a,0xfe,0x8b,
    0xce,0xff,0xd2,0x84,0xc0,0x74,0x19,0x8b,0x06,0x8b,0x50,0x38,
    0x8d,0x4c,0x24,0x10,0x51,0x6a,0xfe,0x8b,0xce,0xff,0xd2,0x5e,
    0x8b,0xe5,0x5d,0xc2,0x04,0x00,0xdd,0xd8,0x5e,0x8b,0xe5,0x5d,
    0xc2,0x04,0x00
};

static const uint16_t stat_bar_fill_relocations[] = {
    0x22u, 0x30u, 0x51u
};


static const uint16_t stat_display_camera_sync_relocations[] = {
    0x00bu,0x014u,0x031u,0x038u,0x044u,0x050u,0x05cu,0x062u,
    0x068u,0x06eu,0x074u,0x07au,0x080u,0x086u,0x08cu,0x092u,
    0x0abu,0x0b5u,0x0c8u,0x0d2u,0x0e5u,0x0efu,0x102u,0x126u,
    0x1a0u,0x1afu,0x1b7u,0x1c6u
};

static const uint32_t stat_display_camera_sync_relocation_targets[] = {
    RVA_STAT_DISPLAY_CAMERA_INIT, RVA_UI_SCENE_GLOBAL,
    RVA_STAT_DISPLAY_CAMERA_INIT, RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x0cu,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x08u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x04u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x0cu,
    RVA_STAT_DISPLAY_SAVED_BOUNDS,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x04u,
    RVA_STAT_DISPLAY_SAVED_BOUNDS + 0x08u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x08u,
    RVA_STAT_DISPLAY_SAVED_BOUNDS + 0x04u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS,
    RVA_STAT_DISPLAY_SAVED_BOUNDS + 0x0cu,
    RVA_STAT_DISPLAY_SAVED_BOUNDS,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x0cu,
    RVA_STAT_DISPLAY_SAVED_BOUNDS + 0x08u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x04u,
    RVA_STAT_DISPLAY_SAVED_BOUNDS + 0x04u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x08u,
    RVA_STAT_DISPLAY_SAVED_BOUNDS + 0x0cu,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS, RVA_CAMERA_MANAGER_GLOBAL,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x04u,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x0cu,
    RVA_STAT_DISPLAY_ACTIVE_BOUNDS + 0x08u
};

static const uint16_t hud_resource_selector_relocations[] = {
    0x034u,0x03au,0x042u,0x048u,0x074u,0x079u,0x086u,0x08du,
    0x095u,0x0c0u,0x0ccu,0x0fau,0x106u,0x138u,0x144u,0x176u,
    0x182u,0x1b4u,0x1c0u,0x1f2u,0x1feu,0x230u,0x23cu,0x26eu,
    0x27au,0x2acu,0x2b8u,0x2e9u
};

static const uint32_t hud_resource_selector_relocation_targets[] = {
    RVA_HUD_RESOURCE_SELECTOR_JUMP_TABLE, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_INLINE_TEXT, RVA_HUD_RESOURCE_TABLE_GLOBAL,
    RVA_HUD_RESOURCE_ERROR_TEXT, RVA_HUD_RESOURCE_FALLBACK_TEXT,
    RVA_HUD_RESOURCE_FALLBACK_TEXT, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_INITIALIZED,
    RVA_HUD_RESOURCE_TABLE_GLOBAL, RVA_HUD_RESOURCE_DEFAULT_TEXT
};

static const uint32_t hud_resource_selector_jump_table_targets[] = {
    RVA_HUD_RESOURCE_SELECTOR + 0x038u,
    RVA_HUD_RESOURCE_SELECTOR + 0x08bu,
    RVA_HUD_RESOURCE_SELECTOR + 0x0beu,
    RVA_HUD_RESOURCE_SELECTOR + 0x0f8u,
    RVA_HUD_RESOURCE_SELECTOR + 0x136u,
    RVA_HUD_RESOURCE_SELECTOR + 0x174u,
    RVA_HUD_RESOURCE_SELECTOR + 0x1b2u,
    RVA_HUD_RESOURCE_SELECTOR + 0x1f0u,
    RVA_HUD_RESOURCE_SELECTOR + 0x22eu,
    RVA_HUD_RESOURCE_SELECTOR + 0x26cu,
    RVA_HUD_RESOURCE_SELECTOR + 0x2aau
};

typedef char stat_display_camera_sync_relocation_count_must_match[
    ARRAY_COUNT(stat_display_camera_sync_relocations) ==
        ARRAY_COUNT(stat_display_camera_sync_relocation_targets) ? 1 : -1];
typedef char hud_resource_selector_relocation_count_must_match[
    ARRAY_COUNT(hud_resource_selector_relocations) ==
        ARRAY_COUNT(hud_resource_selector_relocation_targets) ? 1 : -1];
typedef char stat_display_camera_sync_size_must_match[
    sizeof(stat_display_camera_sync_bytes) == 0x25eu ? 1 : -1];
typedef char hud_resource_selector_size_must_match[
    sizeof(hud_resource_selector_bytes) == 0x2eeu ? 1 : -1];
typedef char hud_string_assign_size_must_match[
    sizeof(hud_string_assign_bytes) == 0xf3u ? 1 : -1];
typedef char hud_resource_selector_jump_table_size_must_match[
    ARRAY_COUNT(hud_resource_selector_jump_table_targets) == 11u ? 1 : -1];


/* UI controller vslot +0x20. The validated switch boundary makes the
 * RemovePlayer IDs above 0x16 no-ops; the selector/jump entries checked below
 * bind IDs 5, 7, and 10 to the exact supported-build handlers. */
static const uint8_t ui_controller_dispatch_bytes[] = {
    0x83,0xec,0x28,0xa1,0x1c,0x8d,0x80,0x00,0x89,0x04,0x24,0x8b,
    0x44,0x24,0x30,0x83,0xc0,0xfb,0x55,0x8b,0xe9,0x83,0xf8,0x11,
    0x0f,0x87,0x98,0x01,0x00,0x00,0x0f,0xb6,0x88,0x84,0xdb,0x49,
    0x00,0x56,0x57,0xff,0x24,0x8d,0x70,0xdb,0x49,0x00,0x8b,0x7c,
    0x24,0x38,0xa1,0x94,0x8d,0x80,0x00,0x8d,0x54,0x7f,0x24,0x8d,
    0x0c,0x90,0x8d,0x44,0x24,0x10,0xe8,0xb9,0x3b,0xf6,0xff,0x8b,
    0x74,0x24,0x10,0x8d,0x4c,0x24,0x10,0xe8,0xdc,0x3b,0xf6,0xff,
    0x85,0xf6,0x0f,0x84,0x58,0x01,0x00,0x00,0x8b,0x4e,0x4c,0xd9,
    0x41,0x40,0xd9,0x5c,0x24,0x3c,0xd9,0x44,0x24,0x3c,0xe8,0xf1,
    0x15,0x1f,0x00,0x56,0x8b,0xc8,0xe8,0x39,0xe1,0x08,0x00,0x8b,
    0x4d,0x6c,0x8b,0x8c,0xb9,0x38,0x01,0x00,0x00,0x83,0xc4,0x04,
    0x85,0xc9,0x0f,0x84,0x28,0x01,0x00,0x00,0x50,0x8d,0xb9,0xe0,
    0x02,0x00,0x00,0xe8,0x78,0xc5,0x11,0x00,0x5f,0x5e,0x5d,0x83,
    0xc4,0x28,0xc2,0x10,0x00,0x53,0x33,0xdb,0xbf,0x90,0x00,0x00,
    0x00,0x8d,0xa4,0x24,0x00,0x00,0x00,0x00,0x8b,0x15,0x94,0x8d,
    0x80,0x00,0x8d,0x0c,0x17,0x8d,0x44,0x24,0x20,0xe8,0x3e,0x3b,
    0xf6,0xff,0x8b,0x74,0x24,0x20,0x8d,0x4c,0x24,0x20,0xe8,0x61,
    0x3b,0xf6,0xff,0x85,0xf6,0x74,0x77,0x8b,0x76,0x4c,0xd9,0x46,
    0x30,0xd9,0x5c,0x24,0x40,0xd9,0x44,0x24,0x40,0xd9,0xc0,0xd9,
    0xee,0xd9,0xc0,0xdd,0xea,0xdf,0xe0,0xdd,0xd9,0xf6,0xc4,0x44,
    0x7b,0x09,0xd9,0x46,0x2c,0xde,0xf2,0xd9,0xc9,0xeb,0x04,0xdd,
    0xd9,0xd9,0xe8,0x8b,0x44,0x24,0x10,0xd9,0x5c,0x24,0x40,0xd9,
    0x44,0x24,0x40,0xd9,0x80,0x48,0x01,0x00,0x00,0xd8,0xd9,0xdf,
    0xe0,0xf6,0xc4,0x41,0x7a,0x08,0xdd,0xd9,0x33,0xc0,0xdd,0xd8,
    0xeb,0x1a,0xd8,0xd9,0xdf,0xe0,0xf6,0xc4,0x41,0x75,0x09,0xdd,
    0xd8,0xb8,0x01,0x00,0x00,0x00,0xeb,0x08,0xd9,0x5e,0x34,0xb8,
    0x02,0x00,0x00,0x00,0x8b,0x4d,0x6c,0x8b,0x11,0x50,0x8b,0x42,
    0x2c,0x53,0x6a,0x11,0xff,0xd0,0x83,0xc7,0x0c,0x43,0x81,0xff,
    0xc0,0x00,0x00,0x00,0x0f,0x8c,0x56,0xff,0xff,0xff,0x5b,0x5f,
    0x5e,0x5d,0x83,0xc4,0x28,0xc2,0x10,0x00,0x8b,0x35,0x94,0x8d,
    0x80,0x00,0x81,0xc6,0x90,0x00,0x00,0x00,0xbf,0x04,0x00,0x00,
    0x00,0x8b,0xce,0x8d,0x44,0x24,0x28,0xe8,0x80,0x3a,0xf6,0xff,
    0x8d,0x4c,0x24,0x28,0xe8,0xa7,0x3a,0xf6,0xff,0x83,0xc6,0x0c,
    0x4f,0x75,0xe6,0x5f,0x5e,0x5d,0x83,0xc4,0x28,0xc2,0x10,0x00,
    0xd9,0x44,0x24,0x40,0x51,0xd8,0x74,0x24,0x48,0x8b,0x4d,0x6c,
    0xd9,0x5c,0x24,0x40,0xd9,0x44,0x24,0x40,0xd9,0x1c,0x24,0xe8,
    0x7c,0x88,0x00,0x00,0x5f,0x5e,0x5d,0x83,0xc4,0x28,0xc2,0x10,
    0x00
};

static const uint16_t ui_controller_dispatch_relocations[] = {
    0x04u, 0x21u, 0x2au, 0x33u, 0xb2u, 0x166u
};

static const uint8_t ui_controller_selector_id5_through_id10[] = {
    0x00u, 0x04u, 0x01u, 0x04u, 0x04u, 0x02u
};

static const uint8_t hud_child_dispatch_prefix[] = {
    0x8b,0x44,0x24,0x04,0x56,0x57,0x8b,0xf9,0x83,0xf8,0x19,0x0f,
    0x87,0x5f,0x01,0x00,0x00,0x0f,0xb6,0x80,0xc4,0x5a,0x4a,0x00,
    0xff,0x24,0x85,0xa8,0x5a,0x4a,0x00
};

static const uint16_t hud_child_dispatch_prefix_relocations[] = {
    0x14u, 0x1bu
};

static const uint8_t hud_portrait_state_handler_bytes[] = {
    0x8b,0x54,0x24,0x10,0x8b,0x84,0x97,0x38,0x01,0x00,0x00,0x85,
    0xc0,0x0f,0x84,0x01,0x01,0x00,0x00,0x8b,0x4c,0x24,0x14,0x39,
    0x88,0x24,0x03,0x00,0x00,0x0f,0x84,0xf1,0x00,0x00,0x00,0xe8,
    0x63,0x4f,0x00,0x00,0x5f,0xb0,0x01,0x5e,0xc2,0x0c,0x00
};

static const uint8_t hud_child_shared_returns[] = {
    0x5fu,0xb0u,0x01u,0x5eu,0xc2u,0x0cu,0x00u,
    0x5fu,0x32u,0xc0u,0x5eu,0xc2u,0x0cu,0x00u
};

static const uint8_t hud_child_id17_selector[] = {0x04u};

static const uint8_t is_player_bytes[] = {
    0x83u,0xecu,0x0cu,0x56u,0x8bu,0xf1u,0x8du,0x4cu,0x24u,0x04u,
    0xe8u,0x81u,0xf4u,0xfeu,0xffu,0x8bu,0x4cu,0x24u,0x14u,0x8du,
    0x44u,0x24u,0x04u,0x50u,0x51u,0xe8u,0xc2u,0xbau,0x19u,0x00u,
    0x83u,0xc4u,0x08u,0x84u,0xc0u,0x74u,0x4du,0x8bu,0x54u,0x24u,
    0x04u,0x53u,0x85u,0xd2u,0x74u,0x1au,0x33u,0xc9u,0x8du,0x86u,
    0x90u,0x00u,0x00u,0x00u,0x83u,0x38u,0x00u,0x74u,0x04u,0x39u,
    0x10u,0x74u,0x1eu,0x41u,0x83u,0xc0u,0x0cu,0x83u,0xf9u,0x04u,
    0x72u,0xeeu,0x8du,0x4cu,0x24u,0x08u,0x32u,0xdbu,0xe8u,0x8du,
    0xdbu,0xfdu,0xffu,0x8au,0xc3u,0x5bu,0x5eu,0x83u,0xc4u,0x0cu,
    0xc2u,0x04u,0x00u,0x8du,0x4cu,0x24u,0x08u,0xb3u,0x01u,0xe8u,
    0x78u,0xdbu,0xfdu,0xffu,0x8au,0xc3u,0x5bu,0x5eu,0x83u,0xc4u,
    0x0cu,0xc2u,0x04u,0x00u,0x8du,0x4cu,0x24u,0x04u,0xe8u,0x65u,
    0xdbu,0xfdu,0xffu,0x32u,0xc0u,0x5eu,0x83u,0xc4u,0x0cu,0xc2u,
    0x04u,0x00u
};

static const uint8_t get_index_bytes[] = {
    0x83u,0xecu,0x0cu,0x56u,0x8bu,0xf1u,0x8du,0x4cu,0x24u,0x04u,
    0xe8u,0xc1u,0xf3u,0xfeu,0xffu,0x8bu,0x4cu,0x24u,0x14u,0x8du,
    0x44u,0x24u,0x04u,0x50u,0x51u,0xe8u,0x02u,0xbau,0x19u,0x00u,
    0x83u,0xc4u,0x08u,0x84u,0xc0u,0x74u,0x55u,0x8bu,0x54u,0x24u,
    0x04u,0x57u,0x83u,0xcfu,0xffu,0x85u,0xd2u,0x74u,0x36u,0x33u,
    0xc0u,0x8du,0x8eu,0x90u,0x00u,0x00u,0x00u,0x8du,0xa4u,0x24u,
    0x00u,0x00u,0x00u,0x00u,0x83u,0x39u,0x00u,0x74u,0x04u,0x39u,
    0x11u,0x74u,0x1cu,0x40u,0x83u,0xc1u,0x0cu,0x83u,0xf8u,0x04u,
    0x72u,0xeeu,0x8du,0x4cu,0x24u,0x08u,0xe8u,0xc5u,0xdau,0xfdu,
    0xffu,0x8bu,0xc7u,0x5fu,0x5eu,0x83u,0xc4u,0x0cu,0xc2u,0x04u,
    0x00u,0x8bu,0xf8u,0x8du,0x4cu,0x24u,0x08u,0xe8u,0xb0u,0xdau,
    0xfdu,0xffu,0x8bu,0xc7u,0x5fu,0x5eu,0x83u,0xc4u,0x0cu,0xc2u,
    0x04u,0x00u,0x8du,0x4cu,0x24u,0x04u,0xe8u,0x9du,0xdau,0xfdu,
    0xffu,0x83u,0xc8u,0xffu,0x5eu,0x83u,0xc4u,0x0cu,0xc2u,0x04u,
    0x00u
};

static const uint8_t all_pending_bytes[] = {
    0x83u,0x79u,0x5cu,0x00u,0x75u,0x0cu,0x83u,0x79u,0x58u,0x00u,
    0x75u,0x06u,0xb8u,0x01u,0x00u,0x00u,0x00u,0xc3u,0x33u,0xc0u,
    0xc3u
};

static const uint8_t in_combat_bytes[] = {
    0x8au,0x81u,0xd4u,0x00u,0x00u,0x00u,0xc3u
};

static const uint8_t gel_cleanup_bytes[] = {
    0x8bu,0x01u,0x33u,0xd2u,0x3bu,0xc2u,0x74u,0x2du,0x56u,0x39u,
    0x48u,0x04u,0x75u,0x06u,0x8bu,0x71u,0x08u,0x89u,0x70u,0x04u,
    0x8bu,0x41u,0x04u,0x3bu,0xc2u,0x74u,0x06u,0x8bu,0x71u,0x08u,
    0x89u,0x70u,0x08u,0x8bu,0x41u,0x08u,0x3bu,0xc2u,0x74u,0x06u,
    0x8bu,0x71u,0x04u,0x89u,0x70u,0x04u,0x89u,0x51u,0x08u,0x89u,
    0x51u,0x04u,0x5eu,0xc3u
};

static const uint8_t gel_wrapper_helper_bytes[] = {
    0x6au,0xffu,0x68u,0xf8u,0x47u,0x69u,0x00u,0x64u,0xa1u,0x00u,
    0x00u,0x00u,0x00u,0x50u,0x64u,0x89u,0x25u,0x00u,0x00u,0x00u,
    0x00u,0x83u,0xecu,0x10u,0x8bu,0x4cu,0x24u,0x20u,0x56u,0x33u,
    0xf6u,0x89u,0x4cu,0x24u,0x08u,0x89u,0x74u,0x24u,0x0cu,0x89u,
    0x74u,0x24u,0x10u,0x3bu,0xceu,0x74u,0x20u,0x8du,0x41u,0x04u,
    0x8bu,0x08u,0x3bu,0xceu,0x74u,0x07u,0x8du,0x54u,0x24u,0x08u,
    0x89u,0x51u,0x04u,0x8bu,0x08u,0x89u,0x4cu,0x24u,0x10u,0x8du,
    0x54u,0x24u,0x08u,0x89u,0x10u,0x8bu,0x4cu,0x24u,0x24u,0x83u,
    0xecu,0x0cu,0x8bu,0xc4u,0x89u,0x74u,0x24u,0x28u,0x89u,0x64u,
    0x24u,0x10u,0x89u,0x08u,0x89u,0x70u,0x04u,0x89u,0x70u,0x08u,
    0x3bu,0xceu,0x74u,0x13u,0x8bu,0x51u,0x04u,0x3bu,0xd6u,0x74u,
    0x03u,0x89u,0x42u,0x04u,0x8bu,0x51u,0x04u,0x89u,0x50u,0x08u,
    0x89u,0x41u,0x04u,0xe8u,0x20u,0xfdu,0xffu,0xffu,0x8bu,0x4cu,
    0x24u,0x08u,0x3bu,0xceu,0x74u,0x2eu,0x8du,0x54u,0x24u,0x08u,
    0x39u,0x51u,0x04u,0x75u,0x07u,0x8bu,0x54u,0x24u,0x10u,0x89u,
    0x51u,0x04u,0x8bu,0x54u,0x24u,0x0cu,0x3bu,0xd6u,0x74u,0x0bu,
    0x8bu,0x4cu,0x24u,0x10u,0x89u,0x4au,0x08u,0x8bu,0x54u,0x24u,
    0x0cu,0x8bu,0x4cu,0x24u,0x10u,0x3bu,0xceu,0x74u,0x03u,0x89u,
    0x51u,0x04u,0x8bu,0x4cu,0x24u,0x14u,0x64u,0x89u,0x0du,0x00u,
    0x00u,0x00u,0x00u,0x5eu,0x83u,0xc4u,0x1cu,0xc2u,0x04u,0x00u
};

static const uint8_t gel_wrapper_factory_bytes[] = {
    0x6au,0xffu,0x68u,0xa3u,0x47u,0x69u,0x00u,0x64u,0xa1u,0x00u,
    0x00u,0x00u,0x00u,0x50u,0x64u,0x89u,0x25u,0x00u,0x00u,0x00u,
    0x00u,0x51u,0x53u,0x56u,0x33u,0xdbu,0x6au,0x18u,0x89u,0x5cu,
    0x24u,0x18u,0xe8u,0x15u,0x6bu,0x24u,0x00u,0x8bu,0xf0u,0x83u,
    0xc4u,0x04u,0x89u,0x74u,0x24u,0x08u,0xc6u,0x44u,0x24u,0x14u,
    0x01u,0x3bu,0xf3u,0x74u,0x16u,0xe8u,0xd4u,0x06u,0x00u,0x00u,
    0x89u,0x5eu,0x0cu,0x89u,0x5eu,0x10u,0x89u,0x5eu,0x14u,0xc7u,
    0x06u,0x98u,0x00u,0x6cu,0x00u,0xebu,0x02u,0x33u,0xf6u,0x56u,
    0x8du,0x4cu,0x24u,0x20u,0x88u,0x5cu,0x24u,0x18u,0xe8u,0xe3u,
    0x03u,0x00u,0x00u,0x8bu,0x44u,0x24u,0x1cu,0x3bu,0xc3u,0x74u,
    0x2eu,0x8du,0x4cu,0x24u,0x1cu,0x39u,0x48u,0x04u,0x75u,0x07u,
    0x8bu,0x54u,0x24u,0x24u,0x89u,0x50u,0x04u,0x8bu,0x4cu,0x24u,
    0x20u,0x3bu,0xcbu,0x74u,0x0bu,0x8bu,0x44u,0x24u,0x24u,0x89u,
    0x41u,0x08u,0x8bu,0x4cu,0x24u,0x20u,0x8bu,0x44u,0x24u,0x24u,
    0x3bu,0xc3u,0x74u,0x03u,0x89u,0x48u,0x04u,0x8bu,0x4cu,0x24u,
    0x0cu,0x8bu,0xc6u,0x5eu,0x5bu,0x64u,0x89u,0x0du,0x00u,0x00u,
    0x00u,0x00u,0x83u,0xc4u,0x10u,0xc2u,0x0cu,0x00u
};

static const uint8_t ptr_registration_constructor_bytes[] = {
    0x83u,0xecu,0x10u,0x56u,0x8bu,0xf0u,0x6au,0x14u,0xc7u,0x06u,
    0x8cu,0x01u,0x6cu,0x00u,0xc7u,0x46u,0x04u,0x01u,0x00u,0x00u,
    0x00u,0xe8u,0x10u,0x64u,0x24u,0x00u,0x33u,0xd2u,0x83u,0xc4u,
    0x04u,0x3bu,0xc2u,0x74u,0x39u,0x8bu,0x0du,0x04u,0x39u,0x7cu,
    0x00u,0x89u,0x08u,0x8bu,0x0du,0x04u,0x39u,0x7cu,0x00u,0x89u,
    0x48u,0x04u,0x8bu,0x0du,0x04u,0x39u,0x7cu,0x00u,0x89u,0x48u,
    0x08u,0x8du,0x48u,0x0cu,0x66u,0x89u,0x50u,0x10u,0x3bu,0xcau,
    0x74u,0x02u,0x89u,0x31u,0x50u,0x8du,0x54u,0x24u,0x10u,0x52u,
    0xe8u,0x2bu,0x06u,0x00u,0x00u,0x8bu,0xc6u,0x5eu,0x83u,0xc4u,
    0x10u,0xc3u,0x8du,0x74u,0x24u,0x04u,0x89u,0x54u,0x24u,0x08u,
    0xc7u,0x44u,0x24u,0x04u,0x7cu,0x01u,0x6cu,0x00u,0xe8u,0x2fu,
    0xffu,0xffu,0xffu
};

static const uint8_t gel_core_destructor_bytes[] = {
    0x6au,0xffu,0x68u,0x83u,0x47u,0x69u,0x00u,0x64u,0xa1u,0x00u,
    0x00u,0x00u,0x00u,0x50u,0x64u,0x89u,0x25u,0x00u,0x00u,0x00u,
    0x00u,0x51u,0x8bu,0x54u,0x24u,0x14u,0x8du,0x42u,0x0cu,0xc7u,
    0x44u,0x24u,0x0cu,0x00u,0x00u,0x00u,0x00u,0x8bu,0x08u,0x85u,
    0xc9u,0x74u,0x35u,0x57u,0x39u,0x41u,0x04u,0x75u,0x06u,0x8bu,
    0x78u,0x08u,0x89u,0x79u,0x04u,0x8bu,0x48u,0x04u,0x85u,0xc9u,
    0x74u,0x06u,0x8bu,0x78u,0x08u,0x89u,0x79u,0x08u,0x8bu,0x48u,
    0x08u,0x85u,0xc9u,0x74u,0x06u,0x8bu,0x78u,0x04u,0x89u,0x79u,
    0x04u,0xc7u,0x40u,0x08u,0x00u,0x00u,0x00u,0x00u,0xc7u,0x40u,
    0x04u,0x00u,0x00u,0x00u,0x00u,0x5fu,0x8du,0x44u,0x24u,0x14u,
    0xc7u,0x44u,0x24u,0x0cu,0xffu,0xffu,0xffu,0xffu,0x50u,0x8du,
    0x44u,0x24u,0x04u,0xc7u,0x02u,0x8cu,0x01u,0x6cu,0x00u,0x89u,
    0x54u,0x24u,0x18u,0xe8u,0x40u,0x08u,0x00u,0x00u,0x8bu,0x04u,
    0x24u,0x3bu,0x05u,0x04u,0x39u,0x7cu,0x00u,0x74u,0x0bu,0x50u,
    0x8du,0x4cu,0x24u,0x18u,0x51u,0xe8u,0xdau,0x05u,0x00u,0x00u,
    0x8bu,0x4cu,0x24u,0x04u,0x64u,0x89u,0x0du,0x00u,0x00u,0x00u,
    0x00u,0x83u,0xc4u,0x10u,0xc2u,0x04u,0x00u
};

static const uint8_t ptr_registry_delete_all_bytes[] = {
    0x83u,0x3du,0x08u,0x39u,0x7cu,0x00u,0x00u,0x74u,0x26u,0x8du,
    0xa4u,0x24u,0x00u,0x00u,0x00u,0x00u,0xa1u,0x04u,0x39u,0x7cu,
    0x00u,0x8bu,0x08u,0x8bu,0x49u,0x0cu,0x85u,0xc9u,0x74u,0x08u,
    0x8bu,0x11u,0x8bu,0x02u,0x6au,0x01u,0xffu,0xd0u,0x83u,0x3du,
    0x08u,0x39u,0x7cu,0x00u,0x00u,0x75u,0xe1u,0xc3u
};

static const uint8_t ptr_registry_find_bytes[] = {
    0x51u,0x53u,0x8bu,0x5cu,0x24u,0x0cu,0x56u,0x8bu,0x35u,0x04u,
    0x39u,0x7cu,0x00u,0x8bu,0x4eu,0x04u,0x80u,0x79u,0x11u,0x00u,
    0x8bu,0xd6u,0x75u,0x1du,0x57u,0x8bu,0x3bu,0xebu,0x03u,0x8du,
    0x49u,0x00u,0x39u,0x79u,0x0cu,0x73u,0x05u,0x8bu,0x49u,0x08u,
    0xebu,0x04u,0x8bu,0xd1u,0x8bu,0x09u,0x80u,0x79u,0x11u,0x00u,
    0x74u,0xecu,0x5fu,0x89u,0x54u,0x24u,0x10u,0x3bu,0xd6u,0x74u,
    0x15u,0x8bu,0x0bu,0x3bu,0x4au,0x0cu,0x72u,0x0eu,0x8du,0x4cu,
    0x24u,0x10u,0x8bu,0x11u,0x5eu,0x89u,0x10u,0x5bu,0x59u,0xc2u,
    0x04u,0x00u,0x89u,0x74u,0x24u,0x08u,0x8du,0x4cu,0x24u,0x08u,
    0x8bu,0x11u,0x5eu,0x89u,0x10u,0x5bu,0x59u,0xc2u,0x04u,0x00u
};

static const uint8_t ptr_registry_erase_entry[] = {
    0x8bu,0x44u,0x24u,0x08u,0x80u,0x78u,0x11u,0x00u,0x74u,0x0au,
    0x68u
};

static const uint8_t ptr_registry_erase_after_text[] = {
    0xe8u,0x1cu,0x6cu,0x25u,0x00u,0x53u,0x55u,0x56u,0x8bu,0xe8u,
    0x57u,0x8du,0x44u,0x24u,0x18u,0xe8u,0x7du,0x04u,0x00u,0x00u
};

static const uint8_t ptr_registry_erase_tail[] = {
    0x00u,0x88u,0x5fu,0x10u,0x55u,0xe8u,0x65u,0x60u,0x24u,0x00u,
    0x83u,0xc4u,0x04u,0x83u,0x3du,0x08u,0x39u,0x7cu,0x00u,0x00u,
    0x76u,0x06u,0x29u,0x1du,0x08u,0x39u,0x7cu,0x00u,0x8bu,0x4cu,
    0x24u,0x18u,0x8bu,0x44u,0x24u,0x14u,0x5fu,0x5eu,0x5du,0x89u,
    0x08u,0x5bu,0xc2u,0x08u,0x00u
};

static const uint16_t gel_wrapper_helper_relocations[] = {0x03u};
static const uint16_t gel_wrapper_factory_relocations[] = {0x03u, 0x47u};
static const uint16_t ptr_registration_constructor_relocations[] = {
    0x0au, 0x25u, 0x2du, 0x36u, 0x68u
};
static const uint16_t gel_core_destructor_relocations[] = {
    0x03u, 0x73u, 0x85u
};
static const uint16_t ptr_registry_delete_all_relocations[] = {
    0x02u, 0x11u, 0x28u
};
static const uint16_t ptr_registry_find_relocations[] = {0x09u};
static const uint16_t ptr_registry_erase_tail_relocations[] = {
    0x0fu, 0x18u
};
static const uint16_t group_remove_core_return_relocations[] = {0x02u};

static const uint8_t opcode_mov_eax_absolute[] = {0xa1u};
static const uint8_t opcode_mov_ecx_absolute[] = {0x8bu, 0x0du};
static const uint8_t opcode_mov_edx_absolute[] = {0x8bu, 0x15u};
static const uint8_t opcode_compare_absolute[] = {0x83u, 0x3du};
static const uint8_t opcode_subtract_absolute[] = {0x29u, 0x1du};

static const uint8_t gel_destructor_bytes[] = {
    0x56u,0x8bu,0xf1u,0x56u,0xe8u,0x17u,0x00u,0x00u,0x00u,0xf6u,
    0x44u,0x24u,0x08u,0x01u,0x74u,0x09u,0x56u,0xe8u,0x09u,0x69u,
    0x24u,0x00u,0x83u,0xc4u,0x04u,0x8bu,0xc6u,0x5eu,0xc2u,0x04u,
    0x00u
};

static const uint8_t gel_raw_entity_bytes[] = {
    0x83u,0xecu,0x0cu,0x8bu,0x51u,0x0cu,0x8du,0x04u,0x24u,0xc7u,
    0x04u,0x24u,0x00u,0x00u,0x00u,0x00u,0xc7u,0x44u,0x24u,0x04u,
    0x00u,0x00u,0x00u,0x00u,0xc7u,0x44u,0x24u,0x08u,0x00u,0x00u,
    0x00u,0x00u,0xe8u,0x7bu,0xffu,0xffu,0xffu,0x8bu,0x04u,0x24u,
    0x85u,0xc0u,0x75u,0x04u,0x83u,0xc4u,0x0cu,0xc3u,0x8du,0x0cu,
    0x24u,0x39u,0x48u,0x04u,0x75u,0x07u,0x8bu,0x54u,0x24u,0x08u,
    0x89u,0x50u,0x04u,0x8bu,0x54u,0x24u,0x04u,0x85u,0xd2u,0x74u,
    0x0bu,0x8bu,0x4cu,0x24u,0x08u,0x89u,0x4au,0x08u,0x8bu,0x54u,
    0x24u,0x04u,0x8bu,0x4cu,0x24u,0x08u,0x85u,0xc9u,0x74u,0x03u,
    0x89u,0x51u,0x04u,0x83u,0xc4u,0x0cu,0xc3u
};

static const uint8_t ai_listener_add_bytes[] = {
    0x8bu,0x54u,0x24u,0x04u,0x56u,0x85u,0xd2u,0x74u,0x10u,0x51u,
    0x8bu,0xc4u,0x8du,0xb1u,0xb0u,0x00u,0x00u,0x00u,0x89u,0x10u,
    0xe8u,0x97u,0x01u,0xfcu,0xffu,0x8du,0x4cu,0x24u,0x08u,0xe8u,
    0xbeu,0xeau,0xf0u,0xffu,0x5eu,0xc2u,0x0cu,0x00u
};

static const uint8_t ai_listener_remove_bytes[] = {
    0x8bu,0x54u,0x24u,0x04u,0x85u,0xd2u,0x74u,0x11u,0x51u,0x8bu,
    0xc4u,0x81u,0xc1u,0xb0u,0x00u,0x00u,0x00u,0x51u,0x89u,0x10u,
    0xe8u,0x07u,0x02u,0xfcu,0xffu,0x8du,0x4cu,0x24u,0x04u,0xe8u,
    0x8eu,0xeau,0xf0u,0xffu,0xc2u,0x0cu,0x00u
};

static const uint8_t formation_add_bytes[] = {
    0x51u,0x8bu,0x4eu,0x30u,0x8bu,0x54u,0x24u,0x08u,0x33u,0xc0u,
    0x57u,0x85u,0xc9u,0x7eu,0x0eu,0x8bu,0xfeu,0x39u,0x17u,0x74u,
    0x0bu,0x40u,0x83u,0xc7u,0x0cu,0x3bu,0xc1u,0x7cu,0xf4u,0x83u,
    0xc8u,0xffu,0x85u,0xd2u,0x74u,0x66u,0x83u,0xf8u,0xffu,0x75u,
    0x61u,0x83u,0xf9u,0x04u,0x74u,0x5cu,0x8du,0x04u,0x49u,0x41u,
    0x8du,0x04u,0x86u,0x89u,0x4eu,0x30u,0xe8u,0x63u,0xeau,0xf4u,
    0xffu,0xd9u,0x46u,0x38u,0x8bu,0x54u,0x24u,0x0cu,0x8bu,0x8au,
    0x94u,0x00u,0x00u,0x00u,0xd9u,0x81u,0x50u,0x01u,0x00u,0x00u,
    0xdeu,0xd9u,0xdfu,0xe0u,0xf6u,0xc4u,0x05u,0x7au,0x05u,0xd9u,
    0x46u,0x38u,0xebu,0x06u,0xd9u,0x81u,0x50u,0x01u,0x00u,0x00u,
    0xd9u,0x5cu,0x24u,0x04u,0x8bu,0xc6u,0xd9u,0x44u,0x24u,0x04u,
    0xd9u,0x5eu,0x38u,0x8bu,0x8au,0x94u,0x00u,0x00u,0x00u,0x89u,
    0x71u,0x40u,0xc6u,0x46u,0x50u,0x00u,0xe8u,0x9du,0x10u,0x00u,
    0x00u,0xb0u,0x01u,0x5fu,0x59u,0xc2u,0x04u,0x00u,0x32u,0xc0u,
    0x5fu,0x59u,0xc2u,0x04u,0x00u
};

static const uint8_t formation_remove_prefix[] = {
    0x83u,0xecu,0x0cu,0x53u,0x55u,0x8bu,0x6cu,0x24u,0x18u,0x8bu,
    0x55u,0x30u,0x56u,0x57u,0x33u,0xffu,0x33u,0xc0u,0x3bu,0xd7u,
    0x7eu,0x16u,0x8bu,0x74u,0x24u,0x24u,0x8bu,0xcdu
};

static const uint8_t formation_remove_early_ret[] = {
    0x32u,0xc0u,0x5fu,0x5eu,0x5du,0x5bu,0x83u,0xc4u,0x0cu,0xc2u,
    0x08u,0x00u
};

static const uint8_t formation_remove_backpointer_clear[] = {
    0x8bu,0x4cu,0x24u,0x24u,0x8bu,0x91u,0x94u,0x00u,0x00u,0x00u,
    0x89u,0x7au,0x40u
};

static const uint8_t formation_remove_final_tail[] = {
    0x8bu,0xc5u,0xc6u,0x45u,0x50u,0x00u,0xe8u,0xafu,0x0eu,0x00u,
    0x00u,0x5fu,0x5eu,0x5du,0xb0u,0x01u,0x5bu,0x83u,0xc4u,0x0cu,
    0xc2u,0x08u,0x00u
};

static const uint8_t resolver_prefix[] = {
    0x6au,0xffu,0x68u
};

static const uint8_t resolver_suffix[] = {
    0x64u,0xa1u,0x00u,0x00u,0x00u,0x00u,0x50u,0x64u,0x89u,0x25u,
    0x00u,0x00u,0x00u,0x00u,0x83u,0xecu,0x0cu,0x53u,0x56u,0x57u,
    0x33u,0xffu,0x89u,0x7cu,0x24u,0x0cu,0x89u,0x7cu,0x24u,0x10u,
    0x89u,0x7cu,0x24u,0x14u,0x8bu,0x44u,0x24u,0x28u,0x50u,0x8du,
    0x74u,0x24u,0x10u,0x89u,0x7cu,0x24u,0x24u,0xe8u,0x65u,0x00u,
    0x00u,0x00u,0x8bu,0x54u,0x24u,0x10u,0x8au
};

static const uint8_t async_middle[] = {
    0x00u,0x75u,0x0cu,0x83u,0x3du
};

static const uint8_t async_prefix[] = {0x83u, 0x3du};

static const uint8_t async_suffix[] = {
    0x00u,0x75u,0x03u,0x32u,0xc0u,0xc3u,0xb0u,0x01u,0xc3u
};

static const uint8_t canonicalizer_prefix[] = {
    0x55u,0x8bu,0xecu,0x83u,0xe4u,0xf8u,0xd9u,0x05u
};

static const uint8_t canonicalizer_suffix[] = {
    0x83u,0xecu,0x24u,0x53u,0x56u,0x8bu,0xf0u,0x8bu,0x4eu,0x30u,
    0xd9u,0x5eu,0x34u,0x57u,0x83u,0xf9u,0x03u,0x0fu,0x85u,0x8cu
};

static const SudekiMpTalosMembershipSymbolDescriptor symbol_descriptors[] = {
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, 0u, 0x400u, 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_NONE,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_NONE,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_GROUP_PLAYERS,
        RVA_GET_GROUP_PLAYERS, 6u, 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_CDECL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_POINTER32,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_PC, RVA_GET_PC,
        (uint32_t)sizeof(get_pc_bytes), 4u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_CDECL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_POINTER32,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER, RVA_ADD_PLAYER,
        (uint32_t)sizeof(add_player_bytes), 4u, 4u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER, RVA_REMOVE_PLAYER,
        (uint32_t)sizeof(remove_player_bytes), 4u, 4u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER, RVA_IS_PLAYER,
        (uint32_t)sizeof(is_player_bytes), 4u, 4u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX, RVA_GET_INDEX,
        (uint32_t)sizeof(get_index_bytes), 4u, 4u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_INT32,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ALL_PENDING_LOADED,
        RVA_ALL_PENDING_LOADED, (uint32_t)sizeof(all_pending_bytes), 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_EAX,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IN_COMBAT, RVA_IN_COMBAT,
        (uint32_t)sizeof(in_combat_bytes), 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE, RVA_ASYNC_ACTIVE,
        24u, 0u, 0u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_CDECL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_RESOLVER, RVA_GEL_RESOLVER,
        64u, 8u, 0u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_CDECL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_CLEANUP, RVA_GEL_CLEANUP,
        (uint32_t)sizeof(gel_cleanup_bytes), 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, RVA_GEL_VTABLE,
        0x30u, 0u, 0u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_NONE,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_NONE,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER, RVA_AI_LISTENER_VTABLE,
        0x20u, 12u, 12u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_ADD, RVA_FORMATION_ADD,
        (uint32_t)sizeof(formation_add_bytes), 4u, 4u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ESI, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE,
        RVA_FORMATION_REMOVE, 0x1ddu, 8u, 8u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_STACK,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_NONE, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_CANONICALIZER,
        RVA_FORMATION_CANONICALIZER, 32u, 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EAX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE,
        RVA_GROUP_REMOVE_EPILOGUE, (uint32_t)sizeof(remove_epilogue_bytes),
        0u, 0u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EDI, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
        RVA_UI_CONTROLLER_DISPATCH,
        (uint32_t)sizeof(ui_controller_dispatch_bytes), 16u, 16u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH,
        RVA_HUD_CHILD_DISPATCH, 0x1a6u, 12u, 12u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_BOOL_AL,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED,
        RVA_PLAYER_SET_ARMED, (uint32_t)sizeof(player_set_armed_bytes),
        4u, 4u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH,
        RVA_STAT_DISPLAY_REFRESH,
        (uint32_t)sizeof(stat_display_refresh_bytes), 8u, 8u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ESI, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL,
        RVA_STAT_BAR_FILL, (uint32_t)sizeof(stat_bar_fill_bytes), 4u, 4u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EDI, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_CAMERA_SYNC,
        RVA_STAT_DISPLAY_CAMERA_SYNC,
        (uint32_t)sizeof(stat_display_camera_sync_bytes), 0u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_THISCALL,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_VOID,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR,
        RVA_HUD_RESOURCE_SELECTOR,
        (uint32_t)sizeof(hud_resource_selector_bytes), 4u, 0u,
        SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_POINTER32,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_ECX, 1u},
    {SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN,
        RVA_HUD_STRING_ASSIGN, (uint32_t)sizeof(hud_string_assign_bytes),
        4u, 4u, SUDEKIMP_TALOS_MEMBERSHIP_CALL_INTERNAL_REGISTER,
        SUDEKIMP_TALOS_MEMBERSHIP_RETURN_POINTER32,
        SUDEKIMP_TALOS_MEMBERSHIP_REGISTER_EDI, 1u}
};

typedef char symbol_descriptor_count_must_match[
    ARRAY_COUNT(symbol_descriptors) == SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_COUNT
        ? 1 : -1];

static uint32_t read_u32(const uint8_t *bytes) {
    uint32_t value;

    memcpy(&value, bytes, sizeof(value));
    return value;
}

static uint16_t read_u16(const uint8_t *bytes) {
    uint16_t value;

    memcpy(&value, bytes, sizeof(value));
    return value;
}

static void fail_validation(
    SudekiMpTalosMembershipValidationResult *result,
    SudekiMpTalosMembershipValidationFailure failure,
    SudekiMpTalosMembershipSymbol symbol,
    uint32_t rva,
    uint32_t expected,
    uint32_t observed
) {
    if (result->failure != SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_OK) return;
    result->failure = (uint32_t)failure;
    result->failed_symbol = (uint32_t)symbol;
    result->failed_rva = rva;
    result->expected_value = expected;
    result->observed_value = observed;
}

static int check_bytes(
    const uint8_t *image,
    uint32_t rva,
    const uint8_t *expected,
    size_t byte_count,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    size_t index;

    ++result->checks_completed;
    for (index = 0u; index < byte_count; ++index) {
        if (image[rva + index] != expected[index]) {
            fail_validation(result,
                SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_STABLE_BYTES,
                symbol, rva + (uint32_t)index, expected[index],
                image[rva + index]);
            return 0;
        }
    }
    return 1;
}

static int check_bytes_ignoring_u32s(
    const uint8_t *image,
    uint32_t rva,
    const uint8_t *expected,
    size_t byte_count,
    const uint16_t *ignored_u32_offsets,
    size_t ignored_u32_count,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    size_t index;

    ++result->checks_completed;
    for (index = 0u; index < byte_count; ++index) {
        size_t ignored_index;
        int ignored = 0;

        for (ignored_index = 0u; ignored_index < ignored_u32_count;
                ++ignored_index) {
            size_t start = ignored_u32_offsets[ignored_index];

            if (index >= start && index < start + sizeof(uint32_t)) {
                ignored = 1;
                break;
            }
        }
        if (!ignored && image[rva + index] != expected[index]) {
            fail_validation(result,
                SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_STABLE_BYTES,
                symbol, rva + (uint32_t)index, expected[index],
                image[rva + index]);
            return 0;
        }
    }
    return 1;
}

static int check_u32(
    const uint8_t *image,
    uint32_t rva,
    uint32_t expected,
    SudekiMpTalosMembershipValidationFailure failure,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    uint32_t observed = read_u32(image + rva);

    ++result->checks_completed;
    if (observed != expected) {
        fail_validation(result, failure, symbol, rva, expected, observed);
        return 0;
    }
    return 1;
}

static int check_rel32_call(
    const uint8_t *image,
    uint32_t call_rva,
    uint32_t expected_target_rva,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    int64_t observed_target;

    ++result->checks_completed;
    if (image[call_rva] != 0xe8u) {
        fail_validation(result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_REL32_TARGET,
            symbol, call_rva, 0xe8u, image[call_rva]);
        return 0;
    }
    observed_target = (int64_t)call_rva + INT64_C(5) +
        (int64_t)(int32_t)read_u32(image + call_rva + 1u);
    if (observed_target != (int64_t)expected_target_rva) {
        fail_validation(result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_REL32_TARGET,
            symbol, call_rva, expected_target_rva,
            (uint32_t)observed_target);
        return 0;
    }
    return 1;
}

static int check_relocated(
    const uint8_t *image,
    uint32_t operand_rva,
    uint32_t loaded_base,
    uint32_t target_rva,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    return check_u32(image, operand_rva, loaded_base + target_rva,
        SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_RELOCATED_OPERAND,
        symbol, result);
}

static int check_relocation_set(
    const uint8_t *image,
    uint32_t window_rva,
    uint32_t loaded_base,
    const uint16_t *operand_offsets,
    const uint32_t *target_rvas,
    size_t relocation_count,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    size_t index;

    for (index = 0u; index < relocation_count; ++index) {
        if (!check_relocated(image, window_rva + operand_offsets[index],
                loaded_base, target_rvas[index], symbol, result)) return 0;
    }
    return 1;
}

static int check_relocated_table(
    const uint8_t *image,
    uint32_t table_rva,
    uint32_t loaded_base,
    const uint32_t *target_rvas,
    size_t entry_count,
    SudekiMpTalosMembershipSymbol symbol,
    SudekiMpTalosMembershipValidationResult *result
) {
    size_t index;

    for (index = 0u; index < entry_count; ++index) {
        if (!check_relocated(image,
                table_rva + (uint32_t)(index * sizeof(uint32_t)),
                loaded_base, target_rvas[index], symbol, result)) return 0;
    }
    return 1;
}

static void mark_validated(
    SudekiMpTalosMembershipValidationResult *result,
    SudekiMpTalosMembershipSymbol symbol
) {
    result->validated_symbol_mask |= SYMBOL_BIT(symbol);
}

SudekiMpTalosMembershipAbiDescriptor
SudekiMpTalosCompanionMembershipAbiDescribe(void) {
    SudekiMpTalosMembershipAbiDescriptor descriptor;
    static const uint32_t digest_words[8] = {
        0x8ceb1d3cu, 0xf667ad90u, 0x6f13252cu, 0xb5bdf762u,
        0xeb018ebbu, 0xecb8bffeu, 0xb92f3b27u, 0xb0dfbb94u
    };

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.abi_version = SUDEKIMP_TALOS_MEMBERSHIP_ABI_VERSION;
    descriptor.preferred_image_base =
        SUDEKIMP_TALOS_MEMBERSHIP_PREFERRED_BASE;
    descriptor.mapped_image_size = SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE;
    descriptor.required_symbol_mask =
        (SYMBOL_BIT(SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_COUNT) - 1u);
    descriptor.symbol_count = SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_COUNT;
    descriptor.native_capacity = SUDEKIMP_TALOS_MEMBERSHIP_NATIVE_CAPACITY;
    descriptor.get_pc_wrapper_size =
        SUDEKIMP_TALOS_MEMBERSHIP_GET_PC_WRAPPER_SIZE;
    descriptor.get_pc_wrapper_helper_rva =
        RVA_GET_PC_WRAPPER_CONSTRUCTOR;
    descriptor.get_pc_wrapper_factory_rva = RVA_GEL_WRAPPER_FACTORY;
    descriptor.get_pc_wrapper_core_destructor_rva =
        RVA_GEL_CORE_DESTRUCTOR;
    descriptor.get_pc_wrapper_scalar_destructor_rva =
        RVA_GEL_DELETING_DESTRUCTOR;
    descriptor.embedded_tptr_size =
        SUDEKIMP_TALOS_MEMBERSHIP_EMBEDDED_TPTR_SIZE;
    descriptor.embedded_tptr_offset = 0x0cu;
    descriptor.ptr_object_registration_constructor_rva =
        RVA_PTR_REGISTRATION_CONSTRUCTOR;
    descriptor.ptr_registry_find_rva = RVA_PTR_REGISTRY_FIND;
    descriptor.ptr_registry_erase_rva = RVA_PTR_REGISTRY_ERASE;
    descriptor.ptr_registry_delete_all_rva = RVA_PTR_REGISTRY_DELETE_ALL;
    descriptor.ptr_registry_root_rva = RVA_PTR_REGISTRY_ROOT;
    descriptor.ptr_registry_count_rva = RVA_PTR_REGISTRY_COUNT;
    descriptor.group_add_core_rva = RVA_GROUP_ADD_CORE;
    descriptor.group_remove_core_rva = RVA_GROUP_REMOVE_CORE;
    descriptor.group_remove_epilogue_rva = RVA_GROUP_REMOVE_EPILOGUE;
    descriptor.ui_controller_global_rva = RVA_UI_CONTROLLER_GLOBAL;
    descriptor.ui_controller_vtable_rva = RVA_UI_CONTROLLER_VTABLE;
    descriptor.ui_controller_dispatch_slot_offset = 0x20u;
    descriptor.ui_controller_dispatch_rva = RVA_UI_CONTROLLER_DISPATCH;
    descriptor.ui_controller_hud_child_offset = 0x6cu;
    descriptor.hud_child_vtable_rva = RVA_HUD_CHILD_VTABLE;
    descriptor.hud_child_dispatch_slot_offset = 0x2cu;
    descriptor.hud_child_dispatch_rva = RVA_HUD_CHILD_DISPATCH;
    descriptor.character_arbiter_offset = 0x90u;
    descriptor.player_set_armed_rva = RVA_PLAYER_SET_ARMED;
    descriptor.character_stat_display_offset = 0xb0u;
    descriptor.stat_display_refresh_rva = RVA_STAT_DISPLAY_REFRESH;
    descriptor.stat_display_constructor_rva = RVA_STAT_DISPLAY_CONSTRUCTOR;
    descriptor.stat_display_primary_vtable_rva =
        RVA_STAT_DISPLAY_PRIMARY_VTABLE;
    descriptor.stat_display_secondary_vtable_rva =
        RVA_STAT_DISPLAY_SECONDARY_VTABLE;
    descriptor.stat_display_health_bar_offset = 0xd0u;
    descriptor.stat_display_last_hp_offset = 0x16cu;
    descriptor.stat_bar_renderer_offset = 0x34u;
    descriptor.stat_bar_handle_offset = 0x4cu;
    descriptor.stat_bar_count_offset = 0x50u;
    descriptor.stat_bar_cache_offset = 0x58u;
    descriptor.stat_bar_fill_rva = RVA_STAT_BAR_FILL;
    descriptor.stat_bar_fill_early_return_rva =
        RVA_STAT_BAR_FILL_EARLY_RETURN;
    descriptor.stat_bar_fill_return_rva = RVA_STAT_BAR_FILL_RETURN;
    descriptor.stat_display_camera_sync_rva =
        RVA_STAT_DISPLAY_CAMERA_SYNC;
    descriptor.stat_display_camera_init_rva =
        RVA_STAT_DISPLAY_CAMERA_INIT;
    descriptor.stat_display_camera_ui_scene_global_rva =
        RVA_UI_SCENE_GLOBAL;
    descriptor.stat_display_camera_saved_bounds_rva =
        RVA_STAT_DISPLAY_SAVED_BOUNDS;
    descriptor.stat_display_camera_active_bounds_rva =
        RVA_STAT_DISPLAY_ACTIVE_BOUNDS;
    descriptor.stat_display_camera_manager_global_rva =
        RVA_CAMERA_MANAGER_GLOBAL;
    descriptor.stat_display_camera_ui_scene_last_float_offset = 0x158u;
    descriptor.stat_display_scene_node_offset = 0x58u;
    descriptor.stat_display_owner_offset = 0xccu;
    descriptor.stat_display_owner_node_offset = 0x08u;
    descriptor.scene_node_dirty_word_offset = 0x2cu;
    descriptor.scene_node_matrix_offset = 0x90u;
    descriptor.scene_node_matrix_float_count = 16u;
    descriptor.camera_manager_active_offset = 0x20u;
    descriptor.camera_active_payload_offset = 0x34u;
    descriptor.camera_payload_position_offset = 0xc0u;
    descriptor.hud_resource_selector_rva = RVA_HUD_RESOURCE_SELECTOR;
    descriptor.hud_resource_selector_jump_table_rva =
        RVA_HUD_RESOURCE_SELECTOR_JUMP_TABLE;
    descriptor.hud_resource_initialized_rva =
        RVA_HUD_RESOURCE_INITIALIZED;
    descriptor.hud_resource_table_global_rva =
        RVA_HUD_RESOURCE_TABLE_GLOBAL;
    descriptor.hud_resource_inline_text_rva =
        RVA_HUD_RESOURCE_INLINE_TEXT;
    descriptor.hud_resource_error_text_rva =
        RVA_HUD_RESOURCE_ERROR_TEXT;
    descriptor.hud_resource_default_text_rva =
        RVA_HUD_RESOURCE_DEFAULT_TEXT;
    descriptor.hud_resource_fallback_text_rva =
        RVA_HUD_RESOURCE_FALLBACK_TEXT;
    descriptor.hud_resource_missing_report_rva =
        RVA_HUD_RESOURCE_MISSING_REPORT;
    descriptor.hud_resource_actor_component_offset = 0x94u;
    descriptor.hud_resource_set_offset = 0x3cu;
    descriptor.hud_resource_count_offset = 0x09u;
    descriptor.hud_resource_entries_offset = 0x04u;
    descriptor.hud_resource_id_offset = 0x19u;
    descriptor.hud_resource_table_count_offset = 0x08u;
    descriptor.hud_resource_table_data_offset = 0x10u;
    descriptor.hud_resource_table_first_slot_offset = 0x4d8u;
    descriptor.hud_resource_table_slot_stride = 4u;
    descriptor.hud_resource_table_slot_count = 11u;
    descriptor.hud_resource_first_table_id = 0x136u;
    descriptor.hud_resource_selected_id_min = 1u;
    descriptor.hud_resource_selected_id_max = 11u;
    descriptor.hud_string_assign_rva = RVA_HUD_STRING_ASSIGN;
    descriptor.hud_string_free_rva = RVA_HUD_STRING_FREE;
    descriptor.hud_string_allocate_rva = RVA_HUD_STRING_ALLOCATE;
    descriptor.hud_string_copy_rva = RVA_HUD_STRING_COPY;
    descriptor.hud_portrait_gizmo_label_offset = 0x2e0u;
    descriptor.hud_string_control_offset = 0u;
    descriptor.hud_string_data_offset = 0x04u;
    descriptor.hud_string_inline_mask = UINT32_C(0x80000000);
    descriptor.hud_string_inline_capacity_utf16 = 0x1cu;
    descriptor.hud_string_proof_max_utf16_units = 27u;
    descriptor.group_members_offset = 0x90u;
    descriptor.group_count_offset = 0xccu;
    descriptor.formation_members_offset = 0xf4u;
    descriptor.formation_count_offset = 0x124u;
    descriptor.ai_manager_group_listener_offset = 0x44u;
    descriptor.group_listener_add_slot_offset = 0x18u;
    descriptor.group_listener_remove_slot_offset = 0x1cu;
    descriptor.group_listener_to_formation_offset = 0xb0u;
    descriptor.ai_manager_formation_offset = 0xf4u;
    memcpy(descriptor.supported_sha256_words, digest_words,
        sizeof(digest_words));
    descriptor.pure_validation_only = 1u;
    descriptor.native_calls_permitted = 0u;
    descriptor.hooks_permitted = 0u;
    descriptor.enabled_by_default = 0u;
    descriptor.external_sha256_required = 1u;
    return descriptor;
}

SudekiMpTalosMembershipSymbolDescriptor
SudekiMpTalosCompanionMembershipAbiDescribeSymbol(uint32_t index) {
    SudekiMpTalosMembershipSymbolDescriptor descriptor;

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.symbol = SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_INVALID;
    if (index < ARRAY_COUNT(symbol_descriptors)) {
        descriptor = symbol_descriptors[index];
    }
    return descriptor;
}

SudekiMpTalosMembershipValidationResult
SudekiMpTalosCompanionMembershipAbiValidateMappedImage(
    const uint8_t *image,
    size_t image_size,
    uint32_t loaded_image_base
) {
    SudekiMpTalosMembershipValidationResult result;
    SudekiMpTalosMembershipAbiDescriptor descriptor =
        SudekiMpTalosCompanionMembershipAbiDescribe();
    static const uint8_t mz[] = {0x4du, 0x5au};
    static const uint8_t pe[] = {0x50u, 0x45u, 0x00u, 0x00u};
    static const uint8_t get_group_prefix[] = {0xa1u};
    static const uint8_t get_group_suffix[] = {0xc3u};
    static const uint8_t gel_type_prefix[] = {0xb8u};
    static const uint8_t gel_type_suffix[] = {0xc3u};
    uint32_t pe_rva;
    uint32_t header_image_base;

    memset(&result, 0, sizeof(result));
    result.abi_version = descriptor.abi_version;
    result.required_symbol_mask = descriptor.required_symbol_mask;
    result.failed_symbol = SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_INVALID;
    result.pure_validation_only = 1u;
    result.native_calls_permitted = 0u;
    result.external_sha256_required = 1u;

    if (image == NULL) {
        fail_validation(&result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_NULL_IMAGE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, 0u, 1u, 0u);
        return result;
    }
    if (image_size != SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE) {
        fail_validation(&result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_IMAGE_SIZE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, 0u,
            SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE, (uint32_t)image_size);
        return result;
    }
    if (loaded_image_base == 0u ||
        (loaded_image_base & 0xffffu) != 0u ||
        loaded_image_base > UINT32_MAX - RVA_MAX_RELOCATED_TARGET) {
        fail_validation(&result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_LOADED_BASE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, 0u,
            SUDEKIMP_TALOS_MEMBERSHIP_PREFERRED_BASE, loaded_image_base);
        return result;
    }

    if (!check_bytes(image, 0u, mz, sizeof(mz),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, &result)) return result;
    pe_rva = read_u32(image + 0x3cu);
    ++result.checks_completed;
    if (pe_rva != 0x128u) {
        fail_validation(&result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_PE_HEADER,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, 0x3cu, 0x128u, pe_rva);
        return result;
    }
    if (!check_bytes(image, pe_rva, pe, sizeof(pe),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, &result) ||
        !check_u32(image, pe_rva + 4u,
            UINT32_C(0x0005014c),
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_PE_HEADER,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, &result)) return result;
    ++result.checks_completed;
    if (read_u16(image + pe_rva + 24u) != 0x010bu) {
        fail_validation(&result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_PE_HEADER,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, pe_rva + 24u,
            0x010bu, read_u16(image + pe_rva + 24u));
        return result;
    }
    header_image_base = read_u32(image + pe_rva + 52u);
    ++result.checks_completed;
    if (header_image_base != loaded_image_base) {
        fail_validation(&result,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_PE_HEADER,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, pe_rva + 52u,
            loaded_image_base, header_image_base);
        return result;
    }
    if (!check_u32(image, pe_rva + 80u,
            SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE,
            SUDEKIMP_TALOS_MEMBERSHIP_VALIDATION_PE_HEADER,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE, &result)) return result;
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IMAGE);

    if (!check_bytes(image, RVA_GET_GROUP_PLAYERS, get_group_prefix,
            sizeof(get_group_prefix),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_GROUP_PLAYERS, &result) ||
        !check_relocated(image, RVA_GET_GROUP_PLAYERS + 1u,
            loaded_image_base, 0x00408d94u,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_GROUP_PLAYERS, &result) ||
        !check_bytes(image, RVA_GET_GROUP_PLAYERS + 5u, get_group_suffix,
            sizeof(get_group_suffix),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_GROUP_PLAYERS, &result))
        return result;
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_GROUP_PLAYERS);

#define CHECK_CALL(call_rva, target_rva, symbol_value) \
    if (!check_rel32_call(image, (call_rva), (target_rva), \
            (symbol_value), &result)) return result
#define CHECK_WINDOW(rva_value, bytes_value, symbol_value) \
    if (!check_bytes(image, (rva_value), (bytes_value), \
            sizeof(bytes_value), (symbol_value), &result)) return result
#define CHECK_MASKED_WINDOW(rva_value, bytes_value, holes_value, symbol_value) \
    if (!check_bytes_ignoring_u32s(image, (rva_value), (bytes_value), \
            sizeof(bytes_value), (holes_value), ARRAY_COUNT(holes_value), \
            (symbol_value), &result)) return result

    CHECK_CALL(RVA_GET_PC + 0x0bu, RVA_GET_PC_LOOKUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_PC);
    CHECK_CALL(RVA_GET_PC + 0x16u, RVA_GET_PC_WRAPPER_CONSTRUCTOR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_PC);
    CHECK_WINDOW(RVA_GET_PC, get_pc_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_PC);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_PC);

    CHECK_CALL(RVA_ADD_PLAYER + 0x0au, RVA_GEL_POINTER_CONSTRUCTOR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_ADD_PLAYER + 0x19u, RVA_GEL_RESOLVER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_ADD_PLAYER + 0x30u, RVA_GROUP_ADD_CORE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_ADD_PLAYER + 0x39u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0x4au, RVA_GEL_POINTER_ASSIGN,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0x64u, RVA_GEL_POINTER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0x74u, RVA_GROUP_PRIMARY_CHANGED,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0x7du, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0xb5u, RVA_GEL_OBSERVER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0xdcu, RVA_PLAYER_SET_ARMED,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_CALL(RVA_GROUP_ADD_CORE + 0xfdu, RVA_STAT_DISPLAY_REFRESH,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_WINDOW(RVA_ADD_PLAYER, add_player_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_WINDOW(RVA_GROUP_ADD_CORE, group_add_core_front,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_WINDOW(RVA_GROUP_ADD_CORE + 0x82u, group_add_core_listener,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    CHECK_WINDOW(RVA_GROUP_ADD_CORE + 0x102u, group_add_core_return,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ADD_PLAYER);

    CHECK_CALL(RVA_REMOVE_PLAYER + 0x0au, RVA_GEL_POINTER_CONSTRUCTOR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_CALL(RVA_REMOVE_PLAYER + 0x19u, RVA_GEL_RESOLVER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_CALL(RVA_REMOVE_PLAYER + 0x31u, RVA_GROUP_REMOVE_CORE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_CALL(RVA_REMOVE_PLAYER + 0x3au, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_CALL(RVA_GROUP_REMOVE_CORE + 0x53u,
        RVA_GROUP_PREPARE_PRIMARY_REMOVAL,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_CALL(RVA_GROUP_REMOVE_CORE + 0x1d2u, RVA_GEL_OBSERVER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_CALL(RVA_GROUP_REMOVE_CORE + 0x1eau,
        RVA_GROUP_REMOVE_EPILOGUE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    if (!check_relocated(image, RVA_GROUP_REMOVE_CORE + 0x1e6u,
            loaded_image_base, RVA_UI_CONTROLLER_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER, &result))
        return result;
    CHECK_WINDOW(RVA_REMOVE_PLAYER, remove_player_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_WINDOW(RVA_GROUP_REMOVE_CORE, group_remove_core_front,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_WINDOW(RVA_GROUP_REMOVE_CORE + 0x74u,
        group_remove_core_compaction,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_WINDOW(RVA_GROUP_REMOVE_CORE + 0x1a6u,
        group_remove_core_listener,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    CHECK_MASKED_WINDOW(RVA_GROUP_REMOVE_CORE + 0x1e4u,
        group_remove_core_return, group_remove_core_return_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_PLAYER);

    CHECK_CALL(RVA_IS_PLAYER + 0x0au, RVA_GEL_POINTER_CONSTRUCTOR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);
    CHECK_CALL(RVA_IS_PLAYER + 0x19u, RVA_GEL_RESOLVER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);
    CHECK_CALL(RVA_IS_PLAYER + 0x4eu, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);
    CHECK_CALL(RVA_IS_PLAYER + 0x63u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);
    CHECK_CALL(RVA_IS_PLAYER + 0x76u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);
    CHECK_WINDOW(RVA_IS_PLAYER, is_player_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IS_PLAYER);

    CHECK_CALL(RVA_GET_INDEX + 0x0au, RVA_GEL_POINTER_CONSTRUCTOR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);
    CHECK_CALL(RVA_GET_INDEX + 0x19u, RVA_GEL_RESOLVER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);
    CHECK_CALL(RVA_GET_INDEX + 0x56u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);
    CHECK_CALL(RVA_GET_INDEX + 0x6bu, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);
    CHECK_CALL(RVA_GET_INDEX + 0x7eu, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);
    CHECK_WINDOW(RVA_GET_INDEX, get_index_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GET_INDEX);

    CHECK_WINDOW(RVA_ALL_PENDING_LOADED, all_pending_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ALL_PENDING_LOADED);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ALL_PENDING_LOADED);
    CHECK_WINDOW(RVA_IN_COMBAT, in_combat_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IN_COMBAT);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_IN_COMBAT);

    CHECK_WINDOW(RVA_ASYNC_ACTIVE, async_prefix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE);
    if (!check_relocated(image, RVA_ASYNC_ACTIVE + 2u, loaded_image_base,
            RVA_ASYNC_PENDING_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE, &result))
        return result;
    CHECK_WINDOW(RVA_ASYNC_ACTIVE + 6u, async_middle,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE);
    if (!check_relocated(image, RVA_ASYNC_ACTIVE + 0x0bu,
            loaded_image_base, RVA_ASYNC_STREAM_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE, &result))
        return result;
    CHECK_WINDOW(RVA_ASYNC_ACTIVE + 0x0fu, async_suffix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_ASYNC_ACTIVE);

    CHECK_WINDOW(RVA_GEL_RESOLVER, resolver_prefix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_RESOLVER);
    if (!check_relocated(image, RVA_GEL_RESOLVER + 3u, loaded_image_base,
            RVA_GEL_RESOLVER_SEH,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_RESOLVER, &result))
        return result;
    CHECK_WINDOW(RVA_GEL_RESOLVER + 7u, resolver_suffix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_RESOLVER);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_RESOLVER);

    CHECK_WINDOW(RVA_GEL_CLEANUP, gel_cleanup_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_CLEANUP);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_CLEANUP);

    if (!check_relocated(image, RVA_GEL_VTABLE, loaded_image_base,
            RVA_GEL_DELETING_DESTRUCTOR,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_VTABLE + 0x10u, loaded_image_base,
            RVA_GEL_GET_RAW_ENTITY,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_VTABLE + 0x2cu, loaded_image_base,
            RVA_GEL_GET_TYPE_NAME,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result))
        return result;
    CHECK_CALL(RVA_GET_PC_WRAPPER_CONSTRUCTOR + 0x7bu,
        RVA_GEL_WRAPPER_FACTORY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_WRAPPER_FACTORY + 0x20u, RVA_NATIVE_HEAP_ALLOCATE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_WRAPPER_FACTORY + 0x37u,
        RVA_PTR_REGISTRATION_CONSTRUCTOR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_WRAPPER_FACTORY + 0x58u, RVA_GEL_OBSERVER_LINK,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x15u,
        RVA_NATIVE_HEAP_ALLOCATE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x50u,
        RVA_PTR_REGISTRY_INSERT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x6cu,
        RVA_PTR_ALLOCATION_FAILURE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_DELETING_DESTRUCTOR + 4u, 0x00001b50u,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_DELETING_DESTRUCTOR + 0x11u, RVA_NATIVE_HEAP_FREE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_CORE_DESTRUCTOR + 0x7bu, RVA_PTR_REGISTRY_FIND,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_CORE_DESTRUCTOR + 0x91u, RVA_PTR_REGISTRY_ERASE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x0fu, RVA_PTR_TREE_ERASE_ERROR,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x1eu, RVA_PTR_TREE_ERASE_PREPARE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0xb8u, RVA_PTR_TREE_MAXIMUM,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x161u, RVA_PTR_TREE_ROTATE_LEFT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x18fu, RVA_PTR_TREE_ROTATE_RIGHT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x1a8u, RVA_PTR_TREE_ROTATE_LEFT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x1beu, RVA_PTR_TREE_ROTATE_RIGHT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x205u, RVA_PTR_TREE_ROTATE_LEFT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x21cu, RVA_PTR_TREE_ROTATE_RIGHT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_PTR_REGISTRY_ERASE + 0x225u, RVA_NATIVE_HEAP_FREE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);

    if (!check_relocated(image, RVA_GET_PC_WRAPPER_CONSTRUCTOR + 3u,
            loaded_image_base, RVA_GEL_RESOLVER_SEH,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_WRAPPER_FACTORY + 3u,
            loaded_image_base, RVA_GEL_FACTORY_SEH,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_WRAPPER_FACTORY + 0x47u,
            loaded_image_base, RVA_GEL_VTABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x0au,
            loaded_image_base, RVA_PTR_OBJECT_VTABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x25u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x2du,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x36u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x68u,
            loaded_image_base, RVA_PTR_ALLOCATION_FAILURE_VTABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_CORE_DESTRUCTOR + 3u,
            loaded_image_base, RVA_GEL_DESTRUCTOR_SEH,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_CORE_DESTRUCTOR + 0x73u,
            loaded_image_base, RVA_PTR_OBJECT_VTABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_GEL_CORE_DESTRUCTOR + 0x85u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_DELETE_ALL + 2u,
            loaded_image_base, RVA_PTR_REGISTRY_COUNT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_DELETE_ALL + 0x11u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_DELETE_ALL + 0x28u,
            loaded_image_base, RVA_PTR_REGISTRY_COUNT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_FIND + 9u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x0bu,
            loaded_image_base, RVA_PTR_REGISTRY_ERROR_TEXT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x56u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x71u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x9eu,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0xbfu,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0xf5u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x132u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x1e0u,
            loaded_image_base, RVA_PTR_REGISTRY_ROOT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x22fu,
            loaded_image_base, RVA_PTR_REGISTRY_COUNT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result) ||
        !check_relocated(image, RVA_PTR_REGISTRY_ERASE + 0x238u,
            loaded_image_base, RVA_PTR_REGISTRY_COUNT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result))
        return result;

    CHECK_MASKED_WINDOW(RVA_GET_PC_WRAPPER_CONSTRUCTOR,
        gel_wrapper_helper_bytes, gel_wrapper_helper_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_MASKED_WINDOW(RVA_GEL_WRAPPER_FACTORY, gel_wrapper_factory_bytes,
        gel_wrapper_factory_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_MASKED_WINDOW(RVA_PTR_REGISTRATION_CONSTRUCTOR,
        ptr_registration_constructor_bytes,
        ptr_registration_constructor_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_MASKED_WINDOW(RVA_GEL_CORE_DESTRUCTOR, gel_core_destructor_bytes,
        gel_core_destructor_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_MASKED_WINDOW(RVA_PTR_REGISTRY_DELETE_ALL,
        ptr_registry_delete_all_bytes, ptr_registry_delete_all_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_MASKED_WINDOW(RVA_PTR_REGISTRY_FIND, ptr_registry_find_bytes,
        ptr_registry_find_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE, ptr_registry_erase_entry,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x0fu,
        ptr_registry_erase_after_text,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x55u,
        opcode_mov_eax_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x6fu,
        opcode_mov_edx_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x9cu,
        opcode_mov_ecx_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0xbdu,
        opcode_mov_edx_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0xf3u,
        opcode_mov_ecx_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x131u,
        opcode_mov_eax_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x1deu,
        opcode_mov_ecx_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x22du,
        opcode_compare_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x236u,
        opcode_subtract_absolute,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_MASKED_WINDOW(RVA_PTR_REGISTRY_ERASE + 0x220u,
        ptr_registry_erase_tail, ptr_registry_erase_tail_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_GEL_DELETING_DESTRUCTOR, gel_destructor_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_CALL(RVA_GEL_GET_RAW_ENTITY + 0x20u, RVA_GEL_POINTER_ASSIGN,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_GEL_GET_RAW_ENTITY, gel_raw_entity_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    CHECK_WINDOW(RVA_GEL_GET_TYPE_NAME, gel_type_prefix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    if (!check_relocated(image, RVA_GEL_GET_TYPE_NAME + 1u,
            loaded_image_base, RVA_GEL_TYPE_NAME_TEXT,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE, &result))
        return result;
    CHECK_WINDOW(RVA_GEL_GET_TYPE_NAME + 5u, gel_type_suffix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_GEL_VTABLE);

    if (!check_relocated(image, RVA_AI_LISTENER_VTABLE + 0x18u,
            loaded_image_base, RVA_AI_LISTENER_ADD,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER, &result) ||
        !check_relocated(image, RVA_AI_LISTENER_VTABLE + 0x1cu,
            loaded_image_base, RVA_AI_LISTENER_REMOVE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER, &result))
        return result;
    CHECK_CALL(RVA_AI_LISTENER_ADD + 0x14u, RVA_FORMATION_ADD,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);
    CHECK_CALL(RVA_AI_LISTENER_ADD + 0x1du, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);
    CHECK_WINDOW(RVA_AI_LISTENER_ADD, ai_listener_add_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);
    CHECK_CALL(RVA_AI_LISTENER_REMOVE + 0x14u, RVA_FORMATION_REMOVE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);
    CHECK_CALL(RVA_AI_LISTENER_REMOVE + 0x1du, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);
    CHECK_WINDOW(RVA_AI_LISTENER_REMOVE, ai_listener_remove_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_AI_LISTENER);

    CHECK_CALL(RVA_FORMATION_ADD + 0x38u, RVA_GEL_POINTER_ASSIGN,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_ADD);
    CHECK_CALL(RVA_FORMATION_ADD + 0x7eu, RVA_FORMATION_CANONICALIZER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_ADD);
    CHECK_WINDOW(RVA_FORMATION_ADD, formation_add_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_ADD);
    mark_validated(&result, SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_ADD);

    CHECK_WINDOW(RVA_FORMATION_REMOVE, formation_remove_prefix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    CHECK_WINDOW(RVA_FORMATION_REMOVE + 0x2cu, formation_remove_early_ret,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    CHECK_CALL(RVA_FORMATION_REMOVE + 0x5cu, RVA_GEL_POINTER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    CHECK_CALL(RVA_FORMATION_REMOVE + 0x115u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    CHECK_WINDOW(RVA_FORMATION_REMOVE + 0x1b9u,
        formation_remove_backpointer_clear,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    CHECK_CALL(RVA_FORMATION_REMOVE + 0x1ccu,
        RVA_FORMATION_CANONICALIZER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    CHECK_WINDOW(RVA_FORMATION_REMOVE + 0x1c6u,
        formation_remove_final_tail,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_REMOVE);

    CHECK_WINDOW(RVA_FORMATION_CANONICALIZER, canonicalizer_prefix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_CANONICALIZER);
    if (!check_relocated(image, RVA_FORMATION_CANONICALIZER + 8u,
            loaded_image_base, RVA_FORMATION_DEFAULT_DISTANCE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_CANONICALIZER,
            &result)) return result;
    CHECK_WINDOW(RVA_FORMATION_CANONICALIZER + 12u, canonicalizer_suffix,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_CANONICALIZER);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_FORMATION_CANONICALIZER);

    if (!check_relocated(image, RVA_GROUP_REMOVE_EPILOGUE + 1u,
            loaded_image_base, RVA_ACTIVE_GROUP_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE, &result))
        return result;
    CHECK_CALL(RVA_GROUP_REMOVE_EPILOGUE + 0x23u, RVA_GEL_POINTER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE);
    CHECK_CALL(RVA_GROUP_REMOVE_EPILOGUE + 0x30u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE);
    CHECK_CALL(RVA_GROUP_REMOVE_EPILOGUE + 0x173u,
        RVA_STAT_DISPLAY_REFRESH,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE);
    CHECK_MASKED_WINDOW(RVA_GROUP_REMOVE_EPILOGUE,
        remove_epilogue_bytes, remove_epilogue_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_REMOVE_EPILOGUE);

    if (!check_relocated(image, RVA_UI_CONTROLLER_VTABLE + 0x20u,
            loaded_image_base, RVA_UI_CONTROLLER_DISPATCH,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_CONTROLLER_DISPATCH + 0x04u,
            loaded_image_base, RVA_UI_SCENE_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_CONTROLLER_DISPATCH + 0x21u,
            loaded_image_base, RVA_UI_DISPATCH_SELECTOR_TABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_CONTROLLER_DISPATCH + 0x2au,
            loaded_image_base, RVA_UI_DISPATCH_JUMP_TABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_CONTROLLER_DISPATCH + 0x33u,
            loaded_image_base, RVA_ACTIVE_GROUP_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_CONTROLLER_DISPATCH + 0xb2u,
            loaded_image_base, RVA_ACTIVE_GROUP_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_CONTROLLER_DISPATCH + 0x166u,
            loaded_image_base, RVA_ACTIVE_GROUP_GLOBAL,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_DISPATCH_JUMP_TABLE,
            loaded_image_base, RVA_UI_CONTROLLER_DISPATCH + 0xa1u,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_DISPATCH_JUMP_TABLE + 4u,
            loaded_image_base, RVA_UI_CONTROLLER_DISPATCH + 0x164u,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result) ||
        !check_relocated(image, RVA_UI_DISPATCH_JUMP_TABLE + 8u,
            loaded_image_base, RVA_UI_CONTROLLER_DISPATCH + 0x2eu,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH,
            &result)) return result;
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x42u, RVA_GEL_POINTER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x4fu, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x6au, RVA_FLOAT_TO_INT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x72u, 0x0012bb60u,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x93u, 0x001b9fc0u,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0xbdu, RVA_GEL_POINTER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0xcau, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x17bu, RVA_GEL_POINTER_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x184u, RVA_GEL_CLEANUP,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_CALL(RVA_UI_CONTROLLER_DISPATCH + 0x1afu, 0x000a63e0u,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_MASKED_WINDOW(RVA_UI_CONTROLLER_DISPATCH,
        ui_controller_dispatch_bytes, ui_controller_dispatch_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    CHECK_WINDOW(RVA_UI_DISPATCH_SELECTOR_TABLE,
        ui_controller_selector_id5_through_id10,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_UI_CONTROLLER_DISPATCH);

    if (!check_relocation_set(image, RVA_HUD_RESOURCE_SELECTOR,
            loaded_image_base, hud_resource_selector_relocations,
            hud_resource_selector_relocation_targets,
            ARRAY_COUNT(hud_resource_selector_relocations),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR,
            &result) ||
        !check_relocated_table(image,
            RVA_HUD_RESOURCE_SELECTOR_JUMP_TABLE, loaded_image_base,
            hud_resource_selector_jump_table_targets,
            ARRAY_COUNT(hud_resource_selector_jump_table_targets),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR,
            &result)) return result;
    CHECK_CALL(RVA_HUD_RESOURCE_SELECTOR + 0x7du,
        RVA_HUD_RESOURCE_MISSING_REPORT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR);
    CHECK_MASKED_WINDOW(RVA_HUD_RESOURCE_SELECTOR,
        hud_resource_selector_bytes, hud_resource_selector_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_RESOURCE_SELECTOR);

    CHECK_CALL(RVA_HUD_STRING_ASSIGN + 0x44u, RVA_HUD_STRING_FREE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    CHECK_CALL(RVA_HUD_STRING_ASSIGN + 0x5fu, RVA_HUD_STRING_ALLOCATE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    CHECK_CALL(RVA_HUD_STRING_ASSIGN + 0x6eu, RVA_HUD_STRING_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    CHECK_CALL(RVA_HUD_STRING_ASSIGN + 0x96u, RVA_HUD_STRING_FREE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    CHECK_CALL(RVA_HUD_STRING_ASSIGN + 0xb4u, RVA_HUD_STRING_COPY,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    CHECK_CALL(RVA_HUD_STRING_ASSIGN + 0xd8u, RVA_HUD_STRING_FREE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    CHECK_WINDOW(RVA_HUD_STRING_ASSIGN, hud_string_assign_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_STRING_ASSIGN);

    if (!check_relocated(image, RVA_HUD_CHILD_VTABLE + 0x2cu,
            loaded_image_base, RVA_HUD_CHILD_DISPATCH,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH, &result) ||
        !check_relocated(image, RVA_HUD_CHILD_DISPATCH + 0x14u,
            loaded_image_base, RVA_HUD_CHILD_SELECTOR_TABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH, &result) ||
        !check_relocated(image, RVA_HUD_CHILD_DISPATCH + 0x1bu,
            loaded_image_base, RVA_HUD_CHILD_JUMP_TABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH, &result) ||
        !check_relocated(image, RVA_HUD_CHILD_JUMP_TABLE + 0x10u,
            loaded_image_base, RVA_HUD_PORTRAIT_STATE_HANDLER,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH, &result))
        return result;
    CHECK_CALL(RVA_HUD_PORTRAIT_STATE_HANDLER + 0x23u,
        RVA_HUD_PORTRAIT_STATE_SETTER,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH);
    CHECK_MASKED_WINDOW(RVA_HUD_CHILD_DISPATCH,
        hud_child_dispatch_prefix, hud_child_dispatch_prefix_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH);
    CHECK_WINDOW(RVA_HUD_PORTRAIT_STATE_HANDLER,
        hud_portrait_state_handler_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH);
    CHECK_WINDOW(RVA_HUD_CHILD_DISPATCH + 0x169u,
        hud_child_shared_returns,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH);
    CHECK_WINDOW(RVA_HUD_CHILD_SELECTOR_TABLE + 0x11u,
        hud_child_id17_selector,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_HUD_CHILD_DISPATCH);

    CHECK_CALL(RVA_PLAYER_SET_ARMED + 0x3bu,
        RVA_SET_ARMED_RELEASE_ATTACHMENT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED);
    CHECK_CALL(RVA_PLAYER_SET_ARMED + 0x42u, RVA_SET_ARMED_RESET_STATE,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED);
    CHECK_CALL(RVA_PLAYER_SET_ARMED + 0x62u, RVA_SET_ARMED_BLEND,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED);
    CHECK_CALL(RVA_PLAYER_SET_ARMED + 0x8au, RVA_SET_ARMED_BLEND,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED);
    CHECK_WINDOW(RVA_PLAYER_SET_ARMED, player_set_armed_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_PLAYER_SET_ARMED);

    if (!check_relocated(image, RVA_STAT_DISPLAY_CONSTRUCTOR + 0x2bu,
            loaded_image_base, RVA_STAT_DISPLAY_PRIMARY_VTABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH,
            &result) ||
        !check_relocated(image, RVA_STAT_DISPLAY_CONSTRUCTOR + 0x32u,
            loaded_image_base, RVA_STAT_DISPLAY_SECONDARY_VTABLE,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH,
            &result) ||
        !check_relocated(image, RVA_STAT_DISPLAY_PRIMARY_VTABLE,
            loaded_image_base, RVA_STAT_DISPLAY_PRIMARY_DESTRUCTOR,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH,
            &result) ||
        !check_relocated(image, RVA_STAT_DISPLAY_SECONDARY_VTABLE,
            loaded_image_base, RVA_STAT_DISPLAY_SECONDARY_DESTRUCTOR,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH,
            &result)) return result;
    CHECK_CALL(RVA_STAT_DISPLAY_REFRESH + 0x50u, RVA_STAT_BAR_FILL,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH);
    CHECK_CALL(RVA_STAT_DISPLAY_REFRESH + 0x62u, RVA_STAT_BAR_FILL,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH);
    CHECK_CALL(RVA_STAT_DISPLAY_REFRESH + 0x69u,
        RVA_STAT_DISPLAY_CAMERA_SYNC,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH);
    CHECK_WINDOW(RVA_STAT_DISPLAY_REFRESH, stat_display_refresh_bytes,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH);
    CHECK_MASKED_WINDOW(RVA_STAT_DISPLAY_CONSTRUCTOR + 0x29u,
        stat_display_constructor_vtables,
        stat_display_constructor_vtable_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_REFRESH);

    if (!check_relocation_set(image, RVA_STAT_DISPLAY_CAMERA_SYNC,
            loaded_image_base, stat_display_camera_sync_relocations,
            stat_display_camera_sync_relocation_targets,
            ARRAY_COUNT(stat_display_camera_sync_relocations),
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_CAMERA_SYNC,
            &result)) return result;
    CHECK_CALL(RVA_STAT_DISPLAY_CAMERA_SYNC + 0x181u, RVA_SQUARE_ROOT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_CAMERA_SYNC);
    CHECK_MASKED_WINDOW(RVA_STAT_DISPLAY_CAMERA_SYNC,
        stat_display_camera_sync_bytes,
        stat_display_camera_sync_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_CAMERA_SYNC);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_DISPLAY_CAMERA_SYNC);

    if (!check_relocated(image, RVA_STAT_BAR_FILL + 0x22u,
            loaded_image_base, RVA_STAT_FILL_SCALE_A,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL, &result) ||
        !check_relocated(image, RVA_STAT_BAR_FILL + 0x30u,
            loaded_image_base, RVA_STAT_FILL_SCALE_B,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL, &result) ||
        !check_relocated(image, RVA_STAT_BAR_FILL + 0x51u,
            loaded_image_base, RVA_STAT_FILL_SCALE_C,
            SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL, &result))
        return result;
    CHECK_CALL(RVA_STAT_BAR_FILL + 0x3cu, RVA_FLOAT_TO_INT,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL);
    CHECK_MASKED_WINDOW(RVA_STAT_BAR_FILL, stat_bar_fill_bytes,
        stat_bar_fill_relocations,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL);
    mark_validated(&result,
        SUDEKIMP_TALOS_MEMBERSHIP_SYMBOL_STAT_BAR_FILL);

#undef CHECK_WINDOW
#undef CHECK_MASKED_WINDOW
#undef CHECK_CALL

    result.seams_valid =
        result.validated_symbol_mask == result.required_symbol_mask;
    return result;
}

#if defined(SUDEKIMP_TALOS_MEMBERSHIP_ABI_TESTING)
static void fixture_write_u16(uint8_t *bytes, uint16_t value) {
    memcpy(bytes, &value, sizeof(value));
}

static void fixture_write_u32(uint8_t *bytes, uint32_t value) {
    memcpy(bytes, &value, sizeof(value));
}

static void fixture_point_call(
    uint8_t *image,
    uint32_t call_rva,
    uint32_t target_rva
) {
    int32_t displacement = (int32_t)(
        (int64_t)target_rva - ((int64_t)call_rva + INT64_C(5)));

    image[call_rva] = 0xe8u;
    memcpy(image + call_rva + 1u, &displacement, sizeof(displacement));
}

static void fixture_apply_relocation_set(
    uint8_t *image,
    uint32_t window_rva,
    uint32_t loaded_image_base,
    const uint16_t *operand_offsets,
    const uint32_t *target_rvas,
    size_t relocation_count
) {
    size_t index;

    for (index = 0u; index < relocation_count; ++index) {
        fixture_write_u32(image + window_rva + operand_offsets[index],
            loaded_image_base + target_rvas[index]);
    }
}

static void fixture_apply_relocated_table(
    uint8_t *image,
    uint32_t table_rva,
    uint32_t loaded_image_base,
    const uint32_t *target_rvas,
    size_t entry_count
) {
    size_t index;

    for (index = 0u; index < entry_count; ++index) {
        fixture_write_u32(image + table_rva +
            (uint32_t)(index * sizeof(uint32_t)),
            loaded_image_base + target_rvas[index]);
    }
}

int SudekiMpTalosCompanionMembershipAbiPopulateFixtureForTesting(
    uint8_t *image,
    size_t image_size,
    uint32_t loaded_image_base
) {
    const uint32_t pe_rva = 0x128u;

    if (image == NULL ||
        image_size != SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE ||
        loaded_image_base == 0u ||
        (loaded_image_base & 0xffffu) != 0u ||
        loaded_image_base > UINT32_MAX - RVA_MAX_RELOCATED_TARGET) return 0;

    memset(image, 0, image_size);
    image[0] = 0x4du;
    image[1] = 0x5au;
    fixture_write_u32(image + 0x3cu, pe_rva);
    fixture_write_u32(image + pe_rva, 0x00004550u);
    fixture_write_u32(image + pe_rva + 4u, 0x0005014cu);
    fixture_write_u16(image + pe_rva + 24u, 0x010bu);
    fixture_write_u32(image + pe_rva + 52u, loaded_image_base);
    fixture_write_u32(image + pe_rva + 80u,
        SUDEKIMP_TALOS_MEMBERSHIP_IMAGE_SIZE);

    image[RVA_GET_GROUP_PLAYERS] = 0xa1u;
    fixture_write_u32(image + RVA_GET_GROUP_PLAYERS + 1u,
        loaded_image_base + 0x00408d94u);
    image[RVA_GET_GROUP_PLAYERS + 5u] = 0xc3u;
    memcpy(image + RVA_GET_PC, get_pc_bytes, sizeof(get_pc_bytes));
    memcpy(image + RVA_ADD_PLAYER, add_player_bytes,
        sizeof(add_player_bytes));
    memcpy(image + RVA_REMOVE_PLAYER, remove_player_bytes,
        sizeof(remove_player_bytes));
    memcpy(image + RVA_GROUP_ADD_CORE, group_add_core_front,
        sizeof(group_add_core_front));
    memcpy(image + RVA_GROUP_ADD_CORE + 0x82u, group_add_core_listener,
        sizeof(group_add_core_listener));
    memcpy(image + RVA_GROUP_ADD_CORE + 0x102u, group_add_core_return,
        sizeof(group_add_core_return));
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0x4au,
        RVA_GEL_POINTER_ASSIGN);
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0x64u,
        RVA_GEL_POINTER_COPY);
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0x74u,
        RVA_GROUP_PRIMARY_CHANGED);
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0x7du,
        RVA_GEL_CLEANUP);
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0xb5u,
        RVA_GEL_OBSERVER_COPY);
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0xdcu,
        RVA_PLAYER_SET_ARMED);
    fixture_point_call(image, RVA_GROUP_ADD_CORE + 0xfdu,
        RVA_STAT_DISPLAY_REFRESH);
    memcpy(image + RVA_GROUP_REMOVE_CORE, group_remove_core_front,
        sizeof(group_remove_core_front));
    memcpy(image + RVA_GROUP_REMOVE_CORE + 0x74u,
        group_remove_core_compaction, sizeof(group_remove_core_compaction));
    memcpy(image + RVA_GROUP_REMOVE_CORE + 0x1a6u,
        group_remove_core_listener, sizeof(group_remove_core_listener));
    memcpy(image + RVA_GROUP_REMOVE_CORE + 0x1e4u,
        group_remove_core_return, sizeof(group_remove_core_return));
    fixture_point_call(image, RVA_GROUP_REMOVE_CORE + 0x53u,
        RVA_GROUP_PREPARE_PRIMARY_REMOVAL);
    fixture_point_call(image, RVA_GROUP_REMOVE_CORE + 0x1d2u,
        RVA_GEL_OBSERVER_COPY);
    fixture_write_u32(image + RVA_GROUP_REMOVE_CORE + 0x1e6u,
        loaded_image_base + RVA_UI_CONTROLLER_GLOBAL);
    fixture_point_call(image, RVA_GROUP_REMOVE_CORE + 0x1eau,
        RVA_GROUP_REMOVE_EPILOGUE);
    memcpy(image + RVA_IS_PLAYER, is_player_bytes,
        sizeof(is_player_bytes));
    memcpy(image + RVA_GET_INDEX, get_index_bytes,
        sizeof(get_index_bytes));
    memcpy(image + RVA_ALL_PENDING_LOADED, all_pending_bytes,
        sizeof(all_pending_bytes));
    memcpy(image + RVA_IN_COMBAT, in_combat_bytes,
        sizeof(in_combat_bytes));

    memcpy(image + RVA_ASYNC_ACTIVE, async_prefix, sizeof(async_prefix));
    fixture_write_u32(image + RVA_ASYNC_ACTIVE + 2u,
        loaded_image_base + RVA_ASYNC_PENDING_GLOBAL);
    memcpy(image + RVA_ASYNC_ACTIVE + 6u, async_middle,
        sizeof(async_middle));
    fixture_write_u32(image + RVA_ASYNC_ACTIVE + 0x0bu,
        loaded_image_base + RVA_ASYNC_STREAM_GLOBAL);
    memcpy(image + RVA_ASYNC_ACTIVE + 0x0fu, async_suffix,
        sizeof(async_suffix));

    memcpy(image + RVA_GEL_RESOLVER, resolver_prefix,
        sizeof(resolver_prefix));
    fixture_write_u32(image + RVA_GEL_RESOLVER + 3u,
        loaded_image_base + RVA_GEL_RESOLVER_SEH);
    memcpy(image + RVA_GEL_RESOLVER + 7u, resolver_suffix,
        sizeof(resolver_suffix));
    memcpy(image + RVA_GEL_CLEANUP, gel_cleanup_bytes,
        sizeof(gel_cleanup_bytes));

    fixture_write_u32(image + RVA_GEL_VTABLE,
        loaded_image_base + RVA_GEL_DELETING_DESTRUCTOR);
    fixture_write_u32(image + RVA_GEL_VTABLE + 0x10u,
        loaded_image_base + RVA_GEL_GET_RAW_ENTITY);
    fixture_write_u32(image + RVA_GEL_VTABLE + 0x2cu,
        loaded_image_base + RVA_GEL_GET_TYPE_NAME);
    memcpy(image + RVA_GEL_DELETING_DESTRUCTOR, gel_destructor_bytes,
        sizeof(gel_destructor_bytes));
    memcpy(image + RVA_GET_PC_WRAPPER_CONSTRUCTOR, gel_wrapper_helper_bytes,
        sizeof(gel_wrapper_helper_bytes));
    memcpy(image + RVA_GEL_WRAPPER_FACTORY, gel_wrapper_factory_bytes,
        sizeof(gel_wrapper_factory_bytes));
    memcpy(image + RVA_PTR_REGISTRATION_CONSTRUCTOR,
        ptr_registration_constructor_bytes,
        sizeof(ptr_registration_constructor_bytes));
    memcpy(image + RVA_GEL_CORE_DESTRUCTOR, gel_core_destructor_bytes,
        sizeof(gel_core_destructor_bytes));
    memcpy(image + RVA_PTR_REGISTRY_DELETE_ALL,
        ptr_registry_delete_all_bytes, sizeof(ptr_registry_delete_all_bytes));
    memcpy(image + RVA_PTR_REGISTRY_FIND, ptr_registry_find_bytes,
        sizeof(ptr_registry_find_bytes));
    memcpy(image + RVA_PTR_REGISTRY_ERASE, ptr_registry_erase_entry,
        sizeof(ptr_registry_erase_entry));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x0fu,
        ptr_registry_erase_after_text, sizeof(ptr_registry_erase_after_text));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x55u,
        opcode_mov_eax_absolute, sizeof(opcode_mov_eax_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x6fu,
        opcode_mov_edx_absolute, sizeof(opcode_mov_edx_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x9cu,
        opcode_mov_ecx_absolute, sizeof(opcode_mov_ecx_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0xbdu,
        opcode_mov_edx_absolute, sizeof(opcode_mov_edx_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0xf3u,
        opcode_mov_ecx_absolute, sizeof(opcode_mov_ecx_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x131u,
        opcode_mov_eax_absolute, sizeof(opcode_mov_eax_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x1deu,
        opcode_mov_ecx_absolute, sizeof(opcode_mov_ecx_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x22du,
        opcode_compare_absolute, sizeof(opcode_compare_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x236u,
        opcode_subtract_absolute, sizeof(opcode_subtract_absolute));
    memcpy(image + RVA_PTR_REGISTRY_ERASE + 0x220u,
        ptr_registry_erase_tail, sizeof(ptr_registry_erase_tail));

    fixture_write_u32(image + RVA_GET_PC_WRAPPER_CONSTRUCTOR + 3u,
        loaded_image_base + RVA_GEL_RESOLVER_SEH);
    fixture_write_u32(image + RVA_GEL_WRAPPER_FACTORY + 3u,
        loaded_image_base + RVA_GEL_FACTORY_SEH);
    fixture_write_u32(image + RVA_GEL_WRAPPER_FACTORY + 0x47u,
        loaded_image_base + RVA_GEL_VTABLE);
    fixture_write_u32(image + RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x0au,
        loaded_image_base + RVA_PTR_OBJECT_VTABLE);
    fixture_write_u32(image + RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x25u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x2du,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x36u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x68u,
        loaded_image_base + RVA_PTR_ALLOCATION_FAILURE_VTABLE);
    fixture_write_u32(image + RVA_GEL_CORE_DESTRUCTOR + 3u,
        loaded_image_base + RVA_GEL_DESTRUCTOR_SEH);
    fixture_write_u32(image + RVA_GEL_CORE_DESTRUCTOR + 0x73u,
        loaded_image_base + RVA_PTR_OBJECT_VTABLE);
    fixture_write_u32(image + RVA_GEL_CORE_DESTRUCTOR + 0x85u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_DELETE_ALL + 2u,
        loaded_image_base + RVA_PTR_REGISTRY_COUNT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_DELETE_ALL + 0x11u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_DELETE_ALL + 0x28u,
        loaded_image_base + RVA_PTR_REGISTRY_COUNT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_FIND + 9u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x0bu,
        loaded_image_base + RVA_PTR_REGISTRY_ERROR_TEXT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x56u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x71u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x9eu,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0xbfu,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0xf5u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x132u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x1e0u,
        loaded_image_base + RVA_PTR_REGISTRY_ROOT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x22fu,
        loaded_image_base + RVA_PTR_REGISTRY_COUNT);
    fixture_write_u32(image + RVA_PTR_REGISTRY_ERASE + 0x238u,
        loaded_image_base + RVA_PTR_REGISTRY_COUNT);

    fixture_point_call(image, RVA_GET_PC_WRAPPER_CONSTRUCTOR + 0x7bu,
        RVA_GEL_WRAPPER_FACTORY);
    fixture_point_call(image, RVA_GEL_WRAPPER_FACTORY + 0x20u,
        RVA_NATIVE_HEAP_ALLOCATE);
    fixture_point_call(image, RVA_GEL_WRAPPER_FACTORY + 0x37u,
        RVA_PTR_REGISTRATION_CONSTRUCTOR);
    fixture_point_call(image, RVA_GEL_WRAPPER_FACTORY + 0x58u,
        RVA_GEL_OBSERVER_LINK);
    fixture_point_call(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x15u,
        RVA_NATIVE_HEAP_ALLOCATE);
    fixture_point_call(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x50u,
        RVA_PTR_REGISTRY_INSERT);
    fixture_point_call(image, RVA_PTR_REGISTRATION_CONSTRUCTOR + 0x6cu,
        RVA_PTR_ALLOCATION_FAILURE);
    fixture_point_call(image, RVA_GEL_DELETING_DESTRUCTOR + 4u,
        RVA_GEL_CORE_DESTRUCTOR);
    fixture_point_call(image, RVA_GEL_DELETING_DESTRUCTOR + 0x11u,
        RVA_NATIVE_HEAP_FREE);
    fixture_point_call(image, RVA_GEL_CORE_DESTRUCTOR + 0x7bu,
        RVA_PTR_REGISTRY_FIND);
    fixture_point_call(image, RVA_GEL_CORE_DESTRUCTOR + 0x91u,
        RVA_PTR_REGISTRY_ERASE);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x0fu,
        RVA_PTR_TREE_ERASE_ERROR);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x1eu,
        RVA_PTR_TREE_ERASE_PREPARE);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0xb8u,
        RVA_PTR_TREE_MAXIMUM);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x161u,
        RVA_PTR_TREE_ROTATE_LEFT);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x18fu,
        RVA_PTR_TREE_ROTATE_RIGHT);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x1a8u,
        RVA_PTR_TREE_ROTATE_LEFT);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x1beu,
        RVA_PTR_TREE_ROTATE_RIGHT);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x205u,
        RVA_PTR_TREE_ROTATE_LEFT);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x21cu,
        RVA_PTR_TREE_ROTATE_RIGHT);
    fixture_point_call(image, RVA_PTR_REGISTRY_ERASE + 0x225u,
        RVA_NATIVE_HEAP_FREE);
    memcpy(image + RVA_GEL_GET_RAW_ENTITY, gel_raw_entity_bytes,
        sizeof(gel_raw_entity_bytes));
    image[RVA_GEL_GET_TYPE_NAME] = 0xb8u;
    fixture_write_u32(image + RVA_GEL_GET_TYPE_NAME + 1u,
        loaded_image_base + RVA_GEL_TYPE_NAME_TEXT);
    image[RVA_GEL_GET_TYPE_NAME + 5u] = 0xc3u;

    fixture_write_u32(image + RVA_AI_LISTENER_VTABLE + 0x18u,
        loaded_image_base + RVA_AI_LISTENER_ADD);
    fixture_write_u32(image + RVA_AI_LISTENER_VTABLE + 0x1cu,
        loaded_image_base + RVA_AI_LISTENER_REMOVE);
    memcpy(image + RVA_AI_LISTENER_ADD, ai_listener_add_bytes,
        sizeof(ai_listener_add_bytes));
    memcpy(image + RVA_AI_LISTENER_REMOVE, ai_listener_remove_bytes,
        sizeof(ai_listener_remove_bytes));

    memcpy(image + RVA_FORMATION_ADD, formation_add_bytes,
        sizeof(formation_add_bytes));
    memcpy(image + RVA_FORMATION_REMOVE, formation_remove_prefix,
        sizeof(formation_remove_prefix));
    memcpy(image + RVA_FORMATION_REMOVE + 0x2cu,
        formation_remove_early_ret, sizeof(formation_remove_early_ret));
    fixture_point_call(image, RVA_FORMATION_REMOVE + 0x5cu,
        RVA_GEL_POINTER_COPY);
    fixture_point_call(image, RVA_FORMATION_REMOVE + 0x115u,
        RVA_GEL_CLEANUP);
    memcpy(image + RVA_FORMATION_REMOVE + 0x1b9u,
        formation_remove_backpointer_clear,
        sizeof(formation_remove_backpointer_clear));
    memcpy(image + RVA_FORMATION_REMOVE + 0x1c6u,
        formation_remove_final_tail, sizeof(formation_remove_final_tail));

    memcpy(image + RVA_FORMATION_CANONICALIZER, canonicalizer_prefix,
        sizeof(canonicalizer_prefix));
    fixture_write_u32(image + RVA_FORMATION_CANONICALIZER + 8u,
        loaded_image_base + RVA_FORMATION_DEFAULT_DISTANCE);
    memcpy(image + RVA_FORMATION_CANONICALIZER + 12u,
        canonicalizer_suffix, sizeof(canonicalizer_suffix));

    memcpy(image + RVA_GROUP_REMOVE_EPILOGUE, remove_epilogue_bytes,
        sizeof(remove_epilogue_bytes));
    fixture_write_u32(image + RVA_GROUP_REMOVE_EPILOGUE + 1u,
        loaded_image_base + RVA_ACTIVE_GROUP_GLOBAL);
    fixture_point_call(image, RVA_GROUP_REMOVE_EPILOGUE + 0x23u,
        RVA_GEL_POINTER_COPY);
    fixture_point_call(image, RVA_GROUP_REMOVE_EPILOGUE + 0x30u,
        RVA_GEL_CLEANUP);
    fixture_point_call(image, RVA_GROUP_REMOVE_EPILOGUE + 0x173u,
        RVA_STAT_DISPLAY_REFRESH);

    memcpy(image + RVA_UI_CONTROLLER_DISPATCH,
        ui_controller_dispatch_bytes,
        sizeof(ui_controller_dispatch_bytes));
    fixture_write_u32(image + RVA_UI_CONTROLLER_VTABLE + 0x20u,
        loaded_image_base + RVA_UI_CONTROLLER_DISPATCH);
    fixture_write_u32(image + RVA_UI_CONTROLLER_DISPATCH + 0x04u,
        loaded_image_base + RVA_UI_SCENE_GLOBAL);
    fixture_write_u32(image + RVA_UI_CONTROLLER_DISPATCH + 0x21u,
        loaded_image_base + RVA_UI_DISPATCH_SELECTOR_TABLE);
    fixture_write_u32(image + RVA_UI_CONTROLLER_DISPATCH + 0x2au,
        loaded_image_base + RVA_UI_DISPATCH_JUMP_TABLE);
    fixture_write_u32(image + RVA_UI_CONTROLLER_DISPATCH + 0x33u,
        loaded_image_base + RVA_ACTIVE_GROUP_GLOBAL);
    fixture_write_u32(image + RVA_UI_CONTROLLER_DISPATCH + 0xb2u,
        loaded_image_base + RVA_ACTIVE_GROUP_GLOBAL);
    fixture_write_u32(image + RVA_UI_CONTROLLER_DISPATCH + 0x166u,
        loaded_image_base + RVA_ACTIVE_GROUP_GLOBAL);
    fixture_write_u32(image + RVA_UI_DISPATCH_JUMP_TABLE,
        loaded_image_base + RVA_UI_CONTROLLER_DISPATCH + 0xa1u);
    fixture_write_u32(image + RVA_UI_DISPATCH_JUMP_TABLE + 4u,
        loaded_image_base + RVA_UI_CONTROLLER_DISPATCH + 0x164u);
    fixture_write_u32(image + RVA_UI_DISPATCH_JUMP_TABLE + 8u,
        loaded_image_base + RVA_UI_CONTROLLER_DISPATCH + 0x2eu);
    memcpy(image + RVA_UI_DISPATCH_SELECTOR_TABLE,
        ui_controller_selector_id5_through_id10,
        sizeof(ui_controller_selector_id5_through_id10));
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x42u,
        RVA_GEL_POINTER_COPY);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x4fu,
        RVA_GEL_CLEANUP);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x6au,
        RVA_FLOAT_TO_INT);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x72u,
        0x0012bb60u);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x93u,
        0x001b9fc0u);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0xbdu,
        RVA_GEL_POINTER_COPY);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0xcau,
        RVA_GEL_CLEANUP);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x17bu,
        RVA_GEL_POINTER_COPY);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x184u,
        RVA_GEL_CLEANUP);
    fixture_point_call(image, RVA_UI_CONTROLLER_DISPATCH + 0x1afu,
        0x000a63e0u);

    memcpy(image + RVA_HUD_RESOURCE_SELECTOR, hud_resource_selector_bytes,
        sizeof(hud_resource_selector_bytes));
    fixture_apply_relocation_set(image, RVA_HUD_RESOURCE_SELECTOR,
        loaded_image_base, hud_resource_selector_relocations,
        hud_resource_selector_relocation_targets,
        ARRAY_COUNT(hud_resource_selector_relocations));
    fixture_apply_relocated_table(image,
        RVA_HUD_RESOURCE_SELECTOR_JUMP_TABLE, loaded_image_base,
        hud_resource_selector_jump_table_targets,
        ARRAY_COUNT(hud_resource_selector_jump_table_targets));
    fixture_point_call(image, RVA_HUD_RESOURCE_SELECTOR + 0x7du,
        RVA_HUD_RESOURCE_MISSING_REPORT);

    memcpy(image + RVA_HUD_STRING_ASSIGN, hud_string_assign_bytes,
        sizeof(hud_string_assign_bytes));
    fixture_point_call(image, RVA_HUD_STRING_ASSIGN + 0x44u,
        RVA_HUD_STRING_FREE);
    fixture_point_call(image, RVA_HUD_STRING_ASSIGN + 0x5fu,
        RVA_HUD_STRING_ALLOCATE);
    fixture_point_call(image, RVA_HUD_STRING_ASSIGN + 0x6eu,
        RVA_HUD_STRING_COPY);
    fixture_point_call(image, RVA_HUD_STRING_ASSIGN + 0x96u,
        RVA_HUD_STRING_FREE);
    fixture_point_call(image, RVA_HUD_STRING_ASSIGN + 0xb4u,
        RVA_HUD_STRING_COPY);
    fixture_point_call(image, RVA_HUD_STRING_ASSIGN + 0xd8u,
        RVA_HUD_STRING_FREE);

    memcpy(image + RVA_HUD_CHILD_DISPATCH, hud_child_dispatch_prefix,
        sizeof(hud_child_dispatch_prefix));
    fixture_write_u32(image + RVA_HUD_CHILD_VTABLE + 0x2cu,
        loaded_image_base + RVA_HUD_CHILD_DISPATCH);
    fixture_write_u32(image + RVA_HUD_CHILD_DISPATCH + 0x14u,
        loaded_image_base + RVA_HUD_CHILD_SELECTOR_TABLE);
    fixture_write_u32(image + RVA_HUD_CHILD_DISPATCH + 0x1bu,
        loaded_image_base + RVA_HUD_CHILD_JUMP_TABLE);
    fixture_write_u32(image + RVA_HUD_CHILD_JUMP_TABLE + 0x10u,
        loaded_image_base + RVA_HUD_PORTRAIT_STATE_HANDLER);
    memcpy(image + RVA_HUD_PORTRAIT_STATE_HANDLER,
        hud_portrait_state_handler_bytes,
        sizeof(hud_portrait_state_handler_bytes));
    fixture_point_call(image, RVA_HUD_PORTRAIT_STATE_HANDLER + 0x23u,
        RVA_HUD_PORTRAIT_STATE_SETTER);
    memcpy(image + RVA_HUD_CHILD_DISPATCH + 0x169u,
        hud_child_shared_returns, sizeof(hud_child_shared_returns));
    memcpy(image + RVA_HUD_CHILD_SELECTOR_TABLE + 0x11u,
        hud_child_id17_selector, sizeof(hud_child_id17_selector));

    memcpy(image + RVA_PLAYER_SET_ARMED, player_set_armed_bytes,
        sizeof(player_set_armed_bytes));
    fixture_point_call(image, RVA_PLAYER_SET_ARMED + 0x3bu,
        RVA_SET_ARMED_RELEASE_ATTACHMENT);
    fixture_point_call(image, RVA_PLAYER_SET_ARMED + 0x42u,
        RVA_SET_ARMED_RESET_STATE);
    fixture_point_call(image, RVA_PLAYER_SET_ARMED + 0x62u,
        RVA_SET_ARMED_BLEND);
    fixture_point_call(image, RVA_PLAYER_SET_ARMED + 0x8au,
        RVA_SET_ARMED_BLEND);

    memcpy(image + RVA_STAT_DISPLAY_REFRESH, stat_display_refresh_bytes,
        sizeof(stat_display_refresh_bytes));
    fixture_point_call(image, RVA_STAT_DISPLAY_REFRESH + 0x50u,
        RVA_STAT_BAR_FILL);
    fixture_point_call(image, RVA_STAT_DISPLAY_REFRESH + 0x62u,
        RVA_STAT_BAR_FILL);
    fixture_point_call(image, RVA_STAT_DISPLAY_REFRESH + 0x69u,
        RVA_STAT_DISPLAY_CAMERA_SYNC);
    memcpy(image + RVA_STAT_DISPLAY_CAMERA_SYNC,
        stat_display_camera_sync_bytes,
        sizeof(stat_display_camera_sync_bytes));
    fixture_apply_relocation_set(image, RVA_STAT_DISPLAY_CAMERA_SYNC,
        loaded_image_base, stat_display_camera_sync_relocations,
        stat_display_camera_sync_relocation_targets,
        ARRAY_COUNT(stat_display_camera_sync_relocations));
    fixture_point_call(image, RVA_STAT_DISPLAY_CAMERA_SYNC + 0x181u,
        RVA_SQUARE_ROOT);
    memcpy(image + RVA_STAT_DISPLAY_CONSTRUCTOR + 0x29u,
        stat_display_constructor_vtables,
        sizeof(stat_display_constructor_vtables));
    fixture_write_u32(image + RVA_STAT_DISPLAY_CONSTRUCTOR + 0x2bu,
        loaded_image_base + RVA_STAT_DISPLAY_PRIMARY_VTABLE);
    fixture_write_u32(image + RVA_STAT_DISPLAY_CONSTRUCTOR + 0x32u,
        loaded_image_base + RVA_STAT_DISPLAY_SECONDARY_VTABLE);
    fixture_write_u32(image + RVA_STAT_DISPLAY_PRIMARY_VTABLE,
        loaded_image_base + RVA_STAT_DISPLAY_PRIMARY_DESTRUCTOR);
    fixture_write_u32(image + RVA_STAT_DISPLAY_SECONDARY_VTABLE,
        loaded_image_base + RVA_STAT_DISPLAY_SECONDARY_DESTRUCTOR);

    memcpy(image + RVA_STAT_BAR_FILL, stat_bar_fill_bytes,
        sizeof(stat_bar_fill_bytes));
    fixture_write_u32(image + RVA_STAT_BAR_FILL + 0x22u,
        loaded_image_base + RVA_STAT_FILL_SCALE_A);
    fixture_write_u32(image + RVA_STAT_BAR_FILL + 0x30u,
        loaded_image_base + RVA_STAT_FILL_SCALE_B);
    fixture_write_u32(image + RVA_STAT_BAR_FILL + 0x51u,
        loaded_image_base + RVA_STAT_FILL_SCALE_C);
    fixture_point_call(image, RVA_STAT_BAR_FILL + 0x3cu,
        RVA_FLOAT_TO_INT);
    return 1;
}
#endif
