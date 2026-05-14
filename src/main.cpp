#include <Runtime.hpp>
#include <Parser.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    Runtime::start();
    try {
        Parser parser;
        std::cout << "Rozpoczynam parsowanie pliku...\n";
        
        
        // Podaj ścieżkę do wygenerowanego z .proto pliku binarnego .pb
        auto scene = parser.parse("tests/test_scene.pb"); 
        
        std::cout << "\n--- SUKCES ---\n";
        std::cout << "Zaladowano projekt: " << scene->getExperimentName() << "\n";
        std::cout << "Liczba obiektow w scenie: " << scene->getObjects().size() << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Blad parsowania: " << e.what() << '\n';
    }
    return 0;
}