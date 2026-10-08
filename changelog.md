# Changelog

Tutte le modifiche rilevanti al progetto sono documentate in questo file.
Il formato segue [Keep a Changelog](https://keepachangelog.com/it-IT/1.1.0/).

## [1.2] - 2026-10-08

### Aggiunto

- Menu ampliato a quattro voci: oltre a risolvere da `data.json` (`1`) e da notazione S/P (`2`), ora `3` mostra `result.json` in notazione S/P senza risolvere e `4` salva in `data.json` un circuito scritto in notazione S/P senza risolvere.
- Deduzione **per sottrazione**: in un blocco in serie, nota la resistenza (o la ddp) complessiva e tutte quelle dei figli tranne una, il valore del figlio ignoto viene ricavato per differenza; in un blocco in parallelo vale lo stesso per la corrente. In parallelo, nota la resistenza equivalente (non nulla) e tutte le resistenze dei rami tranne una, quella ignota si ricava da `1/Rx = 1/R - somma(1/Ri)`.

### Modificato

- Dopo un'operazione conclusa con successo il programma torna al menu iniziale invece di attendere un Invio e chiudersi; termina con codice `1` quando un'operazione fallisce o quando l'input finisce.
- Messaggio per una scelta non valida aggiornato a `Scelta non valida: digita un numero da 1 a 4.`
- Nel README: versione 1.2, descrizione delle quattro opzioni, della deduzione per sottrazione e del ritorno al menu.

## [1.1] - 2026-10-07

### Aggiunto

- Menu iniziale da terminale: `1` per leggere da `data.json`, `2` per inserire il circuito in notazione S/P. La scelta si conferma con Invio.
- Parser della **notazione S/P**: componenti `{R=..., V=..., I=...}` con grandezze facoltative, operatori `S` (serie) e `P` (parallelo), parentesi per l'ordine delle operazioni e valori del gruppo dopo la parentesi chiusa (es. `({R=100} S {R=200}){V=24}`). Mescolare `S` e `P` senza parentesi è un errore.
- Segnalazione degli errori di notazione con messaggio e `^` sotto il punto del problema; la riga può essere riscritta senza riavviare il programma.
- Risultato mostrato a terminale in notazione S/P **in entrambe le opzioni**, oltre al salvataggio in `result.json`.
- Attesa di un Invio prima della chiusura, anche dopo un errore.
- Nel README, la sezione "Notazione S/P" e l'aggiornamento di utilizzo, limiti ed errori.

### Modificato

- Il salvataggio di `result.json` avviene dopo la stampa del risultato: se la scrittura del file fallisce, il risultato resta comunque visibile a terminale.
- La costruzione dell'albero di blocchi passa da un metodo pubblico `aggiungiFiglio`, usato sia dal lettore JSON sia dal parser.

## [1.0] - 2026-10-07

Prima versione stabile.

### Aggiunto

- Risolutore di circuiti resistivi ad albero, con blocchi **serie** e **parallelo** annidabili a qualsiasi profondità.
- Lettura del circuito da `data.json` e scrittura del risultato in `result.json`.
- Deduzione automatica di resistenza, ddp e corrente tramite legge di Ohm, regole di serie e parallelo e propagazione di ddp (nel parallelo) e corrente (nella serie) tra blocchi padre e figli, ripetuta fino a stabilità.
- Chiavi di input accettate sia nella forma semplice (`ddp`) sia con unità (`ddp(V)`, `corrente(A)`, `resistenza(OHM)`), anche mescolate nello stesso file.
- Errore esplicito se nello stesso blocco compaiono entrambe le forme della stessa grandezza.
- Output con chiavi sempre nella forma con unità e in ordine fisso: `tipo`, `corrente(A)`, `ddp(V)`, `resistenza(OHM)`, `figli`. L'ordine è garantito dall'uso di `nlohmann::ordered_json`.
- Le grandezze non determinabili vengono omesse dall'output; i valori non finiti sono scritti come `null`.
- Segnalazione su `stderr` quando i dati sono insufficienti (codice di uscita 0), ed errori con codice di uscita 1 per file mancante, JSON non valido, tipi di campo errati e `figli` non array.
- `data.json` di esempio che mostra le funzionalità principali.
- `README.md` con istruzioni d'uso, formato dei file, funzionamento del risolutore e limiti noti.

### Corretto

- **Parallelo con resistenza di un ramo ancora ignota:** una resistenza non ancora calcolata veniva letta come 0, perciò la resistenza del parallelo risultava infinita (`null` nell'output) e la sua corrente 0, senza alcun avviso. Ora una resistenza ignota viene ignorata finché non è nota.
- **Parallelo con un ramo in corto circuito (R = 0):** la resistenza equivalente risultava infinita invece di 0. Ora vale correttamente 0.

### Limiti noti

Elenco completo nella sezione "Limiti noti" del `README.md`. I principali:

- Solo reti serie-parallelo, in corrente continua e puramente resistive.
- Nessuna deduzione per sottrazione (ad esempio ddp di un ramo ricavata da ddp totale e ddp degli altri).
- Nessun controllo di coerenza tra i dati forniti.
- Un `tipo` non riconosciuto viene trattato come `"serie"` senza avviso.