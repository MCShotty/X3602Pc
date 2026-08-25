#include <file.h>
#include <image.h>
#include <symbol.h>

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: ac6_xex_symbols <input.xex> <output.tsv>\n";
        return EXIT_FAILURE;
    }

    const auto file = LoadFile(argv[1]);
    if (file.empty())
    {
        std::cerr << "Unable to read XEX input.\n";
        return EXIT_FAILURE;
    }

    auto image = Image::ParseImage(file.data(), file.size());
    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    if (!output)
    {
        std::cerr << "Unable to open output file.\n";
        return EXIT_FAILURE;
    }

    output << "kind\taddress\tsize\tname\n";
    output << "image\t0x" << std::uppercase << std::hex << image.base
           << "\t0x" << image.size << "\tbase\n";
    output << "entry\t0x" << image.entry_point << "\t0x0\tentry_point\n";

    std::size_t functionCount = 0;
    for (const auto& symbol : image.symbols)
    {
        if (symbol.type != Symbol_Function)
        {
            continue;
        }

        output << "import\t0x"
               << std::setfill('0') << std::setw(8) << symbol.address
               << "\t0x" << symbol.size
               << "\t" << symbol.name << "\n";
        ++functionCount;
    }

    std::cout << "image_base=0x" << std::uppercase << std::hex << image.base
              << " image_size=0x" << image.size
              << " entry_point=0x" << image.entry_point
              << " function_imports=" << std::dec << functionCount << "\n";
    return EXIT_SUCCESS;
}
