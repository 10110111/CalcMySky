# ShowMySky API {#showmysky-api}

ShowMySky uses Qt for its rendering and data processing, so the API reflects this: it relies on `QOpenGLFunctions_3_3_Core`, `QOpenGLShaderProgram`, `QString` etc.

To use ShowMySky from your C++ application, you need to do the following.

## Initializing atmosphere renderer

1. **Include the header.** In the C++ source include [<code>\<ShowMySky/AtmosphereRenderer.hpp\></code>](api_2AtmosphereRenderer_8hpp.html) to get access to the relevant classes.
2. **Load ShowMySky library.** This can be done either by linking to it directly as a usual shared object, or by `dlopen`/`LoadLibrary[Ex]`/`QLibrary::load` etc. The name of the library to load is in the #SHOWMYSKY_LIB_NAME macro.
3. **Check ABI verison.** To do this, find a symbol named `ShowMySky_ABI_version` of type `uint32_t` in the loaded library (via `dlsym`/`GetProcAddress`/`QLibrary::resolve` etc.), compare its value with #ShowMySky_ABI_version macro defined in the header included in step 1. If it doesn't match, the library is incompatible with the header, use of such a library has undefined behavior.
4. **Locate API entry point.** If ABI version is validated successfully in step 3, then find a symbol named #ShowMySky_AtmosphereRenderer_create with the function-pointer type `decltype(ShowMySky_AtmosphereRenderer_create)`.
5. **Create the settings object.** To provide the renderer with the necessary settings, ShowMySky uses a helper interface, #ShowMySky::Settings. This interface must be implemented by some object in the application. A pointer to this object will later be supplied to the constructor of the renderer.
6. **Create a function to render target OpenGL surface.** See [Surface rendering and view direction shader](#surface-and-view-dir) section for details.
7. **Construct `AtmosphereRenderer`.** This is done by calling #ShowMySky_AtmosphereRenderer_create, the function resolved in step 4, passing path to the atmosphere model directory, as well as a pointer to `QOpenGLFunctions_3_3_Core` for the OpenGL context in which the renderer will work.

## <a name="surface-and-view-dir"> Surface rendering and view direction shader </a>

In OpenGL there are many ways to draw a surface to be displayed on the screen or saved into a texture. One of the simplest ways is to draw a single `GL_QUADS` item, having set up current vertex array and supplied the necessary uniforms to the current shader program. Here's an example of such a surface rendering function:

```cpp
const std::function drawSurface=[](QOpenGLShaderProgram& program)
{
    program.setUniformValue("zoomFactor", zoomFactor());
    program.setUniformValue("cameraRotation", calcCamRotation());
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
};
```

Aside from the many ways to draw a surface, there's a lot of different projections to choose from, as well as different connections of the their parameters to the surface drawing process. Thus, we need some way to find out what direction in the physical 3D space a given pixel on the surface corresponds to. ShowMySky API makes it possible for the app to provide its own shaders (vertex and fragment) that implement the function that takes no arguments and yields the direction in 3D space.

The coordinate system is such that north is along the \f$x\f$ axis, west is along the \f$y\f$ axis, and zenith is along the \f$z\f$ axis.

Here's an example of the vertex and fragment shaders that implement this function to work in tandem with the `drawSurface` example above:

```cpp
// Vertex shader
#version 330
in vec3 vertex;
out vec3 position;
void main()
{
    position=vertex;
    gl_Position=vec4(position,1);
}
```

```cpp
// Fragment shader
#version 330
in vec3 position;
uniform float zoomFactor;
uniform mat3 cameraRotation;
const float PI=3.1415926535897932;
vec3 calcViewDir()
{
    vec2 pos=position.xy/zoomFactor;
    // pos.x and pos.y are supposed to vary from -1 to 1.
    // The following computation implements equirectangular
    // projection with the center defined by cameraRotation.
    return cameraRotation*vec3(cos(pos.x*PI)*cos(pos.y*(PI/2)),
                               sin(pos.x*PI)*cos(pos.y*(PI/2)),
                               sin(pos.y*(PI/2)));
}
```

## Loading atmosphere model

Data loading is a bit involved. Because loading can potentially take dozens of seconds (for heavy models), it's done in steps, making it possible for application to indicate progress in the UI instead of freezing for the duration of loading. The procedure is as follows:

1. Prepare source code for the shaders that implement `calcViewDir` function (see [Surface rendering and view direction shader](#surface-and-view-dir) section for details).
2. Initialize the loading process by a call to ShowMySky::AtmosphereRenderer::initDataLoading. If initialization fails (e.g. data path doesn't exist), this function will throw ShowMySky::Error. The return value of this function is the total number of loading steps to do.
3. Repeatedly call ShowMySky::AtmosphereRenderer::stepDataLoading, checking its return value. If this function fails (e.g. a data file is missing), it will throw ShowMySky::Error. The return value tells current progress that can be used in the UI. When number of steps done becomes equal to number of steps to do, loading is finished.

## Rendering

Readiness of the renderer to rendering can be queried by a call to ShowMySky::AtmosphereRenderer::isReadyToRender. Once `true` is returned, basic rendering can be done by calling ShowMySky::AtmosphereRenderer::draw. If textures need to be reloaded (see below), reloading is done synchronously, which may throw ShowMySky::Error if it fails.

In some cases the `draw` call may take much longer than it normally would. This is because, to avoid loading the whole textures into VRAM, only a slice of texture data corresponding to current camera altitude (determined by ShowMySky::Settings::altitude) is loaded. When camera altitude changes, there may be a need to reload another slice. If the application simply calls ShowMySky::AtmosphereRenderer::draw, the reloading may take some time, making the application appear to freeze.

To avoid freezes, the application can drive the reloading step by step. To implement this, the following steps are needed.

1. Initialize preparation to draw by calling ShowMySky::AtmosphereRenderer::initPreparationToDraw. If the return value is zero, there's no need to reload anything, so drawing can be done as usual. Otherwise, the return value tells the total number of steps to be taken for reloading.
2. If there's a nonzero number of steps to take, repeatedly call ShowMySky::AtmosphereRenderer::stepPreparationToDraw. If this function fails, it throws ShowMySky::Error. Return value of this function indicates progress of reloading: number of steps done and total number of steps to do. This can be used in the UI.
3. Now call ShowMySky::AtmosphereRenderer::draw to actually render the scene.
