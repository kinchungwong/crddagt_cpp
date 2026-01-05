#include <iostream>
#include <stdexcept>

int main(int argc, char** argv)
{
    try
    {
        std::cout << "\n\n====== crddagt ======\n" << std::flush;

        // ====== placeholder - leave empty ======

        std::cout << "\n\n====== normal exit ======\n" << std::flush;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\n\nError:\n" << e.what() << "\n" << std::flush;
        std::cout << "\n\n====== abnormal exit ======\n" << std::flush;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
