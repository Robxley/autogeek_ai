#include <gtest/gtest.h>
#include "../../src/RecordingEngine/Log.hpp"

TEST(LogSystem, Initialization) {
    // Initialiser le système
    agk::Log::Init();

    // Vérifier l'allocation des pointeurs partagés vers spdlog
    auto coreLogger = agk::Log::GetCoreLogger();
    auto clientLogger = agk::Log::GetClientLogger();

    ASSERT_NE(coreLogger, nullptr);
    ASSERT_NE(clientLogger, nullptr);
    
    // Vérifier les noms internes (qui valident que spdlog a bien instancié les sinks)
    EXPECT_EQ(coreLogger->name(), "CORE");
    EXPECT_EQ(clientLogger->name(), "CLIENT");
}

TEST(LogSystem, MarcosCompilation) {
    // Simplement s'assurer que les macros compilent et peuvent écrire
    // sans crasher l'application. On ne peut pas facilement tester le flux stdout 
    // généré par spdlog avec les macros sans complexifier le test avec des sinks custom.
    // Mais l'exécution est suffisante pour CTest.
    
    agk::Log::Init();

    // On s'assure que cela tourne sans erreur
    EXPECT_NO_THROW({
        AGK_CORE_INFO("Test unitaire du Logger CORE : succès !");
        AGK_INFO("Test unitaire du Logger CLIENT : succès !");
    });
}
