#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "include/json.hpp"

using json = nlohmann::ordered_json;

struct Parametro {
private:
    inline static std::vector<double> param = {};
    inline static std::vector<bool> definitions = {};
    inline static size_t maxId = 0;
    const size_t id;
public:
    Parametro() : id(maxId++) {
        param.emplace_back(0.0);
        definitions.push_back(false);
    }

    [[nodiscard]] double& operator()() const {
        return param[id];
    }
    [[nodiscard]] double& getParam() const {
        return param[id];
    }
    [[nodiscard]] decltype(definitions)::reference isDefined() const {
        return definitions[id];
    }

    Parametro& operator = (const Parametro& other) {
        if (!other.isDefined())
            return *this;

        isDefined() = true;
        getParam() = other.getParam();
        return *this;
    }

    Parametro& operator = (const double other) {
        isDefined() = true;
        getParam() = other;
        return *this;
    }
};

struct ElectricNode {
    Parametro resistenza;
    Parametro ddp;
    Parametro corrente;

    bool solve() {
        if (!resistenza.isDefined()) {
            if (!ddp.isDefined() || !corrente.isDefined())
                return false;
            resistenza = ddp() / corrente();
            return true;
        }
        if (!ddp.isDefined()) {
            if (resistenza.getParam() == 0.0) {
                ddp = 0.0;
                return true;
            }
            if (!resistenza.isDefined() || !corrente.isDefined())
                return false;
            ddp = resistenza() * corrente();
            return true;
        }
        if (!corrente.isDefined()) {
            if (resistenza.getParam() == 0.0) {
                return false;
            }
            if (!resistenza.isDefined() || !ddp.isDefined())
                return false;
            corrente = ddp() / resistenza();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool allDefined() const {
        return resistenza.isDefined() && corrente.isDefined() && ddp.isDefined();
    }
};

struct ElectricBlock {
    ElectricNode params;
    std::vector<ElectricBlock*> internal;
    const enum blockType { SERIE, PARALLELO } tipo = SERIE;

    explicit ElectricBlock(const blockType bt, ElectricNode params,
        const std::vector<ElectricBlock*>& internal = {})
    : params(std::move(params)), internal(internal), tipo(bt) {}

    static std::unique_ptr<ElectricBlock> fromJson(const json& j) {
        if (!j.is_object())
            throw std::runtime_error("Un blocco deve essere un oggetto JSON");

        ElectricNode nodo;
        leggi(j, "resistenza", "resistenza(OHM)", nodo.resistenza);
        leggi(j, "ddp", "ddp(V)", nodo.ddp);
        leggi(j, "corrente", "corrente(A)", nodo.corrente);

        const blockType bt = (j.contains("tipo") && j["tipo"] == "parallelo")
            ? PARALLELO : SERIE;

        auto blocco = std::make_unique<ElectricBlock>(bt, nodo);

        if (j.contains("figli")) {
            if (!j["figli"].is_array())
                throw std::runtime_error("\"figli\" deve essere un array");
            for (const json& figlio : j["figli"]) {
                blocco -> aggiungiFiglio(fromJson(figlio));
            }
        }
        return blocco;
    }

    // Ordine di output: tipo, corrente (A), ddp (V), resistenza (Ohm), figli
    [[nodiscard]] json toJson() const {
        json j;
        j["tipo"] = tipo == SERIE ? "serie" : "parallelo";
        scrivi(j, "corrente(A)", params.corrente);
        scrivi(j, "ddp(V)", params.ddp);
        scrivi(j, "resistenza(OHM)", params.resistenza);

        if (!internal.empty()) {
            j["figli"] = json::array();
            for (const ElectricBlock* figlio : internal)
                j["figli"].push_back(figlio -> toJson());
        }
        return j;
    }

    // Aggiunge un figlio e ne assume la proprietà
    void aggiungiFiglio(std::unique_ptr<ElectricBlock> figlio) {
        internal.push_back(figlio.get());
        owned.push_back(std::move(figlio));
    }

    // Notazione S/P: componente = {R=.., V=.., I=..}, gruppo = (a S b S c){R=.., V=.., I=..}
    // Le grandezze ignote vengono omesse.
    [[nodiscard]] std::string toNotazione() const {
        std::string contenuto;
        aggiungiGrandezza(contenuto, "R", params.resistenza);
        aggiungiGrandezza(contenuto, "V", params.ddp);
        aggiungiGrandezza(contenuto, "I", params.corrente);

        if (internal.empty())
            return "{" + contenuto + "}";

        const char* operatore = tipo == SERIE ? " S " : " P ";
        std::string out = "(";
        for (size_t i = 0; i < internal.size(); ++i) {
            if (i > 0)
                out += operatore;
            out += internal[i] -> toNotazione();
        }
        out += ")";
        if (!contenuto.empty())
            out += "{" + contenuto + "}";
        return out;
    }

    bool solve() {
        solveChanges();

        return isAllDefined();
    }

    bool solveChanges() {
        bool hasChanged = true;
        bool looped = false;
        while (hasChanged) {
            hasChanged = false;
            for (ElectricBlock* block : internal) {
                if (block -> solveChanges())
                    hasChanged = true;
            }
            if (tipo == SERIE) {
                if (risolviSerie())
                    hasChanged = true;
            }
            else
                if (risolviParallelo())
                    hasChanged = true;
            if (hasChanged)
                looped = true;
        }
        return looped;
    }

    [[nodiscard]] bool isAllDefined() const {
        for (const ElectricBlock* block : internal)
            if (!block -> isAllDefined())
                return false;
        return params.allDefined();
    }
private:
    std::vector<std::unique_ptr<ElectricBlock>> owned;

    // Cerca il campo sia come "ddp" sia come "ddp(V)". Se ci sono entrambi è ambiguo: errore.
    static const json* cerca(const json& j, const char* chiave, const char* chiaveUnita) {
        const bool semplice = j.contains(chiave);
        const bool conUnita = j.contains(chiaveUnita);
        if (semplice && conUnita)
            throw std::runtime_error(std::string("Specificati sia \"") + chiave +
                "\" sia \"" + chiaveUnita + "\" nello stesso blocco");
        if (semplice) return &j.at(chiave);
        if (conUnita) return &j.at(chiaveUnita);
        return nullptr;
    }

    // Imposta il parametro se il campo è un numero. Assente o null = resta indefinito.
    static void leggi(const json& j, const char* chiave, const char* chiaveUnita, Parametro& p) {
        const json* valore = cerca(j, chiave, chiaveUnita);
        if (!valore || valore->is_null())
            return;
        if (!valore->is_number())
            throw std::runtime_error(std::string("\"") + chiave + "\" deve essere un numero");
        p = valore->get<double>();
    }

    static void scrivi(json& j, const char* chiave, const Parametro& p) {
        if (!p.isDefined())
            return;
        if (std::isfinite(p()))
            j[chiave] = p();
        else
            j[chiave] = nullptr;
    }

    // Cifre significative mostrate nella notazione (il JSON mantiene la precisione piena)
    static constexpr int CIFRE_NOTAZIONE = 10;

    static std::string formattaNumero(const double v) {
        if (std::isnan(v))
            return "nan";
        if (std::isinf(v))
            return v < 0 ? "-inf" : "inf";
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.*g", CIFRE_NOTAZIONE, v);
        return buffer;
    }

    static void aggiungiGrandezza(std::string& contenuto, const char* nome, const Parametro& p) {
        if (!p.isDefined())
            return;
        if (!contenuto.empty())
            contenuto += ", ";
        contenuto += std::string(nome) + "=" + formattaNumero(p());
    }

    // Restituisce l'unico figlio con la grandezza indefinita; nullptr se sono zero o più di uno
    [[nodiscard]] ElectricBlock* unicoIgnoto(Parametro ElectricNode::* campo) const {
        ElectricBlock* ignoto = nullptr;
        for (ElectricBlock* blocco : internal) {
            if ((blocco -> params.*campo).isDefined())
                continue;
            if (ignoto != nullptr)
                return nullptr;   // più di un ignoto: non deducibile
            ignoto = blocco;
        }
        return ignoto;
    }

    // Totale noto e un solo figlio ignoto: ignoto = totale - somma degli altri
    bool propagaDifferenza(Parametro ElectricNode::* campo) const {
        if (!(params.*campo).isDefined())
            return false;
        ElectricBlock* ignoto = unicoIgnoto(campo);
        if (!ignoto)
            return false;

        double somma = 0.0;
        for (const ElectricBlock* blocco : internal)
            if (blocco != ignoto)
                somma += (blocco -> params.*campo)();

        (ignoto -> params.*campo) = (params.*campo)() - somma;
        return true;
    }

    bool risolviSerie() {
        bool hasChanged = params.solve();

        // Se la resistenza non è definita prova a sommare i blocchi interni
        if (!params.resistenza.isDefined() && !internal.empty()) {
            params.resistenza = 0.0;

            // Prova a sommare tutti i blocchi, se fallisce, resetta la resistenza su indefinito
            for (const ElectricBlock* blocco : internal) {
                if (blocco -> params.resistenza.isDefined())
                    params.resistenza.getParam() += blocco -> params.resistenza();
                else {
                    params.resistenza.isDefined() = false;
                    break;
                }
            }

            if (params.resistenza.isDefined())
                hasChanged = true;
        }

        // Resistenza definita e un solo figlio ignoto: propaga in basso
        if (propagaDifferenza(&ElectricNode::resistenza))
            hasChanged = true;

        // Se la ddp non è definita prova a sommare le ddp interne
        if (!params.ddp.isDefined() && !internal.empty()) {
            params.ddp = 0.0;
            for (const ElectricBlock* blocco : internal) {
                if (blocco -> params.ddp.isDefined())
                    params.ddp.getParam() += blocco -> params.ddp();
                else {
                    params.ddp.isDefined() = false;
                    break;
                }
            }
            if (params.ddp.isDefined())
                hasChanged = true;
        }

        // ddp definita e un solo figlio ignoto: propaga in basso
        if (propagaDifferenza(&ElectricNode::ddp))
            hasChanged = true;

        // Se la corrente non è definita, cerca tra i blocchi interni
        if (!params.corrente.isDefined()) {
            for (const ElectricBlock* blocco : internal) {
                if (!blocco -> params.corrente.isDefined())
                    continue;
                params.corrente = blocco -> params.corrente();
                break;
            }
            if (params.corrente.isDefined())
                hasChanged = true;
        }

        // Se la corrente è definita, imposta tutti i blocchi interni
        if (params.corrente.isDefined()) {
            for (ElectricBlock* blocco : internal) {
                if (!blocco->params.corrente.isDefined()) {
                    hasChanged = true;
                    blocco -> params.corrente = params.corrente;
                }
            }
        }

        if (params.solve())
            hasChanged = true;
        return hasChanged;
    }

    bool risolviParallelo() {
        bool hasChanged = params.solve();

        // Se la resistenza non è definita prova a calcolarla dai blocchi interni
        if (!params.resistenza.isDefined() && !internal.empty()) {
            // Un ramo in corto circuito (R definita e uguale a 0) azzera la resistenza del parallelo.
            const bool cortoCircuito = std::ranges::any_of(internal,
                [](const ElectricBlock* block) {
                    return block -> params.resistenza.isDefined() && block -> params.resistenza() == 0.0;
                });

            if (cortoCircuito) {
                params.resistenza = 0.0;
                hasChanged = true;
            }
            else {
                double sommaConduttanze = 0.0;
                bool tutteNote = true;
                for (const ElectricBlock* blocco : internal) {
                    if (!blocco -> params.resistenza.isDefined()) {
                        tutteNote = false;
                        break;
                    }
                    sommaConduttanze += 1.0 / blocco -> params.resistenza();
                }
                if (tutteNote) {
                    params.resistenza = 1.0 / sommaConduttanze;
                    hasChanged = true;
                }
            }
        }

        // Resistenza definita (e non nulla) e un solo figlio ignoto: propaga in basso
        // 1/Rx = 1/R - somma(1/Ri)
        if (params.resistenza.isDefined() && params.resistenza() != 0.0) {
            if (ElectricBlock* ignoto = unicoIgnoto(&ElectricNode::resistenza)) {
                double sommaConduttanze = 0.0;
                for (const ElectricBlock* blocco : internal)
                    if (blocco != ignoto)
                        sommaConduttanze += 1.0 / blocco -> params.resistenza();

                ignoto -> params.resistenza = 1.0 / (1.0 / params.resistenza() - sommaConduttanze);
                hasChanged = true;
            }
        }

        // Se la ddp non è definita, cerca di dedurla da un componente interno
        if (!params.ddp.isDefined()) {
            for (const ElectricBlock* blocco : internal) {
                if (!blocco -> params.ddp.isDefined())
                    continue;
                params.ddp = blocco -> params.ddp;
                break;
            }
            if (params.ddp.isDefined())
                hasChanged = true;
        }

        // Se la ddp è definita, imposta i blocchi interni ancora ignoti
        if (params.ddp.isDefined()) {
            for (ElectricBlock* blocco : internal) {
                if (!blocco -> params.ddp.isDefined()) {
                    blocco -> params.ddp = params.ddp;
                    hasChanged = true;
                }
            }
        }

        // Se la corrente non è definita prova a sommare i blocchi interni
        if (!params.corrente.isDefined() && !internal.empty()) {
            params.corrente = 0.0;
            for (const ElectricBlock* blocco : internal) {
                if (blocco -> params.corrente.isDefined())
                    params.corrente.getParam() += blocco -> params.corrente();
                else {
                    params.corrente.isDefined() = false;
                    break;
                }
            }
            if (params.corrente.isDefined())
                hasChanged = true;
        }

        // Corrente definita e un solo figlio ignoto: propaga in basso
        if (propagaDifferenza(&ElectricNode::corrente))
            hasChanged = true;

        if (params.solve())
            hasChanged = true;
        return hasChanged;
    }
};

// ---------------------------------------------------------------------------
// Parser della notazione S/P
//
//   espressione := termine { OP termine }     OP = S oppure P (tutti uguali nello stesso livello)
//   termine     := '{' [ grandezza { ',' grandezza } ] '}'
//                | '(' espressione ')' [ '{' grandezza { ',' grandezza } '}' ]
//   grandezza   := ( R | V | I ) '=' numero
//
// Le parentesi servono per l'ordine delle operazioni: mescolare S e P senza parentesi è un errore.
// I valori dopo una parentesi chiusa appartengono al blocco racchiuso (es. la ddp totale).
// ---------------------------------------------------------------------------
struct ErroreNotazione final : std::runtime_error {
    size_t posizione;
    ErroreNotazione(const std::string& messaggio, const size_t pos)
        : std::runtime_error(messaggio), posizione(pos) {}
};

class ParserNotazione {
public:
    explicit ParserNotazione(std::string testo) : s(std::move(testo)) {}

    std::unique_ptr<ElectricBlock> parse() {
        auto blocco = espressione();
        salta();
        if (!fine())
            erroreInatteso();
        return blocco;
    }

private:
    std::string s;
    size_t pos = 0;

    [[noreturn]] void errore(const std::string& messaggio) const {
        throw ErroreNotazione(messaggio, pos);
    }

    [[noreturn]] void erroreInatteso() const {
        const char c = s[pos];
        if (c == '{' || c == '(')
            errore("manca l'operatore S o P tra due elementi");
        if (c == ')')
            errore("')' senza '(' corrispondente");
        errore(std::string("carattere inatteso '") + c + "'");
    }

    [[nodiscard]] bool fine() const { return pos >= s.size(); }

    void salta() {
        while (!fine() && std::isspace(static_cast<unsigned char>(s[pos])))
            ++pos;
    }

    std::unique_ptr<ElectricBlock> espressione() {
        std::vector<std::unique_ptr<ElectricBlock>> termini;
        termini.push_back(termine());

        char op = 0;
        while (true) {
            salta();
            if (fine())
                break;
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[pos])));
            if (c != 'S' && c != 'P')
                break;
            if (op != 0 && c != op)
                errore("S e P nello stesso livello: usare le parentesi per indicare l'ordine");
            op = c;
            ++pos;
            termini.push_back(termine());
        }

        if (termini.size() == 1)
            return std::move(termini[0]);

        auto blocco = std::make_unique<ElectricBlock>(
            op == 'S' ? ElectricBlock::SERIE : ElectricBlock::PARALLELO, ElectricNode{});
        for (auto& t : termini)
            blocco -> aggiungiFiglio(std::move(t));
        return blocco;
    }

    std::unique_ptr<ElectricBlock> termine() {
        salta();
        if (fine())
            errore("atteso '{' o '(' ma la riga finisce qui");

        if (s[pos] == '{') {
            ElectricNode nodo;
            parametri(nodo);
            return std::make_unique<ElectricBlock>(ElectricBlock::SERIE, nodo);
        }

        if (s[pos] == '(') {
            ++pos;
            auto interno = espressione();
            salta();
            if (fine())
                errore("manca ')' di chiusura");
            if (s[pos] != ')')
                erroreInatteso();
            ++pos;

            salta();
            if (!fine() && s[pos] == '{') {
                const size_t inizio = pos;
                ElectricNode extra;
                parametri(extra);
                unisci(interno -> params.resistenza, extra.resistenza, 'R', inizio);
                unisci(interno -> params.ddp, extra.ddp, 'V', inizio);
                unisci(interno -> params.corrente, extra.corrente, 'I', inizio);
            }
            return interno;
        }

        errore(std::string("atteso '{' o '(', trovato '") + s[pos] + "'");
    }

    // Riceve i valori scritti dopo una parentesi chiusa. Se la parentesi racchiude un solo
    // componente che ha già quella grandezza, il dato sarebbe doppio: errore.
    static void unisci(Parametro& destinazione, const Parametro& sorgente,
                       const char nome, const size_t posizione) {
        if (!sorgente.isDefined())
            return;
        if (destinazione.isDefined())
            throw ErroreNotazione(std::string("la grandezza ") + nome +
                " e' indicata due volte (dentro e fuori le parentesi)", posizione);
        destinazione = sorgente;
    }

    // Legge { R=.., V=.., I=.. } a partire da '{'
    void parametri(ElectricNode& nodo) {
        ++pos;  // '{'
        salta();
        if (!fine() && s[pos] == '}') {
            ++pos;
            return;
        }

        while (true) {
            salta();
            if (fine())
                errore("manca '}' di chiusura");

            Parametro* p = nullptr;
            const char nome = static_cast<char>(std::toupper(static_cast<unsigned char>(s[pos])));
            if (nome == 'R')
                p = &nodo.resistenza;
            else if (nome == 'V')
                p = &nodo.ddp;
            else if (nome == 'I')
                p = &nodo.corrente;
            else
                errore(std::string("grandezza sconosciuta '") + s[pos] + "': usare R, V o I");
            if (p -> isDefined())
                errore(std::string("la grandezza ") + nome + " compare due volte");
            ++pos;

            salta();
            if (fine() || s[pos] != '=')
                errore("atteso '=' dopo la grandezza");
            ++pos;

            salta();
            *p = numero();

            salta();
            if (fine())
                errore("manca '}' di chiusura");
            if (s[pos] == ',') {
                ++pos;
                continue;
            }
            if (s[pos] == '}') {
                ++pos;
                return;
            }
            errore("atteso ',' o '}'");
        }
    }

    // Numero decimale con punto (es. 0.5, 1e-3). Non accetta inf, nan, esadecimali.
    double numero() {
        const size_t inizio = pos;
        const std::string ammessi = "+-.eE";
        while (!fine() && (std::isdigit(static_cast<unsigned char>(s[pos]))
                           || ammessi.find(s[pos]) != std::string::npos))
            ++pos;

        const std::string token = s.substr(inizio, pos - inizio);
        char* fineToken = nullptr;
        const double v = std::strtod(token.c_str(), &fineToken);
        if (token.empty() || *fineToken != '\0') {
            pos = inizio;
            errore("atteso un numero");
        }
        if (!std::isfinite(v)) {
            pos = inizio;
            errore("il valore deve essere un numero finito");
        }
        return v;
    }
};

namespace terminal {
    bool rigaVuota(const std::string& riga) {
        return riga.find_first_not_of(" \t\r\n") == std::string::npos;
    }

    // Restituisce '1', '2', '3' o '4'; 0 se l'input è terminato
    char chiediScelta() {
        std::string riga;
        while (true) {
            std::cout << "Scelta: ";
            if (!std::getline(std::cin, riga))
                return 0;
            const size_t i = riga.find_first_not_of(" \t\r");
            if (i != std::string::npos && riga[i] >= '1' && riga[i] <= '4'
                && riga.find_first_not_of(" \t\r", i + 1) == std::string::npos)
                return riga[i];
            std::cout << "Scelta non valida: digita un numero da 1 a 4.\n";
        }
    }

    // Scrive il circuito in formato JSON; restituisce 0 se va a buon fine, 1 altrimenti
    int salvaJson(const ElectricBlock& circuito, const char* nomeFile) {
        std::ofstream out(nomeFile);
        if (!out) {
            std::cerr << "Impossibile scrivere " << nomeFile << '\n';
            return 1;
        }
        out << circuito.toJson().dump(2);
        out.close();
        if (!out) {
            std::cerr << "Errore durante la scrittura di " << nomeFile << '\n';
            return 1;
        }
        return 0;
    }

    // Legge un file JSON e lo converte in circuito; nullptr (con messaggio) se il file non si apre
    std::unique_ptr<ElectricBlock> caricaJson(const char* nomeFile) {
        std::ifstream in(nomeFile);
        if (!in) {
            std::cerr << "Impossibile aprire " << nomeFile << '\n';
            return nullptr;
        }
        const json dati = json::parse(in);
        return ElectricBlock::fromJson(dati);
    }

    enum class EsitoInput { OK, ANNULLATO, TERMINATO };

    // Chiede una notazione S/P finché è valida. Riga vuota = annullato, fine input = terminato.
    EsitoInput chiediNotazione(std::unique_ptr<ElectricBlock>& circuito) {
        std::cout << "\nScrivi il circuito su una sola riga: S = serie, P = parallelo,\n"
                     "parentesi per l'ordine delle operazioni.\n"
                     "Componente: {R=..., V=..., I=...} (le grandezze ignote si omettono).\n"
                     "Dopo una parentesi chiusa puoi indicare i valori del gruppo, es. {V=24}.\n"
                     "Esempio: ({R=100} S ({R=200} P {R=300})){V=24}\n"
                     "Riga vuota per annullare.\n";

        std::string riga;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, riga)) {
                std::cerr << "Input terminato\n";
                return EsitoInput::TERMINATO;
            }
            if (rigaVuota(riga)) {
                std::cout << "Nessun circuito inserito.\n";
                return EsitoInput::ANNULLATO;
            }

            try {
                circuito = ParserNotazione(riga).parse();
                return EsitoInput::OK;
            }
            catch (const ErroreNotazione& e) {
                std::cerr << "Errore di notazione: " << e.what() << "\n  " << riga
                          << "\n  " << std::string(e.posizione, ' ') << "^\n"
                          << "Riprova (riga vuota per annullare).\n";
            }
        }
    }

    // Comune a entrambe le opzioni: risolve, mostra la notazione e salva result.json
    int concludi(ElectricBlock& circuito) {
        if (!circuito.solve())
            std::cerr << "Dati insufficienti: alcuni valori restano indefiniti\n";

        std::cout << "\nRisultato:\n" << circuito.toNotazione() << '\n';

        if (salvaJson(circuito, "result.json") != 0)
            return 1;
        std::cout << "\nRisultato salvato anche in result.json\n";
        return 0;
    }

    // Opzione 1: legge data.json, risolve e salva result.json
    int opzioneFile() {
        const auto circuito = caricaJson("data.json");
        if (!circuito)
            return 1;
        return concludi(*circuito);
    }

    // Opzione 2: legge la notazione S/P da tastiera, risolve e salva result.json
    int opzioneNotazione() {
        std::unique_ptr<ElectricBlock> circuito;
        switch (chiediNotazione(circuito)) {
            case EsitoInput::OK:        return concludi(*circuito);
            case EsitoInput::ANNULLATO: return 0;
            case EsitoInput::TERMINATO: return 1;
        }
        return 1;
    }

    // Opzione 3: legge result.json e lo mostra in notazione S/P (senza risolvere)
    int opzioneMostraRisultato() {
        const auto circuito = caricaJson("result.json");
        if (!circuito)
            return 1;
        std::cout << "\nContenuto di result.json:\n" << circuito -> toNotazione() << '\n';
        return 0;
    }

    // Opzione 4: legge la notazione S/P da tastiera e la scrive in data.json (senza risolvere)
    int opzioneScriviDati() {
        std::unique_ptr<ElectricBlock> circuito;
        switch (chiediNotazione(circuito)) {
            case EsitoInput::OK:
                break;
            case EsitoInput::ANNULLATO:
                return 0;
            case EsitoInput::TERMINATO:
                return 1;
        }
        if (salvaJson(*circuito, "data.json") != 0)
            return 1;
        std::cout << "\nCircuito salvato in data.json (usa l'opzione 1 per risolverlo)\n";
        return 0;
    }

    int esegui() {
        try {
            std::cout << "=== Risolutore di circuiti resistivi - v1.2 ===\n\n"
                         "  [1] Leggi da file (data.json), risolvi e salva su file (result.json)\n"
                         "  [2] Scrivi il circuito in notazione S/P, risolvi e salva su result.json\n"
                         "  [3] Mostra result.json in notazione S/P\n"
                         "  [4] Scrivi un circuito in notazione S/P e salvalo in data.json\n\n"
                         "S/P = serie/parallelo. Digita un numero da 1 a 4 e premi Invio.\n\n";

            switch (chiediScelta()) {
                case '1': return opzioneFile();
                case '2': return opzioneNotazione();
                case '3': return opzioneMostraRisultato();
                case '4': return opzioneScriviDati();
                default:  return 1;
            }
        }
        catch (const json::exception& e) {
            std::cerr << "Errore JSON: " << e.what() << '\n';
            return 1;
        }
        catch (const std::exception& e) {
            std::cerr << "Errore: " << e.what() << '\n';
            return 1;
        }
    }
}

int main() {
    while (true) {
        const int codice = terminal::esegui();
        if (codice != 0)
            return codice;
    }
    return 0;
}