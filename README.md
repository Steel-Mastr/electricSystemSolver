# Risolutore di circuiti resistivi

**Versione 1.1**

Programma C++ che legge un circuito resistivo descritto come albero di blocchi **serie** e **parallelo**, ricava tutte le grandezze che riesce a dedurre (resistenza, differenza di potenziale, corrente) e mostra il risultato a terminale e in `result.json`.

Il circuito si può fornire in due modi: da file (`data.json`) oppure scrivendolo direttamente nel terminale con una **notazione a lettere** (S = serie, P = parallelo).

Non devi indicare tutti i valori: basta fornire quelli noti. Il programma propaga le informazioni su e giù per l'albero, applicando la legge di Ohm e le regole di serie e parallelo, finché non c'è più nulla da dedurre.

---

## Indice

1. [Requisiti e compilazione](#requisiti-e-compilazione)
2. [Utilizzo](#utilizzo)
3. [Formato di input (JSON)](#formato-di-input-json)
4. [Formato di output (JSON)](#formato-di-output-json)
5. [Notazione S/P](#notazione-sp)
6. [Esempio completo](#esempio-completo)
7. [Come ragiona il risolutore](#come-ragiona-il-risolutore)
8. [Limiti noti](#limiti-noti)
9. [Errori e codici di uscita](#errori-e-codici-di-uscita)

---

## Requisiti e compilazione

- Compilatore con supporto **C++20** (il codice usa `std::ranges`), ad esempio GCC 10+, Clang 13+, MSVC 2019 16.10+.
- La libreria [nlohmann/json](https://github.com/nlohmann/json) in versione *single header*, posizionata in `include/json.hpp`.

Struttura attesa:

```
.
├── main.cpp
├── include/
│   └── json.hpp
├── data.json          (input)
└── result.json        (generato)
```

Compilazione:

```bash
g++ -std=c++20 -O2 -o circuito main.cpp
```

---

## Utilizzo

Avvia `./circuito`: compare un menu. Digita il numero della scelta e premi Invio (la scelta non è a tasto singolo).

| Scelta | Cosa fa |
|---|---|
| `1` | Legge il circuito da `data.json`, nella directory da cui hai lanciato il programma. |
| `2` | Ti chiede di scrivere il circuito in [notazione S/P](#notazione-sp) su una riga. |

**In entrambi i casi** il programma:

1. mostra a terminale il risultato in notazione S/P;
2. salva lo stesso risultato in `result.json` (nella directory corrente, sovrascrivendolo);
3. attende un Invio prima di chiudersi, così la finestra non sparisce subito. Succede anche dopo un errore.

I nomi `data.json` e `result.json` sono fissi e il programma non accetta argomenti da riga di comando. L'opzione 2 non legge né modifica `data.json`.

Se i dati non bastano per ricavare ogni valore, il programma lo segnala su `stderr` con il messaggio `Dati insufficienti: alcuni valori restano indefiniti` e mostra e salva comunque tutto ciò che è riuscito a calcolare.

---

## Formato di input (JSON)

Vale per l'opzione 1.

L'input è un singolo oggetto JSON che rappresenta il blocco radice. Ogni blocco può contenere altri blocchi nel campo `figli`, a qualsiasi profondità.

| Campo | Tipo | Obbligatorio | Significato |
|---|---|---|---|
| `tipo` | stringa | no | `"serie"` oppure `"parallelo"`. Se assente (o con qualsiasi altro valore) il blocco è in **serie**. |
| `corrente` o `corrente(A)` | numero | no | Corrente in ampere. |
| `ddp` o `ddp(V)` | numero | no | Differenza di potenziale in volt. |
| `resistenza` o `resistenza(OHM)` | numero | no | Resistenza in ohm. |
| `figli` | array di blocchi | no | Blocchi contenuti. Un blocco senza figli è un componente (una "foglia"). |

Note:

- Per ogni grandezza puoi usare la forma semplice (`ddp`) o quella con unità (`ddp(V)`), anche mescolandole nello stesso file. **Non puoi usarle entrambe nello stesso blocco**: il programma si ferma con un errore, perché i due valori potrebbero essere diversi.
- Un campo assente oppure `null` significa "valore ignoto".
- I valori vanno espressi nelle unità di base (A, V, Ω), senza prefissi: `0.005` e non `5 mA`.
- Il blocco radice deve essere un oggetto JSON e `figli`, se presente, deve essere un array.

---

## Formato di output (JSON)

`result.json` ha la stessa struttura dell'input, con questi accorgimenti:

- Le chiavi sono sempre nella forma con unità e in quest'ordine: `tipo`, `corrente(A)`, `ddp(V)`, `resistenza(OHM)`, `figli`.
- Le grandezze rimaste ignote vengono **omesse**.
- Un valore non finito (ad esempio la resistenza di un circuito con corrente nulla) viene scritto come `null`.
- `tipo` compare sempre, quindi i componenti senza `tipo` in input risultano `"serie"`.

---

## Notazione S/P

Vale per l'opzione 2, ed è anche il formato in cui il risultato viene mostrato a terminale in entrambe le opzioni. È la notazione "a lettere" dei circuiti serie-parallelo: **S** per la serie, **P** per il parallelo, parentesi per l'ordine delle operazioni.

**Componente**

```
{R=100, V=12, I=0.12}
```

`R` è la resistenza (Ω), `V` la ddp (V), `I` la corrente (A). Le grandezze ignote si omettono: `{R=100}`, `{V=3, I=0.12}`, e `{}` è un componente di cui non si sa nulla. L'ordine delle grandezze è libero in input, mentre in output è sempre R, V, I.

**Operazioni**

```
{R=100} S {R=200}                      serie di due componenti
{R=200} P {R=300}                      parallelo di due componenti
{R=1} S {R=2} S {R=3}                  catena di più elementi dello stesso tipo
({R=200} P {R=300}) S {R=100}          parentesi per raggruppare
```

**Valori di un gruppo.** Dopo una parentesi chiusa puoi scrivere le grandezze del gruppo intero, ad esempio la ddp totale:

```
({R=100} S {R=200}){V=24}
```

**Regole**

- `S` e `P` possono essere scritte anche in minuscolo, come `R`, `V` e `I`. Spazi e a capo tra gli elementi sono liberi.
- Non esiste precedenza tra `S` e `P`: nello stesso livello si può usare un solo tipo di operatore. `{R=1} S {R=2} P {R=3}` è un errore, va scritto `({R=1} S {R=2}) P {R=3}` oppure `{R=1} S ({R=2} P {R=3})`.
- Una catena senza parentesi diventa un unico blocco con più figli. Un gruppo tra parentesi resta un blocco a sé.
- I numeri usano il **punto** decimale e possono avere esponente (`0.5`, `1e-3`). Non sono accettati `inf`, `nan`, valori esadecimali né prefissi come `k` o `m`.
- Se le parentesi racchiudono un solo componente, i valori scritti dopo la parentesi si aggiungono a quel componente; se la stessa grandezza è già indicata dentro, è un errore.

**Errori.** Quando la riga non è valida, il programma mostra il messaggio e un `^` sotto il punto del problema, poi ti lascia riprovare. Una riga vuota annulla.

```
> {R=1} S {R=2} P {R=3}
Errore di notazione: S e P nello stesso livello: usare le parentesi per indicare l'ordine
  {R=1} S {R=2} P {R=3}
                ^
```

**Notazione in output.** Il risultato usa la stessa notazione, con tutte le grandezze calcolate per ogni componente e per ogni gruppo. Nella notazione i numeri sono mostrati con 10 cifre significative; il JSON mantiene la precisione piena.

---

## Esempio completo

Il file `data.json` allegato mostra in un solo circuito la maggior parte delle funzionalità:

```json
{
  "tipo": "serie",
  "ddp(V)": 24,
  "figli": [
    { "resistenza(OHM)": 100 },
    {
      "tipo": "parallelo",
      "figli": [
        { "resistenza": 200 },
        { "resistenza": 300 },
        {
          "tipo": "serie",
          "figli": [
            { "resistenza": 50 },
            { "resistenza": 150 }
          ]
        }
      ]
    },
    { "corrente(A)": 0.12, "ddp": 3 }
  ]
}
```

Cosa viene dedotto, in ordine di apparizione:

- L'ultimo componente ha `ddp` e `corrente` ma non la resistenza: il programma ricava **25 Ω**.
- La corrente di 0,12 A, nota solo in quel componente, risale alla radice (in una serie la corrente è comune) e poi scende a tutti gli altri blocchi in serie.
- Il parallelo ha resistenza equivalente **75 Ω**; con 0,12 A che lo attraversano ai suoi capi cadono **9 V**.
- I 9 V si propagano a ogni ramo del parallelo; da ciascuno si ricava la corrente (0,045 A, 0,03 A, 0,045 A) e la loro somma torna a 0,12 A.
- Nel ramo in serie interno i 9 V si ripartiscono in 2,25 V e 6,75 V.

Risultato:

| Blocco | Resistenza (Ω) | ddp (V) | Corrente (A) |
|---|---|---|---|
| Radice (serie) | 200 | 24 | 0,12 |
| R = 100 | 100 | 12 | 0,12 |
| Parallelo | 75 | 9 | 0,12 |
| ↳ R = 200 | 200 | 9 | 0,045 |
| ↳ R = 300 | 300 | 9 | 0,03 |
| ↳ Serie interna | 200 | 9 | 0,045 |
| &nbsp;&nbsp;&nbsp;↳ R = 50 | 50 | 2,25 | 0,045 |
| &nbsp;&nbsp;&nbsp;↳ R = 150 | 150 | 6,75 | 0,045 |
| Ultimo componente | 25 | 3 | 0,12 |

**Lo stesso circuito in notazione S/P** (puoi incollarlo nell'opzione 2):

```
({R=100} S ({R=200} P {R=300} P ({R=50} S {R=150})) S {V=3, I=0.12}){V=24}
```

Con questo input, o con l'opzione 1 su `data.json`, a terminale compare il risultato (su una sola riga):

```
({R=100, V=12, I=0.12} S ({R=200, V=9, I=0.045} P {R=300, V=9, I=0.03} P ({R=50, V=2.25, I=0.045} S {R=150, V=6.75, I=0.045}){R=200, V=9, I=0.045}){R=75, V=9, I=0.12} S {R=25, V=3, I=0.12}){R=200, V=24, I=0.12}
```

Il `result.json` è identico nei due casi. Anche la notazione stampata può essere incollata di nuovo nell'opzione 2 e dà lo stesso risultato.

Per vedere il risolutore "ragionare", prova a cancellare un dato alla volta (ad esempio la `ddp` della radice) e osserva cosa resta calcolabile.

---

## Come ragiona il risolutore

Il programma ripete i passaggi seguenti, su tutti i blocchi dai figli verso la radice, finché una passata completa non cambia più nulla.

**Legge di Ohm, su ogni blocco.** Se due tra resistenza, ddp e corrente sono note, ricava la terza.

**Blocco in serie**

- Resistenza = somma delle resistenze dei figli (se tutte note).
- ddp = somma delle ddp dei figli (se tutte note).
- La corrente è la stessa per tutti: se è nota in un figlio o nel blocco, viene assegnata a tutti gli altri.

**Blocco in parallelo**

- 1 / R = somma di 1 / Ri dei figli (se tutte note). Se un ramo ha resistenza 0 (corto circuito), la resistenza del parallelo è 0.
- La ddp è la stessa per tutti: se è nota in un figlio o nel blocco, viene assegnata a tutti gli altri.
- Corrente = somma delle correnti dei figli (se tutte note).

---

## Limiti noti

**Modello del circuito**

- Sono supportate solo reti **serie-parallelo ad albero**. Ponti (ad esempio Wheatstone), maglie generiche e nodi condivisi tra rami non sono rappresentabili.
- Solo circuiti **resistivi in corrente continua**. Non esistono condensatori, induttori, diodi, né generatori con resistenza interna come elementi dedicati.
- Nessun prefisso o conversione di unità: tutto in A, V, Ω.

**Capacità di deduzione**

- Il programma **non deduce per sottrazione**. Esempi: in una serie con ddp totale nota e ddp di un solo figlio noto, la ddp dell'altro figlio non viene ricavata; in un parallelo con corrente totale e corrente di un ramo note, la corrente dell'altro ramo non viene ricavata.
- Se i dati non bastano, i valori restano ignoti (e quindi omessi dall'output). Non viene fatto alcun tentativo di stima.
- Con resistenza 0 (corto circuito) e ddp nota, la corrente non è determinabile e resta ignota.

**Validazione dei dati**

- Non viene verificata la **coerenza** dei dati. Un blocco con `resistenza: 10`, `ddp: 5` e `corrente: 3` viene accettato così com'è, anche se 10 × 3 ≠ 5. Lo stesso vale per un blocco in serie la cui `ddp` dichiarata non coincide con la somma delle ddp dei figli.
- Valori negativi, nulli o fuori scala fisica non vengono controllati.
- Un `tipo` diverso da `"parallelo"` (compresi errori di battitura o maiuscole, come `"Parallelo"`) viene trattato come `"serie"` senza alcun avviso.

**Numeri e output**

- I calcoli usano numeri `double`; l'output non viene arrotondato, quindi possono comparire cifre decimali di troppo (ad esempio `0.30000000000000004`).
- Un valore non finito (per esempio la resistenza con corrente 0 e ddp non nulla) viene scritto come `null`. Se riusi `result.json` come input, quel `null` verrà letto come "ignoto".

**Uso da riga di comando**

- File e percorsi fissi: `data.json` in input e `result.json` in output, nella directory corrente. Nessun argomento da riga di comando.
- Il menu richiede di premere Invio dopo la scelta; non è un tasto singolo.
- L'esito "dati insufficienti" non cambia il codice di uscita (resta 0): per accorgersene bisogna leggere `stderr`.

**Notazione S/P**

- Una sola riga per circuito, senza precedenza tra `S` e `P` (servono le parentesi) e con il punto come separatore decimale.
- Nell'output i numeri hanno 10 cifre significative, quindi la notazione stampata è meno precisa del JSON. Un valore non finito è scritto come `inf` o `nan`, ma queste parole non sono accettate in input.
- Un blocco JSON con un solo figlio (caso possibile solo con l'opzione 1) viene stampato come `({R=5, V=10, I=2}){R=5, V=10, I=2}`. Se incolli quella riga nell'opzione 2 ottieni un errore, perché le stesse grandezze risultano indicate sia dentro sia fuori le parentesi.

---

## Errori e codici di uscita

| Situazione | Messaggio su `stderr` | Codice di uscita | `result.json` |
|---|---|---|---|
| Tutto calcolato | nessuno | 0 | scritto |
| Dati insufficienti | `Dati insufficienti: alcuni valori restano indefiniti` | 0 | scritto (parziale) |
| `data.json` non trovato | `Impossibile aprire data.json` | 1 | non toccato |
| JSON non valido | `Errore JSON: ...` | 1 | non toccato |
| Blocco che non è un oggetto | `Errore: Un blocco deve essere un oggetto JSON` | 1 | non toccato |
| `figli` non è un array | `Errore: "figli" deve essere un array` | 1 | non toccato |
| Grandezza non numerica | `Errore: "ddp" deve essere un numero` (o la grandezza interessata) | 1 | non toccato |
| Forma semplice e forma con unità insieme | `Errore: Specificati sia "ddp" sia "ddp(V)" nello stesso blocco` | 1 | non toccato |
| Impossibile scrivere `result.json` | `Impossibile scrivere result.json` | 1 | non scritto |

Il programma attende sempre un Invio prima di chiudersi, anche dopo un errore.

Altri casi:

| Situazione | Comportamento | Codice di uscita |
|---|---|---|
| Scelta del menu diversa da `1` o `2` | `Scelta non valida: digita 1 oppure 2.` e richiede la scelta | – |
| Notazione non valida (opzione 2) | `Errore di notazione: ...` con `^` sotto il punto del problema, poi puoi riprovare | – |
| Riga vuota nell'opzione 2 | `Nessun circuito inserito.` e chiusura | 0 |
| Input terminato (stdin chiuso) prima della scelta o della notazione | chiusura | 1 |
| Impossibile scrivere `result.json` | il risultato viene comunque mostrato a terminale | 1 |

In caso di errore, un eventuale `result.json` di una esecuzione precedente resta com'è: non fidarti di quel file se il programma è terminato con codice 1.