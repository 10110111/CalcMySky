#ifndef INCLUDE_ONCE_5041B5F1_BF78_4C88_B28F_A06F80CB073A
#define INCLUDE_ONCE_5041B5F1_BF78_4C88_B28F_A06F80CB073A

#include <memory>

class QOpenGLContext;
class QOffscreenSurface;
std::pair<std::unique_ptr<QOffscreenSurface>, std::unique_ptr<QOpenGLContext>> initOpenGL();

#endif
