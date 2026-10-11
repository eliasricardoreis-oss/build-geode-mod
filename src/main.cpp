#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>

using namespace geode::prelude;

class $modify(PathfinderPlayLayer, PlayLayer) {
    int m_frameCounter = 0;
    bool m_isAnalyzing = false;

    bool init(GJGameLevel* level) {
        if (!PlayLayer::init(level)) return false;
        m_frameCounter = 0;
        m_isAnalyzing = false;
        return true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        // Verifica se o bot está ativo
        if (!Mod::get()->getSettingValue<bool>("enable-bot")) return;
        if (!m_player1) return;

        // Otimização: Controla o intervalo de frames para não sobrecarregar a CPU
        int interval = Mod::get()->getSettingValue<int64_t>("update-interval");
        m_frameCounter++;
        if (m_frameCounter < interval) return;
        m_frameCounter = 0;

        // 1. EXTRAÇÃO DE DADOS DO JOGADOR
        CCPoint playerPos = m_player1->getPosition();
        float playerSpeed = m_player1->m_playerSpeed; 
        bool isDual = m_levelSettings ? m_levelSettings->m_twoPlayerMode : false;
        bool isHolding = m_player1->m_isHolding;

        // Detecção de Gamemode Atual
        std::string gamemode = "Cube";
        if (m_player1->m_isShip) gamemode = "Ship";
        else if (m_player1->m_isBall) gamemode = "Ball";
        else if (m_player1->m_isUfo) gamemode = "UFO";
        else if (m_player1->m_isWave) gamemode = "Wave";
        else if (m_player1->m_isRobot) gamemode = "Robot";
        else if (m_player1->m_isSpider) gamemode = "Spider";
        else if (m_player1->m_isSwing) gamemode = "Swing Copter";

        // 2. VARREDURA DE MEMÓRIA DE OBJETOS PRÓXIMOS (Hazards, Blocos, Portais, Pads)
        CCArray* objects = m_objects;
        if (!objects) return;

        int hazardsNearby = 0;
        int speedPortalsNearby = 0;
        int padsOrOrbsNearby = 0;
        float closestHazardDist = 9999.0f;

        // Varredura Otimizada (Raio de proximidade em relação à posição do player)
        unsigned int objCount = objects->count();
        for (unsigned int i = 0; i < objCount; i++) {
            GameObject* obj = typeinfo_cast<GameObject*>(objects->objectAtIndex(i));
            if (!obj) continue;

            // Filtra apenas objetos na frente ou muito próximos do jogador
            float dist = obj->getPosition().getDistance(playerPos);
            if (dist < 300.0f && obj->getPosition().x >= playerPos.x - 20.0f) {
                int id = obj->m_objectID;

                // Verificação de Perigos (Spikes, Saws, Monsters)
                if (obj->m_isHazard || id == 8 || id == 39 || id == 103 || (id >= 109 && id <= 115)) {
                    hazardsNearby++;
                    if (dist < closestHazardDist) {
                        closestHazardDist = dist;
                    }
                }
                // Speed Portals (Ex: 200=0.5x, 201=1x, 202=2x, 203=3x, 1334=4x)
                else if (id == 200 || id == 201 || id == 202 || id == 203 || id == 1334) {
                    speedPortalsNearby++;
                }
                // Jump Pads / Orbs
                else if (obj->m_isPad || obj->m_isOrb) {
                    padsOrOrbsNearby++;
                }
            }
        }

        // 3. LOG DE DIAGNÓSTICO LOCAL (Pathfinding Heurístico Base)
        log::info("Pos: ({:.1f}, {:.1f}) | Modo: {} | Vel: {:.1f} | Dual: {} | Perigos: {} (Dist mais prox: {:.1f})", 
            playerPos.x, playerPos.y, gamemode, playerSpeed, isDual, hazardsNearby, closestHazardDist);

        // Exemplo de reação automática de emergência baseada em memória local (Pathfinding Instantâneo)
        if (closestHazardDist < 50.0f && !isHolding && gamemode == "Cube") {
            // Lógica de pulo emergencial preventivo se necessário
        }

        // 4. INTEGRAÇÃO ASSÍNCRONA COM O GEMINI 2.5 FLASH
        std::string apiKey = Mod::get()->getSettingValue<std::string>("gemini-api-key");
        if (!apiKey.empty() && !m_isAnalyzing) {
            m_isAnalyzing = true;

            // Monta o payload JSON com o estado atual do jogo extraído da memória
            std::string jsonBody = fmt::format(
                "{{\"contents\": [{{\"parts\": [{{\"text\": \"Geometry Dash State - X: {:.1f}, Y: {:.1f}, Mode: {}, Speed: {:.1f}, Dual: {}, Hazards Ahead: {}, Closest Hazard Dist: {:.1f}. What is the optimal immediate action?\"}}]}}]}}",
                playerPos.x, playerPos.y, gamemode, playerSpeed, isDual ? "true" : "false", hazardsNearby, closestHazardDist
            );

            std::string url = fmt::format("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={}", apiKey);

            // Requisição Web Assíncrona do Geode (Não trava o jogo)
            web::WebRequest req();
            // Configura cabeçalho e envia POST
            // (Nota: a API web do Geode lida com o cliente HTTP de forma limpa em background)
            web::WebRequest().header("Content-Type", "application/json").body(jsonBody).post(url, [this](web::WebResponse* res) {
                m_isAnalyzing = false;
                if (res->ok()) {
                    std::string responseStr = res->string().unwrapOr("");
                    // Aqui você processaria a resposta do Gemini caso queira ajustar parâmetros dinamicamente
                }
            });
        }
    }
};
