// Single translation unit for header-only third-party implementations.

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
