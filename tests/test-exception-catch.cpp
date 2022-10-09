#include <memory>
#include <iostream>
#include <QLibrary>
#include "../ShowMySky/api/ShowMySky/AtmosphereRenderer.hpp"

int main()
{
    decltype(::ShowMySky_AtmosphereRenderer_create)* ShowMySky_AtmosphereRenderer_create=nullptr;
    try
    {
        if(!ShowMySky_AtmosphereRenderer_create)
        {
            QLibrary showMySky(LIBRARY_FILE_PATH, ShowMySky_ABI_version);
            if(!showMySky.load())
                throw std::runtime_error("Failed to load ShowMySky library");
            const auto abi=reinterpret_cast<const quint32*>(showMySky.resolve("ShowMySky_ABI_version"));
            if(!abi)
                throw std::runtime_error("Failed to determine ABI version of ShowMySky library.");
            if(*abi != ShowMySky_ABI_version)
                throw std::runtime_error(QString("ABI version of ShowMySky library is %1, but this program has been compiled against version %2.")
                                    .arg(*abi).arg(ShowMySky_ABI_version).toStdString());
            ShowMySky_AtmosphereRenderer_create=reinterpret_cast<decltype(ShowMySky_AtmosphereRenderer_create)>(
                                                showMySky.resolve("ShowMySky_AtmosphereRenderer_create"));
            if(!ShowMySky_AtmosphereRenderer_create)
                throw std::runtime_error("Failed to resolve the function to create AtmosphereRenderer");
        }

        const std::function drawSurface=[](QOpenGLShaderProgram&){};
        QString pathToData; // empty => invalid, should result in an exception
        std::unique_ptr<ShowMySky::AtmosphereRenderer>
            renderer(ShowMySky_AtmosphereRenderer_create(nullptr,&pathToData,nullptr,&drawSurface));
    }
    catch(ShowMySky::Error const& ex)
    {
        std::cerr << "Caught expected exception: " << ex.errorType().toStdString() << ": " << ex.what().toStdString() << "\n";
        return 0;
    }
    catch(std::runtime_error const& ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    std::cerr << "Test is broken: no exception was thrown\n";
    return 1;
}
