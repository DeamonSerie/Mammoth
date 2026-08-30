// Single translation unit instantiating the vendored stb_image_write
// implementation (used for PNG / JPEG export). The header lives in
// thirdparty/stb/ and is found via the compiler include path.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
