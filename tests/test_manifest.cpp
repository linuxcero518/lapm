#include <catch2/catch_test_macros.hpp>

#include "lapm/manifest.hpp"

using namespace lapm;

TEST_CASE("Manifest parses legacy .info", "[manifest]") {
    const std::string text =
        "Nombre: nano\n"
        "versión: 8.7\n"
        "Revisión: 1\n"
        "Estado: No instalado\n"
        "dependencias: libc ncurses\n"
        "Mantenedor: linuxcero518@gmail.com\n"
        "Homepage: https://nano-editor.org\n"
        "Tamaño: 1.5 MB\n"
        "Espacio en disco: 3.2 MB\n"
        "Descripción: A small text editor\n";

    auto m = Manifest::parse(text);
    REQUIRE(m.has_value());
    REQUIRE(m->name == "nano");
    REQUIRE(m->version == "8.7");
    REQUIRE(m->revision == "1");
    REQUIRE(m->status == "No instalado");
    REQUIRE(m->dependencies.size() == 2);
    REQUIRE(m->dependencies[0] == "libc");
    REQUIRE(m->dependencies[1] == "ncurses");
    REQUIRE(m->download_size == "1.5 MB");
    REQUIRE(m->disk_size == "3.2 MB");
}

TEST_CASE("Manifest roundtrips", "[manifest]") {
    Manifest m;
    m.name = "nano";
    m.version = "8.7";
    m.revision = "1";
    m.status = "Instalado";
    m.dependencies = {"libc"};
    m.maintainer = "x@y";
    m.download_size = "1.5 MB";
    m.disk_size = "3 MB";

    auto text = m.to_string();
    auto parsed = Manifest::parse(text);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->name == m.name);
    REQUIRE(parsed->dependencies == m.dependencies);
}

TEST_CASE("Manifest parse_human_size", "[manifest]") {
    REQUIRE(Manifest::parse_human_size("1.5 MB") == 1);
    REQUIRE(Manifest::parse_human_size("12 KB") == 12);
    REQUIRE(Manifest::parse_human_size("") == 0);
    REQUIRE(Manifest::parse_human_size("garbage") == 0);
}
